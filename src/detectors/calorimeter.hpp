#pragma once
#include <SHiP/SimHit.hpp>
#include <SHiP/detectors/CaloHit.hpp>
#include <random>
#include <vector>

namespace Shannon {

class Calorimeter {
   public:
    explicit Calorimeter(std::mt19937& rng, double sigma_x = 0.1, double sigma_y = 0.1)
        : rng_(rng), dx_(0.0, sigma_x), dy_(0.0, sigma_y) {}

    ::SHiP::CaloHit digitise(::SHiP::SimHit const& sim_hit) {
        auto reconstructed = SHiP::fromSimHit(sim_hit);
        reconstructed.position = {sim_hit.position[0] + dx_(rng_), sim_hit.position[1] + dy_(rng_),
                                  sim_hit.position[2]};

        return ::SHiP::CaloHit{.recHit = reconstructed};
    }

   private:
    std::mt19937& rng_;
    std::normal_distribution<double> dx_;
    std::normal_distribution<double> dy_;
};
}  // namespace Shannon
