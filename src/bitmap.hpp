#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astrakv {
namespace bitmap {

// ==============================================================================
// Helpers – MSB-first Redis-compatible bit operations on std::string
// ==============================================================================

inline uint8_t PopCount(uint8_t byte) {
  uint8_t count = 0;
  while (byte) {
    count += byte & 1;
    byte >>= 1;
  }
  return count;
}

inline bool GetBit(const std::string& s, int64_t offset) {
  if (offset < 0) return false;
  int64_t byte_off = offset / 8;
  int bit_in_byte  = 7 - (offset % 8);
  if (byte_off >= static_cast<int64_t>(s.size())) return false;
  return (static_cast<uint8_t>(s[static_cast<size_t>(byte_off)]) >> bit_in_byte) & 1;
}

inline std::string SetBit(const std::string& s, int64_t offset, int value) {
  std::string result = s;
  if (offset < 0) return result;
  int64_t byte_off = offset / 8;
  if (byte_off + 1 > static_cast<int64_t>(result.size())) {
    result.resize(static_cast<size_t>(byte_off + 1), '\0');
  }
  int bit_in_byte = 7 - (offset % 8);
  uint8_t& byte_ref = reinterpret_cast<uint8_t&>(result[static_cast<size_t>(byte_off)]);
  if (value) {
    byte_ref |= (1 << bit_in_byte);
  } else {
    byte_ref &= ~(1 << bit_in_byte);
  }
  return result;
}

inline int64_t BitCount(const std::string& s, int64_t start, int64_t end) {
  int64_t len = static_cast<int64_t>(s.size());
  if (len == 0) return 0;
  if (start < 0) start = len + start;
  if (end < 0) end = len + end;
  if (start < 0) start = 0;
  if (end >= len) end = len - 1;
  if (start > end) return 0;

  int64_t count = 0;
  for (int64_t i = start; i <= end; ++i) {
    count += PopCount(static_cast<uint8_t>(s[static_cast<size_t>(i)]));
  }
  return count;
}

inline int64_t BitPos(const std::string& s, int bit, int64_t start, int64_t end) {
  int64_t len = static_cast<int64_t>(s.size());
  if (len == 0) return -1;
  if (start < 0) start = len + start;
  if (end < 0) end = len + end;
  if (start < 0) start = 0;
  if (end >= len) end = len - 1;
  if (start > end) return -1;

  bool target = (bit == 1);
  for (int64_t byte_idx = start; byte_idx <= end; ++byte_idx) {
    uint8_t byte = static_cast<uint8_t>(s[static_cast<size_t>(byte_idx)]);
    for (int bit_idx = 0; bit_idx < 8; ++bit_idx) {
      int msb_idx = 7 - bit_idx;
      if (((byte >> msb_idx) & 1) == (target ? 1 : 0)) {
        return byte_idx * 8 + bit_idx;
      }
    }
  }
  return -1;
}

inline std::string BitOp(const std::string& op,
                          const std::vector<std::string>& values) {
  if (values.empty()) return {};
  if (op == "NOT" && values.size() != 1) return {};

  size_t max_len = 0;
  for (const auto& v : values) {
    if (v.size() > max_len) max_len = v.size();
  }

  std::string result(max_len, '\0');

  if (op == "AND") {
    for (size_t i = 0; i < max_len; ++i) {
      uint8_t byte = 0xFF;
      for (const auto& v : values) {
        if (i < v.size()) {
          byte &= static_cast<uint8_t>(v[i]);
        } else {
          byte = 0;
        }
      }
      result[i] = static_cast<char>(byte);
    }
  } else if (op == "OR") {
    for (size_t i = 0; i < max_len; ++i) {
      uint8_t byte = 0;
      for (const auto& v : values) {
        if (i < v.size()) byte |= static_cast<uint8_t>(v[i]);
      }
      result[i] = static_cast<char>(byte);
    }
  } else if (op == "XOR") {
    for (size_t i = 0; i < max_len; ++i) {
      uint8_t byte = 0;
      for (const auto& v : values) {
        if (i < v.size()) byte ^= static_cast<uint8_t>(v[i]);
      }
      result[i] = static_cast<char>(byte);
    }
  } else if (op == "NOT") {
    for (size_t i = 0; i < max_len; ++i) {
      result[i] = static_cast<char>(~static_cast<uint8_t>(values[0][i]));
    }
  }

  return result;
}

}  // namespace bitmap
}  // namespace astrakv
