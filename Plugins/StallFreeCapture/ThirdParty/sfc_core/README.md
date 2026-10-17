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

Third-party licenses: see `NOTICE` and `vendor/README.md`.
