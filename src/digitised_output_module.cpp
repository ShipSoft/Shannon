// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// digitised_output_module.cpp — Phlex module plugin
//
// Observers for digitised output:
//   - RNTuple parallel writer that stores each detector's hit collection in
//     its own RNTuple within a single ROOT output file
//   - Validation histograms

#include "TFile.h"
#include "TH1D.h"
#include "phlex/core/product_selector.hpp"
#include "phlex/module.hpp"

#include <ROOT/Hist/ConvertToTH1.hxx>
#include <ROOT/RFile.hxx>
#include <ROOT/RHist.hxx>
#include <ROOT/RHistConcurrentFiller.hxx>
#include <ROOT/RHistFillContext.hxx>
#include <ROOT/RNTupleFillContext.hxx>
#include <ROOT/RNTupleFillStatus.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleParallelWriter.hxx>

#include <SHiP/detectors/CaloHit.hpp>
#include <SHiP/detectors/SBTHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <oneapi/tbb/enumerable_thread_specific.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using ROOT::REntry;
using ROOT::RNTupleModel;
using ROOT::RNTupleParallelWriter;
using ROOT::Experimental::RFile;
using ROOT::Experimental::RHist;
using ROOT::Experimental::RHistConcurrentFiller;
using ROOT::Experimental::RHistFillContext;

using UBTHits = std::vector<SHiP::UBTHit>;
using SBTHits = std::vector<SHiP::SBTHit>;
using StrawTubesHits = std::vector<SHiP::StrawTubesHit>;
using CaloHits = std::vector<SHiP::CaloHit>;
using TimeDetHits = std::vector<SHiP::TimeDetHit>;

class RNTupleFileService {
   public:
    explicit RNTupleFileService(std::string const& filename)
        : file_{TFile::Open(filename.c_str(), "RECREATE")} {
        if (!file_ || file_->IsZombie()) {
            throw std::runtime_error{"Could not create ROOT output file: " + filename};
        }
    }

    TFile& file() { return *file_; }
    std::mutex& mutex() { return mutex_; }

   private:
    // Keep this before file_, or explicitly destroy the writers before this
    // service is destroyed.
    std::unique_ptr<TFile> file_;
    std::mutex mutex_;
};

template <typename Hit>
class TypedHitWriter {
   public:
    TypedHitWriter(RNTupleFileService& file_service, std::string_view ntuple_name)
        : file_service_{file_service} {
        auto model = ROOT::RNTupleModel::CreateBare();
        model->MakeField<Hit>("hit");

        std::scoped_lock lock{file_service_.mutex()};

        writer_ = ROOT::RNTupleParallelWriter::Append(std::move(model), ntuple_name,
                                                      file_service_.file());
    }

    void write(Hit const& hit) {
        auto& state = states_.local();

        if (!state.context) {
            state.context = writer_->CreateFillContext();
            state.entry = state.context->CreateEntry();
        }

        *state.entry->template GetPtr<Hit>("hit") = hit;

        ROOT::RNTupleFillStatus status;
        state.context->FillNoFlush(*state.entry, status);

        if (status.ShouldFlushCluster()) {
            state.context->FlushColumns();

            std::scoped_lock lock{file_service_.mutex()};
            state.context->FlushCluster();
        }
    }

   private:
    struct FillState {
        std::shared_ptr<ROOT::RNTupleFillContext> context;
        std::unique_ptr<ROOT::REntry> entry;
    };

    RNTupleFileService& file_service_;
    std::unique_ptr<ROOT::RNTupleParallelWriter> writer_;
    tbb::enumerable_thread_specific<FillState> states_;
};

class HitRNTupleWriter {
   public:
    explicit HitRNTupleWriter(std::string const& filename)
        : file_service_{filename},
          calo_{file_service_, "calorimeter_hits"},
          sbt_{file_service_, "sbt_hits"},
          straw_{file_service_, "straw_tubes_hits"},
          timedet_{file_service_, "timing_detector_hits"},
          ubt_{file_service_, "ubt_hits"} {}

    void write_calo(std::vector<SHiP::CaloHit> const& hits) {
        for (auto const& hit : hits) {
            calo_.write(hit);
        }
    }

    void write_sbt(std::vector<SHiP::SBTHit> const& hits) {
        for (auto const& hit : hits) {
            sbt_.write(hit);
        }
    }

    void write_straw(std::vector<SHiP::StrawTubesHit> const& hits) {
        for (auto const& hit : hits) {
            straw_.write(hit);
        }
    }

    void write_timedet(std::vector<SHiP::TimeDetHit> const& hits) {
        for (auto const& hit : hits) {
            timedet_.write(hit);
        }
    }

    void write_ubt(std::vector<SHiP::UBTHit> const& hits) {
        for (auto const& hit : hits) {
            ubt_.write(hit);
        }
    }

   private:
    // Declared first so it is destroyed last.
    RNTupleFileService file_service_;

