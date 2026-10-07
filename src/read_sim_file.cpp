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

#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleView.hxx>

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
// Validate the requested event count while it is still a double: converting an
// out-of-range double to an integer is undefined behaviour. 2^53 is the largest
// range over which a double holds every integer exactly.
std::uint64_t checked_event_count(double nToGen) {
    constexpr double max_count = 9007199254740992.0;  // 2^53
    if (!std::isfinite(nToGen) || nToGen < 1.0)
        throw std::invalid_argument("The number of events to run over must be positive.");
    if (nToGen > max_count)
        throw std::invalid_argument("The number of events to run over must not exceed 2^53.");
    return static_cast<std::uint64_t>(std::llround(nToGen));
}

class ascendingTimeGenerator {
   public:
    ascendingTimeGenerator(double nToGen, double maxTime, std::uint32_t seed, std::uint32_t stream)
        : m_maxTime(maxTime), m_n(checked_event_count(nToGen)), m_seed(seed), m_stream(stream) {
        if (m_maxTime <= 0.0)
            throw std::invalid_argument(
                "The maximum time of the spill fraction must be positive. Check your options.");
    };
    [[nodiscard]]
    double next(const std::uint64_t evtNumber) {
        // k = evtNumber + 1 must satisfy k <= m_n, otherwise Beta(k, m_n - k + 1)
        // has a non-positive shape parameter.
        if (evtNumber >= m_n)
            throw std::invalid_argument(
                "Trying to generate a time for an event beyond the number generated. Check your "
                "PoT.");
        const std::uint64_t k = evtNumber + 1;
        SHiP::random::PhiloxRng rng{m_seed, m_stream, k};
        return rng.beta_dist_approx(static_cast<double>(k), static_cast<double>(m_n - k + 1),
                                    SHiP::random::Precision::Bits53) *
               m_maxTime;
    }

   private:
    double m_maxTime = 0.;
    std::uint64_t m_n = 0;
    std::uint32_t m_seed = 0;
    std::uint32_t m_stream = 0;
};
}  // namespace

PHLEX_REGISTER_PROVIDERS(m, config) {
    auto const input_file = config.get<std::string>("input_file");
    auto const ntuple_name = config.get<std::string>("ntuple_name");
    auto const particles_field =
        config.get<std::string>("particles_field", std::string{"sim_particles"});
    auto const hits_field = config.get<std::string>("hits_field", std::string{"sim_hits"});
    auto const layer = phlex::experimental::identifier{config.get<std::string>("layer")};
    double const pot_sim{config.get<double>("pot", 10000)};  // Simulated protons on target
    double const nEntries{
        config.get<double>("entries", 10000)};  // How many indices you are running over
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

    double const spill_time_ns = 1.2e9;         // Total length of a spill in ns
    double const nominal_pot_per_spill = 4e13;  // PoT per spill
    auto const seed = static_cast<std::uint32_t>(config.get<int>("seed", 0));
    if (pot_sim > nominal_pot_per_spill)
        throw std::runtime_error("Provided simulated PoT is greater than a single spill");

    double const high_time =
        spill_time_ns * pot_sim / nominal_pot_per_spill;  // Length of time simulated

    auto timeGenerator =
        std::make_shared<ascendingTimeGenerator>(nEntries, high_time, seed, time_offset_stream);

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

    // Provide a random time.
    m.provide(
         "provide_time",
         [timeGenerator](data_cell_index const& id) -> double {
             return timeGenerator->next(id.number());
         },
         concurrency::unlimited)
        .output_product("rntuple_source", "time", layer);
}
