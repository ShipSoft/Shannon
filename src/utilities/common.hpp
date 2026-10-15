#pragma once

// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// read_sim_file.cpp — Phlex source plugin
//
// Some common utility functions.

#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Validate the requested event count while it is still a double: converting an
// out-of-range double to an integer is undefined behaviour. 2^53 is the largest
// range over which a double holds every integer exactly.
std::uint64_t checked_event_count(double nToGen) {
    constexpr double max_count = 9007199254740992.0;  // 2^53
    if (!std::isfinite(nToGen) || nToGen < 1.0)
        throw std::invalid_argument("The number of events to run over must be positive.");
    if (nToGen > max_count)
        throw std::invalid_argument("The number of events to run over must not exceed 2^53.");
    return static_cast<std::uint64_t>(std::llround(nToGen));
}