    TypedHitWriter<SHiP::CaloHit> calo_;
    TypedHitWriter<SHiP::SBTHit> sbt_;
    TypedHitWriter<SHiP::StrawTubesHit> straw_;
    TypedHitWriter<SHiP::TimeDetHit> timedet_;
    TypedHitWriter<SHiP::UBTHit> ubt_;
};

using HistD = RHist<double>;
using FillerD = RHistConcurrentFiller<double>;
using ContextD = RHistFillContext<double>;

std::shared_ptr<HistD> make_hist(int nbins, double low, double high) {
    return std::make_shared<HistD>(static_cast<std::uint64_t>(nbins), std::make_pair(low, high));
}

// Validation histograms for digitised hits (thread-safe via per-thread
// RHistFillContext atomic fillers)
class DigiHistogrammer {
   public:
    explicit DigiHistogrammer(std::string filename)
        : filename_{std::move(filename)},
          h_ubt_multiplicity_{make_hist(1000, -0.5, 999.5)},
          h_sbt_multiplicity_{make_hist(1000, -0.5, 999.5)},
          h_straw_tubes_multiplicity_{make_hist(1000, -0.5, 999.5)},
          h_calorimeter_multiplicity_{make_hist(1000, -0.5, 999.5)},
          h_timing_detector_multiplicity_{make_hist(1000, -0.5, 999.5)},
          h_hit_x_{make_hist(200, -3000., 3000.)},
          h_hit_y_{make_hist(200, -6000., 6000.)},
          h_hit_z_{make_hist(200, -1000., 12000.)},
          f_ubt_multiplicity_{h_ubt_multiplicity_},
          f_sbt_multiplicity_{h_sbt_multiplicity_},
          f_straw_tubes_multiplicity_{h_straw_tubes_multiplicity_},
          f_calorimeter_multiplicity_{h_calorimeter_multiplicity_},
          f_timing_detector_multiplicity_{h_timing_detector_multiplicity_},
          f_hit_x_{h_hit_x_},
          f_hit_y_{h_hit_y_},
          f_hit_z_{h_hit_z_} {}

    void observe(UBTHits const& ubt_hits, SBTHits const& sbt_hits,
                 StrawTubesHits const& straw_tubes_hits, CaloHits const& calorimeter_hits,
                 TimeDetHits const& timing_detector_hits) {
        auto& ctxs = fill_contexts_.local();
        if (!ctxs.ubt_multiplicity) {
            ctxs.ubt_multiplicity = f_ubt_multiplicity_.CreateFillContext();
            ctxs.sbt_multiplicity = f_sbt_multiplicity_.CreateFillContext();
            ctxs.straw_tubes_multiplicity = f_straw_tubes_multiplicity_.CreateFillContext();
            ctxs.calorimeter_multiplicity = f_calorimeter_multiplicity_.CreateFillContext();
            ctxs.timing_detector_multiplicity = f_timing_detector_multiplicity_.CreateFillContext();
            ctxs.hit_x = f_hit_x_.CreateFillContext();
            ctxs.hit_y = f_hit_y_.CreateFillContext();
            ctxs.hit_z = f_hit_z_.CreateFillContext();
        }
        auto observe_hits = [&ctxs](auto const& hits, ContextD& multiplicity) {
            multiplicity.Fill(static_cast<double>(hits.size()));
            for (auto const& hit : hits) {
                auto const& pos = hit.recHit.position;
                ctxs.hit_x->Fill(pos[0]);
                ctxs.hit_y->Fill(pos[1]);
                ctxs.hit_z->Fill(pos[2]);
            }
        };
        observe_hits(ubt_hits, *ctxs.ubt_multiplicity);
        observe_hits(sbt_hits, *ctxs.sbt_multiplicity);
        observe_hits(straw_tubes_hits, *ctxs.straw_tubes_multiplicity);
        observe_hits(calorimeter_hits, *ctxs.calorimeter_multiplicity);
        observe_hits(timing_detector_hits, *ctxs.timing_detector_multiplicity);
    }

    ~DigiHistogrammer() {
        // Destroy per-thread fill contexts so their stats flush back into the
        // histograms before we read them, and so the concurrent fillers see no
        // live contexts at their own destruction (which would std::terminate).
        fill_contexts_.clear();
        try {
            auto file = RFile::Recreate(filename_);
            auto put = [&](char const* name, char const* title, HistD const& h) {
                auto th1 = ROOT::Experimental::Hist::ConvertToTH1D(h);
                th1->SetNameTitle(name, title);
                file->Put(name, *th1);
            };
            put("h_ubt_multiplicity", "UBT hits per event;N;Events", *h_ubt_multiplicity_);
            put("h_sbt_multiplicity", "SBT hits per event;N;Events", *h_sbt_multiplicity_);
            put("h_straw_tubes_multiplicity", "Straw-tube hits per event;N;Events",
                *h_straw_tubes_multiplicity_);
            put("h_calorimeter_multiplicity", "Calorimeter hits per event;N;Events",
                *h_calorimeter_multiplicity_);
            put("h_timing_detector_multiplicity", "Timing-detector hits per event;N;Events",
                *h_timing_detector_multiplicity_);
            put("h_hit_x", "Digitised hit x position;x [mm];Entries", *h_hit_x_);
            put("h_hit_y", "Digitised hit y position;y [mm];Entries", *h_hit_y_);
            put("h_hit_z", "Digitised hit z position;z [mm];Entries", *h_hit_z_);
        } catch (std::exception const& e) {
            // RException, filesystem errors etc. — must not escape the destructor.
            try {
                spdlog::error(
                    "digitised_output_module: failed to write validation histograms to '{}': {}",
                    filename_, e.what());
            } catch (...) {
            }
        }
    }

