# Contributing to Shannon

Thank you for your interest in contributing to Shannon! As part of the SHiP Collaboration, we follow a set of standards to ensure code quality and maintainability.

## Development Workflow

1. **Fork and Clone**: Create a fork of the repository and clone it locally.
2. **Environment**: Ensure you have the required dependencies (ROOT, Geant4, Pythia8, Phlex, etc.). Using the SHiP software stack container is recommended.
3. **Pre-commit Hooks**: We use [prek](https://github.com/j178/prek) to enforce coding standards, with the hook tools provided by the pixi `lint` environment. Install the hooks before making changes:
   ```bash
   pixi run install-hooks
   ```
   Run all hooks on all files with:
   ```bash
   pixi run lint
   ```
4. **Branching**: Create a feature branch for your changes.
5. **Coding Standards**:
   - Follow the existing C++ style (enforced by `clang-format` and `cpplint`).
   - Ensure all files have the correct SPDX license headers (REUSE compliant).
6. **Commits**: We follow [Conventional Commits](https://www.conventionalcommits.org/). This helps in automated changelog generation.
   - `feat: ...` for new features
   - `fix: ...` for bug fixes
   - `docs: ...` for documentation changes
   - `style: ...` for formatting
   - `refactor: ...` for code refactoring
7. **Testing**:
   - Build and run the example workflow:
     ```bash
     pixi run build
     phlex -c workflows/digitise_hits.jsonnet
     ```
   - Add new workflows or benchmarks if you introduce new features.
8. **Submission**: Open a Pull Request against the `main` branch. Ensure the CI passes.

## Coding Style

- **C++**: We use C++23. Style is defined in `.clang-format`.
- **Configuration**: Workflows are defined using [Jsonnet](https://jsonnet.org/).

## Licensing

This project is licensed under the **LGPL-3.0-or-later**. All contributions must be compatible with this license. Each new file must include an SPDX identifier and copyright notice.
