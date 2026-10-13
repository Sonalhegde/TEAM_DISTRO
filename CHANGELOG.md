# Changelog

All notable changes to this project are documented in this file.

## [Unreleased] - 2026-10-13

### Changed
- Reorganized repository structure into standard modular directories.
- Moved 3D CAD files and images to `hardware/3d-print/`.
- Renamed STL files to remove spaces (`smps-clamp.stl`, `pcb-foot.stl`, `pipe-clamp.stl`).
- Separated test code into `tests/simulation/` and `tests/firmware-bench/`.
- Moved simulation code to `simulation/web/` and archived previous version to `simulation/archive/`.
- Configured automated GitHub Pages deployment workflow in `.github/workflows/pages.yml`.
- Rewrote documentation to comply with ASD-STE100 Simplified Technical English.