   private:
    struct FillContexts {
        std::shared_ptr<ContextD> ubt_multiplicity;
        std::shared_ptr<ContextD> sbt_multiplicity;
        std::shared_ptr<ContextD> straw_tubes_multiplicity;
        std::shared_ptr<ContextD> calorimeter_multiplicity;
        std::shared_ptr<ContextD> timing_detector_multiplicity;
        std::shared_ptr<ContextD> hit_x;
        std::shared_ptr<ContextD> hit_y;
        std::shared_ptr<ContextD> hit_z;
    };

    std::string filename_;
    std::shared_ptr<HistD> h_ubt_multiplicity_, h_sbt_multiplicity_, h_straw_tubes_multiplicity_,
        h_calorimeter_multiplicity_, h_timing_detector_multiplicity_;
    std::shared_ptr<HistD> h_hit_x_, h_hit_y_, h_hit_z_;
    FillerD f_ubt_multiplicity_, f_sbt_multiplicity_, f_straw_tubes_multiplicity_,
        f_calorimeter_multiplicity_, f_timing_detector_multiplicity_;
    FillerD f_hit_x_, f_hit_y_, f_hit_z_;
    tbb::enumerable_thread_specific<FillContexts> fill_contexts_;
};

// No-op observer for benchmarking pure framework overhead.
class DigiNoop {
   public:
    void observe(UBTHits const&, SBTHits const&, StrawTubesHits const&, CaloHits const&,
                 TimeDetHits const&) {}
};

}  // namespace

PHLEX_REGISTER_ALGORITHMS(m, config) {
    using namespace phlex;

    auto mode = config.get<std::string>("mode", std::string{"digi"});
    auto rntuple_file = config.get<std::string>("rntuple_file", std::string{"digitised_hits.root"});
    auto histo_file = config.get<std::string>("histo_file", std::string{"digi_validation.root"});
    auto creator = config.get<std::string>("creator", std::string{"digitise_hits"});
    auto layer = config.get<std::string>("layer", std::string{"spill"});

    if (mode != "digi" && mode != "noop")
        throw std::runtime_error("Unknown digitised_output_module mode: '" + mode +
                                 "' (expected 'digi' or 'noop')");

    // product_selector's members move from whatever they are given (even
    // lvalues), so hand each selector its own identifier copies.
    auto selector = [&creator, &layer](char const* suffix) {
        return product_selector{.creator = phlex::experimental::identifier{creator},
                                .layer = phlex::experimental::identifier{layer},
                                .suffix = phlex::experimental::identifier{suffix}};
    };
    // Positional: must match the parameter order of the observe() overloads.
    auto hit_selectors = [&selector](auto& registered) {
        registered.input_family(selector("ubt_hits"), selector("sbt_hits"),
                                selector("straw_tubes_hits"), selector("calorimeter_hits"),
                                selector("timing_detector_hits"));
    };

    if (mode == "noop") {
        auto noop = m.make<DigiNoop>();
        auto registered = noop.observe("noop", &DigiNoop::observe, concurrency::unlimited);
        hit_selectors(registered);
        return;
    }

    auto writer = m.make<HitRNTupleWriter>(rntuple_file);

    writer.observe("write_calo_hits", &HitRNTupleWriter::write_calo, concurrency::unlimited)
        .input_family(selector("calorimeter_hits"));

    writer.observe("write_sbt_hits", &HitRNTupleWriter::write_sbt, concurrency::unlimited)
        .input_family(selector("sbt_hits"));

    writer.observe("write_straw_hits", &HitRNTupleWriter::write_straw, concurrency::unlimited)
        .input_family(selector("straw_tubes_hits"));

    writer.observe("write_timedet_hits", &HitRNTupleWriter::write_timedet, concurrency::unlimited)
        .input_family(selector("timing_detector_hits"));

    writer.observe("write_ubt_hits", &HitRNTupleWriter::write_ubt, concurrency::unlimited)
        .input_family(selector("ubt_hits"));

    auto histogrammer = m.make<DigiHistogrammer>(histo_file);
    auto registered_histogrammer =
        histogrammer.observe("validate", &DigiHistogrammer::observe, concurrency::unlimited);
    hit_selectors(registered_histogrammer);
}
