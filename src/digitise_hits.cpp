#include <iostream>
#include <random>
#include <vector>

#include "phlex/module.hpp"
#include "phlex/core/product_selector.hpp"

#include <SHiP/SimHit.hpp>
#include <SHiP/RecHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <SHiP/detectors/CaloHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
#include <SHiP/detectors/SBTHit.hpp>
#include <SHiP/detectors/DetectorID.hpp>

#include <detectors/calorimeter.hpp>
#include <detectors/straw_tubes.hpp>
#include <detectors/upstream_tagger.hpp>
#include <detectors/timing_detector.hpp>
#include <detectors/surround_tagger.hpp>

using namespace phlex;

std::random_device rd{};
std::mt19937 rng{rd()};

using DigitisedHit = std::variant<
    ::SHiP::UBTHit,
    ::SHiP::SBTHit,
    ::SHiP::StrawTubesHit,
    ::SHiP::CaloHit,
    ::SHiP::TimeDetHit >;

using DigitisedHits = std::tuple<
    std::vector<::SHiP::UBTHit>,
    std::vector<::SHiP::SBTHit>,
    std::vector<::SHiP::StrawTubesHit>,
    std::vector<::SHiP::CaloHit>,
    std::vector<::SHiP::TimeDetHit> >;

using DigitiseFunction = std::function<DigitisedHit(::SHiP::SimHit const&)>;

namespace Shannon {

class Digitiser {
public:
    explicit Digitiser(std::mt19937& rng)
        : straw_tubes_{rng},
          calorimeter_{rng},
          upstream_tagger_{rng},
          surround_tagger_{rng},
          timing_detector_{rng}
    {
        register_detector(SHiP::DetectorID::StrawTubes, straw_tubes_);
        register_detector(SHiP::DetectorID::UpstreamTagger, upstream_tagger_);
        register_detector(SHiP::DetectorID::SurroundTagger, surround_tagger_);
        register_detector(SHiP::DetectorID::TimingDetector, timing_detector_);
        register_detector(SHiP::DetectorID::Calorimeter, calorimeter_);         
    }

    Digitiser(Digitiser const&) = delete;
    Digitiser& operator=(Digitiser const&) = delete;
    Digitiser(Digitiser&&) = delete;
    Digitiser& operator=(Digitiser&&) = delete;

    template<typename Detector>
    void register_detector(SHiP::DetectorID id, Detector& detector)
    {
        digitisers_.emplace(id, [&detector](::SHiP::SimHit const& hit) -> DigitisedHit {return detector.digitise(hit);});
    }

    [[nodiscard]]
    DigitisedHits operator()(std::vector<::SHiP::SimHit> const& sim_hits)
    {
        DigitisedHits result;
        for (auto const& sim_hit : sim_hits) {
            auto hit = digitise(sim_hit);
            std::visit([&result](auto&& concrete_hit) {
                using Hit = std::remove_cvref_t<decltype(concrete_hit)>;
                std::get<std::vector<Hit>>(result).push_back(std::forward<decltype(concrete_hit)>(concrete_hit));
            },std::move(hit));
        }
        return result;
    }

    [[nodiscard]]
    DigitisedHit digitise(::SHiP::SimHit const& hit)
    {
        auto const id = static_cast<SHiP::DetectorID>(hit.detectorId);
        auto const it = digitisers_.find(id);

        if (it == digitisers_.end()) {
            throw std::runtime_error{ "No digitiser registered for detector ID " + std::to_string(hit.detectorId)};
        }
        return it->second(hit);
    }

private:
    StrawTubes straw_tubes_;
    Calorimeter calorimeter_;
    UpstreamTagger upstream_tagger_;
    SurroundTagger surround_tagger_;
    TimingDetector timing_detector_;

    std::unordered_map<SHiP::DetectorID, DigitiseFunction> digitisers_;
};
}  // namespace Shannon

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  auto const layer = config.get<std::string>("layer");
  auto digitiser = std::make_shared<Shannon::Digitiser>(rng);

  m.transform(
         "digitise_hits",
         [digitiser](
             std::vector<::SHiP::SimHit> const& sim_hits)
         {
             return (*digitiser)(sim_hits);
         },
         concurrency::serial)
        .input_family(product_selector{.creator = "rntuple_source", .layer = layer, .suffix="sim_hits"})
        .output_product_suffixes(
            "straw_tubes_hits",
            "calorimeter_hits",
            "ubt_hits",
            "sbt_hits",
            "timing_detector_hits");
};


