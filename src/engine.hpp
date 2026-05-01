// AstrakV - KV engine combining DashMap + ZSet + List + Hash + Set + Metadata + RocksDB
#pragma once

#include "dash_map.hpp"
#include "eviction_2q.hpp"
#include "eviction_policy.hpp"
#include "linked_list.hpp"
#include "metadata.hpp"
#include "rocksdb_adapter.hpp"
#include "stream_data.hpp"
#include "vector_data.hpp"
#include "bitmap.hpp"
#include "hll.hpp"
#include "zset/bplustree_zset.hpp"

#include "simdjson.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace astrakv {

class Engine {
public:
  struct Options {
    std::string     path;
    size_t          shards   = 16;
    size_t          max_mb   = 0;
    EvictionPolicy  eviction = EvictionPolicy::k2Q;
  };

  struct Stats {
    int64_t put_count    = 0;
    int64_t get_count    = 0;
    int64_t hits         = 0;
    int64_t misses       = 0;
    int64_t evicted      = 0;
    int64_t expired      = 0;
    int64_t key_count    = 0;
    int64_t memory_bytes = 0;
  };

  explicit Engine(const Options &opts);
  ~Engine();

  Engine(const Engine &)            = delete;
  Engine &operator=(const Engine &) = delete;
  Engine(Engine &&)                 = delete;
  Engine &operator=(Engine &&)      = delete;

  // ── Pure KV ──
  bool put(std::string_view key, std::span<const uint8_t> value);
  std::optional<std::vector<uint8_t>> get(std::string_view key);
  bool del(std::string_view key);
  bool exists(std::string_view key);
  KeyType type(std::string_view key);

  // ── TTL ──
  bool expire(std::string_view key, int64_t ms);
  std::optional<int64_t> ttl(std::string_view key);

  // ── ZSet ──
  bool zadd(std::string_view key, std::string_view member, double score);
  std::optional<double> zscore(std::string_view key, std::string_view member);
  bool zrem(std::string_view key, std::string_view member);
  int64_t zcard(std::string_view key);
  std::vector<std::pair<std::string, double>> zrange(std::string_view key,
                                                      int64_t start, int64_t stop,
                                                      bool reverse);
  std::vector<std::pair<std::string, double>> zrangebyscore(std::string_view key,
                                                             double min, double max);

  // ── List ──
  bool lpush(std::string_view key, std::span<const uint8_t> value);
  bool rpush(std::string_view key, std::span<const uint8_t> value);
  std::optional<std::vector<uint8_t>> lpop(std::string_view key);
  std::optional<std::vector<uint8_t>> rpop(std::string_view key);
  int64_t llen(std::string_view key);

  // ── Hash ──
  bool hset(std::string_view key, std::string_view field, std::string_view value);
  std::optional<std::string> hget(std::string_view key, std::string_view field);
  bool hdel(std::string_view key, std::string_view field);
  bool hexists(std::string_view key, std::string_view field);
  std::vector<std::pair<std::string, std::string>> hgetall(std::string_view key);
  int64_t hlen(std::string_view key);
  std::optional<int64_t> hincrby(std::string_view key, std::string_view field, int64_t delta);
  template <typename F> void forEachHash(std::string_view key, F &&callback);

  // ── Set ──
  bool sadd(std::string_view key, std::string_view member);
  bool srem(std::string_view key, std::string_view member);
  bool sismember(std::string_view key, std::string_view member);
  std::vector<std::string> smembers(std::string_view key);
  int64_t scard(std::string_view key);
  template <typename F> void forEachSet(std::string_view key, F &&callback);

  // ── Iteration ──
  std::vector<std::pair<std::string, std::vector<uint8_t>>> all_entries();
  std::vector<std::string> all_keys();

  // ── Stream ──
  std::string xadd(std::string_view key, const std::string &id,
                   const std::vector<std::pair<std::string, std::string>> &fields);
  std::vector<StreamEntry> xread(std::string_view key, const std::string &start_id, int64_t count);
  std::vector<StreamEntry> xrange(std::string_view key, const std::string &start, const std::string &end, int64_t count);
  int64_t xlen(std::string_view key);
  bool xdel(std::string_view key, const std::string &id);
  int64_t xtrim(std::string_view key, int64_t maxlen);

  // ── JSON ──
  bool jsonset(std::string_view key, std::string_view path, const std::string &json_value);
  std::optional<std::string> jsonget(std::string_view key, std::string_view path);
  bool jsondel(std::string_view key, std::string_view path);

