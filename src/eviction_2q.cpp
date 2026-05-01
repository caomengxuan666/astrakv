#include "eviction_2q.hpp"

namespace astrakv {

Eviction2Q::Eviction2Q(double probationary_ratio)
    : probationary_ratio_(probationary_ratio) {}

void Eviction2Q::RecordAccess(const std::string &key, bool is_new) {
  absl::MutexLock lock(&mutex_);
  auto it = key_state_.find(key);
  if (it == key_state_.end()) {
    // New key: add to probationary queue (FIFO)
    probationary_queue_.push_back(key);
    key_state_[key] = State::kProbationary;
    return;
  }
  if (it->second == State::kProbationary) {
    // Second access: promote to protected queue (LRU)
    promote_to_protected(key);
  } else {
    // Already protected: move to back (most-recently-used)
    for (auto qit = protected_queue_.begin(); qit != protected_queue_.end(); ++qit) {
      if (*qit == key) {
        protected_queue_.erase(qit);
        break;
      }
    }
    protected_queue_.push_back(key);
  }
}

void Eviction2Q::promote_to_protected(const std::string &key) {
  // Remove from probationary
  for (auto it = probationary_queue_.begin(); it != probationary_queue_.end(); ++it) {
    if (*it == key) {
      probationary_queue_.erase(it);
      break;
    }
  }
  // Demote protected LRU if needed
  size_t total = probationary_queue_.size() + protected_queue_.size();
  size_t max_protected = static_cast<size_t>(total * (1.0 - probationary_ratio_));
  while (protected_queue_.size() > max_protected && !protected_queue_.empty()) {
    std::string demoted = protected_queue_.front();
    protected_queue_.pop_front();
    key_state_[demoted] = State::kProbationary;
    probationary_queue_.push_back(demoted);
  }
  // Add to protected (LRU end)
  protected_queue_.push_back(key);
  key_state_[key] = State::kProtected;
}

void Eviction2Q::RemoveKey(const std::string &key) {
  absl::MutexLock lock(&mutex_);
  auto it = key_state_.find(key);
  if (it == key_state_.end()) return;
  if (it->second == State::kProbationary) {
    for (auto qit = probationary_queue_.begin(); qit != probationary_queue_.end(); ++qit) {
      if (*qit == key) { probationary_queue_.erase(qit); break; }
    }
  } else {
    for (auto qit = protected_queue_.begin(); qit != protected_queue_.end(); ++qit) {
      if (*qit == key) { protected_queue_.erase(qit); break; }
    }
  }
  key_state_.erase(it);
}

std::optional<std::string> Eviction2Q::GetVictim() {
  absl::MutexLock lock(&mutex_);
  // Evict from probationary first (FIFO)
  if (!probationary_queue_.empty()) {
    std::string victim = probationary_queue_.front();
    probationary_queue_.pop_front();
    key_state_.erase(victim);
    return victim;
  }
  // Evict from protected (LRU)
  if (!protected_queue_.empty()) {
    std::string victim = protected_queue_.front();
    protected_queue_.pop_front();
    key_state_.erase(victim);
    return victim;
  }
  return std::nullopt;
}

size_t Eviction2Q::Size() const {
  absl::MutexLock lock(&mutex_);
  return key_state_.size();
}

}  // namespace astrakv
