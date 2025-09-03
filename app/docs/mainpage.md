# Post Office — System Overview

Welcome to the interactive documentation for the Post Office project. This site is generated from all sources under `app/` and includes libraries, executables, tests, and design pages.

- Source tree: `src/`, `libs/`, `include/`, `tests/`
- Generated docs: `docs/html/` (this site), `docs/latex/` (PDF-ready)

## Quick navigation

- Libraries: @ref libraries "Post Office Libraries"
- Executables: @ref executables "Simulation Processes"
- Utilities: @ref utils "Utilities"
- Logging: @ref logger "Logging"
- Networking: @ref net "Networking"
- Storage: @ref storage "Storage"
- Performance: @ref perf "Performance"
- Sysinfo: @ref sysinfo "System Information"

## Build & test

See the project README for build, run, and test instructions. The Makefile also provides `make doc` to regenerate this documentation.

---

@remark This documentation extracts comments from the C sources and headers. Where symbols lack comments, Doxygen shows inferred members thanks to `EXTRACT_ALL=YES`.