  // ── Vector ──
  bool vecadd(std::string_view key, const std::string &id, std::span<const float> vec);
  std::vector<float> vecget(std::string_view key, const std::string &id);
  bool vecdel(std::string_view key, const std::string &id);
  std::vector<std::pair<std::string, float>> vecsearch(std::string_view key, std::span<const float> query, size_t k);
  int64_t vecsize(std::string_view key);

  // ── Stats ──
  int64_t count() const;
  int64_t memory_usage() const;

  // ── Maintenance ──
  int64_t      CleanupExpired();
  void         set_eviction(EvictionPolicy p) { opts_.eviction = p; }
  EvictionPolicy get_eviction() const { return opts_.eviction; }
  Stats        stats() const;

  std::string last_error() const { return last_error_; }

 private:
  // Type aliases (must come before method declarations that use them)
  using ZSetType   = ZSetBPlus<std::string, double>;
  using HashType   = DashMap<std::string, std::string>;
  using SetType    = DashMap<std::string, uint8_t>;
  using StreamType = StreamData;
  using JsonType   = std::string;  // raw JSON text, parsed on demand with simdjson
  using VectorType = VectorData;
  using BytesMap   = DashMap<std::string, std::vector<uint8_t>>;
  using ZSetMap    = DashMap<std::string, std::shared_ptr<ZSetType>>;
  using ListMap    = DashMap<std::string, std::shared_ptr<StringList>>;
  using HashMap    = DashMap<std::string, std::shared_ptr<HashType>>;
  using SetMap     = DashMap<std::string, std::shared_ptr<SetType>>;
  using StreamMap  = DashMap<std::string, std::shared_ptr<StreamType>>;
  using JsonMap    = DashMap<std::string, std::shared_ptr<JsonType>>;
  using VectorMap  = DashMap<std::string, std::shared_ptr<VectorType>>;

  std::string make_key(std::string_view k) const { return std::string(k); }

  // ZSet helpers
  std::shared_ptr<ZSetType> get_or_create_zset(const std::string &key);
  void persist_zset(const std::string &key, const ZSetType &zs);
  bool load_zset(const std::string &key, ZSetType &zs);

  // List helpers
  std::shared_ptr<StringList> get_or_create_list(const std::string &key);
  void persist_list(const std::string &key, const StringList &lst);
  bool load_list(const std::string &key, StringList &lst);

  // Hash helpers
  std::shared_ptr<HashType> get_or_create_hash(const std::string &key);
  void persist_hash(const std::string &key, const HashType &h);
  bool load_hash(const std::string &key, HashType &h);

  // Set helpers
  std::shared_ptr<SetType> get_or_create_set(const std::string &key);
  void persist_set(const std::string &key, const SetType &s);
  bool load_set(const std::string &key, SetType &s);

  // Stream helpers
  std::shared_ptr<StreamType> get_or_create_stream(const std::string &key);
  void persist_stream(const std::string &key, const StreamType &s);
  bool load_stream(const std::string &key, StreamType &s);

  // JSON helpers
  std::shared_ptr<JsonType> get_or_create_json(const std::string &key);
  void persist_json(const std::string &key, const JsonType &j);
  bool load_json(const std::string &key, JsonType &j);

  // Vector helpers
  std::shared_ptr<VectorType> get_or_create_vector(const std::string &key, size_t dim);
  void persist_vector(const std::string &key, const VectorType &v);
  bool load_vector(const std::string &key, VectorType &v);

  void check_memory_and_evict();
  void record_access(const std::string &key);

  Options                     opts_;
  Eviction2Q                  eviction_2q_;
  mutable std::atomic<int64_t> put_count_{0}, get_count_{0}, hits_{0}, misses_{0};
  mutable std::atomic<int64_t> evicted_{0}, expired_{0};
  BytesMap                    bytes_;
  ZSetMap                     zsets_;
  ListMap                     lists_;
  HashMap                     hashes_;
  SetMap                      sets_;
  StreamMap                   streams_;
  JsonMap                     jsons_;
  VectorMap                   vectors_;
  KeyMetadataManager          meta_;
  std::unique_ptr<RocksDBAdapter> rocksdb_;
  mutable std::string         last_error_;
};

// Template implementations (must be in header)
template <typename F>
void Engine::forEachHash(std::string_view key, F &&callback) {
  auto k = make_key(key);
  auto h = get_or_create_hash(k);
  if (!h) return;
  h->ForEach(std::forward<F>(callback));
}

template <typename F>
void Engine::forEachSet(std::string_view key, F &&callback) {
  auto k = make_key(key);
  auto s = get_or_create_set(k);
  if (!s) return;
  s->ForEach([&](const std::string &member, uint8_t) { callback(member); });
}

}  // namespace astrakv
