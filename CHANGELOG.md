# Changelog

All notable changes to this project will be documented in this file.

## [0.2.0] - 2026-07-22

### Features

- Write digitised hits and validation histograms in output module
- Add smoke-test workflow

### Bug fixes

- *(output)* Align digitised rntuple names and trim dead includes/config
- Match digitised product suffixes to output tuple order
- Open a separate RNTuple reader per provider
- Emit RUNPATH so the data-model overlay wins at runtime
- Include <type_traits> directly for std::remove_cvref_t
- Reject colliding rntuple and histogram output paths
- Follow data-model rename of DetectorID to detector_id
- Remove stale smoke outputs and validate contents with ROOT
- Reject event counts exceeding the uint32_t event counter

### Refactor

- Make RNTuple field names configurable with defaults
- Delegate detector smearing to a shared gaussian smearer
- Seed a counter-based Philox RNG per event

### Documentation

- Add Doxygen configuration

### Styling

- Apply precommit fixes and sort output

### Build

- Pin data-model overlay install libdir to lib
- Guard CMP0167 setting for CMake versions before 3.30

## [0.1.0] - 2026-07-13

### Features

- Add initial digitisation code

### Bug fixes

- Close namespace and balance braces in digitise_hits
- Add missing include guard in surround_tagger

### Documentation

- Update CONTRIBUTING.md for pixi and prek workflow

### Styling

- Reformat digitise_hits
- Apply clang-format, gersemi and prek fixes

### Miscellaneous

- Initialise repository with README and license
- Rename project from trout to Shannon
- Adopt REUSE license layout
- Add prek hooks and lint environment
- Record formatting commit in git-blame-ignore-revs
- Add GitHub Actions workflows
