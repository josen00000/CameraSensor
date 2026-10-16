#pragma once

#include <cstdint>

namespace sfc {

class Pcg32 {
 public:
  Pcg32(uint64_t seed, uint64_t stream = 0);
  ~Pcg32();

  Pcg32(const Pcg32&) = delete;
  Pcg32& operator=(const Pcg32&) = delete;

  uint32_t Next();

  void Seed(uint64_t seed, uint64_t stream = 0);

 private:
  class Impl;
  Impl* impl_;
};

}  // namespace sfc
