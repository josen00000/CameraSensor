#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace sfc {

class Pcg32 {
 public:
  using Snapshot = std::string;

  Pcg32(uint64_t seed, uint64_t stream = 0);
  Pcg32(const Pcg32& other);
  Pcg32& operator=(const Pcg32& other);
  Pcg32(Pcg32&&) noexcept = default;
  Pcg32& operator=(Pcg32&&) noexcept = default;
  ~Pcg32();

  uint32_t Next();

  void Seed(uint64_t seed, uint64_t stream = 0);

  Snapshot GetState() const;
  void SetState(const Snapshot& snapshot);

 private:
  struct Rng;
  std::unique_ptr<Rng> rng_;
};

}  // namespace sfc
