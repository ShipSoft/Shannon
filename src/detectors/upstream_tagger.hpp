// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// upstream_tagger.hpp — Upstream Background Tagger digitiser

#pragma once

#include "gaussian_smearer.hpp"

#include <SHiP/SimHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
#include <random>

namespace Shannon {

class UpstreamTagger {
   public:
    explicit UpstreamTagger(std::mt19937& rng, double sigma_x = 0.1, double sigma_y = 0.1)
        : smearer_{rng, sigma_x, sigma_y} {}

    // Placeholder: Gaussian smearing until the UBT digitisation model exists
    ::SHiP::UBTHit digitise(::SHiP::SimHit const& sim_hit) {
        return {.recHit = smearer_.smear(sim_hit)};
    }

   private:
    GaussianSmearer smearer_;
};

}  // namespace Shannon
