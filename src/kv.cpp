#include "astrakv/kv.hpp"
#include "engine.hpp"

#include <cstring>
#include <memory>

namespace astrakv {

// ── Kv ──

Kv::Kv(const Options &opts) {
  Engine::Options o;
  o.path   = opts.path;
  o.shards = opts.shards;
  o.max_mb = opts.max_memory_mb;
  handle_ = new Engine(o);
}

Kv::~Kv() { delete static_cast<Engine *>(handle_); }

static inline Engine *eng(void *h) { return static_cast<Engine *>(h); }

bool Kv::put(std::string_view key, std::span<const uint8_t> value) {
  return eng(handle_)->put(key, value);
}
std::optional<std::vector<uint8_t>> Kv::get(std::string_view key) {
  return eng(handle_)->get(key);
}
bool Kv::del(std::string_view key) {
  return eng(handle_)->del(key);
}
bool Kv::exists(std::string_view key) {
  return eng(handle_)->exists(key);
}

bool Kv::expire(std::string_view key, int64_t ms) {
  return eng(handle_)->expire(key, ms);
}
std::optional<int64_t> Kv::ttl(std::string_view key) {
  return eng(handle_)->ttl(key);
}

bool Kv::zadd(std::string_view key, std::string_view member, double score) {
  return eng(handle_)->zadd(key, member, score);
}
std::optional<double> Kv::zscore(std::string_view key, std::string_view member) {
  return eng(handle_)->zscore(key, member);
}
bool Kv::zrem(std::string_view key, std::string_view member) {
  return eng(handle_)->zrem(key, member);
}
int64_t Kv::zcard(std::string_view key) {
  return eng(handle_)->zcard(key);
}
std::vector<std::pair<std::string, double>> Kv::zrange(
    std::string_view key, int64_t start, int64_t stop, bool reverse) {
  return eng(handle_)->zrange(key, start, stop, reverse);
}
std::vector<std::pair<std::string, double>> Kv::zrangebyscore(
    std::string_view key, double min, double max) {
  return eng(handle_)->zrangebyscore(key, min, max);
}

bool Kv::lpush(std::string_view key, std::span<const uint8_t> value) {
  return eng(handle_)->lpush(key, value);
}
bool Kv::rpush(std::string_view key, std::span<const uint8_t> value) {
  return eng(handle_)->rpush(key, value);
}
std::optional<std::vector<uint8_t>> Kv::lpop(std::string_view key) {
  return eng(handle_)->lpop(key);
}
std::optional<std::vector<uint8_t>> Kv::rpop(std::string_view key) {
  return eng(handle_)->rpop(key);
}
int64_t Kv::llen(std::string_view key) {
  return eng(handle_)->llen(key);
}

// Hash
bool Kv::hset(std::string_view key, std::string_view field, std::string_view value) {
  return eng(handle_)->hset(key, field, value);
}
std::optional<std::string> Kv::hget(std::string_view key, std::string_view field) {
  return eng(handle_)->hget(key, field);
}
bool Kv::hdel(std::string_view key, std::string_view field) {
  return eng(handle_)->hdel(key, field);
}
bool Kv::hexists(std::string_view key, std::string_view field) {
  return eng(handle_)->hexists(key, field);
}
std::vector<std::pair<std::string, std::string>> Kv::hgetall(std::string_view key) {
  return eng(handle_)->hgetall(key);
}
int64_t Kv::hlen(std::string_view key) {
  return eng(handle_)->hlen(key);
}
std::optional<int64_t> Kv::hincrby(std::string_view key, std::string_view field, int64_t delta) {
  return eng(handle_)->hincrby(key, field, delta);
}

// Set
bool Kv::sadd(std::string_view key, std::string_view member) {
  return eng(handle_)->sadd(key, member);
}
bool Kv::srem(std::string_view key, std::string_view member) {
  return eng(handle_)->srem(key, member);
}
bool Kv::sismember(std::string_view key, std::string_view member) {
  return eng(handle_)->sismember(key, member);
}
std::vector<std::string> Kv::smembers(std::string_view key) {
  return eng(handle_)->smembers(key);
}
int64_t Kv::scard(std::string_view key) {
  return eng(handle_)->scard(key);
}

// Stream
std::string Kv::xadd(std::string_view key, const std::string &id,
                     const std::vector<std::pair<std::string, std::string>> &fields) {
  return eng(handle_)->xadd(key, id, fields);
}
std::vector<StreamEntry> Kv::xread(std::string_view key, const std::string &start_id, int64_t count) {
  return eng(handle_)->xread(key, start_id, count);
}
std::vector<StreamEntry> Kv::xrange(std::string_view key, const std::string &start, const std::string &end, int64_t count) {
  return eng(handle_)->xrange(key, start, end, count);
}
int64_t Kv::xlen(std::string_view key) {
  return eng(handle_)->xlen(key);
}
bool Kv::xdel(std::string_view key, const std::string &id) {
  return eng(handle_)->xdel(key, id);
}
int64_t Kv::xtrim(std::string_view key, int64_t maxlen) {
  return eng(handle_)->xtrim(key, maxlen);
}

// JSON
bool Kv::jsonset(std::string_view key, std::string_view path, const std::string &json_value) {
  return eng(handle_)->jsonset(key, path, json_value);
}
std::optional<std::string> Kv::jsonget(std::string_view key, std::string_view path) {
  return eng(handle_)->jsonget(key, path);
}
bool Kv::jsondel(std::string_view key, std::string_view path) {
  return eng(handle_)->jsondel(key, path);
}

// Vector
bool Kv::vecadd(std::string_view key, const std::string &id, std::span<const float> vec) {
  return eng(handle_)->vecadd(key, id, vec);
}
std::vector<float> Kv::vecget(std::string_view key, const std::string &id) {
  return eng(handle_)->vecget(key, id);
}
bool Kv::vecdel(std::string_view key, const std::string &id) {
  return eng(handle_)->vecdel(key, id);
}
std::vector<std::pair<std::string, float>> Kv::vecsearch(std::string_view key, std::span<const float> query, size_t k) {
  return eng(handle_)->vecsearch(key, query, k);
}
int64_t Kv::vecsize(std::string_view key) {
  return eng(handle_)->vecsize(key);
}

Kv::Stats Kv::stats() const {
  auto s = eng(handle_)->stats();
  return {s.put_count, s.get_count, s.hits, s.misses,
          s.evicted, s.expired, s.key_count, s.memory_bytes};
}
int64_t Kv::count() const { return eng(handle_)->count(); }
int64_t Kv::memory_usage() const { return eng(handle_)->memory_usage(); }

void Kv::set_eviction(EvictionPolicy p) {
  eng(handle_)->set_eviction(static_cast<astrakv::EvictionPolicy>(p));
}
Kv::EvictionPolicy Kv::get_eviction() const {
  return static_cast<Kv::EvictionPolicy>(eng(handle_)->get_eviction());
}

// ── Iter ──

struct KvIter {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
  size_t pos = 0;
};

Kv::Iter::Iter(Kv *kv) {
  auto *it  = new KvIter;
  it->entries = static_cast<Engine *>(kv->handle_)->all_entries();
  it->pos     = 0;
  iter_ = it;
}

Kv::Iter::~Iter() { delete static_cast<KvIter *>(iter_); }

std::optional<std::pair<std::string, std::vector<uint8_t>>>
Kv::Iter::next() {
  auto *it = static_cast<KvIter *>(iter_);
  if (it->pos >= it->entries.size()) return std::nullopt;
  auto &[k, v] = it->entries[it->pos++];
  return std::make_pair(k, v);
}

Kv::Iter Kv::iter() { return Iter(this); }

}  // namespace astrakv
