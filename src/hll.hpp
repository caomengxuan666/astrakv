#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>

namespace astrakv {

// ==============================================================================
// HyperLogLog – approximate distinct-count (cardinality) estimation
// ==============================================================================
// 16384 registers × 6 bits = 12288 bytes per sketch
//
// A 64-bit hash is split:
//   bits [63:50] → register index  (14 bits → 16384 registers)
//   bits [49:0]  → used to count leading zeros
// ==============================================================================

class HyperLogLog {
public:
  HyperLogLog() { std::memset(registers_, 0, sizeof(registers_)); }

  void Add(uint64_t hash) {
    size_t reg_idx = hash >> 50;
    uint8_t count = CountLeadingZeros(hash & 0x00FFFFFFFFFFFFFFULL);
    uint8_t current = GetRegister(reg_idx);
    if (count > current) {
      SetRegister(reg_idx, count);
    }
  }

  void Merge(const HyperLogLog& other) {
    for (size_t i = 0; i < kNumRegisters; ++i) {
      uint8_t a = GetRegister(i);
      uint8_t b = other.GetRegister(i);
      if (b > a) SetRegister(i, b);
    }
  }

  [[nodiscard]] uint64_t Estimate() const {
    double sum = 0.0;
    uint64_t zero_count = 0;
    for (size_t i = 0; i < kNumRegisters; ++i) {
      uint8_t reg = GetRegister(i);
      sum += 1.0 / (1ULL << reg);
      if (reg == 0) ++zero_count;
    }
    double alpha = 0.7213 / (1.0 + 1.079 / kNumRegisters);
    double estimate = alpha * kNumRegisters * kNumRegisters / sum;

    if (estimate <= 2.5 * kNumRegisters && zero_count > 0) {
      estimate = kNumRegisters * std::log(static_cast<double>(kNumRegisters) / zero_count);
    } else if (estimate > (1ULL << 32) * 30) {
      estimate = -(1ULL << 32) * std::log(1.0 - estimate / (1ULL << 32));
    }
    return static_cast<uint64_t>(estimate);
  }

  constexpr size_t MemoryUsage() const { return sizeof(registers_); }

  void Clear() { std::memset(registers_, 0, sizeof(registers_)); }

  // Raw access for serialization
  const uint8_t* Data() const { return registers_; }
  size_t DataSize() const { return sizeof(registers_); }
  void LoadData(const uint8_t* src, size_t size) {
    size_t n = size < sizeof(registers_) ? size : sizeof(registers_);
    std::memcpy(registers_, src, n);
  }

private:
  static constexpr size_t kNumRegisters    = 16384;  // 2^14
  static constexpr size_t kBitsPerRegister = 6;

  uint8_t registers_[kNumRegisters * kBitsPerRegister / 8];  // 12288 bytes

  uint8_t GetRegister(size_t idx) const {
    size_t bit_pos   = idx * kBitsPerRegister;
    size_t byte_pos  = bit_pos / 8;
    size_t bit_off   = bit_pos % 8;
    uint16_t value   = static_cast<uint16_t>(registers_[byte_pos]) |
                       (static_cast<uint16_t>(registers_[byte_pos + 1]) << 8);
    return (value >> bit_off) & 0x3F;
  }

  void SetRegister(size_t idx, uint8_t val) {
    size_t bit_pos   = idx * kBitsPerRegister;
    size_t byte_pos  = bit_pos / 8;
    size_t bit_off   = bit_pos % 8;
    uint16_t mask    = 0x3F << static_cast<uint16_t>(bit_off);
    uint16_t new_val = static_cast<uint16_t>(registers_[byte_pos]) |
                       (static_cast<uint16_t>(registers_[byte_pos + 1]) << 8);
    new_val          = (new_val & ~mask) | ((val & 0x3F) << static_cast<uint16_t>(bit_off));
    registers_[byte_pos]     = static_cast<uint8_t>(new_val & 0xFF);
    registers_[byte_pos + 1] = static_cast<uint8_t>((new_val >> 8) & 0xFF);
  }

  static uint8_t CountLeadingZeros(uint64_t value) {
    if (value == 0) return 50;
    uint8_t count = 0;
    while ((value & (1ULL << 49)) == 0) {
      ++count;
      value <<= 1;
    }
    return count;
  }
};

// ==============================================================================
// MurmurHash64 – MurmurHash3-style 64-bit hash (seeded)
// ==============================================================================
inline uint64_t MurmurHash64(const void* key_data, size_t key_len, uint64_t seed = 0x9747b28c) {
  const uint64_t m = 0xc6a4a7935bd1e995;
  const int r = 47;
  uint64_t h = seed ^ (key_len * m);

  const uint64_t* data = static_cast<const uint64_t*>(key_data);
  size_t blocks = key_len / 8;
  for (size_t i = 0; i < blocks; ++i) {
    uint64_t k = data[i];
    k *= m;
    k ^= k >> r;
    k *= m;
    h ^= k;
    h *= m;
  }

  const uint8_t* tail = static_cast<const uint8_t*>(key_data) + blocks * 8;
  uint64_t k = 0;
  switch (key_len & 7) {
    case 7: k ^= static_cast<uint64_t>(tail[6]) << 48; [[fallthrough]];
    case 6: k ^= static_cast<uint64_t>(tail[5]) << 40; [[fallthrough]];
    case 5: k ^= static_cast<uint64_t>(tail[4]) << 32; [[fallthrough]];
    case 4: k ^= static_cast<uint64_t>(tail[3]) << 24; [[fallthrough]];
    case 3: k ^= static_cast<uint64_t>(tail[2]) << 16; [[fallthrough]];
    case 2: k ^= static_cast<uint64_t>(tail[1]) << 8;  [[fallthrough]];
    case 1: k ^= static_cast<uint64_t>(tail[0]);
            k *= m;
            k ^= k >> r;
            k *= m;
            h ^= k;
            break;
    default: break;
  }

  h ^= h >> r;
  h *= m;
  h ^= h >> r;
  return h;
}

}  // namespace astrakv
