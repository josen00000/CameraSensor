#include "sfc/xxhash_wrapper.h"

#define XXH_INLINE_ALL
#include "../vendor/xxhash/xxhash.h"

namespace sfc {

uint64_t XxHash64(const void* data, size_t len, uint64_t seed) {
  return XXH64(data, len, seed);
}

}  // namespace sfc
