#include "stream_data.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>

namespace astrakv {

std::string StreamData::auto_generate_id() {
  using namespace std::chrono;
  uint64_t now_ms = static_cast<uint64_t>(
    duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());

  if (now_ms < last_ms_) now_ms = last_ms_;

  if (now_ms == last_ms_) {
    last_seq_++;
  } else {
    last_ms_ = now_ms;
    last_seq_ = 0;
  }

  return std::to_string(last_ms_) + "-" + std::to_string(last_seq_);
}

int StreamData::compare_ids(const std::string &a, const std::string &b) {
  auto parse = [](const std::string &s) -> std::pair<uint64_t, uint64_t> {
    auto dash = s.find('-');
    if (dash == std::string::npos) return {0, 0};
    char *end = nullptr;
    uint64_t ms = std::strtoull(s.data(), &end, 10);
    uint64_t seq = std::strtoull(s.data() + dash + 1, nullptr, 10);
    return {ms, seq};
  };

  auto [ms1, seq1] = parse(a);
  auto [ms2, seq2] = parse(b);

  if (ms1 != ms2) return ms1 < ms2 ? -1 : 1;
  if (seq1 != seq2) return seq1 < seq2 ? -1 : 1;
  return 0;
}

std::string StreamData::XAdd(const std::string &id,
                             const std::vector<std::pair<std::string, std::string>> &fields) {
  std::string new_id;
  if (id == "*") {
    new_id = auto_generate_id();
  } else {
    if (!entries_.empty()) {
      if (compare_ids(id, entries_.back().id) <= 0) {
        return "";
      }
    }
    new_id = id;
    auto dash = new_id.find('-');
    if (dash != std::string::npos) {
      last_ms_ = std::strtoull(new_id.data(), nullptr, 10);
      last_seq_ = std::strtoull(new_id.data() + dash + 1, nullptr, 10);
    }
  }

  entries_.push_back(StreamEntry(new_id, fields));
  return new_id;
}

std::vector<StreamEntry> StreamData::XRead(const std::string &start_id, int64_t count) const {
  std::vector<StreamEntry> result;
  for (auto &e : entries_) {
    if (compare_ids(e.id, start_id) > 0) {
      result.push_back(e);
      if (count > 0 && static_cast<int64_t>(result.size()) >= count) break;
    }
  }
  return result;
}

std::vector<StreamEntry> StreamData::XRange(const std::string &start_id,
                                            const std::string &end_id, int64_t count) const {
  std::vector<StreamEntry> result;
  for (auto &e : entries_) {
    if (compare_ids(e.id, start_id) < 0) continue;
    if (compare_ids(e.id, end_id) > 0) break;
    result.push_back(e);
    if (count > 0 && static_cast<int64_t>(result.size()) >= count) break;
  }
  return result;
}

size_t StreamData::XLen() const {
  return entries_.size();
}

bool StreamData::XDel(const std::string &id) {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->id == id) {
      entries_.erase(it);
      return true;
    }
  }
  return false;
}

size_t StreamData::XTrim(int64_t maxlen) {
  size_t removed = 0;
  while (static_cast<int64_t>(entries_.size()) > maxlen && !entries_.empty()) {
    entries_.pop_front();
    removed++;
  }
  return removed;
}

size_t StreamData::MemoryUsage() const {
  size_t total = 0;
  for (auto &e : entries_) {
    total += e.id.size() + 16;
    for (auto &[field, value] : e.fields) {
      total += field.size() + value.size() + 32;
    }
  }
  return total;
}

}  // namespace astrakv
