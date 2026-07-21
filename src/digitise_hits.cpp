// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// digitise_hits.cpp — Phlex module plugin
//
// Digitises simulated hits into per-detector reconstructed hit collections.
// A fresh counter-based RNG is seeded per event from (seed, event number),
// so results are reproducible under full concurrency.

#include "phlex/core/product_selector.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

#include <tuple>
#include <type_traits>

#include <SHiP/SimHit.hpp>
#include <SHiP/detectors/CaloHit.hpp>
#include <SHiP/detectors/DetectorID.hpp>
#include <SHiP/detectors/SBTHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
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

// Stream selector separating the digitiser's draws from other users of the
// same seed (cf. PhiloxRng's key_hi parameter).
constexpr std::uint32_t digitise_stream = 0xD161715E;

using DigitisedHit = std::variant<::SHiP::UBTHit, ::SHiP::SBTHit, ::SHiP::StrawTubesHit,
                                  ::SHiP::CaloHit, ::SHiP::TimeDetHit>;

using DigitisedHits = std::tuple<std::vector<::SHiP::UBTHit>, std::vector<::SHiP::SBTHit>,
                                 std::vector<::SHiP::StrawTubesHit>, std::vector<::SHiP::CaloHit>,
                                 std::vector<::SHiP::TimeDetHit>>;

class Digitiser {
   public:
    [[nodiscard]]
    DigitisedHits operator()(std::vector<::SHiP::SimHit> const& sim_hits,
                             Shannon::PhiloxRng& rng) const {
        DigitisedHits result;
        for (auto const& sim_hit : sim_hits) {
            std::visit(
                [&result](auto&& concrete_hit) {
                    using Hit = std::remove_cvref_t<decltype(concrete_hit)>;
                    std::get<std::vector<Hit>>(result).push_back(
                        std::forward<decltype(concrete_hit)>(concrete_hit));
                },
                digitise(sim_hit, rng));
        }
        return result;
    }

    [[nodiscard]]
    DigitisedHit digitise(::SHiP::SimHit const& hit, Shannon::PhiloxRng& rng) const {
        switch (static_cast<SHiP::DetectorID>(hit.detectorId)) {
            case SHiP::DetectorID::UpstreamTagger:
                return upstream_tagger_.digitise(hit, rng);
            case SHiP::DetectorID::SurroundTagger:
                return surround_tagger_.digitise(hit, rng);
            case SHiP::DetectorID::StrawTubes:
                return straw_tubes_.digitise(hit, rng);
            case SHiP::DetectorID::Calorimeter:
                return calorimeter_.digitise(hit, rng);
            case SHiP::DetectorID::TimingDetector:
                return timing_detector_.digitise(hit, rng);
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
    auto const layer = config.get<std::string>("layer");
    auto const seed = static_cast<std::uint32_t>(config.get<int>("seed", 0));

    m.transform(
         "digitise_hits",
         [seed, digitiser = Digitiser{}](data_cell_index const& id,
                                         std::vector<::SHiP::SimHit> const& sim_hits) {
             Shannon::PhiloxRng rng{seed, digitise_stream, static_cast<std::uint32_t>(id.number())};
             return digitiser(sim_hits, rng);
         },
         concurrency::unlimited)
        .input_family(
            product_selector{.creator = "rntuple_source", .layer = layer, .suffix = "id"},
            product_selector{.creator = "rntuple_source", .layer = layer, .suffix = "sim_hits"})
        // Positional: must match the element order of the DigitisedHits tuple.
        .output_product_suffixes("ubt_hits", "sbt_hits", "straw_tubes_hits", "calorimeter_hits",
                                 "timing_detector_hits");
};
