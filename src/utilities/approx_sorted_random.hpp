#pragma once
// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// read_sim_file.cpp — Phlex source plugin
//
// This is a tool to generate random numbers approximately in order.
// It does so by sampling from an approximate beta distribution.
// The kth ordered statistic of a uniform distribution with n total
// draws is distributed as a beta function beta(k, n-k+1)

#include "common.hpp"

#include <SHiP/random/philox_rng.hpp>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class approx_sorted_random {
   public:
    approx_sorted_random(std::uint32_t seed, std::uint32_t stream = 0xBEEFCAFE,
                         const double maxTime = 1., const std::uint64_t nPoT = 10000)
        : m_maxTime(maxTime), m_n(checked_event_count(nPoT)), m_seed(seed), m_stream(stream) {
        if (m_maxTime <= 0.0)
            throw std::invalid_argument(
                "The maximum time of the spill fraction must be positive. Check your options.");
    };
    [[nodiscard]]
    double next(const std::uint64_t evtNumber) {
        // k = evtNumber + 1 must satisfy k <= m_n, otherwise Beta(k, m_n - k + 1)
        // has a non-positive shape parameter.
        if (evtNumber >= m_n)
            throw std::invalid_argument(
                "Trying to generate a time for an event beyond the number generated. Check your "
                "PoT.");
        const std::uint64_t k = evtNumber + 1;
        SHiP::random::PhiloxRng rng{m_seed, m_stream, k};
        return rng.beta_dist_approx(static_cast<double>(k), static_cast<double>(m_n - k + 1),
                                    SHiP::random::Precision::Bits53) *
               m_maxTime;
    }

   private:
    double m_maxTime = 0.;
    std::uint64_t m_n = 0;
    std::uint32_t m_seed = 0;
    std::uint32_t m_stream = 0;
};
