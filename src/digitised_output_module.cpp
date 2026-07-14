// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// sim_output_module.cpp — Phlex module plugin
//
// Observers for simulation output:
//   - RNTuple parallel writer for reconstruction
//   - Validation histograms

#include "TFile.h"
#include "TH1D.h"
#include "phlex/core/product_selector.hpp"
#include "phlex/module.hpp"

#include <ROOT/RNTupleFillContext.hxx>
#include <ROOT/RNTupleFillStatus.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleParallelWriter.hxx>
#include <ROOT/RNTupleWriteOptions.hxx>

#include <SHiP/MCParticle.hpp>
#include <SHiP/RecHit.hpp>
#include <SHiP/SimHit.hpp>
#include <SHiP/SimParticle.hpp>
#include <SHiP/SimResult.hpp>
#include <SHiP/detectors/CaloHit.hpp>
#include <SHiP/detectors/SBTHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
#include <boost/histogram.hpp>
#include <memory>
#include <mutex>
#include <oneapi/tbb/enumerable_thread_specific.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using ROOT::REntry;
using ROOT::RNTupleModel;
using ROOT::RNTupleParallelWriter;

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
          calo_{file_service_, "calo_hits"},
          sbt_{file_service_, "sbt_hits"},
          straw_{file_service_, "straw_tubes_hits"},
          timedet_{file_service_, "timedet_hits"},
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

}  // namespace

PHLEX_REGISTER_ALGORITHMS(m, config) {
    using namespace phlex;
    auto filename = config.get<std::string>("rntuple_file", std::string{"digitised_hits.root"});

    auto writer = m.make<HitRNTupleWriter>(filename);

    writer.observe("write_calo_hits", &HitRNTupleWriter::write_calo, concurrency::unlimited)
        .input_family(product_selector{
            .creator = "digitise_hits"_id, .layer = "event"_id, .suffix = "calorimeter_hits"_id});

    writer.observe("write_sbt_hits", &HitRNTupleWriter::write_sbt, concurrency::unlimited)
        .input_family(product_selector{
            .creator = "digitise_hits"_id, .layer = "event"_id, .suffix = "sbt_hits"_id});

    writer.observe("write_straw_hits", &HitRNTupleWriter::write_straw, concurrency::unlimited)
        .input_family(product_selector{
            .creator = "digitise_hits"_id, .layer = "event"_id, .suffix = "straw_tubes_hits"_id});

    writer.observe("write_timedet_hits", &HitRNTupleWriter::write_timedet, concurrency::unlimited)
        .input_family(product_selector{.creator = "digitise_hits"_id,
                                       .layer = "event"_id,
                                       .suffix = "timing_detector_hits"_id});

    writer.observe("write_ubt_hits", &HitRNTupleWriter::write_ubt, concurrency::unlimited)
        .input_family(product_selector{
            .creator = "digitise_hits"_id, .layer = "event"_id, .suffix = "ubt_hits"_id});
}
