// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// calorimeter.hpp — Calorimeter digitiser

#pragma once

#include "gaussian_smearer.hpp"

#include <SHiP/SimHit.hpp>
#include <SHiP/detectors/CaloHit.hpp>

namespace Shannon {

class Calorimeter {
   public:
    // Placeholder: Gaussian smearing until the calorimeter digitisation model exists
    ::SHiP::CaloHit digitise(::SHiP::SimHit const& sim_hit, PhiloxRng& rng) const {
        return {.recHit = smearer_.smear(sim_hit, rng)};
    }

   private:
    GaussianSmearer smearer_;
};

}  // namespace Shannon
