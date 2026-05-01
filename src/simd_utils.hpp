#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Platform and compiler detection (self-contained, no external deps)
#if defined(_MSC_VER)
#define ASTRA_SIMD_MSVC 1
#elif defined(__clang__)
#define ASTRA_SIMD_CLANG 1
#elif defined(__GNUC__)
#define ASTRA_SIMD_GCC 1
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#define ASTRA_SIMD_X86 1
#include <emmintrin.h>   // SSE2
#include <immintrin.h>   // AVX/AVX2
#include <smmintrin.h>   // SSE4.1
#define ASTRA_SIMD_HAS_SSE2 1
#if defined(__AVX2__)
#define ASTRA_SIMD_HAS_AVX2 1
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define ASTRA_SIMD_HAS_NEON 1
#elif defined(__arm__) || defined(_M_ARM)
#if defined(__ARM_NEON)
#include <arm_neon.h>
#define ASTRA_SIMD_HAS_NEON 1
#endif
#endif

// Force inline
#if defined(ASTRA_SIMD_MSVC)
#define ASTRA_SIMD_FORCE_INLINE __forceinline
#else
#define ASTRA_SIMD_FORCE_INLINE __attribute__((always_inline)) inline
#endif

namespace astrakv::simd {

// ==============================================================================
// CaseInsensitiveEquals
// ==============================================================================

#if defined(ASTRA_SIMD_HAS_SSE2)
ASTRA_SIMD_FORCE_INLINE bool case_insensitive_equals_sse2(const char* a,
                                                          const char* b,
                                                          size_t len) {
  const size_t vec_size = 16;
  for (size_t i = 0; i + vec_size <= len; i += vec_size) {
    __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
    __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i));

    // Force uppercase: if byte > 'Z', set bit 5 (0x20) to lower it
    __m128i mask = _mm_cmpgt_epi8(va, _mm_set1_epi8('Z'));
    __m128i va_upper = _mm_or_si128(va, _mm_and_si128(mask, _mm_set1_epi8(0x20)));

    mask = _mm_cmpgt_epi8(vb, _mm_set1_epi8('Z'));
    __m128i vb_upper = _mm_or_si128(vb, _mm_and_si128(mask, _mm_set1_epi8(0x20)));

    __m128i cmp = _mm_cmpeq_epi8(va_upper, vb_upper);
    if (_mm_movemask_epi8(cmp) != 0xFFFF) {
      return false;
    }
  }
  return true;
}
#endif

ASTRA_SIMD_FORCE_INLINE bool CaseInsensitiveEquals(const char* a,
                                                   const char* b,
                                                   size_t len) {
#if defined(ASTRA_SIMD_HAS_SSE2)
  if (len >= 16) {
    if (!case_insensitive_equals_sse2(a, b, len)) {
      return false;
    }
    size_t processed = (len / 16) * 16;
    a += processed;
    b += processed;
    len -= processed;
  }
#endif
  for (size_t i = 0; i < len; ++i) {
    unsigned char ca = static_cast<unsigned char>(a[i]);
    unsigned char cb = static_cast<unsigned char>(b[i]);
    if (ca >= 'a' && ca <= 'z') ca -= 32;
    if (cb >= 'a' && cb <= 'z') cb -= 32;
    if (ca != cb) return false;
  }
  return true;
}

// ==============================================================================
// HasZero – check if buffer contains a zero byte
// ==============================================================================

#if defined(ASTRA_SIMD_HAS_AVX2)
ASTRA_SIMD_FORCE_INLINE bool has_zero_avx2(const char* data, size_t len) {
  const __m256i zero = _mm256_setzero_si256();
  for (size_t i = 0; i + 32 <= len; i += 32) {
    __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
    unsigned mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, zero));
    if (mask != 0) return true;
  }
  return false;
}
#endif

#if defined(ASTRA_SIMD_HAS_SSE2)
ASTRA_SIMD_FORCE_INLINE bool has_zero_sse2(const char* data, size_t len) {
  const __m128i zero = _mm_setzero_si128();
  for (size_t i = 0; i + 16 <= len; i += 16) {
    __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
    unsigned mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, zero));
    if (mask != 0) return true;
  }
  return false;
}
#endif

ASTRA_SIMD_FORCE_INLINE bool HasZero(const char* data, size_t len) {
#if defined(ASTRA_SIMD_HAS_AVX2)
  if (len >= 32) {
    if (has_zero_avx2(data, len)) return true;
    size_t processed = (len / 32) * 32;
    data += processed;
    len -= processed;
  }
#endif
#if defined(ASTRA_SIMD_HAS_SSE2)
  if (len >= 16) {
    if (has_zero_sse2(data, len)) return true;
    size_t processed = (len / 16) * 16;
    data += processed;
    len -= processed;
  }
#endif
  for (size_t i = 0; i < len; ++i) {
    if (data[i] == '\0') return true;
  }
  return false;
}

// ==============================================================================
// HashString – FNV-1a 64-bit
// ==============================================================================

ASTRA_SIMD_FORCE_INLINE uint64_t HashString(const char* data, size_t len) {
  uint64_t hash = 14695981039346656037ULL;
  for (size_t i = 0; i < len; ++i) {
    hash ^= static_cast<uint64_t>(data[i]);
    hash *= 1099511628211ULL;
  }
  return hash;
}

}  // namespace astrakv::simd
