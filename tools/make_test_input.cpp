// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// make_test_input.cpp — deterministic simulation-like input for smoke tests
//
// Writes an RNTuple in the layout produced by the simulation chain
// (sim_particles + sim_hits per event), with hits covering all detector
// IDs, so the digitisation workflow can run without Geant4 or aegir.

#include <ROOT/RFile.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleWriter.hxx>
#include <TObjString.h>

#include <SHiP/EventHeader.hpp>
#include <SHiP/SimHit.hpp>
#include <SHiP/SimParticle.hpp>
#include <SHiP/detectors/detector_id.hpp>
#include <SHiP/random/philox_rng.hpp>
#include <boost/json.hpp>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::string const filename = argc > 1 ? argv[1] : "smoke_input.root";
    auto const n_events = argc > 2 ? std::stoul(argv[2]) : 10UL;
    // The event loop counts (and seeds the RNG) with a uint64_t.
    if (n_events > std::numeric_limits<std::uint64_t>::max()) {
        std::fprintf(stderr, "n_events out of range: %lu\n", n_events);
        return 1;
    }

    auto model = ROOT::RNTupleModel::Create();
    auto const particles = model->MakeField<std::vector<SHiP::SimParticle>>("sim_particles");
    auto const hits = model->MakeField<std::vector<SHiP::SimHit>>("sim_hits");
    auto const header = model->MakeField<SHiP::EventHeader>("event_header");
    auto const evt_number = model->MakeField<std::uint64_t>("event_number");
    auto writer = ROOT::RNTupleWriter::Recreate(std::move(model), "events", filename);

    constexpr SHiP::detector_id detectors[] = {
        SHiP::detector_id::UpstreamTagger, SHiP::detector_id::SurroundTagger,
        SHiP::detector_id::StrawTubes, SHiP::detector_id::Calorimeter,
        SHiP::detector_id::TimingDetector};

    for (std::uint64_t event = 0; event < n_events; ++event) {
        SHiP::random::PhiloxRng rng{0, 0x7E57DA7A, event};

        particles->clear();
        hits->clear();
        *header = SHiP::EventHeader{};
        *evt_number = event;

        auto const n_tracks = 1 + static_cast<int>(rng.uniform(0.0, 3.0));
        for (int track = 0; track < n_tracks; ++track) {
            SHiP::SimParticle particle;
            particle.track_id = track;
            particle.parent_id = -1;
            particle.pdg_code = 13;
            particle.momentum = {rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5),
                                 rng.uniform(10.0, 100.0)};
            particle.energy = particle.momentum[2];
            particles->push_back(particle);

            for (auto const detector : detectors) {
                SHiP::SimHit hit;
                hit.detector_id = static_cast<std::int32_t>(detector);
                hit.track_id = track;
                hit.pdg_code = particle.pdg_code;
                hit.position = {rng.uniform(-500.0, 500.0), rng.uniform(-500.0, 500.0),
                                rng.uniform(0.0, 10000.0)};
                hit.momentum = particle.momentum;
                hit.energy_deposit = rng.uniform(0.0, 0.1);
                hit.time = rng.uniform(0.0, 100.0);
                hit.path_length = rng.uniform(0.0, 10.0);
                hits->push_back(hit);
            }
        }
        writer->Fill();
    }
    writer.reset();
    boost::json::object const summary{
        {"schema_version", 1},
        {"producer", "aegir"},
        {"mode", "MC"},
        {"filter_empty", true},
        {"n_events_requested", n_events},
        {"n_events_processed", n_events},
        {"n_events_written", n_events},
        {"max_event_number_saved", n_events - 1},
    };

    auto file = ROOT::Experimental::RFile::Update(filename);
    TObjString const json{boost::json::serialize(summary).c_str()};
    file->Put("file_summary", json);

    std::printf("Wrote %lu events to %s\n", n_events, filename.c_str());
    return 0;
}
