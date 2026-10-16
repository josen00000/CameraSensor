#pragma once

#include <cstdint>
#include <cstddef>

namespace sfc {

uint64_t XxHash64(const void* data, size_t len, uint64_t seed = 0);

}  // namespace sfc
