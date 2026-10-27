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

## Concurrency test helpers (`tests/support/`)

Headers for typed queue contract suites and multi-threaded tests (used by ring cards later).

**Typed fixture** — derive from `sfc::test::QueueTypedFixture<YourQueue>` and register types with `TYPED_TEST_SUITE`:

```cpp
#include "support/typed_fixture.h"
#include "sfc/mutex_queue.h"

template <typename Queue>
class MyQueueTest : public sfc::test::QueueTypedFixture<Queue> {};
using MyQueueTypes = ::testing::Types<sfc::MutexBoundedQueue<int>>;
TYPED_TEST_SUITE(MyQueueTest, MyQueueTypes);

TYPED_TEST(MyQueueTest, Example) {
  TypeParam queue(this->kDefaultCapacity, sfc::FullPolicy::Reject);
  EXPECT_EQ(queue.Push(1), sfc::PushStatus::Pushed);
}
```

**Thread runner** — run a body on `N` workers; all threads are joined before return:

```cpp
#include "support/thread_runner.h"

sfc::test::RunOnThreads(4, [](std::size_t worker_index) {
  EXPECT_LT(worker_index, 4u);
});
```

**Hang guard** — fail or return `TimedOut` when a cooperative body does not finish in time (default 2 s):

```cpp
#include "support/hang_guard.h"

const auto result = sfc::test::RunWithHangGuard(
    [](std::atomic<bool>& stop) {
      while (!stop.load()) { /* work */ }
    },
    std::chrono::seconds(2));
```
