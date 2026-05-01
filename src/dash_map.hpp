// Copyright 2026 AstrakV Project
// Thread-safe concurrent hash map with sharded locking
// Default: ThreadSafe=true (sharded absl::Mutex per bucket)
// Set ThreadSafe=false for no-sharing architectures
#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/hash/hash.h>
#include <absl/strings/string_view.h>
#include <absl/synchronization/mutex.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace astrakv {

struct DashHash {
  using is_transparent = void;
  template <typename T>
  size_t operator()(const T &value) const {
    return absl::Hash<T>{}(value);
  }
};

template <typename Key>
struct StringEqual {
  bool operator()(const Key &lhs, const Key &rhs) const { return lhs == rhs; }
};

template <>
struct StringEqual<std::string> {
  using is_transparent = void;
  bool operator()(absl::string_view lhs, absl::string_view rhs) const { return lhs == rhs; }
  bool operator()(absl::string_view lhs, const std::string &rhs) const { return lhs == rhs; }
  bool operator()(const std::string &lhs, absl::string_view rhs) const { return lhs == rhs; }
  bool operator()(const std::string &lhs, const std::string &rhs) const { return lhs == rhs; }
};

// ThreadSafe=true:  sharded with absl::Mutex per shard (reader/writer locks)
// ThreadSafe=false: single flat_hash_map, no locking (no-sharing use)
template <typename Key, typename Value, bool ThreadSafe = true>
class DashMap {
public:
  using ValueType = Value;
  using MapType   = absl::flat_hash_map<Key, Value, DashHash, StringEqual<Key>>;

  explicit DashMap(size_t num_shards = 16) : size_(0) {
    if constexpr (ThreadSafe) {
      shards_.reserve(num_shards);
      for (size_t i = 0; i < num_shards; ++i) {
        shards_.push_back(std::make_unique<Shard>());
      }
    }
  }

  DashMap(const DashMap &)            = delete;
  DashMap &operator=(const DashMap &) = delete;
  DashMap(DashMap &&)                 = delete;
  DashMap &operator=(DashMap &&)      = delete;

  // ── Insert ──
  bool Insert(const Key &key, const Value &value) {
    if constexpr (ThreadSafe) {
      size_t idx = ShardIndex(key);
      absl::MutexLock lock(&shards_[idx]->mutex);
      auto [it, inserted] = shards_[idx]->map.insert_or_assign(key, value);
      if (inserted) size_.fetch_add(1, std::memory_order_relaxed);
      return inserted;
    } else {
      auto [it, inserted] = single_.insert_or_assign(key, value);
      if (inserted) ++size_unlocked_;
      return inserted;
    }
  }

  // ── Get ──
  bool Get(const Key &key, Value *out_value) const {
    if constexpr (ThreadSafe) {
      size_t idx = ShardIndex(key);
      absl::ReaderMutexLock lock(&shards_[idx]->mutex);
      auto it = shards_[idx]->map.find(key);
      if (it != shards_[idx]->map.end()) {
        if (out_value) *out_value = it->second;
        return true;
      }
      return false;
    } else {
      auto it = single_.find(key);
      if (it != single_.end()) {
        if (out_value) *out_value = it->second;
        return true;
      }
      return false;
    }
  }

  // ── Remove ──
  bool Remove(const Key &key) {
    if constexpr (ThreadSafe) {
      size_t idx = ShardIndex(key);
      absl::MutexLock lock(&shards_[idx]->mutex);
      auto erased = shards_[idx]->map.erase(key);
      if (erased > 0) {
        size_.fetch_sub(1, std::memory_order_relaxed);
        return true;
      }
      return false;
    } else {
      auto erased = single_.erase(key);
      if (erased > 0) { --size_unlocked_; return true; }
      return false;
    }
  }

  // ── Contains ──
  bool Contains(const Key &key) const {
    if constexpr (ThreadSafe) {
      size_t idx = ShardIndex(key);
      absl::ReaderMutexLock lock(&shards_[idx]->mutex);
      return shards_[idx]->map.contains(key);
    } else {
      return single_.contains(key);
    }
  }

  // ── Size ──
  size_t Size() const {
    if constexpr (ThreadSafe) return size_.load(std::memory_order_relaxed);
    else return size_unlocked_;
  }

  bool Empty() const { return Size() == 0; }

  // ── Clear ──
  void Clear() {
    if constexpr (ThreadSafe) {
      for (auto &s : shards_) {
        absl::MutexLock lock(&s->mutex);
        s->map.clear();
      }
      size_.store(0, std::memory_order_relaxed);
    } else {
      single_.clear();
      size_unlocked_ = 0;
    }
  }

