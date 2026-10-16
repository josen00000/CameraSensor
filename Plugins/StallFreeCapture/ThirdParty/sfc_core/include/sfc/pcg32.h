#pragma once

#include <cstdint>
#include <memory>

namespace sfc {

class Pcg32 {
 public:
  struct Snapshot {
    uint64_t multiplier;
    uint64_t inc;
    uint64_t state;
  };

  Pcg32(uint64_t seed, uint64_t stream = 0);
  Pcg32(const Pcg32& other);
  Pcg32& operator=(const Pcg32& other);
  Pcg32(Pcg32&& other) noexcept;
  Pcg32& operator=(Pcg32&& other) noexcept;
  ~Pcg32();

  uint32_t Next();

  void Seed(uint64_t seed, uint64_t stream = 0);

  Snapshot GetState() const;
  bool SetState(const Snapshot& snapshot);

 private:
  struct Rng;
  std::unique_ptr<Rng> rng_;
};

}  // namespace sfc
