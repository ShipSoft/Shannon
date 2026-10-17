# Changelog

All notable changes to this project will be documented in this file.

## [0.4.0] - 2026-10-17

### Refactor

- [**breaking**] Move Shannon into the SHiP namespace

### Miscellaneous

- Add advisory builds against ShipSoft library mains

### Build

- Resolve SHiP packages from ship-experiment/stable
## [0.3.0] - 2026-10-14

### Features

- Save a copy of sim hits
- Add version
- Copy sim particles
- Add random time-offset to hits
- [**breaking**] Require phlex 0.4.1

### Bug fixes

- Let a failed git diff fail the clang-tidy-diff task

### Refactor

- Reuse selector identifiers now that phlex no longer moves from them
- Take PhiloxRng from ship-random

### Documentation

- Adopt AI policy
- Describe what the clang-tidy gate actually does, and fix the local check
- *(output)* Drop the selector lvalue-move note

### Miscellaneous

- Update pixi lock file
- Remove redundant PR checklist template
- Grant config-sync job explicit permissions
- Update pixi lock file
- Sync shared configs
- Gate merges on uniform aggregator checks
- Check C++ against the Core Guidelines with clang-tidy
- *(lint)* Move clang-format to 23 and gersemi to 0.29.1
- Update pixi lock file

### Build

- Consume shipdatamodel 0.3.0 and drop the data-model overlay
- Keep shipdatamodel below 0.6
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
