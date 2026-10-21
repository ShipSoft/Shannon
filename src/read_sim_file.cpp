// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// read_sim_file.cpp — Phlex source plugin
//
// Provides simulated particles and hits read from an RNTuple file.

#include "phlex/configuration.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"
#include "utilities/approx_sorted_random.hpp"
#include "utilities/sorted_random.hpp"

#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleView.hxx>
#include <TFile.h>
#include <TObjString.h>

#include <SHiP/SimHit.hpp>
#include <SHiP/SimParticle.hpp>
#include <SHiP/random/philox_rng.hpp>
#include <boost/json.hpp>
#include <cmath>
#include <cstdint>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace phlex;
using namespace phlex::experimental::literals;
constexpr std::uint32_t time_offset_stream = 0x71BD91A0;

// This is a class to generate numbers in an approximately ascending time order.
// The key point is that the nth order statistic of a uniform distribution is
// distributed according to a beta distribution.
// Because you are drawing from a distribution the results are not strictly ordered - but you
// can execute this in any order. The exact solution from Bentley and Saxe would have to be executed
// serially and there is no guarantee phlex will pass each row in sequence.
namespace {
template <typename T>
concept TimeGenerator = requires(T& g, std::uint64_t n) {
    { g.next(n) } -> std::convertible_to<double>;
};

template <TimeGenerator TimeGen>
void register_time_provider(auto& m, std::shared_ptr<TimeGen> gen,
                            std::shared_ptr<ROOT::RNTupleReader> reader,
                            std::shared_ptr<ROOT::RNTupleView<std::uint64_t>> view,
                            phlex::experimental::identifier const& layer) {
    // Provide a random time.
    m.provide(
         "provide_time",
         [gen = std::move(gen), reader = std::move(reader),
          view = std::move(view)](data_cell_index const& id) -> double {
             auto const entry_index = static_cast<ROOT::NTupleSize_t>(id.number());
             return gen->next((*view)(entry_index));
         },
         concurrency::serial)
        .output_product("rntuple_source", "time", layer);
}

std::optional<double> read_pot_from_summary(std::string const& filename) {
    std::unique_ptr<TFile> file{TFile::Open(filename.c_str(), "READ")};
    if (!file || file->IsZombie()) {
        throw std::runtime_error("cannot open " + filename);
    }

    const auto* json = file->Get<TObjString>("file_summary");
    if (!json) {
        return std::nullopt;  // older aegir file, no summary
    }

    auto const summary = boost::json::parse(json->GetString().Data()).as_object();
    if (summary.at("schema_version").to_number<int>() != 1) {
        throw std::runtime_error("unsupported file_summary schema_version");
    }

    auto const n_processed = summary.at("n_events_processed").to_number<std::uint64_t>();
    auto const n_written = summary.at("n_events_written").to_number<std::uint64_t>();
    auto const max_id = summary.at("max_event_number_saved").to_number<std::int64_t>();

    // Written can equal generated when nothing was filtered.
    if (n_written > n_processed) {
        throw std::runtime_error("file_summary: n_events_written (" + std::to_string(n_written) +
                                 ") exceeds n_events_processed (" + std::to_string(n_processed) +
                                 ")");
    }
    // Event ids are 0-based, so the largest valid id is n_processed - 1
    // (-1 if no events were written).
    if (max_id >= static_cast<std::int64_t>(n_processed))
        if (std::cmp_greater_equal(max_id, n_processed)) {
            throw std::runtime_error("file_summary: max_event_id_saved (" + std::to_string(max_id) +
                                     ") is not below n_events_processed (" +
                                     std::to_string(n_processed) + ")");
        }

    return static_cast<double>(n_processed);
}

}  // namespace

