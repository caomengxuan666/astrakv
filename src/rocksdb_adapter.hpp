#pragma once

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace astrakv {

class RocksDBAdapter {
 public:
  explicit RocksDBAdapter(const std::string& db_path);

  ~RocksDBAdapter();

  bool Put(const std::string& key, const std::string& value);
  std::optional<std::string> Get(const std::string& key);
  bool Delete(const std::string& key);
  bool Exists(const std::string& key);
  bool BatchPut(
      const std::vector<std::pair<std::string, std::string>>& kvs);
  size_t GetApproximateCount();
  bool Flush();
  bool Compact();

  bool IsOpen() const { return db_ != nullptr; }
  const std::string& GetPath() const { return db_path_; }

  RocksDBAdapter(const RocksDBAdapter&) = delete;
  RocksDBAdapter& operator=(const RocksDBAdapter&) = delete;
  RocksDBAdapter(RocksDBAdapter&&) = delete;
  RocksDBAdapter& operator=(RocksDBAdapter&&) = delete;

 private:
  std::string db_path_;
  std::unique_ptr<rocksdb::DB> db_;
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace astrakv
