#include "sfc/pcg32.h"

#include <pcg_random.hpp>
#include <sstream>

namespace sfc {

struct Pcg32::Rng {
  pcg32 engine;

  explicit Rng(uint64_t seed, uint64_t stream) : engine(seed, stream) {}
};

Pcg32::Pcg32(uint64_t seed, uint64_t stream)
    : rng_(std::make_unique<Rng>(seed, stream)) {}

Pcg32::Pcg32(const Pcg32& other)
    : rng_(std::make_unique<Rng>(0, 0)) {
  std::ostringstream out;
  out << other.rng_->engine;
  std::istringstream in(out.str());
  in >> rng_->engine;
}

Pcg32& Pcg32::operator=(const Pcg32& other) {
  if (this != &other) {
    SetState(other.GetState());
  }
  return *this;
}

Pcg32::~Pcg32() = default;

uint32_t Pcg32::Next() { return rng_->engine(); }

void Pcg32::Seed(uint64_t seed, uint64_t stream) {
  rng_->engine.seed(seed, stream);
}

Pcg32::Snapshot Pcg32::GetState() const {
  std::ostringstream out;
  out << rng_->engine;
  return out.str();
}

void Pcg32::SetState(const Snapshot& snapshot) {
  std::istringstream in(snapshot);
  in >> rng_->engine;
}

}  // namespace sfc
