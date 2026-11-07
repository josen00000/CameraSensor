# sfc_core

Plain C++ library used by the StallFreeCapture plugin (deterministic RNG, hashing, fixed-step timing).

## API conventions

The public API is **exception-free**: invalid inputs are reported via `std::optional` (for example `FixedStepClock::Create`) or `bool` return values (for example `Pcg32::SetState`). Callers compiled with exceptions disabled (including Unreal Engine targets) can use these types from headers without pulling in exception-throwing constructors.

## Build and test

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

### Sanitizer presets (Linux/macOS; not available on Windows)

CI runs the full `ctest` suite under ThreadSanitizer and AddressSanitizer (+ UBSan via the `asan` preset). Locally, from this directory:

```bash
export TSAN_OPTIONS=halt_on_error=1
export ASAN_OPTIONS=halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1

cmake --preset tsan && cmake --build --preset tsan && ctest --preset tsan --output-on-failure
cmake --preset asan && cmake --build --preset asan && ctest --preset asan --output-on-failure
```

Long-running stress tests (for example lock-free ring stress from T24) may later be tagged with a GTest/ctest label such as `stress`; use `ctest -L stress` or `ctest -LE stress` to include or exclude them without changing the sanitizer presets.

Third-party licenses: see `NOTICE` and `vendor/README.md`.
