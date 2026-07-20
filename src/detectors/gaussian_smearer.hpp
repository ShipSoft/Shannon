// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// gaussian_smearer.hpp — placeholder smearing model shared by all detectors
//
// Applies a Gaussian x/y position smearing to a simulated hit. Detector
// digitisers delegate here until their detector-specific models become
// available.

#pragma once

#include <SHiP/RecHit.hpp>
#include <SHiP/SimHit.hpp>
#include <random>

namespace Shannon {

class GaussianSmearer {
   public:
    explicit GaussianSmearer(std::mt19937& rng, double sigma_x = 0.1, double sigma_y = 0.1)
        : rng_(rng), dx_(0.0, sigma_x), dy_(0.0, sigma_y) {}

    ::SHiP::RecHit smear(::SHiP::SimHit const& sim_hit) {
        auto reconstructed = SHiP::fromSimHit(sim_hit);
        reconstructed.position = {sim_hit.position[0] + dx_(rng_), sim_hit.position[1] + dy_(rng_),
                                  sim_hit.position[2]};

        return reconstructed;
    }

   private:
    std::mt19937& rng_;
    std::normal_distribution<double> dx_;
    std::normal_distribution<double> dy_;
};

}  // namespace Shannon
