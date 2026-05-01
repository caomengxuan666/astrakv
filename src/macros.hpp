#pragma once

#include <cstdint>

// ==============================================================================
// Platform Detection
// ==============================================================================
#if defined(_WIN32) || defined(_WIN64)
#define ASTRA_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#define ASTRA_PLATFORM_MACOS
#elif TARGET_OS_IPHONE
#define ASTRA_PLATFORM_IOS
#endif
#elif defined(__linux__)
#define ASTRA_PLATFORM_LINUX
#elif defined(__ANDROID__)
#define ASTRA_PLATFORM_ANDROID
#endif

// ==============================================================================
// Compiler Detection
// ==============================================================================
#if defined(_MSC_VER)
#define ASTRA_MSVC
#elif defined(__clang__)
#define ASTRA_CLANG
#elif defined(__GNUC__)
#define ASTRA_GCC
#endif

// ==============================================================================
// Architecture Detection
// ==============================================================================
#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#define ASTRA_ARCH_X64
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ASTRA_ARCH_ARM64
#elif defined(__i386) || defined(_M_IX86)
#define ASTRA_ARCH_X86
#elif defined(__arm__) || defined(_M_ARM)
#define ASTRA_ARCH_ARM32
#endif

// ==============================================================================
// Branch Prediction Hints
// ==============================================================================
#if defined(ASTRA_MSVC)
#define ASTRA_LIKELY(x)   (x)
#define ASTRA_UNLIKELY(x) (x)
#else
#define ASTRA_LIKELY(x)   __builtin_expect(!!(x), 1)
#define ASTRA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

// ==============================================================================
// Force Inline
// ==============================================================================
#if defined(ASTRA_MSVC)
#define ASTRA_FORCE_INLINE __forceinline
#define ASTRA_NEVER_INLINE  __declspec(noinline)
#else
#define ASTRA_FORCE_INLINE __attribute__((always_inline)) inline
#define ASTRA_NEVER_INLINE  __attribute__((noinline))
#endif

// ==============================================================================
// Cache Line Size
// ==============================================================================
constexpr size_t ASTRA_CACHE_LINE_SIZE = 64;

#if defined(ASTRA_MSVC)
#define ASTRA_CACHE_LINE_ALIGNED __declspec(align(64))
#else
#define ASTRA_CACHE_LINE_ALIGNED __attribute__((aligned(64)))
#endif

// ==============================================================================
// Restrict / NoAlias
// ==============================================================================
#if defined(ASTRA_MSVC)
#define ASTRA_RESTRICT __restrict
#else
#define ASTRA_RESTRICT __restrict__
#endif

// ==============================================================================
// No Discard ([[nodiscard]] for older codebases)
// ==============================================================================
#define ASTRA_NO_DISCARD [[nodiscard]]

// ==============================================================================
// Unreachable
// ==============================================================================
#if defined(ASTRA_MSVC)
#define ASTRA_UNREACHABLE() __assume(0)
#else
#define ASTRA_UNREACHABLE() __builtin_unreachable()
#endif

// ==============================================================================
// Disable Copy / Move
// ==============================================================================
#define ASTRA_DISABLE_COPY(ClassName)       \
  ClassName(const ClassName&) = delete;     \
  ClassName& operator=(const ClassName&) = delete;

#define ASTRA_DISABLE_MOVE(ClassName)       \
  ClassName(ClassName&&) = delete;          \
  ClassName& operator=(ClassName&&) = delete;

#define ASTRA_DISABLE_COPY_MOVE(ClassName)  \
  ASTRA_DISABLE_COPY(ClassName)             \
  ASTRA_DISABLE_MOVE(ClassName)

// ==============================================================================
// Unused Attribute
// ==============================================================================
#if defined(ASTRA_MSVC)
#define ASTRA_UNUSED
#else
#define ASTRA_UNUSED __attribute__((unused))
#endif
