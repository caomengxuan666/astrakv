#include "rocksdb_adapter.hpp"

#include <filesystem>

namespace astrakv {

RocksDBAdapter::RocksDBAdapter(const std::string& db_path)
    : db_path_(db_path), logger_(spdlog::get("astrakv")) {
  if (!logger_) {
    logger_ = spdlog::default_logger();
  }

  std::filesystem::path path(db_path_);
  if (!std::filesystem::exists(path.parent_path())) {
    std::filesystem::create_directories(path.parent_path());
  }

  rocksdb::Options options;
  options.create_if_missing = true;
  options.compression = rocksdb::kZlibCompression;

  rocksdb::Status status = rocksdb::DB::Open(options, db_path_, &db_);
  if (!status.ok()) {
    logger_->error("Failed to open RocksDB at {}: {}", db_path_,
                   status.ToString());
    db_ = nullptr;
    return;
  }
  logger_->info("RocksDB opened successfully at {}", db_path_);
}

RocksDBAdapter::~RocksDBAdapter() {
  if (db_) {
    db_->Close();
    logger_->info("RocksDB closed at {}", db_path_);
  }
}

bool RocksDBAdapter::Put(const std::string& key, const std::string& value) {
  if (!db_) {
    logger_->error("RocksDB is not open");
    return false;
  }
  rocksdb::Status status = db_->Put(rocksdb::WriteOptions(), key, value);
  if (!status.ok()) {
    logger_->error("Failed to put key {}: {}", key, status.ToString());
    return false;
  }
  return true;
}

std::optional<std::string> RocksDBAdapter::Get(const std::string& key) {
  if (!db_) {
    logger_->error("RocksDB is not open");
    return std::nullopt;
  }
  std::string value;
  rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), key, &value);
  if (status.IsNotFound()) return std::nullopt;
  if (!status.ok()) {
    logger_->error("Failed to get key {}: {}", key, status.ToString());
    return std::nullopt;
  }
  return value;
}

bool RocksDBAdapter::Delete(const std::string& key) {
  if (!db_) {
    logger_->error("RocksDB is not open");
    return false;
  }
  rocksdb::Status status = db_->Delete(rocksdb::WriteOptions(), key);
  if (!status.ok()) {
    logger_->error("Failed to delete key {}: {}", key, status.ToString());
    return false;
  }
  return true;
}

bool RocksDBAdapter::Exists(const std::string& key) {
  if (!db_) {
    logger_->error("RocksDB is not open");
    return false;
  }
  std::string value;
  rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), key, &value);
  return status.ok();
}

bool RocksDBAdapter::BatchPut(
    const std::vector<std::pair<std::string, std::string>>& kvs) {
  if (!db_) {
    logger_->error("RocksDB is not open");
    return false;
  }
  rocksdb::WriteBatch batch;
  for (const auto& kv : kvs) {
    batch.Put(kv.first, kv.second);
  }
  rocksdb::Status status = db_->Write(rocksdb::WriteOptions(), &batch);
  if (!status.ok()) {
    logger_->error("Failed to batch write: {}", status.ToString());
    return false;
  }
  return true;
}

size_t RocksDBAdapter::GetApproximateCount() {
  if (!db_) {
    logger_->error("RocksDB is not open");
    return 0;
  }
  uint64_t count;
  if (!db_->GetIntProperty(rocksdb::DB::Properties::kEstimateNumKeys,
                           &count)) {
    logger_->error("Failed to get approximate count");
    return 0;
  }
  return static_cast<size_t>(count);
}

bool RocksDBAdapter::Flush() {
  if (!db_) {
    logger_->error("RocksDB is not open");
    return false;
  }
  rocksdb::Status status = db_->Flush(rocksdb::FlushOptions());
  if (!status.ok()) {
    logger_->error("Failed to flush: {}", status.ToString());
    return false;
  }
  return true;
}

bool RocksDBAdapter::Compact() {
  if (!db_) {
    logger_->error("RocksDB is not open");
    return false;
  }
  rocksdb::Status status =
      db_->CompactRange(rocksdb::CompactRangeOptions(), nullptr, nullptr);
  if (!status.ok()) {
    logger_->error("Failed to compact: {}", status.ToString());
    return false;
  }
  return true;
}

}  // namespace astrakv
