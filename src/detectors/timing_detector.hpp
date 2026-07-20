// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// timing_detector.hpp — Timing Detector digitiser

#pragma once

#include "gaussian_smearer.hpp"

#include <SHiP/SimHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>

namespace Shannon {

class TimingDetector {
   public:
    // Placeholder: Gaussian smearing until the timing-detector digitisation model exists
    ::SHiP::TimeDetHit digitise(::SHiP::SimHit const& sim_hit, PhiloxRng& rng) const {
        return {.recHit = smearer_.smear(sim_hit, rng)};
    }

   private:
    GaussianSmearer smearer_;
};

}  // namespace Shannon