  // ── Bulk read ──
  std::vector<Key> AllKeys() const {
    std::vector<Key> keys;
    if constexpr (ThreadSafe) {
      for (auto &s : shards_) {
        absl::ReaderMutexLock lock(&s->mutex);
        keys.reserve(keys.size() + s->map.size());
        for (const auto &[k, _] : s->map) keys.push_back(k);
      }
    } else {
      keys.reserve(single_.size());
      for (const auto &[k, _] : single_) keys.push_back(k);
    }
    return keys;
  }

  std::vector<std::pair<Key, Value>> AllEntries() const {
    std::vector<std::pair<Key, Value>> pairs;
    if constexpr (ThreadSafe) {
      for (auto &s : shards_) {
        absl::ReaderMutexLock lock(&s->mutex);
        pairs.reserve(pairs.size() + s->map.size());
        for (const auto &[k, v] : s->map) pairs.emplace_back(k, v);
      }
    } else {
      pairs.reserve(single_.size());
      for (const auto &[k, v] : single_) pairs.emplace_back(k, v);
    }
    return pairs;
  }

  // Iterate without copying — callback receives (key, value) per entry under reader lock
  template <typename F>
  void ForEach(F &&callback) const {
    if constexpr (ThreadSafe) {
      for (auto &s : shards_) {
        absl::ReaderMutexLock lock(&s->mutex);
        for (const auto &[k, v] : s->map) callback(k, v);
      }
    } else {
      for (const auto &[k, v] : single_) callback(k, v);
    }
  }

  size_t NumShards() const {
    if constexpr (ThreadSafe) return shards_.size();
    else return 1;
  }

  // ── Heterogeneous lookup for std::string keys ──
  template <typename K = Key>
  std::enable_if_t<std::is_same_v<K, std::string>, bool>
  Insert(absl::string_view key, const Value &value) {
    return Insert(std::string(key), value);
  }

  template <typename K = Key>
  std::enable_if_t<std::is_same_v<K, std::string>, bool>
  Get(absl::string_view key, Value *out_value) const {
    if constexpr (ThreadSafe) {
      size_t idx = ShardIndex(absl::Hash<absl::string_view>{}(key));
      absl::ReaderMutexLock lock(&shards_[idx]->mutex);
      auto it = shards_[idx]->map.find(key);
      if (it != shards_[idx]->map.end()) {
        if (out_value) *out_value = it->second;
        return true;
      }
      return false;
    } else {
      auto it = single_.find(key);
      if (it != single_.end()) {
        if (out_value) *out_value = it->second;
        return true;
      }
      return false;
    }
  }

  template <typename K = Key>
  std::enable_if_t<std::is_same_v<K, std::string>, bool>
  Remove(absl::string_view key) {
    return Remove(std::string(key));
  }

  template <typename K = Key>
  std::enable_if_t<std::is_same_v<K, std::string>, bool>
  Contains(absl::string_view key) const {
    if constexpr (ThreadSafe) {
      size_t idx = ShardIndex(absl::Hash<absl::string_view>{}(key));
      absl::ReaderMutexLock lock(&shards_[idx]->mutex);
      return shards_[idx]->map.find(key) != shards_[idx]->map.end();
    } else {
      return single_.find(key) != single_.end();
    }
  }

  // ── Low-level: raw map access (caller must hold lock!) ──
  // For bulk operations that acquire their own locks
  template <bool TS = ThreadSafe>
  std::enable_if_t<TS, const MapType &> ShardMap(size_t idx) const {
    return shards_[idx]->map;
  }
  template <bool TS = ThreadSafe>
  std::enable_if_t<TS, absl::Mutex &> ShardMutex(size_t idx) const {
    return shards_[idx]->mutex;
  }

private:
  struct Shard {
    MapType          map;
    mutable absl::Mutex mutex;
  };

  size_t ShardIndex(const Key &key) const {
    return DashHash{}(key) % shards_.size();
  }
  size_t ShardIndex(size_t hv) const { return hv % shards_.size(); }

  // ThreadSafe storage
  std::vector<std::unique_ptr<Shard>> shards_;
  std::atomic<size_t>                 size_;

  // Non-thread-safe storage
  MapType single_;
  mutable size_t size_unlocked_ = 0;
};

using StringMap  = DashMap<std::string, std::string>;
using BytesMap   = DashMap<std::string, std::vector<uint8_t>>;

}  // namespace astrakv
