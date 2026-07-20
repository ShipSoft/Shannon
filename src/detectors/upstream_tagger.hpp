// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// upstream_tagger.hpp — Upstream Background Tagger digitiser

#pragma once

#include "gaussian_smearer.hpp"

#include <SHiP/SimHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>

namespace Shannon {

class UpstreamTagger {
   public:
    // Placeholder: Gaussian smearing until the UBT digitisation model exists
    ::SHiP::UBTHit digitise(::SHiP::SimHit const& sim_hit, PhiloxRng& rng) const {
        return {.recHit = smearer_.smear(sim_hit, rng)};
    }

   private:
    GaussianSmearer smearer_;
};

}  // namespace Shannon
