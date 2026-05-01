#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dash_map.hpp"

namespace astrakv {

enum class KeyType : uint8_t {
  kNone  = 0,
  kBytes = 1,
  kZSet  = 2,
  kList  = 3,
  kHash  = 4,
  kSet   = 5,
  kStream = 6,
  kJson   = 7,
  kVector = 8,
};

struct KeyMetadata {
  KeyType type;
  std::optional<int64_t> expire_time_ms;
  uint64_t version = 0;
  uint32_t access_time_ms = 0;
  uint8_t lfu_counter = 0;

  KeyMetadata() : type(KeyType::kNone) {}
  explicit KeyMetadata(KeyType t) : type(t) {}

  bool IsExpired() const {
    if (!expire_time_ms.has_value()) return false;
    return *expire_time_ms > 0 && *expire_time_ms < GetCurrentTimeMs();
  }

  void SetExpireMs(int64_t ms) {
    if (ms <= 0) expire_time_ms = std::nullopt;
    else expire_time_ms = ms;
  }

  void SetExpireSeconds(int64_t seconds) { SetExpireMs(seconds * 1000); }

  std::optional<int64_t> GetTtlMs() const {
    if (!expire_time_ms.has_value()) return std::nullopt;
    int64_t now = GetCurrentTimeMs();
    if (*expire_time_ms <= 0 || *expire_time_ms <= now) return std::nullopt;
    return *expire_time_ms - now;
  }

  std::optional<int64_t> GetTtlSeconds() const {
    auto ttl_ms = GetTtlMs();
    if (!ttl_ms.has_value()) return std::nullopt;
    return *ttl_ms / 1000;
  }

  static int64_t GetCurrentTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
  }

  void UpdateAccess() {
    access_time_ms =
        static_cast<uint32_t>(GetCurrentTimeMs() & 0xFFFFFF);
    lfu_counter = std::min(static_cast<uint8_t>(lfu_counter + 1),
                           static_cast<uint8_t>(255));
  }

  uint32_t GetAccessTime() const { return access_time_ms; }
  uint8_t GetLFUCounter() const { return lfu_counter; }
  void ResetLFUCounter() { lfu_counter = lfu_counter >> 1; }
};

class KeyMetadataManager {
 public:
  KeyMetadataManager() = default;

  void RegisterKey(const std::string& key, KeyType type) {
    KeyMetadata existing;
    uint64_t new_version = 1;
    if (metadata_map_.Get(key, &existing)) {
      new_version = existing.version + 1;
    }
    KeyMetadata metadata(type);
    metadata.version = new_version;
    metadata_map_.Insert(key, std::move(metadata));
  }

  void UnregisterKey(const std::string& key) { metadata_map_.Remove(key); }

  bool IsValid(const std::string& key) {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return false;
    if (metadata.IsExpired()) {
      metadata_map_.Remove(key);
      return false;
    }
    return true;
  }

  std::optional<KeyType> GetKeyType(const std::string& key) {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return std::nullopt;
    if (metadata.IsExpired()) {
      metadata_map_.Remove(key);
      return std::nullopt;
    }
    return metadata.type;
  }

  bool SetExpireMs(const std::string& key, int64_t expire_time_ms) {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return false;
    metadata.SetExpireMs(expire_time_ms);
    metadata_map_.Insert(key, std::move(metadata));
    return true;
  }

  bool SetExpireSeconds(const std::string& key, int64_t seconds) {
    return SetExpireMs(key, KeyMetadata::GetCurrentTimeMs() + seconds * 1000);
  }

  int64_t GetTtlMs(const std::string& key) const {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return -2;
    if (metadata.IsExpired()) return -2;
    if (!metadata.expire_time_ms.has_value()) return -1;
    auto ttl_ms = metadata.GetTtlMs();
    if (!ttl_ms.has_value()) return -1;
    return *ttl_ms;
  }

  int64_t GetTtlSeconds(const std::string& key) {
    int64_t ttl_ms = GetTtlMs(key);
    if (ttl_ms < 0) return ttl_ms;
    return ttl_ms / 1000;
  }

  std::optional<int64_t> GetExpireTimeMs(const std::string& key) {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return std::nullopt;
    if (metadata.IsExpired()) return std::nullopt;
    return metadata.expire_time_ms;
  }

  bool Persist(const std::string& key) {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return false;
    metadata.expire_time_ms = std::nullopt;
    metadata_map_.Insert(key, std::move(metadata));
    return true;
  }

  std::vector<std::string> GetAllKeys() { return metadata_map_.AllKeys(); }

  uint64_t GetKeyVersion(const std::string& key) {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return 0;
    return metadata.version;
  }

  void IncrementKeyVersion(const std::string& key) {
    KeyMetadata metadata;
    if (metadata_map_.Get(key, &metadata)) {
      metadata.version++;
      metadata_map_.Insert(key, std::move(metadata));
    }
  }

  void Clear() { metadata_map_.Clear(); }

  bool UpdateAccessInfo(const std::string& key) {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return false;
    metadata.UpdateAccess();
    metadata_map_.Insert(key, std::move(metadata));
    return true;
  }

  uint32_t GetAccessTime(const std::string& key) {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return 0;
    return metadata.GetAccessTime();
  }

  uint8_t GetLFUCounter(const std::string& key) {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return 0;
    return metadata.GetLFUCounter();
  }

  void DecayLFUCounters() {
    auto entries = metadata_map_.AllEntries();
    for (auto& [key, metadata] : entries) {
      metadata.ResetLFUCounter();
      metadata_map_.Insert(key, std::move(metadata));
    }
  }

  bool HasTTL(const std::string& key) {
    KeyMetadata metadata;
    if (!metadata_map_.Get(key, &metadata)) return false;
    return metadata.expire_time_ms.has_value();
  }

  size_t Size() const { return metadata_map_.Size(); }

 private:
  DashMap<std::string, KeyMetadata> metadata_map_;
};

}  // namespace astrakv
