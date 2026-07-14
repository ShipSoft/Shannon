// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// make_test_input.cpp — deterministic simulation-like input for smoke tests
//
// Writes an RNTuple in the layout produced by the simulation chain
// (sim_particles + sim_hits per event), with hits covering all detector
// IDs, so the digitisation workflow can run without Geant4 or aegir.

#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleWriter.hxx>

#include <SHiP/SimHit.hpp>
#include <SHiP/SimParticle.hpp>
#include <SHiP/detectors/detector_id.hpp>
#include <cstdint>
#include <cstdio>
#include <philox_rng.hpp>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::string const filename = argc > 1 ? argv[1] : "smoke_input.root";
    auto const n_events = argc > 2 ? std::stoul(argv[2]) : 10UL;

    auto model = ROOT::RNTupleModel::Create();
    auto particles = model->MakeField<std::vector<SHiP::SimParticle>>("sim_particles");
    auto hits = model->MakeField<std::vector<SHiP::SimHit>>("sim_hits");
    auto writer = ROOT::RNTupleWriter::Recreate(std::move(model), "events", filename);

    constexpr SHiP::detector_id detectors[] = {
        SHiP::detector_id::UpstreamTagger, SHiP::detector_id::SurroundTagger,
        SHiP::detector_id::StrawTubes, SHiP::detector_id::Calorimeter,
        SHiP::detector_id::TimingDetector};

    for (std::uint32_t event = 0; event < n_events; ++event) {
        Shannon::PhiloxRng rng{0, 0x7E57DA7A, event};

        particles->clear();
        hits->clear();

        auto const n_tracks = 1 + static_cast<int>(rng.uniform(0.0, 3.0));
        for (int track = 0; track < n_tracks; ++track) {
            SHiP::SimParticle particle;
            particle.trackId = track;
            particle.parentId = -1;
            particle.pdgCode = 13;
            particle.momentum = {rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5),
                                 rng.uniform(10.0, 100.0)};
            particle.energy = particle.momentum[2];
            particles->push_back(particle);

            for (auto const detector : detectors) {
                SHiP::SimHit hit;
                hit.detectorId = static_cast<std::int32_t>(detector);
                hit.trackId = track;
                hit.pdgCode = particle.pdgCode;
                hit.position = {rng.uniform(-500.0, 500.0), rng.uniform(-500.0, 500.0),
                                rng.uniform(0.0, 10000.0)};
                hit.momentum = particle.momentum;
                hit.energyDeposit = rng.uniform(0.0, 0.1);
                hit.time = rng.uniform(0.0, 100.0);
                hit.pathLength = rng.uniform(0.0, 10.0);
                hits->push_back(hit);
            }
        }
        writer->Fill();
    }

    std::printf("Wrote %lu events to %s\n", n_events, filename.c_str());
    return 0;
}
