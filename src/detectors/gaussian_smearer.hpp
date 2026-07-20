// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// gaussian_smearer.hpp — placeholder smearing model shared by all detectors
//
// Applies a Gaussian x/y position smearing to a simulated hit. Detector
// digitisers delegate here until their detector-specific models become
// available.

#pragma once

#include "philox_rng.hpp"

#include <SHiP/RecHit.hpp>
#include <SHiP/SimHit.hpp>

namespace Shannon {

class GaussianSmearer {
   public:
    explicit GaussianSmearer(double sigma_x = 0.1, double sigma_y = 0.1)
        : sigma_x_{sigma_x}, sigma_y_{sigma_y} {}

    ::SHiP::RecHit smear(::SHiP::SimHit const& sim_hit, PhiloxRng& rng) const {
        auto reconstructed = SHiP::fromSimHit(sim_hit);
        reconstructed.position = {sim_hit.position[0] + rng.gaussian(0.0, sigma_x_),
                                  sim_hit.position[1] + rng.gaussian(0.0, sigma_y_),
                                  sim_hit.position[2]};

        return reconstructed;
    }

   private:
    double sigma_x_;
    double sigma_y_;
};

}  // namespace Shannon
