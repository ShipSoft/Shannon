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

#include <SHiP/EventHeader.hpp>
#include <SHiP/SimHit.hpp>
#include <SHiP/SimParticle.hpp>
#include <SHiP/random/philox_rng.hpp>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
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
                            std::shared_ptr<ROOT::RNTupleView<SHiP::EventHeader>> view,
                            phlex::experimental::identifier const& layer) {
    // Provide a random time.
    m.provide(
         "provide_time",
         [gen = std::move(gen), reader = std::move(reader),
          view = std::move(view)](data_cell_index const& id) -> double {
             auto entry_index = static_cast<ROOT::NTupleSize_t>(id.number());
             return gen->next((*view)(entry_index).aegir_event_id);
         },
         concurrency::serial)
        .output_product("rntuple_source", "time", layer);
}

}  // namespace

PHLEX_REGISTER_PROVIDERS(m, config) {
    auto const input_file = config.get<std::string>("input_file");
    auto const ntuple_name = config.get<std::string>("ntuple_name");
    auto const particles_field =
        config.get<std::string>("particles_field", std::string{"sim_particles"});
    auto const hits_field = config.get<std::string>("hits_field", std::string{"sim_hits"});
    auto const layer = phlex::experimental::identifier{config.get<std::string>("layer")};
    double const pot_sim{config.get<double>("pot", 10000)};  // Simulated protons on target
    double const spill_time_ns = 1.2e9;                      // Total length of a spill in ns
    double const nominal_pot_per_spill = 4e13;               // PoT per spill
    // Negated inclusive range check so that NaN is rejected. Must run before pot_sim is
    // used or converted to an integer.
    if (!(pot_sim >= 1.0 && pot_sim <= nominal_pot_per_spill))
        throw std::invalid_argument("pot must be between 1 and one spill (4e13).");
    auto const n_events = static_cast<std::uint64_t>(std::llround(pot_sim));
    auto const time_gen_mode =
        config.get<std::string>("time_generator", std::string{"approximate"});

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

    std::shared_ptr<ROOT::RNTupleReader> evt_header_reader =
        ROOT::RNTupleReader::Open(ntuple_name, input_file);
    auto evt_header_view = std::make_shared<ROOT::RNTupleView<SHiP::EventHeader>>(
        evt_header_reader->GetView<SHiP::EventHeader>("event_header"));

    auto const seed = static_cast<std::uint32_t>(config.get<int>("seed", 0));

    double const high_time =
        spill_time_ns * pot_sim / nominal_pot_per_spill;  // Length of time simulated

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
        register_time_provider(
            m,
            std::make_shared<approx_sorted_random>(seed, time_offset_stream, high_time, n_events),
            std::move(evt_header_reader), std::move(evt_header_view), layer);
    } else if (time_gen_mode == "exact") {
        register_time_provider(
            m, std::make_shared<sorted_random>(seed, time_offset_stream, high_time, n_events),
            std::move(evt_header_reader), std::move(evt_header_view), layer);
    } else {
        throw std::invalid_argument("time_generator must be 'approximate' or 'exact'. You put: " +
                                    time_gen_mode);
    }
}
