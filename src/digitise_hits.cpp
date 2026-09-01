// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// digitise_hits.cpp — Phlex module plugin
//
// Digitises simulated hits into per-detector reconstructed hit collections.
// A fresh counter-based RNG is seeded per event from (seed, event number),
// so results are reproducible under full concurrency.

#include "ShannonConfig.h"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

#include <tuple>
#include <type_traits>

#include <SHiP/SimHit.hpp>
#include <SHiP/detectors/CaloHit.hpp>
#include <SHiP/detectors/SBTHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
#include <SHiP/detectors/detector_id.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <cstdint>
#include <detectors/calorimeter.hpp>
#include <detectors/straw_tubes.hpp>
#include <detectors/surround_tagger.hpp>
#include <detectors/timing_detector.hpp>
#include <detectors/upstream_tagger.hpp>
#include <philox_rng.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace phlex;

namespace {

// Stream selectors separating different draws on the same seed (cf.
// PhiloxRng's key_hi parameter) so they're uncorrelated.
constexpr std::uint32_t digitise_stream = 0xD161715E;
constexpr std::uint32_t time_offset_stream = 0x71BD91A0;

using DigitisedHit = std::variant<::SHiP::UBTHit, ::SHiP::SBTHit, ::SHiP::StrawTubesHit,
                                  ::SHiP::CaloHit, ::SHiP::TimeDetHit>;

using DigitisedHits = std::tuple<std::vector<::SHiP::UBTHit>, std::vector<::SHiP::SBTHit>,
                                 std::vector<::SHiP::StrawTubesHit>, std::vector<::SHiP::CaloHit>,
                                 std::vector<::SHiP::TimeDetHit>>;

class Digitiser {
   public:
    [[nodiscard]]
    DigitisedHits operator()(std::vector<::SHiP::SimHit> const& sim_hits, double event_time_offset,
                             Shannon::PhiloxRng& rng) const {
        DigitisedHits result;
        for (auto const& sim_hit : sim_hits) {
            std::visit(
                [&result](auto&& concrete_hit) {
                    using Hit = std::remove_cvref_t<decltype(concrete_hit)>;
                    std::get<std::vector<Hit>>(result).push_back(
                        std::forward<decltype(concrete_hit)>(concrete_hit));
                },
                digitise(sim_hit, event_time_offset, rng));
        }
        return result;
    }

    [[nodiscard]]
    DigitisedHit digitise(::SHiP::SimHit const& hit, double event_time_offset,
                          Shannon::PhiloxRng& rng) const {
        switch (static_cast<SHiP::detector_id>(hit.detectorId)) {
            case SHiP::detector_id::UpstreamTagger:
                return upstream_tagger_.digitise(hit, event_time_offset, rng);
            case SHiP::detector_id::SurroundTagger:
                return surround_tagger_.digitise(hit, event_time_offset, rng);
            case SHiP::detector_id::StrawTubes:
                return straw_tubes_.digitise(hit, event_time_offset, rng);
            case SHiP::detector_id::Calorimeter:
                return calorimeter_.digitise(hit, event_time_offset, rng);
            case SHiP::detector_id::TimingDetector:
                return timing_detector_.digitise(hit, event_time_offset, rng);
        }
        throw std::runtime_error{"No digitiser registered for detector ID " +
                                 std::to_string(hit.detectorId)};
    }

   private:
    Shannon::UpstreamTagger upstream_tagger_;
    Shannon::SurroundTagger surround_tagger_;
    Shannon::StrawTubes straw_tubes_;
    Shannon::Calorimeter calorimeter_;
    Shannon::TimingDetector timing_detector_;
};

}  // namespace

PHLEX_REGISTER_ALGORITHMS(m, config) {
    printVersion();
    auto const layer = config.get<std::string>("layer");
    auto const seed = static_cast<std::uint32_t>(config.get<int>("seed", 0));

    using BigFloat = boost::multiprecision::cpp_dec_float_50;
    BigFloat const pot_sim{config.get<int>("pot", 10000)};  // Simulated protons on target
    BigFloat const spill_time_ns{"1.2e9"};                  // Total length of a spill in ns
    BigFloat const nominal_pot_per_spill{"4e13"};           // PoT per spill

    double const high_time = static_cast<double>(
        spill_time_ns * pot_sim / nominal_pot_per_spill);  // Length of time simulated

    m.transform(
         "digitise_hits",
         [seed, high_time, digitiser = Digitiser{}](data_cell_index const& id,
                                                    std::vector<::SHiP::SimHit> const& sim_hits) {
             Shannon::PhiloxRng time_rng{seed, time_offset_stream,
                                         static_cast<std::uint32_t>(id.number())};
             double event_time_offset = time_rng.uniform(0.0, high_time);
             Shannon::PhiloxRng rng{seed, digitise_stream, static_cast<std::uint32_t>(id.number())};
             return digitiser(sim_hits, event_time_offset, rng);
         },
         concurrency::unlimited)
        .input_family(
            product_selector{.creator = "rntuple_source", .layer = layer, .suffix = "id"},
            product_selector{.creator = "rntuple_source", .layer = layer, .suffix = "sim_hits"})
        // Positional: must match the element order of the DigitisedHits tuple.
        .output_product_suffixes("ubt_hits", "sbt_hits", "straw_tubes_hits", "calorimeter_hits",
                                 "timing_detector_hits");
};