PHLEX_REGISTER_PROVIDERS(m, config) {
    auto const input_file = config.get<std::string>("input_file");
    auto const ntuple_name = config.get<std::string>("ntuple_name");
    auto const particles_field =
        config.get<std::string>("particles_field", std::string{"sim_particles"});
    auto const hits_field = config.get<std::string>("hits_field", std::string{"sim_hits"});
    auto const layer = phlex::experimental::identifier{config.get<std::string>("layer")};
    double const spill_time_ns = 1.2e9;         // Total length of a spill in ns
    double const nominal_pot_per_spill = 4e13;  // PoT per spill

    auto const time_gen_mode =
        config.get<std::string>("time_generator", std::string{"approximate"});

    // First try and get some useful numbers out of the input file and the config
    auto const pot_from_file = read_pot_from_summary(input_file);
    auto const pot_from_cfg = config.get_if_present<double>("pot");
    if (!pot_from_file && !pot_from_cfg)
        throw std::invalid_argument("no file_summary in input and no 'pot' in config");

    // File summary takes precedence but a warning just in case
    if (pot_from_file && pot_from_cfg && *pot_from_file != *pot_from_cfg)
        spdlog::warn(
            "read_sim_file: config pot ({}) disagrees with file_summary of '{}' ({}); "
            "using file_summary",
            *pot_from_cfg, input_file, *pot_from_file);
    double const pot_sim = pot_from_file ? *pot_from_file : *pot_from_cfg;
    double const high_time =
        spill_time_ns * pot_sim / nominal_pot_per_spill;  // Length of time simulated
    auto const n_events = static_cast<std::uint64_t>(std::llround(pot_sim));

    // Open the file through TFile once so ROOT learns the class layouts stored
    // in it. Without them, ROOT 6.40.04 aborts in RFieldMeta when data-model's
    // I/O rules have to convert an older class version (e.g. aegir 0.4 output,
    // written with data-model 0.5). RNTupleReader::Open by path does not load
    // them. The fix is on ROOT's 6.40 patches branch (root-project/root#23293)
    // but not in a release yet; drop this block once pixi.lock has it.
    // The layouts stay registered after the file is closed, so the readers
    // below keep their own independent file handles.
    {
        std::unique_ptr<TFile> const file{TFile::Open(input_file.c_str())};
        if (!file || file->IsZombie()) {
            throw std::runtime_error("read_sim_file: cannot open '" + input_file + "'");
        }
    }

    // Each provider owns its own reader: providers are separate graph nodes
    // that can run concurrently, and RNTupleReader is not thread-safe.
    std::shared_ptr<ROOT::RNTupleReader> particle_reader =
        ROOT::RNTupleReader::Open(ntuple_name, input_file);
    auto particle_view = std::make_shared<ROOT::RNTupleView<std::vector<SHiP::SimParticle>>>(
        particle_reader->GetView<std::vector<SHiP::SimParticle>>(particles_field));

    std::shared_ptr<ROOT::RNTupleReader> hit_reader =
        ROOT::RNTupleReader::Open(ntuple_name, input_file);
    auto hit_view = std::make_shared<ROOT::RNTupleView<std::vector<SHiP::SimHit>>>(
        hit_reader->GetView<std::vector<SHiP::SimHit>>(hits_field));

    std::shared_ptr<ROOT::RNTupleReader> evt_number_reader =
        ROOT::RNTupleReader::Open(ntuple_name, input_file);
    auto evt_number_view = std::make_shared<ROOT::RNTupleView<std::uint64_t>>(
        evt_number_reader->GetView<std::uint64_t>("event_number"));

    // Note this is a separate seed than the one used for the smearing in the detector digitisations
    auto const time_seed = static_cast<std::uint32_t>(config.get<int>("time_seed", 0));

    m.provide(
         "read_rntuple",
         [reader = std::move(particle_reader), view = std::move(particle_view)](
             data_cell_index const& id) -> std::vector<SHiP::SimParticle> {
             auto entry_index = static_cast<ROOT::NTupleSize_t>(id.number());
             return (*view)(entry_index);
         },
         concurrency::serial)
        .output_product("rntuple_source", "sim_particles", layer);

    m.provide(
         "read_rntuple_hits",
         [reader = std::move(hit_reader),
          view = std::move(hit_view)](data_cell_index const& id) -> std::vector<SHiP::SimHit> {
             auto entry_index = static_cast<ROOT::NTupleSize_t>(id.number());
             return (*view)(entry_index);
         },
         concurrency::serial)
        .output_product("rntuple_source", "sim_hits", layer);

    // Expose the cell index itself so downstream algorithms can seed
    // per-event counter-based RNGs from the event number.
    m.provide(
         "provide_id", [](data_cell_index const& id) { return id; }, concurrency::unlimited)
        .output_product("rntuple_source", "id", layer);
    if (time_gen_mode == "approximate") {
        register_time_provider(m,
                               std::make_shared<approx_sorted_random>(time_seed, time_offset_stream,
                                                                      high_time, n_events),
                               std::move(evt_number_reader), std::move(evt_number_view), layer);
    } else if (time_gen_mode == "exact") {
        register_time_provider(
            m, std::make_shared<sorted_random>(time_seed, time_offset_stream, high_time, n_events),
            std::move(evt_number_reader), std::move(evt_number_view), layer);
    } else {
        throw std::invalid_argument("time_generator must be 'approximate' or 'exact'. You put: " +
                                    time_gen_mode);
    }
}
