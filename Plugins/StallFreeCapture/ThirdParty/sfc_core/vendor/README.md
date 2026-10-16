# Vendored Third-Party Libraries

This directory contains vendored third-party libraries used by sfc_core.

## PCG32 Random Number Generator

- **Version**: v0.98.1 + local patch (see `pcg-cpp/PATCHES.md`)
- **Source**: https://github.com/imneme/pcg-cpp
- **License**: Apache License 2.0 (see `pcg-cpp/LICENSE.txt`)
- **Pinned at**: Git tag v0.98.1 (`726e919c2dd22a3cdaccb0674c5422ab4fa7a216`)
- **Files**:
  - `pcg-cpp/pcg_random.hpp` — PCG RNG (patched for MSVC `always_inline`)
  - `pcg-cpp/pcg_extras.hpp` — PCG helpers
  - `pcg-cpp/pcg_uint128.hpp` — 128-bit integers for PCG
  - `pcg-cpp/LICENSE.txt` — Apache 2.0 license
  - `pcg-cpp/PATCHES.md` — list of local edits vs upstream

## xxHash

- **Version**: v0.8.2
- **Source**: https://github.com/Cyan4973/xxHash
- **License**: BSD 2-Clause License (see `xxhash/LICENSE`)
- **Pinned at**: Git tag v0.8.2
- **Files**:
  - `xxhash/xxhash.h` — xxHash header with inline implementations
  - `xxhash/LICENSE` — BSD 2-Clause license

See also `../NOTICE` for third-party attribution in distributions.
