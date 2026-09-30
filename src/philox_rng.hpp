// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// philox_rng.hpp — counter-based RNG for reproducible digitisation
//
// Random123 Philox 4x32 is deterministic per seed with no shared state, so
// each event seeds a fresh instance and digitisation is reproducible and
// thread-safe by construction (ported from aegir).
//
// The counter advances sequentially (ctr[0]++ per 4-word block) and the
// Philox output block is buffered; uniform() returns successive words of the
// buffered output.

#pragma once

#include <Random123/philox.h>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>

namespace Shannon {

// Resolution of the uniform draws feeding the derived distributions
// (gaussian, gamma_wh, beta_dist). Bits32 is the historical default and keeps
// existing digitisation output bit-identical; Bits53 uses uniform53() and
// extends the Box–Muller tail from ~6.7σ to ~8.6σ at the cost of twice as
// many Philox words per deviate.
enum class Precision { Bits32, Bits53 };

class PhiloxRng {
   public:
    // key_hi selects an independent stream, so different generators seeded
    // with the same seed draw uncorrelated sequences. substream initializes
    // the counter, giving each (seed, key_hi, substream) triple a disjoint
    // counter range — use it for per-event sub-streams of one seed without
    // perturbing the key (a key derived as seed ^ event would collide across
    // seeds: XOR is not injective in (seed, event)).
    // The sub-stream index is 64 bits wide, split over counter words 1 (low)
    // and 2 (high), so event numbers beyond 2^32 don't wrap onto earlier
    // events. Indices below 2^32 leave word 2 at zero, as before.
    explicit PhiloxRng(std::uint32_t seed, std::uint32_t key_hi = 0xBEEFCAFE,
                       std::uint64_t substream = 0)
        : key_{{seed, key_hi}},
          ctr_{{0, static_cast<std::uint32_t>(substream),
                static_cast<std::uint32_t>(substream >> 32), 0}} {}

    double uniform() {
        if (idx_ >= 4) {
            buf_ = rng_(ctr_, key_);
            ctr_[0]++;
            idx_ = 0;
        }
        // Map a 32-bit word to [0, 1)
        return buf_[idx_++] * (1.0 / 4294967296.0);
    }

    double uniform(double lo, double hi) { return lo + (hi - lo) * uniform(); }

    // Higher-resolution draw: combines two consecutive 32-bit Philox words
    // into a 64-bit integer and keeps the top 53 bits, matching double's
    // full mantissa precision (cf. uniform(), which only has 32 bits of
    // resolution — too coarse when the sampled range spans many orders of
    // magnitude, e.g. ps-scale jitter within a multi-second spill).
    double uniform53() {
        if (idx_ > 2) {  // fewer than 2 words left in the buffer
            buf_ = rng_(ctr_, key_);
            ctr_[0]++;
            idx_ = 0;
        }
        std::uint64_t const hi = buf_[idx_++];
        std::uint64_t const lo = buf_[idx_++];
        std::uint64_t const bits = (hi << 32) | lo;
        return static_cast<double>(bits >> 11) * (1.0 / 9007199254740992.0);  // top 53 bits / 2^53
    }

    double uniform53(double lo, double hi) { return lo + (hi - lo) * uniform53(); }

    // Box–Muller transform; uses the cosine branch only, drawing two
    // uniforms per deviate. uniform() returns [0, 1), so flip the first
    // draw to (0, 1] to keep the log argument nonzero.
    double gaussian(double mean, double sigma, Precision precision = Precision::Bits32) {
        double u1 = 1.0 - draw(precision);
        double u2 = draw(precision);
        return mean +
               sigma * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
    }

    // An approximation of a gamma function
    double gamma_wh(double alpha, double scale = 1, Precision precision = Precision::Bits32) {
        if (alpha <= 0.0)
            throw std::invalid_argument(
                "Provided alpha for gamma function approximation must be greater than 0.");
        const double a = 1.0 - 1.0 / (9.0 * alpha);
        const double b = 1.0 / (3.0 * std::sqrt(alpha));

        double x;

        do {
            x = a + b * gaussian(0.0, 1.0, precision);
        } while (x <= 0.0);

        return scale * alpha * x * x * x;
    }

    // An approximation of a beta distribution
    double beta_dist(double alpha, double zeta, Precision precision = Precision::Bits32) {
        const double X = gamma_wh(alpha, 1, precision);
        const double Y = gamma_wh(zeta, 1, precision);
        return X / (X + Y);
    }

   private:
    double draw(Precision precision) {
        return precision == Precision::Bits53 ? uniform53() : uniform();
    }

    r123::Philox4x32 rng_;
    r123::Philox4x32::key_type key_;
    r123::Philox4x32::ctr_type ctr_;
    r123::Philox4x32::ctr_type buf_{};
    int idx_ = 4;
};

}  // namespace Shannon
