// AstrakV - Eviction policy definitions
#pragma once

#include <cstdint>
#include <string>

namespace astrakv {

enum class EvictionPolicy : uint8_t {
  kNoEviction      = 0,
  kAllKeysLRU      = 1,
  kVolatileLRU     = 2,
  kAllKeysLFU      = 3,
  kVolatileLFU     = 4,
  kAllKeysRandom   = 5,
  kVolatileRandom  = 6,
  kVolatileTTL     = 7,
  k2Q              = 8,
};

}  // namespace astrakv
