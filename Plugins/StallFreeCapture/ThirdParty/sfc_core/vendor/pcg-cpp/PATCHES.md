# Local patches against upstream PCG C++

**Upstream tag:** `v0.98.1`  
**Upstream commit (annotated tag target):** `726e919c2dd22a3cdaccb0674c5422ab4fa7a216`  
**Source:** https://github.com/imneme/pcg-cpp

## `pcg_random.hpp`

| Location | Change |
|----------|--------|
| After `#define PCG_RAND_HPP_INCLUDED 1` | Add `PCG_ALWAYS_INLINE` macro: `__forceinline` on MSVC; on GCC/Clang `__attribute__((always_inline))` plus an explicit `inline` keyword. |
| `extended::operator()` (~line 1215) | Replace `__attribute__((always_inline))` with `PCG_ALWAYS_INLINE`. |

**Reason:** MSVC does not accept GCC-style `__attribute__((always_inline))`.

## Not patched (handled in CMake)

- **Endianness:** Upstream `pcg_uint128.hpp` endian detection is unchanged. `sfc_core` defines `PCG_LITTLE_ENDIAN=1` when compiling (little-endian targets only).

## Unmodified vendored headers

- `pcg_extras.hpp` — upstream v0.98.1, no edits
- `pcg_uint128.hpp` — upstream v0.98.1, no edits (endianness via compile definition)
- `LICENSE.txt` — upstream v0.98.1, no edits
