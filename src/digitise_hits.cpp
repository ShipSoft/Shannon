// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// digitise_hits.cpp — Phlex module plugin
//
// Digitises simulated hits into per-detector reconstructed hit collections.

#include "phlex/core/product_selector.hpp"
#include "phlex/module.hpp"

#include <tuple>

#include <SHiP/SimHit.hpp>
#include <SHiP/detectors/CaloHit.hpp>
#include <SHiP/detectors/DetectorID.hpp>
#include <SHiP/detectors/SBTHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
#include <detectors/calorimeter.hpp>
#include <detectors/straw_tubes.hpp>
#include <detectors/surround_tagger.hpp>
#include <detectors/timing_detector.hpp>
#include <detectors/upstream_tagger.hpp>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace phlex;

namespace {

std::random_device rd{};
std::mt19937 rng{rd()};

using DigitisedHit = std::variant<::SHiP::UBTHit, ::SHiP::SBTHit, ::SHiP::StrawTubesHit,
                                  ::SHiP::CaloHit, ::SHiP::TimeDetHit>;

using DigitisedHits = std::tuple<std::vector<::SHiP::UBTHit>, std::vector<::SHiP::SBTHit>,
                                 std::vector<::SHiP::StrawTubesHit>, std::vector<::SHiP::CaloHit>,
                                 std::vector<::SHiP::TimeDetHit>>;

class Digitiser {
   public:
    explicit Digitiser(std::mt19937& rng)
        : upstream_tagger_{rng},
          surround_tagger_{rng},
          straw_tubes_{rng},
          calorimeter_{rng},
          timing_detector_{rng} {}

    [[nodiscard]]
    DigitisedHits operator()(std::vector<::SHiP::SimHit> const& sim_hits) {
        DigitisedHits result;
        for (auto const& sim_hit : sim_hits) {
            std::visit(
                [&result](auto&& concrete_hit) {
                    using Hit = std::remove_cvref_t<decltype(concrete_hit)>;
                    std::get<std::vector<Hit>>(result).push_back(
                        std::forward<decltype(concrete_hit)>(concrete_hit));
                },
                digitise(sim_hit));
        }
        return result;
    }

    [[nodiscard]]
    DigitisedHit digitise(::SHiP::SimHit const& hit) {
        switch (static_cast<SHiP::DetectorID>(hit.detectorId)) {
            case SHiP::DetectorID::UpstreamTagger:
                return upstream_tagger_.digitise(hit);
            case SHiP::DetectorID::SurroundTagger:
                return surround_tagger_.digitise(hit);
            case SHiP::DetectorID::StrawTubes:
                return straw_tubes_.digitise(hit);
            case SHiP::DetectorID::Calorimeter:
                return calorimeter_.digitise(hit);
            case SHiP::DetectorID::TimingDetector:
                return timing_detector_.digitise(hit);
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
    auto digitiser = std::make_shared<Digitiser>(rng);

    m.transform(
         "digitise_hits",
         [digitiser](std::vector<::SHiP::SimHit> const& sim_hits) {
             return (*digitiser)(sim_hits);
         },
         concurrency::serial)
        .input_family(
            product_selector{.creator = "rntuple_source", .layer = layer, .suffix = "sim_hits"})
        // Positional: must match the element order of the DigitisedHits tuple.
        .output_product_suffixes("ubt_hits", "sbt_hits", "straw_tubes_hits", "calorimeter_hits",
                                 "timing_detector_hits");
};
