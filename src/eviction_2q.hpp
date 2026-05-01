// AstrakV - Dragonfly-inspired 2Q eviction strategy
// Based on "2Q: A Low Overhead High Performance Buffer Management Replacement Algorithm"
#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>

#include <deque>
#include <optional>
#include <string>

namespace astrakv {

class Eviction2Q {
public:
  explicit Eviction2Q(double probationary_ratio = 0.067);

  // Record that a key was accessed. If new key, enters probationary queue.
  void RecordAccess(const std::string &key, bool is_new);
  // Remove a key from tracking (e.g. on explicit delete).
  void RemoveKey(const std::string &key);
  // Get a victim key to evict (LRU/FIFO depending on queue).
  std::optional<std::string> GetVictim();
  // Number of keys tracked.
  size_t Size() const;

private:
  enum class State { kProbationary, kProtected };

  void promote_to_protected(const std::string &key);

  std::deque<std::string> probationary_queue_;   // FIFO — new keys
  std::deque<std::string> protected_queue_;      // LRU — frequently accessed
  absl::flat_hash_map<std::string, State> key_state_;
  double probationary_ratio_;
  mutable absl::Mutex mutex_;
};

}  // namespace astrakv
