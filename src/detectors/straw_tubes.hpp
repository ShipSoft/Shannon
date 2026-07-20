// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// straw_tubes.hpp — Straw Tube tracker digitiser

#pragma once

#include "gaussian_smearer.hpp"

#include <SHiP/SimHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>

namespace Shannon {

class StrawTubes {
   public:
    // Placeholder: Gaussian smearing until the straw-tube digitisation model exists
    ::SHiP::StrawTubesHit digitise(::SHiP::SimHit const& sim_hit, PhiloxRng& rng) const {
        return {.recHit = smearer_.smear(sim_hit, rng)};
    }

   private:
    GaussianSmearer smearer_;
};

}  // namespace Shannon
