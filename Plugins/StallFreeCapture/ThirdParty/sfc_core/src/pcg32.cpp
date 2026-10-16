#include "sfc/pcg32.h"

#include <pcg_random.hpp>
#include <sstream>
#include <utility>

namespace sfc {

struct Pcg32::Rng {
  pcg32 engine;

  explicit Rng(uint64_t seed, uint64_t stream) : engine(seed, stream) {}
};

namespace {

bool ApplySnapshot(pcg32& engine, const Pcg32::Snapshot& snapshot) {
  std::ostringstream encoded;
  encoded << snapshot.multiplier << ' ' << snapshot.inc << ' ' << snapshot.state;
  std::istringstream decoded(encoded.str());
  decoded >> engine;
  return !decoded.fail();
}

Pcg32::Snapshot ReadSnapshot(const pcg32& engine) {
  std::ostringstream out;
  out << engine;
  std::istringstream in(out.str());
  Pcg32::Snapshot snapshot{};
  in >> snapshot.multiplier >> snapshot.inc >> snapshot.state;
  return snapshot;
}

}  // namespace

Pcg32::Pcg32(uint64_t seed, uint64_t stream)
    : rng_(std::make_unique<Rng>(seed, stream)) {}

Pcg32::Pcg32(const Pcg32& other)
    : rng_(std::make_unique<Rng>(0, 0)) {
  SetState(other.GetState());
}

Pcg32& Pcg32::operator=(const Pcg32& other) {
  if (this != &other) {
    SetState(other.GetState());
  }
  return *this;
}

Pcg32::Pcg32(Pcg32&& other) noexcept : rng_(std::exchange(other.rng_, {})) {
  other.rng_ = std::make_unique<Rng>(0, 0);
}

Pcg32& Pcg32::operator=(Pcg32&& other) noexcept {
  if (this != &other) {
    rng_ = std::exchange(other.rng_, {});
    other.rng_ = std::make_unique<Rng>(0, 0);
  }
  return *this;
}

Pcg32::~Pcg32() = default;

uint32_t Pcg32::Next() { return rng_->engine(); }

void Pcg32::Seed(uint64_t seed, uint64_t stream) {
  rng_->engine.seed(seed, stream);
}

Pcg32::Snapshot Pcg32::GetState() const { return ReadSnapshot(rng_->engine); }

bool Pcg32::SetState(const Snapshot& snapshot) {
  pcg32 trial(0, 0);
  if (!ApplySnapshot(trial, snapshot)) {
    return false;
  }
  const Snapshot applied = ReadSnapshot(trial);
  if (applied.multiplier != snapshot.multiplier || applied.inc != snapshot.inc ||
      applied.state != snapshot.state) {
    return false;
  }
  return ApplySnapshot(rng_->engine, snapshot);
}

}  // namespace sfc
