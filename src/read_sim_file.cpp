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
#include <stdio.h>
#include <string>

using namespace phlex;
using namespace phlex::experimental::literals;
PHLEX_REGISTER_PROVIDERS(m, config) {
    auto const input_file = config.get<std::string>("input_file");
    auto const ntuple_name = config.get<std::string>("ntuple_name");
    auto const field_name = config.get<std::string>("field_name");
    // auto const layer       = config.get<std::string>("layer");
    auto const layer = phlex::experimental::identifier{config.get<std::string>("layer")};
    auto reader = std::shared_ptr<ROOT::RNTupleReader>(
        ROOT::RNTupleReader::Open(ntuple_name, input_file).release());
    auto view = std::make_shared<ROOT::RNTupleView<std::vector<SHiP::SimParticle>>>(
        reader->GetView<std::vector<SHiP::SimParticle>>(field_name));

    auto view_hit = std::make_shared<ROOT::RNTupleView<std::vector<SHiP::SimHit>>>(
        reader->GetView<std::vector<SHiP::SimHit>>("sim_hits"));

    m.provide(
         "read_rntuple",
         [reader, view](data_cell_index const& id) -> std::vector<SHiP::SimParticle> {
             auto entry_index = static_cast<ROOT::NTupleSize_t>(id.number());
             return (*view)(entry_index);
         },
         concurrency::serial)
        .output_product("rntuple_source", "sim_particles", layer);

    m.provide(
         "read_rntuple_hits",
         [reader, view_hit](data_cell_index const& id) -> std::vector<SHiP::SimHit> {
             auto entry_index = static_cast<ROOT::NTupleSize_t>(id.number());
             return (*view_hit)(entry_index);
         },
         concurrency::serial)
        .output_product("rntuple_source", "sim_hits", layer);
}
