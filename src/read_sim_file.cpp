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

#include <philox_rng.hpp>

#include <SHiP/SimHit.hpp>
#include <SHiP/SimParticle.hpp>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

using namespace phlex;
using namespace phlex::experimental::literals;
constexpr std::uint32_t time_offset_stream = 0x71BD91A0;

namespace{
    class ascendingTimeGenerator {
        public:
            ascendingTimeGenerator(const double& nToGen, const double& maxTime) : m_I(nToGen), m_maxTime(maxTime){};
            [[nodiscard]]
            double next(::Shannon::PhiloxRng rng){
                if(m_I <= 0)
                    throw std::invalid_argument("Shannon time randomisation: tried to process an event beyond the anticipated number.");
                m_lnCurrMax += log(rng.uniform53(0., 1.))/m_I;
                --m_I;
                return (1. - exp(m_lnCurrMax)) * m_maxTime ;
            }
        private:
            double m_lnCurrMax = 0.;
            double m_maxTime = 0.;
            int m_I = 0.;
    };
}


PHLEX_REGISTER_PROVIDERS(m, config) {
    auto const input_file = config.get<std::string>("input_file");
    auto const ntuple_name = config.get<std::string>("ntuple_name");
    auto const particles_field =
        config.get<std::string>("particles_field", std::string{"sim_particles"});
    auto const hits_field = config.get<std::string>("hits_field", std::string{"sim_hits"});
    auto const layer = phlex::experimental::identifier{config.get<std::string>("layer")};
    double const pot_sim{config.get<double>("pot", 10000)};  // Simulated protons on target
    double const nEntries{config.get<double>("entries", 10000)};  // How many indices you are running over
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

    double const spill_time_ns = 1.2e9;                      // Total length of a spill in ns
    double const nominal_pot_per_spill = 4e13;               // PoT per spill
    auto const seed = static_cast<std::uint32_t>(config.get<int>("seed", 0));
    if (pot_sim > nominal_pot_per_spill)
        throw std::runtime_error("Provided simulated PoT is greater than a single spill");

    double const high_time =
        spill_time_ns * pot_sim / nominal_pot_per_spill;  // Length of time simulated

    Shannon::PhiloxRng time_rng{seed, time_offset_stream};
    auto timeGenerator = std::make_shared<ascendingTimeGenerator>(nEntries, high_time);

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

    // Provide a random time. Has to be done in serial to keep increasing time order
    m.provide(
        "provide_time", [timeGenerator, time_rng](data_cell_index const& id) -> double {
            return timeGenerator->next(time_rng);
        }, concurrency::serial)
        .output_product("rntuple_source", "time", layer);
}
