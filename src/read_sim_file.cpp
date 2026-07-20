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
#include <memory>
#include <string>
#include <vector>

using namespace phlex;
using namespace phlex::experimental::literals;

PHLEX_REGISTER_PROVIDERS(m, config) {
    auto const input_file = config.get<std::string>("input_file");
    auto const ntuple_name = config.get<std::string>("ntuple_name");
    auto const particles_field =
        config.get<std::string>("particles_field", std::string{"sim_particles"});
    auto const hits_field = config.get<std::string>("hits_field", std::string{"sim_hits"});
    auto const layer = phlex::experimental::identifier{config.get<std::string>("layer")};

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
}
