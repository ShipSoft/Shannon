// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// rntuple_entries_driver.cpp — Phlex driver plugin
//
// Emits one data cell per entry of the input RNTuple, so the event count comes
// from the input rather than from a hand-set total. Must be given the same
// input_file and ntuple_name as read_sim_file.

#include "phlex/configuration.hpp"
#include "phlex/driver.hpp"
#include "phlex/model/fixed_hierarchy.hpp"

#include <ROOT/RNTupleReader.hxx>
#include <TFile.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <vector>

PHLEX_REGISTER_DRIVER(d, config) {
    auto const input_file = config.get<std::string>("input_file");
    auto const ntuple_name = config.get<std::string>("ntuple_name");
    auto const layer = config.get<std::string>("layer", std::string{"spill"});

    {
        std::unique_ptr<TFile> const file{TFile::Open(input_file.c_str())};
        if (!file || file->IsZombie()) {
            throw std::runtime_error("rntuple_entries_driver: cannot open '" + input_file + "'");
        }
    }

    std::size_t n_events = ROOT::RNTupleReader::Open(ntuple_name, input_file)->GetNEntries();
    spdlog::info("rntuple_entries_driver: '{}' has {} entries", input_file, n_events);

    // Optional cap, to process just the first part of the input.
    if (auto const max_events = config.get_if_present<std::int64_t>("max_events")) {
        if (*max_events < 1) {
            throw std::invalid_argument("rntuple_entries_driver: max_events must be >= 1");
        }
        if (static_cast<std::size_t>(*max_events) < n_events) {
            n_events = static_cast<std::size_t>(*max_events);
        }
    }

    // phlex matches the first parameter's type exactly, so the cursor is taken
    // by value rather than const&.
    return d.driver(phlex::fixed_hierarchy{std::vector<std::vector<std::string>>{{"job", layer}}},
                    [n_events, layer](phlex::data_cell_cursor job) {
                        for (std::size_t i = 0; i < n_events; ++i) {
                            job.yield_child(layer, i);
                        }
                    });
}
