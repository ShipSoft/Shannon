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
#include <SHiP/detectors/SBTHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
#include <SHiP/detectors/detector_id.hpp>
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

class megaNum {
   public:
    megaNum() : m_base(0), m_exponent(0) {}

    explicit megaNum(const double& inNum) {
        if (inNum == 0) {
            m_base = 0.0;
            m_exponent = 0;
            return;
        }
        m_exponent = static_cast<int>(std::floor(std::log10(static_cast<double>(inNum))));
        m_base = static_cast<double>(inNum) / std::pow(10.0, m_exponent);
        normalise();
    };

    explicit megaNum(int value) : megaNum(static_cast<double>(value)) {}

    megaNum(const double& _base, const int& _expo) : m_base(_base), m_exponent(_expo) {
        normalise();
    }

    operator double() { return m_base * pow(10., m_exponent); }

    megaNum operator+(const megaNum& rhs) const {
        if (m_base == 0.0)
            return rhs;
        if (rhs.m_base == 0.0)
            return *this;

        megaNum out;
        int diff = m_exponent - rhs.m_exponent;

        if (diff >= 0) {
            out.m_exponent = m_exponent;
            out.m_base = m_base + rhs.m_base * std::pow(10.0, -diff);
        } else {
            out.m_exponent = rhs.m_exponent;
            out.m_base = m_base * std::pow(10.0, diff) + rhs.m_base;
        }
        out.normalise();
        return out;
    }

    megaNum operator-(const megaNum& rhs) const {
        if (m_base == 0.0)
            return rhs;
        if (rhs.m_base == 0.0)
            return *this;

        megaNum out;
        int diff = m_exponent - rhs.m_exponent;

        if (diff >= 0) {
            out.m_exponent = m_exponent;
            out.m_base = m_base - rhs.m_base * std::pow(10.0, -diff);
        } else {
            out.m_exponent = rhs.m_exponent;
            out.m_base = m_base * std::pow(10.0, diff) - rhs.m_base;
        }
        out.normalise();
        return out;
    }

    megaNum operator/(const megaNum& rhs) const {
        if (rhs.m_base == 0.0)
            throw std::runtime_error("Division by zero");
        megaNum returnNum(m_base / rhs.m_base, m_exponent - rhs.m_exponent);
        returnNum.normalise();
        return returnNum;
    }

    megaNum operator*(const megaNum& rhs) const {
        megaNum returnNum(m_base * rhs.m_base, m_exponent + rhs.m_exponent);
        returnNum.normalise();
        return returnNum;
    }

    int getExpo() { return m_exponent; }
    double getBase() { return m_base; }

   private:
    void normalise() {
        if (m_base == 0.0) {
            m_exponent = 0;
            return;
        }
        while (std::abs(m_base) >= 10.0) {
            m_base /= 10.0;
            ++m_exponent;
        }
        while (std::abs(m_base) < 1.0) {
            m_base *= 10.0;
            --m_exponent;
        }
    }
    int m_exponent = 1;
    double m_base = 0.;
};

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
    DigitisedHits operator()(std::vector<::SHiP::SimHit> const& sim_hits, const double& time_offset,
                             Shannon::PhiloxRng& rng) const {
        DigitisedHits result;
        for (auto const& sim_hit : sim_hits) {
            std::visit(
                [&result](auto&& concrete_hit) {
                    using Hit = std::remove_cvref_t<decltype(concrete_hit)>;
                    std::get<std::vector<Hit>>(result).push_back(
                        std::forward<decltype(concrete_hit)>(concrete_hit));
                },
                digitise(sim_hit, time_offset, rng));
        }
        return result;
    }

    [[nodiscard]]
    DigitisedHit digitise(::SHiP::SimHit const& hit, double const& time_offset,
                          Shannon::PhiloxRng& rng) const {
        switch (static_cast<SHiP::detector_id>(hit.detectorId)) {
            case SHiP::detector_id::UpstreamTagger:
                return upstream_tagger_.digitise(hit, time_offset, rng);
            case SHiP::detector_id::SurroundTagger:
                return surround_tagger_.digitise(hit, time_offset, rng);
            case SHiP::detector_id::StrawTubes:
                return straw_tubes_.digitise(hit, time_offset, rng);
            case SHiP::detector_id::Calorimeter:
                return calorimeter_.digitise(hit, time_offset, rng);
            case SHiP::detector_id::TimingDetector:
                return timing_detector_.digitise(hit, time_offset, rng);
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
    auto const pot_sim = static_cast<megaNum>(config.get<int>("pot", 10000));
    megaNum spillTime(1.2, 9);
    megaNum spillPoT(4., 13);

    double high_time = spillTime * pot_sim / spillPoT;

    m.transform(
         "digitise_hits",
         [seed, high_time, digitiser = Digitiser{}](data_cell_index const& id,
                                                    std::vector<::SHiP::SimHit> const& sim_hits) {
             Shannon::PhiloxRng rng{seed, digitise_stream, static_cast<std::uint32_t>(id.number())};
             double time_offset = rng.uniform(0.0, high_time);
             return digitiser(sim_hits, time_offset, rng);
         },
         concurrency::unlimited)
        .input_family(
            product_selector{.creator = "rntuple_source", .layer = layer, .suffix = "id"},
            product_selector{.creator = "rntuple_source", .layer = layer, .suffix = "sim_hits"})
        // Positional: must match the element order of the DigitisedHits tuple.
        .output_product_suffixes("ubt_hits", "sbt_hits", "straw_tubes_hits", "calorimeter_hits",
                                 "timing_detector_hits");
};
