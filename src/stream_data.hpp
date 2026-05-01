#pragma once
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace astrakv {

struct StreamEntry {
  std::string id;
  std::vector<std::pair<std::string, std::string>> fields;

  StreamEntry() = default;
  StreamEntry(std::string id_, std::vector<std::pair<std::string, std::string>> f)
    : id(std::move(id_)), fields(std::move(f)) {}
};

class StreamData {
public:
  std::string XAdd(const std::string &id,
                   const std::vector<std::pair<std::string, std::string>> &fields);

  std::vector<StreamEntry> XRead(const std::string &start_id, int64_t count) const;

  std::vector<StreamEntry> XRange(const std::string &start_id,
                                  const std::string &end_id, int64_t count = -1) const;

  size_t XLen() const;
  bool XDel(const std::string &id);
  size_t XTrim(int64_t maxlen);

  size_t Size() const { return entries_.size(); }
  size_t MemoryUsage() const;

  std::vector<StreamEntry> AllEntries() const {
    return std::vector<StreamEntry>(entries_.begin(), entries_.end());
  }

private:
  std::deque<StreamEntry> entries_;
  uint64_t last_ms_ = 0;
  uint64_t last_seq_ = 0;

  std::string auto_generate_id();
  static int compare_ids(const std::string &a, const std::string &b);
};

}  // namespace astrakv
