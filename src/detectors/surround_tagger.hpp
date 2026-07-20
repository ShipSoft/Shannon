// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// surround_tagger.hpp — Surrounding Background Tagger digitiser

#pragma once

#include "gaussian_smearer.hpp"

#include <SHiP/SimHit.hpp>
#include <SHiP/detectors/SBTHit.hpp>

namespace Shannon {

class SurroundTagger {
   public:
    // Placeholder: Gaussian smearing until the SBT digitisation model exists
    ::SHiP::SBTHit digitise(::SHiP::SimHit const& sim_hit, PhiloxRng& rng) const {
        return {.recHit = smearer_.smear(sim_hit, rng)};
    }

   private:
    GaussianSmearer smearer_;
};

}  // namespace Shannon
