// AstrakV - C++ API
// Convenience header for C++ users (wraps kv.h as a modern class)
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "stream_data.hpp"

namespace astrakv {

class Kv {
public:
  struct Options {
    std::string path;
    size_t      shards        = 16;
    size_t      max_memory_mb = 0;
  };

  explicit Kv(const Options &opts);
  ~Kv();

  Kv(const Kv &)            = delete;
  Kv &operator=(const Kv &) = delete;
  Kv(Kv &&)                 = delete;
  Kv &operator=(Kv &&)      = delete;

  bool put(std::string_view key, std::span<const uint8_t> value);
  std::optional<std::vector<uint8_t>> get(std::string_view key);
  bool del(std::string_view key);
  bool exists(std::string_view key);

  // TTL
  bool expire(std::string_view key, int64_t ms);
  std::optional<int64_t> ttl(std::string_view key);

  // ZSet
  bool zadd(std::string_view key, std::string_view member, double score);
  std::optional<double> zscore(std::string_view key, std::string_view member);
  bool zrem(std::string_view key, std::string_view member);
  int64_t zcard(std::string_view key);
  std::vector<std::pair<std::string, double>> zrange(std::string_view key,
                                                       int64_t start, int64_t stop,
                                                       bool reverse = false);
  std::vector<std::pair<std::string, double>> zrangebyscore(std::string_view key,
                                                             double min, double max);

  // List
  bool lpush(std::string_view key, std::span<const uint8_t> value);
  bool rpush(std::string_view key, std::span<const uint8_t> value);
  std::optional<std::vector<uint8_t>> lpop(std::string_view key);
  std::optional<std::vector<uint8_t>> rpop(std::string_view key);
  int64_t llen(std::string_view key);

  // Hash
  bool hset(std::string_view key, std::string_view field, std::string_view value);
  std::optional<std::string> hget(std::string_view key, std::string_view field);
  bool hdel(std::string_view key, std::string_view field);
  bool hexists(std::string_view key, std::string_view field);
  std::vector<std::pair<std::string, std::string>> hgetall(std::string_view key);
  int64_t hlen(std::string_view key);
  std::optional<int64_t> hincrby(std::string_view key, std::string_view field, int64_t delta);

  // Set
  bool sadd(std::string_view key, std::string_view member);
  bool srem(std::string_view key, std::string_view member);
  bool sismember(std::string_view key, std::string_view member);
  std::vector<std::string> smembers(std::string_view key);
  int64_t scard(std::string_view key);

  // Stream
  std::string xadd(std::string_view key, const std::string &id,
                   const std::vector<std::pair<std::string, std::string>> &fields);
  std::vector<StreamEntry> xread(std::string_view key, const std::string &start_id, int64_t count);
  std::vector<StreamEntry> xrange(std::string_view key, const std::string &start, const std::string &end, int64_t count);
  int64_t xlen(std::string_view key);
  bool xdel(std::string_view key, const std::string &id);
  int64_t xtrim(std::string_view key, int64_t maxlen);

  // JSON
  bool jsonset(std::string_view key, std::string_view path, const std::string &json_value);
  std::optional<std::string> jsonget(std::string_view key, std::string_view path);
  bool jsondel(std::string_view key, std::string_view path);

  // Vector
  bool vecadd(std::string_view key, const std::string &id, std::span<const float> vec);
  std::vector<float> vecget(std::string_view key, const std::string &id);
  bool vecdel(std::string_view key, const std::string &id);
  std::vector<std::pair<std::string, float>> vecsearch(std::string_view key, std::span<const float> query, size_t k);
  int64_t vecsize(std::string_view key);

  // Iterator
  class Iter {
  public:
    explicit Iter(Kv *kv);
    ~Iter();
    std::optional<std::pair<std::string, std::vector<uint8_t>>> next();
  private:
    void *iter_;
  };
  Iter iter();

  // Stats
  struct Stats {
    int64_t put_count;
    int64_t get_count;
    int64_t hits;
    int64_t misses;
    int64_t evicted;
    int64_t expired;
    int64_t key_count;
    int64_t memory_bytes;
  };
  Stats stats() const;
  int64_t count() const;
  int64_t memory_usage() const;

  // Eviction
  enum class EvictionPolicy : int {
    kNoEviction     = 0,
    kAllKeysLRU     = 1,
    kVolatileLRU    = 2,
    kAllKeysLFU     = 3,
    kVolatileLFU    = 4,
    kAllKeysRandom  = 5,
    kVolatileRandom = 6,
    kVolatileTTL    = 7,
    k2Q             = 8,
  };
  void set_eviction(EvictionPolicy p);
  EvictionPolicy get_eviction() const;

private:
  void *handle_;
};

// Convenience: string overloads
inline bool put(Kv &kv, std::string_view key, std::string_view value) {
  return kv.put(key, std::span<const uint8_t>(
    reinterpret_cast<const uint8_t *>(value.data()), value.size()));
}
inline std::optional<std::string> get_str(Kv &kv, std::string_view key) {
  auto v = kv.get(key);
  if (!v) return std::nullopt;
  return std::string(reinterpret_cast<const char *>(v->data()), v->size());
}

}  // namespace astrakv
