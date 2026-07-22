#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Validate the smoke-test outputs of the digitisation workflow.

Checks that the digitised-output file contains all per-detector RNTuples and
that the validation file contains all histograms, each with entries.
"""

import os
import sys

import ROOT

NTUPLES = [
    "calorimeter_hits",
    "sbt_hits",
    "straw_tubes_hits",
    "timing_detector_hits",
    "ubt_hits",
]

HISTOGRAMS = [
    "h_ubt_multiplicity",
    "h_sbt_multiplicity",
    "h_straw_tubes_multiplicity",
    "h_calorimeter_multiplicity",
    "h_timing_detector_multiplicity",
    "h_hit_x",
    "h_hit_y",
    "h_hit_z",
]


def check_ntuples(path):
    errors = []
    for name in NTUPLES:
        try:
            reader = ROOT.RNTupleReader.Open(name, path)
        except Exception as e:
            errors.append(f"{path}: cannot open RNTuple '{name}': {e}")
            continue
        if reader.GetNEntries() == 0:
            errors.append(f"{path}: RNTuple '{name}' has no entries")
    return errors


def check_histograms(path):
    try:
        file = ROOT.TFile.Open(path)
    except OSError as e:
        return [f"{path}: cannot open file: {e}"]
    if not file or file.IsZombie():
        return [f"{path}: cannot open file"]
    errors = []
    for name in HISTOGRAMS:
        hist = file.Get(name)
        if not hist:
            errors.append(f"{path}: missing histogram '{name}'")
        elif hist.GetEntries() == 0:
            errors.append(f"{path}: histogram '{name}' has no entries")
    return errors


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <digi_output.root> <validation.root>", file=sys.stderr)
        sys.exit(2)
    errors = check_ntuples(sys.argv[1]) + check_histograms(sys.argv[2])
    for error in errors:
        print(error, file=sys.stderr)
    sys.stdout.flush()
    sys.stderr.flush()
    # Skip interpreter shutdown to dodge a PyROOT RNTuple teardown crash.
    os._exit(1 if errors else 0)
