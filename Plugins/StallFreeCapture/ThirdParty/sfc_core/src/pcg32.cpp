#include "sfc/pcg32.h"

#include "../vendor/pcg-cpp/pcg_random.hpp"

namespace sfc {

class Pcg32::Impl {
 public:
  explicit Impl(uint64_t seed, uint64_t stream) : rng_(seed, stream) {}

  uint32_t Next() { return rng_(); }

  void Seed(uint64_t seed, uint64_t stream) { rng_.seed(seed, stream); }

 private:
  pcg32 rng_;
};

Pcg32::Pcg32(uint64_t seed, uint64_t stream) : impl_(new Impl(seed, stream)) {}

Pcg32::~Pcg32() { delete impl_; }

uint32_t Pcg32::Next() { return impl_->Next(); }

void Pcg32::Seed(uint64_t seed, uint64_t stream) { impl_->Seed(seed, stream); }

}  // namespace sfc
