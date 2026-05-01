#include "engine.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>
#include <vector_data.hpp>

namespace astrakv {

// ── Serialization helpers (internal free functions) ──

static std::string serialize_zset(const ZSetBPlus<std::string, double> &zs) {
  auto all = zs.GetAll();
  uint32_t count = static_cast<uint32_t>(all.size());
  size_t total = 4; // count
  for (auto &[mem, score] : all) total += 4 + mem.size() + 8;

  std::string out;
  out.resize(total);
  char *ptr = out.data();
  std::memcpy(ptr, &count, 4); ptr += 4;
  for (auto &[mem, score] : all) {
    uint32_t mlen = static_cast<uint32_t>(mem.size());
    std::memcpy(ptr, &mlen, 4);       ptr += 4;
    std::memcpy(ptr, mem.data(), mlen); ptr += mlen;
    std::memcpy(ptr, &score, 8);      ptr += 8;
  }
  return out;
}

static bool deserialize_zset(const std::string &data, ZSetBPlus<std::string, double> &zs) {
  if (data.size() < 4) return false;
  const char *ptr = data.data();
  const char *end = data.data() + data.size();
  uint32_t count;
  std::memcpy(&count, ptr, 4); ptr += 4;
  for (uint32_t i = 0; i < count; ++i) {
    if (ptr + 4 > end) return false;
    uint32_t mlen;
    std::memcpy(&mlen, ptr, 4); ptr += 4;
    if (ptr + mlen + 8 > end) return false;
    std::string member(ptr, mlen); ptr += mlen;
    double score;
    std::memcpy(&score, ptr, 8); ptr += 8;
    zs.Add(member, score);
  }
  return true;
}

static std::string serialize_list(const StringList &lst) {
  auto range = lst.Range(0, -1);
  uint32_t count = static_cast<uint32_t>(range.size());
  size_t total = 4;
  for (auto &item : range) total += 4 + item.size();

  std::string out;
  out.resize(total);
  char *ptr = out.data();
  std::memcpy(ptr, &count, 4); ptr += 4;
  for (auto &item : range) {
    uint32_t ilen = static_cast<uint32_t>(item.size());
    std::memcpy(ptr, &ilen, 4);        ptr += 4;
    std::memcpy(ptr, item.data(), ilen); ptr += ilen;
  }
  return out;
}

static bool deserialize_list(const std::string &data, StringList &lst) {
  if (data.size() < 4) return false;
  const char *ptr = data.data();
  const char *end = data.data() + data.size();
  uint32_t count;
  std::memcpy(&count, ptr, 4); ptr += 4;
  for (uint32_t i = 0; i < count; ++i) {
    if (ptr + 4 > end) return false;
    uint32_t ilen;
    std::memcpy(&ilen, ptr, 4); ptr += 4;
    if (ptr + ilen > end) return false;
    std::string item(ptr, ilen); ptr += ilen;
    lst.PushRight(std::move(item));
  }
  return true;
}

static std::string serialize_hash(const DashMap<std::string, std::string> &h) {
  auto all = h.AllEntries();
  uint32_t count = static_cast<uint32_t>(all.size());
  size_t total = 4; // count
  for (auto &[field, value] : all) total += 4 + field.size() + 4 + value.size();

  std::string out;
  out.resize(total);
  char *ptr = out.data();
  std::memcpy(ptr, &count, 4); ptr += 4;
  for (auto &[field, value] : all) {
    uint32_t flen = static_cast<uint32_t>(field.size());
    std::memcpy(ptr, &flen, 4);          ptr += 4;
    std::memcpy(ptr, field.data(), flen); ptr += flen;
    uint32_t vlen = static_cast<uint32_t>(value.size());
    std::memcpy(ptr, &vlen, 4);           ptr += 4;
    std::memcpy(ptr, value.data(), vlen); ptr += vlen;
  }
  return out;
}

static bool deserialize_hash(const std::string &data, DashMap<std::string, std::string> &h) {
  if (data.size() < 4) return false;
  const char *ptr = data.data();
  const char *end = data.data() + data.size();
  uint32_t count;
  std::memcpy(&count, ptr, 4); ptr += 4;
  for (uint32_t i = 0; i < count; ++i) {
    if (ptr + 4 > end) return false;
    uint32_t flen;
    std::memcpy(&flen, ptr, 4); ptr += 4;
    if (ptr + flen > end) return false;
    std::string field(ptr, flen); ptr += flen;
    if (ptr + 4 > end) return false;
    uint32_t vlen;
    std::memcpy(&vlen, ptr, 4); ptr += 4;
    if (ptr + vlen > end) return false;
    std::string value(ptr, vlen); ptr += vlen;
    h.Insert(std::move(field), std::move(value));
  }
  return true;
}

static std::string serialize_set(const DashMap<std::string, uint8_t> &s) {
  auto keys = s.AllKeys();
  uint32_t count = static_cast<uint32_t>(keys.size());
  size_t total = 4;
  for (auto &member : keys) total += 4 + member.size();

  std::string out;
  out.resize(total);
  char *ptr = out.data();
  std::memcpy(ptr, &count, 4); ptr += 4;
  for (auto &member : keys) {
    uint32_t mlen = static_cast<uint32_t>(member.size());
    std::memcpy(ptr, &mlen, 4);            ptr += 4;
    std::memcpy(ptr, member.data(), mlen); ptr += mlen;
  }
  return out;
}

static bool deserialize_set(const std::string &data, DashMap<std::string, uint8_t> &s) {
  if (data.size() < 4) return false;
  const char *ptr = data.data();
  const char *end = data.data() + data.size();
  uint32_t count;
  std::memcpy(&count, ptr, 4); ptr += 4;
  for (uint32_t i = 0; i < count; ++i) {
    if (ptr + 4 > end) return false;
    uint32_t mlen;
    std::memcpy(&mlen, ptr, 4); ptr += 4;
    if (ptr + mlen > end) return false;
    std::string member(ptr, mlen); ptr += mlen;
    s.Insert(std::move(member), 0);
  }
  return true;
}

static std::string serialize_vector(const VectorData &v) {
  auto all = v.AllEntries();
  uint32_t dim = static_cast<uint32_t>(v.Dimension());
  uint32_t count = static_cast<uint32_t>(all.size());
  size_t total = 4 + 4; // dim + count
  for (auto &[id, vec] : all) total += 4 + id.size() + dim * 4;

  std::string out;
  out.resize(total);
  char *ptr = out.data();
  std::memcpy(ptr, &dim, 4); ptr += 4;
  std::memcpy(ptr, &count, 4); ptr += 4;
  for (auto &[id, vec] : all) {
    uint32_t ilen = static_cast<uint32_t>(id.size());
    std::memcpy(ptr, &ilen, 4); ptr += 4;
    std::memcpy(ptr, id.data(), ilen); ptr += ilen;
    std::memcpy(ptr, vec.data(), dim * 4); ptr += dim * 4;
  }
  return out;
}

static bool deserialize_vector(const std::string &data, VectorData &v) {
  if (data.size() < 8) return false;
  const char *ptr = data.data();
  const char *end = data.data() + data.size();
  uint32_t dim;
  std::memcpy(&dim, ptr, 4); ptr += 4;
  uint32_t count;
  std::memcpy(&count, ptr, 4); ptr += 4;
  for (uint32_t i = 0; i < count; ++i) {
    if (ptr + 4 > end) return false;
    uint32_t ilen;
    std::memcpy(&ilen, ptr, 4); ptr += 4;
    if (ptr + ilen + dim * 4 > end) return false;
    std::string id(ptr, ilen); ptr += ilen;
    std::span<const float> vec(reinterpret_cast<const float *>(ptr), dim);
    ptr += dim * 4;
    v.Add(id, vec);
  }
  return true;
}

static std::string serialize_stream(const StreamData &s) {
  auto all = s.AllEntries();
  uint32_t count = static_cast<uint32_t>(all.size());
  size_t total = 4;
  for (auto &e : all) {
    total += 4 + e.id.size() + 4;
    for (auto &[f, v] : e.fields) total += 4 + f.size() + 4 + v.size();
  }
  std::string out;
  out.resize(total);
  char *ptr = out.data();
  std::memcpy(ptr, &count, 4); ptr += 4;
  for (auto &e : all) {
    uint32_t ilen = static_cast<uint32_t>(e.id.size());
    std::memcpy(ptr, &ilen, 4); ptr += 4;
    std::memcpy(ptr, e.id.data(), ilen); ptr += ilen;
    uint32_t fc = static_cast<uint32_t>(e.fields.size());
    std::memcpy(ptr, &fc, 4); ptr += 4;
    for (auto &[f, v] : e.fields) {
      uint32_t fl = static_cast<uint32_t>(f.size());
      std::memcpy(ptr, &fl, 4); ptr += 4;
      std::memcpy(ptr, f.data(), fl); ptr += fl;
      uint32_t vl = static_cast<uint32_t>(v.size());
      std::memcpy(ptr, &vl, 4); ptr += 4;
      std::memcpy(ptr, v.data(), vl); ptr += vl;
    }
  }
  return out;
}

static bool deserialize_stream(const std::string &data, StreamData &s) {
  if (data.size() < 4) return false;
  const char *ptr = data.data();
  const char *end = data.data() + data.size();
  uint32_t count;
  std::memcpy(&count, ptr, 4); ptr += 4;
  for (uint32_t i = 0; i < count; ++i) {
    if (ptr + 4 > end) return false;
    uint32_t ilen;
    std::memcpy(&ilen, ptr, 4); ptr += 4;
    if (ptr + ilen + 4 > end) return false;
    std::string id(ptr, ilen); ptr += ilen;
    uint32_t fc;
    std::memcpy(&fc, ptr, 4); ptr += 4;
    std::vector<std::pair<std::string, std::string>> fields;
    for (uint32_t j = 0; j < fc; ++j) {
      if (ptr + 4 > end) return false;
      uint32_t fl;
      std::memcpy(&fl, ptr, 4); ptr += 4;
      if (ptr + fl + 4 > end) return false;
      std::string f(ptr, fl); ptr += fl;
      uint32_t vl;
      std::memcpy(&vl, ptr, 4); ptr += 4;
      if (ptr + vl > end) return false;
      std::string v(ptr, vl); ptr += vl;
      fields.emplace_back(std::move(f), std::move(v));
    }
    s.XAdd(id, fields);
  }
  return true;
}

// ── Engine ──

Engine::Engine(const Options &opts)
    : opts_(opts)
    , bytes_(opts.shards)
    , zsets_(opts.shards)
    , lists_(opts.shards)
    , hashes_(opts.shards)
    , sets_(opts.shards)
    , streams_(opts.shards)
    , jsons_(opts.shards)
    , vectors_(opts.shards)
    , meta_()
{
  if (!opts.path.empty()) {
    rocksdb_ = std::make_unique<RocksDBAdapter>(opts.path);
    if (!rocksdb_->IsOpen()) {
      last_error_ = "Failed to open RocksDB at " + opts.path;
      spdlog::error(last_error_);
    }
  }
}

Engine::~Engine() = default;

// ── Persistence helpers ──

void Engine::persist_zset(const std::string &key, const ZSetType &zs) {
  if (!rocksdb_) return;
  rocksdb_->Put(key, serialize_zset(zs));
}

void Engine::persist_list(const std::string &key, const StringList &lst) {
  if (!rocksdb_) return;
  rocksdb_->Put(key, serialize_list(lst));
}

void Engine::persist_hash(const std::string &key, const HashType &h) {
  if (!rocksdb_) return;
  rocksdb_->Put(key, serialize_hash(h));
}

void Engine::persist_set(const std::string &key, const SetType &s) {
  if (!rocksdb_) return;
  rocksdb_->Put(key, serialize_set(s));
}

void Engine::persist_vector(const std::string &key, const VectorType &v) {
  if (!rocksdb_) return;
  rocksdb_->Put(key, serialize_vector(v));
}

bool Engine::load_zset(const std::string &key, ZSetType &zs) {
  if (!rocksdb_) return false;
  auto v = rocksdb_->Get(key);
  if (!v) return false;
  return deserialize_zset(*v, zs);
}

bool Engine::load_list(const std::string &key, StringList &lst) {
  if (!rocksdb_) return false;
  auto v = rocksdb_->Get(key);
  if (!v) return false;
  return deserialize_list(*v, lst);
}

bool Engine::load_hash(const std::string &key, HashType &h) {
  if (!rocksdb_) return false;
  auto v = rocksdb_->Get(key);
  if (!v) return false;
  return deserialize_hash(*v, h);
}

bool Engine::load_set(const std::string &key, SetType &s) {
  if (!rocksdb_) return false;
  auto v = rocksdb_->Get(key);
  if (!v) return false;
  return deserialize_set(*v, s);
}

bool Engine::load_vector(const std::string &key, VectorType &v) {
  if (!rocksdb_) return false;
  auto data = rocksdb_->Get(key);
  if (!data) return false;
  return deserialize_vector(*data, v);
}

std::shared_ptr<Engine::ZSetType> Engine::get_or_create_zset(const std::string &key) {
  std::shared_ptr<ZSetType> zs;
  if (zsets_.Get(key, &zs)) return zs;
  zs = std::make_shared<ZSetType>();
  if (load_zset(key, *zs)) {
    zsets_.Insert(key, zs);
    meta_.RegisterKey(key, KeyType::kZSet);
    return zs;
  }
  return nullptr;
}

std::shared_ptr<StringList> Engine::get_or_create_list(const std::string &key) {
  std::shared_ptr<StringList> lst;
  if (lists_.Get(key, &lst)) return lst;
  lst = std::make_shared<StringList>();
  if (load_list(key, *lst)) {
    lists_.Insert(key, lst);
    meta_.RegisterKey(key, KeyType::kList);
    return lst;
  }
  return nullptr;
}

std::shared_ptr<Engine::HashType> Engine::get_or_create_hash(const std::string &key) {
  std::shared_ptr<HashType> h;
  if (hashes_.Get(key, &h)) return h;
  h = std::make_shared<HashType>();
  if (load_hash(key, *h)) {
    hashes_.Insert(key, h);
    meta_.RegisterKey(key, KeyType::kHash);
    return h;
  }
  return nullptr;
}

std::shared_ptr<Engine::SetType> Engine::get_or_create_set(const std::string &key) {
  std::shared_ptr<SetType> s;
  if (sets_.Get(key, &s)) return s;
  s = std::make_shared<SetType>();
  if (load_set(key, *s)) {
    sets_.Insert(key, s);
    meta_.RegisterKey(key, KeyType::kSet);
    return s;
  }
  return nullptr;
}

std::shared_ptr<Engine::VectorType> Engine::get_or_create_vector(
    const std::string &key, size_t dim) {
  std::shared_ptr<VectorType> v;
  if (vectors_.Get(key, &v)) return v;
  v = std::make_shared<VectorType>(dim);
  if (load_vector(key, *v)) {
    vectors_.Insert(key, v);
    meta_.RegisterKey(key, KeyType::kVector);
    return v;
  }
  return nullptr;
}

std::shared_ptr<Engine::StreamType> Engine::get_or_create_stream(const std::string &key) {
  std::shared_ptr<StreamType> s;
  if (streams_.Get(key, &s)) return s;
  s = std::make_shared<StreamType>();
  if (load_stream(key, *s)) {
    streams_.Insert(key, s);
    meta_.RegisterKey(key, KeyType::kStream);
    return s;
  }
  return nullptr;
}

void Engine::persist_stream(const std::string &key, const StreamType &s) {
  if (!rocksdb_) return;
  rocksdb_->Put(key, serialize_stream(s));
}

bool Engine::load_stream(const std::string &key, StreamType &s) {
  if (!rocksdb_) return false;
  auto data = rocksdb_->Get(key);
  if (!data) return false;
  return deserialize_stream(*data, s);
}

std::shared_ptr<Engine::JsonType> Engine::get_or_create_json(const std::string &key) {
  std::shared_ptr<JsonType> j;
  if (jsons_.Get(key, &j)) return j;
  j = std::make_shared<JsonType>();
  if (load_json(key, *j)) {
    jsons_.Insert(key, j);
    meta_.RegisterKey(key, KeyType::kJson);
    return j;
  }
  return nullptr;
}

void Engine::persist_json(const std::string &key, const JsonType &j) {
  if (!rocksdb_) return;
  rocksdb_->Put(key, j);
}

bool Engine::load_json(const std::string &key, JsonType &j) {
  if (!rocksdb_) return false;
  auto data = rocksdb_->Get(key);
  if (!data) return false;
  j = std::move(*data);
  return true;
}

// ── Pure KV ──

bool Engine::put(std::string_view key_, std::span<const uint8_t> value) {
  auto key   = make_key(key_);
  auto bytes = std::vector<uint8_t>(value.begin(), value.end());

  bytes_.Insert(key, bytes);
  meta_.RegisterKey(key, KeyType::kBytes);
  eviction_2q_.RecordAccess(key, true);
  put_count_++;

  if (rocksdb_) {
    rocksdb_->Put(key, std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
  }
  check_memory_and_evict();
  return true;
}

std::optional<std::vector<uint8_t>> Engine::get(std::string_view key_) {
  auto key = make_key(key_);
  get_count_++;

  if (!meta_.IsValid(key)) { misses_++; return std::nullopt; }

  std::vector<uint8_t> val;
  if (bytes_.Get(key, &val)) {
    record_access(key);
    hits_++;
    return val;
  }

  if (rocksdb_) {
    auto v = rocksdb_->Get(key);
    if (v) {
      std::vector<uint8_t> bytes(v->begin(), v->end());
      bytes_.Insert(key, bytes);
      record_access(key);
      hits_++;
      return bytes;
    }
  }
  misses_++;
  return std::nullopt;
}

bool Engine::del(std::string_view key_) {
  auto key = make_key(key_);
  bool removed = false;
  removed |= bytes_.Remove(key);
  if (auto z = zsets_.Get(key, nullptr)) removed |= zsets_.Remove(key);
  if (auto l = lists_.Get(key, nullptr)) removed |= lists_.Remove(key);
  if (auto h = hashes_.Get(key, nullptr)) removed |= hashes_.Remove(key);
  if (auto s = sets_.Get(key, nullptr)) removed |= sets_.Remove(key);
  if (auto st = streams_.Get(key, nullptr)) removed |= streams_.Remove(key);
  if (auto j = jsons_.Get(key, nullptr)) removed |= jsons_.Remove(key);
  if (auto v = vectors_.Get(key, nullptr)) removed |= vectors_.Remove(key);
  eviction_2q_.RemoveKey(key);
  meta_.UnregisterKey(key);
  if (rocksdb_) rocksdb_->Delete(key);
  return removed;
}

bool Engine::exists(std::string_view key_) {
  return meta_.IsValid(make_key(key_));
}

KeyType Engine::type(std::string_view key_) {
  auto t = meta_.GetKeyType(make_key(key_));
  return t.value_or(KeyType::kNone);
}

// ── TTL ──

bool Engine::expire(std::string_view key_, int64_t ms) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return false;
  meta_.SetExpireMs(key, ms);
  return true;
}

std::optional<int64_t> Engine::ttl(std::string_view key_) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return std::nullopt;
  return meta_.GetTtlMs(key);
}

// ── ZSet ──

bool Engine::zadd(std::string_view key_, std::string_view member, double score) {
  auto key = make_key(key_);
  std::shared_ptr<ZSetType> zs;
  if (!zsets_.Get(key, &zs)) {
    zs = std::make_shared<ZSetType>();
    zsets_.Insert(key, zs);
    meta_.RegisterKey(key, KeyType::kZSet);
  }
  zs->Add(std::string(member), score);  // returns true if new, false if updated — we don't care
  persist_zset(key, *zs);
  check_memory_and_evict();
  return true;
}

std::optional<double> Engine::zscore(std::string_view key_, std::string_view member) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return std::nullopt;
  auto zs = get_or_create_zset(key);
  if (!zs) return std::nullopt;
  return zs->GetScore(std::string(member));
}

bool Engine::zrem(std::string_view key_, std::string_view member) {
  auto key = make_key(key_);
  std::shared_ptr<Engine::ZSetType> zs;
  if (!zsets_.Get(key, &zs)) return false;
  bool ok = zs->Remove(std::string(member));
  persist_zset(key, *zs);
  return ok;
}

int64_t Engine::zcard(std::string_view key_) {
  auto key = make_key(key_);
  auto zs = get_or_create_zset(key);
  if (!zs) return 0;
  return static_cast<int64_t>(zs->Size());
}

std::vector<std::pair<std::string, double>> Engine::zrange(
    std::string_view key_, int64_t start, int64_t stop, bool reverse) {
  auto key = make_key(key_);
  auto zs = get_or_create_zset(key);
  if (!zs) return {};
  return zs->GetRangeByRank(static_cast<uint64_t>(start),
                             static_cast<uint64_t>(stop), reverse, true);
}

std::vector<std::pair<std::string, double>> Engine::zrangebyscore(
    std::string_view key_, double min, double max) {
  auto key = make_key(key_);
  auto zs = get_or_create_zset(key);
  if (!zs) return {};
  return zs->GetRangeByScore(min, max, true);
}

// ── List ──

bool Engine::lpush(std::string_view key_, std::span<const uint8_t> value) {
  auto key  = make_key(key_);
  std::shared_ptr<StringList> lst;
  if (!lists_.Get(key, &lst)) {
    lst = std::make_shared<StringList>();
    lists_.Insert(key, lst);
    meta_.RegisterKey(key, KeyType::kList);
  }
  lst->PushLeft(std::string(reinterpret_cast<const char *>(value.data()), value.size()));
  persist_list(key, *lst);

  check_memory_and_evict();
  return true;
}

bool Engine::rpush(std::string_view key_, std::span<const uint8_t> value) {
  auto key  = make_key(key_);
  std::shared_ptr<StringList> lst;
  if (!lists_.Get(key, &lst)) {
    lst = std::make_shared<StringList>();
    lists_.Insert(key, lst);
    meta_.RegisterKey(key, KeyType::kList);
  }
  lst->PushRight(std::string(reinterpret_cast<const char *>(value.data()), value.size()));
  persist_list(key, *lst);

  check_memory_and_evict();
  return true;
}

std::optional<std::vector<uint8_t>> Engine::lpop(std::string_view key_) {
  auto key = make_key(key_);
  auto lst = get_or_create_list(key);
  if (!lst) return std::nullopt;
  auto v = lst->PopLeft();
  if (!v) return std::nullopt;
  persist_list(key, *lst);
  return std::vector<uint8_t>(v->begin(), v->end());
}

std::optional<std::vector<uint8_t>> Engine::rpop(std::string_view key_) {
  auto key = make_key(key_);
  auto lst = get_or_create_list(key);
  if (!lst) return std::nullopt;
  auto v = lst->PopRight();
  if (!v) return std::nullopt;
  persist_list(key, *lst);
  return std::vector<uint8_t>(v->begin(), v->end());
}

int64_t Engine::llen(std::string_view key_) {
  auto key = make_key(key_);
  auto lst = get_or_create_list(key);
  if (!lst) return 0;
  return static_cast<int64_t>(lst->Size());
}

// ── Hash ──

bool Engine::hset(std::string_view key_, std::string_view field, std::string_view value) {
  auto key = make_key(key_);
  std::shared_ptr<HashType> h;
  if (!hashes_.Get(key, &h)) {
    h = std::make_shared<HashType>();
    hashes_.Insert(key, h);
    meta_.RegisterKey(key, KeyType::kHash);
  }
  h->Insert(std::string(field), std::string(value));
  persist_hash(key, *h);

  check_memory_and_evict();
  return true;
}

std::optional<std::string> Engine::hget(std::string_view key_, std::string_view field) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return std::nullopt;
  auto h = get_or_create_hash(key);
  if (!h) return std::nullopt;
  std::string val;
  if (!h->Get(std::string(field), &val)) return std::nullopt;
  return val;
}

bool Engine::hdel(std::string_view key_, std::string_view field) {
  auto key = make_key(key_);
  auto h = get_or_create_hash(key);
  if (!h) return false;
  bool ok = h->Remove(std::string(field));
  persist_hash(key, *h);
  return ok;
}

bool Engine::hexists(std::string_view key_, std::string_view field) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return false;
  auto h = get_or_create_hash(key);
  if (!h) return false;
  return h->Contains(std::string(field));
}

std::vector<std::pair<std::string, std::string>> Engine::hgetall(std::string_view key_) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return {};
  auto h = get_or_create_hash(key);
  if (!h) return {};
  return h->AllEntries();
}

int64_t Engine::hlen(std::string_view key_) {
  auto key = make_key(key_);
  auto h = get_or_create_hash(key);
  if (!h) return 0;
  return static_cast<int64_t>(h->Size());
}

std::optional<int64_t> Engine::hincrby(std::string_view key_, std::string_view field, int64_t delta) {
  auto key = make_key(key_);
  std::shared_ptr<HashType> h;
  if (!hashes_.Get(key, &h)) {
    h = std::make_shared<HashType>();
    if (load_hash(key, *h)) {
      hashes_.Insert(key, h);
      meta_.RegisterKey(key, KeyType::kHash);
    } else {
      hashes_.Insert(key, h);
      meta_.RegisterKey(key, KeyType::kHash);
    }
  }

  int64_t current = 0;
  std::string existing_val;
  if (h->Get(std::string(field), &existing_val)) {
    char *end = nullptr;
    current = std::strtoll(existing_val.c_str(), &end, 10);
    if (end == existing_val.c_str()) current = 0;
  }
  int64_t new_val = current + delta;
  h->Insert(std::string(field), std::to_string(new_val));
  persist_hash(key, *h);
  return new_val;
}

// ── Set ──

bool Engine::sadd(std::string_view key_, std::string_view member) {
  auto key = make_key(key_);
  std::shared_ptr<SetType> s;
  if (!sets_.Get(key, &s)) {
    s = std::make_shared<SetType>();
    sets_.Insert(key, s);
    meta_.RegisterKey(key, KeyType::kSet);
  }
  s->Insert(std::string(member), 0);  // Insert returns true if new — ignore
  persist_set(key, *s);
  check_memory_and_evict();
  return true;
}

bool Engine::srem(std::string_view key_, std::string_view member) {
  auto key = make_key(key_);
  auto s = get_or_create_set(key);
  if (!s) return false;
  bool ok = s->Remove(std::string(member));
  persist_set(key, *s);
  return ok;
}

bool Engine::sismember(std::string_view key_, std::string_view member) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return false;
  auto s = get_or_create_set(key);
  if (!s) return false;
  return s->Contains(std::string(member));
}

std::vector<std::string> Engine::smembers(std::string_view key_) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return {};
  auto s = get_or_create_set(key);
  if (!s) return {};
  return s->AllKeys();
}

int64_t Engine::scard(std::string_view key_) {
  auto key = make_key(key_);
  auto s = get_or_create_set(key);
  if (!s) return 0;
  return static_cast<int64_t>(s->Size());
}

// ── Stream ──

std::string Engine::xadd(std::string_view key_, const std::string &id,
                         const std::vector<std::pair<std::string, std::string>> &fields) {
  auto key = make_key(key_);
  std::shared_ptr<StreamType> s;
  if (!streams_.Get(key, &s)) {
    s = std::make_shared<StreamType>();
    streams_.Insert(key, s);
    meta_.RegisterKey(key, KeyType::kStream);
  }
  std::string result = s->XAdd(id, fields);
  if (!result.empty()) persist_stream(key, *s);

  check_memory_and_evict();
  return result;
}

std::vector<StreamEntry> Engine::xread(std::string_view key_, const std::string &start_id, int64_t count) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return {};
  auto s = get_or_create_stream(key);
  if (!s) return {};
  return s->XRead(start_id, count);
}

std::vector<StreamEntry> Engine::xrange(std::string_view key_, const std::string &start,
                                        const std::string &end, int64_t count) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return {};
  auto s = get_or_create_stream(key);
  if (!s) return {};
  return s->XRange(start, end, count);
}

int64_t Engine::xlen(std::string_view key_) {
  auto key = make_key(key_);
  auto s = get_or_create_stream(key);
  if (!s) return 0;
  return static_cast<int64_t>(s->XLen());
}

bool Engine::xdel(std::string_view key_, const std::string &id) {
  auto key = make_key(key_);
  auto s = get_or_create_stream(key);
  if (!s) return false;
  bool ok = s->XDel(id);
  if (ok) persist_stream(key, *s);
  return ok;
}

int64_t Engine::xtrim(std::string_view key_, int64_t maxlen) {
  auto key = make_key(key_);
  auto s = get_or_create_stream(key);
  if (!s) return 0;
  int64_t removed = static_cast<int64_t>(s->XTrim(maxlen));
  if (removed > 0) persist_stream(key, *s);
  return removed;
}

// ── JSON ──

bool Engine::jsonset(std::string_view key_, std::string_view path, const std::string &json_value) {
  // Validate JSON with simdjson before storing
  simdjson::dom::parser parser;
  if (parser.parse(json_value).error() != simdjson::SUCCESS) return false;

  auto key = make_key(key_);
  std::shared_ptr<JsonType> j;
  if (!jsons_.Get(key, &j)) {
    j = std::make_shared<JsonType>();
    jsons_.Insert(key, j);
    meta_.RegisterKey(key, KeyType::kJson);
  }
  *j = json_value;  // store raw JSON text
  persist_json(key, *j);
  return true;
}

std::optional<std::string> Engine::jsonget(std::string_view key_, std::string_view path) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return std::nullopt;
  auto j = get_or_create_json(key);
  if (!j) return std::nullopt;

  simdjson::dom::parser parser;
  auto doc = parser.parse(*j);
  if (doc.error()) return std::nullopt;

  if (path.empty() || path == "." || path == "$") {
    return simdjson::to_string(doc.value());
  }
  auto result = doc.value().at_pointer(std::string(path));
  if (result.error()) return std::nullopt;
  return simdjson::to_string(result.value());
}

bool Engine::jsondel(std::string_view key_, std::string_view path) {
  auto key = make_key(key_);
  // Only support root-level deletion for simplicity
  if (!path.empty() && path != "." && path != "$") return false;
  jsons_.Remove(key);
  meta_.UnregisterKey(key);
  if (rocksdb_) rocksdb_->Delete(key);
  return true;
}

// ── Vector ──

bool Engine::vecadd(std::string_view key_, const std::string &id,
                    std::span<const float> vec) {
  auto key = make_key(key_);
  std::shared_ptr<VectorType> v;
  if (!vectors_.Get(key, &v)) {
    v = std::make_shared<VectorType>(vec.size());
    vectors_.Insert(key, v);
    meta_.RegisterKey(key, KeyType::kVector);
  }
  bool added = v->Add(id, vec);
  if (added) persist_vector(key, *v);
  check_memory_and_evict();
  return added;
}

std::vector<float> Engine::vecget(std::string_view key_, const std::string &id) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return {};
  auto v = get_or_create_vector(key, 0);
  if (!v) return {};
  return v->Get(id);
}

bool Engine::vecdel(std::string_view key_, const std::string &id) {
  auto key = make_key(key_);
  auto v = get_or_create_vector(key, 0);
  if (!v) return false;
  bool ok = v->Remove(id);
  if (ok) persist_vector(key, *v);
  return ok;
}

std::vector<std::pair<std::string, float>> Engine::vecsearch(
    std::string_view key_, std::span<const float> query, size_t k) {
  auto key = make_key(key_);
  if (!meta_.IsValid(key)) return {};
  auto v = get_or_create_vector(key, query.size());
  if (!v) return {};
  return v->Search(query, k);
}

int64_t Engine::vecsize(std::string_view key_) {
  auto key = make_key(key_);
  auto v = get_or_create_vector(key, 0);
  if (!v) return 0;
  return static_cast<int64_t>(v->Size());
}

// ── Iteration ──

std::vector<std::pair<std::string, std::vector<uint8_t>>> Engine::all_entries() {
  return bytes_.AllEntries();
}

std::vector<std::string> Engine::all_keys() {
  return meta_.GetAllKeys();
}

// ── Stats ──

int64_t Engine::count() const {
  return static_cast<int64_t>(meta_.Size());
}

int64_t Engine::memory_usage() const {
  int64_t total = 0;
  for (auto &[k, v] : bytes_.AllEntries()) {
    total += static_cast<int64_t>(k.size() + v.size());
  }
  for (auto &k : zsets_.AllKeys()) {
    total += static_cast<int64_t>(k.size());
    std::shared_ptr<ZSetType> zs;
    if (zsets_.Get(k, &zs)) {
      total += static_cast<int64_t>(zs->MemoryUsage());
    }
  }
  for (auto &k : lists_.AllKeys()) {
    total += static_cast<int64_t>(k.size());
    std::shared_ptr<StringList> lst;
    if (lists_.Get(k, &lst)) {
      total += static_cast<int64_t>(lst->MemoryUsage());
    }
  }
  for (auto &k : hashes_.AllKeys()) {
    total += static_cast<int64_t>(k.size());
    std::shared_ptr<HashType> h;
    if (hashes_.Get(k, &h)) {
      total += static_cast<int64_t>(h->Size() * 64);
    }
  }
  for (auto &k : sets_.AllKeys()) {
    total += static_cast<int64_t>(k.size());
    std::shared_ptr<SetType> s;
    if (sets_.Get(k, &s)) {
      total += static_cast<int64_t>(s->Size() * 40);
    }
  }
  for (auto &k : streams_.AllKeys()) {
    total += static_cast<int64_t>(k.size());
    std::shared_ptr<StreamType> s;
    if (streams_.Get(k, &s)) {
      total += static_cast<int64_t>(s->MemoryUsage());
    }
  }
  for (auto &k : jsons_.AllKeys()) {
    total += static_cast<int64_t>(k.size());
    std::shared_ptr<JsonType> j;
    if (jsons_.Get(k, &j)) {
      total += static_cast<int64_t>(j->size() + 64);
    }
  }
  for (auto &k : vectors_.AllKeys()) {
    total += static_cast<int64_t>(k.size());
    std::shared_ptr<VectorType> v;
    if (vectors_.Get(k, &v)) {
      total += static_cast<int64_t>(v->MemoryUsage());
    }
  }
  total += static_cast<int64_t>(meta_.Size() * 64);
  return total;
}

Engine::Stats Engine::stats() const {
  return {put_count_.load(), get_count_.load(), hits_.load(), misses_.load(),
          evicted_.load(), expired_.load(), count(), memory_usage()};
}

// ── Maintenance ──

void Engine::record_access(const std::string &key) {
  meta_.UpdateAccessInfo(key);
  if (opts_.eviction == EvictionPolicy::k2Q) {
    eviction_2q_.RecordAccess(key, false);
  }
}

int64_t Engine::CleanupExpired() {
  auto keys = meta_.GetAllKeys();
  int64_t cleaned = 0;

  for (const auto &key : keys) {
    if (!meta_.IsValid(key)) {
      bytes_.Remove(key);
      zsets_.Remove(key);
      lists_.Remove(key);
      hashes_.Remove(key);
      sets_.Remove(key);
      streams_.Remove(key);
      jsons_.Remove(key);
      vectors_.Remove(key);
      eviction_2q_.RemoveKey(key);

      if (rocksdb_) rocksdb_->Delete(key);
      cleaned++;
    }
  }
  expired_ += cleaned;
  return cleaned;
}

static std::optional<std::string> select_victim_lru(const std::vector<std::string> &keys,
                                                     KeyMetadataManager &meta) {
  const std::string *best = nullptr;
  uint32_t best_time = UINT32_MAX;
  for (const auto &k : keys) {
    uint32_t t = meta.GetAccessTime(k);
    if (t < best_time) { best_time = t; best = &k; }
  }
  return best ? std::optional<std::string>(*best) : std::nullopt;
}

static std::optional<std::string> select_victim_lfu(const std::vector<std::string> &keys,
                                                     KeyMetadataManager &meta) {
  const std::string *best = nullptr;
  uint8_t best_cnt = UINT8_MAX;
  for (const auto &k : keys) {
    uint8_t c = meta.GetLFUCounter(k);
    if (c < best_cnt) { best_cnt = c; best = &k; }
  }
  return best ? std::optional<std::string>(*best) : std::nullopt;
}

static std::optional<std::string> select_victim_random(const std::vector<std::string> &keys) {
  if (keys.empty()) return std::nullopt;
  return keys[std::rand() % keys.size()];
}

static std::optional<std::string> select_victim_ttl(const std::vector<std::string> &keys,
                                                     KeyMetadataManager &meta) {
  const std::string *best = nullptr;
  int64_t best_ttl = INT64_MAX;
  for (const auto &k : keys) {
    int64_t ttl = meta.GetTtlMs(k);
    if (ttl > 0 && ttl < best_ttl) { best_ttl = ttl; best = &k; }
  }
  return best ? std::optional<std::string>(*best) : std::nullopt;
}

void Engine::check_memory_and_evict() {
  CleanupExpired();
  if (opts_.max_mb == 0 || opts_.eviction == EvictionPolicy::kNoEviction) return;

  int64_t limit = static_cast<int64_t>(opts_.max_mb) * 1024 * 1024;
  // Only check when memory >= 80% of limit (avoid overhead on every write)
  int64_t current = memory_usage();
  if (current < limit * 8 / 10) return;

  auto keys = meta_.GetAllKeys();
  if (keys.empty()) return;

  // Filter by policy scope (volatile vs allkeys)
  std::vector<std::string> candidates;
  bool volatile_only = opts_.eviction == EvictionPolicy::kVolatileLRU ||
                       opts_.eviction == EvictionPolicy::kVolatileLFU ||
                       opts_.eviction == EvictionPolicy::kVolatileRandom ||
                       opts_.eviction == EvictionPolicy::kVolatileTTL;
  for (const auto &k : keys) {
    if (volatile_only && meta_.GetTtlMs(k) <= 0) continue;
    candidates.push_back(k);
  }
  if (candidates.empty()) return;

  size_t max_evict = std::max(size_t(1), candidates.size() / 10);
  size_t evicted = 0;

  for (size_t i = 0; i < max_evict && memory_usage() > limit; ++i) {
    std::optional<std::string> victim;
    if (opts_.eviction == EvictionPolicy::k2Q) {
      victim = eviction_2q_.GetVictim();
    } else if (opts_.eviction == EvictionPolicy::kAllKeysLRU ||
               opts_.eviction == EvictionPolicy::kVolatileLRU) {
      victim = select_victim_lru(candidates, meta_);
    } else if (opts_.eviction == EvictionPolicy::kAllKeysLFU ||
               opts_.eviction == EvictionPolicy::kVolatileLFU) {
      victim = select_victim_lfu(candidates, meta_);
    } else if (opts_.eviction == EvictionPolicy::kAllKeysRandom ||
               opts_.eviction == EvictionPolicy::kVolatileRandom) {
      victim = select_victim_random(candidates);
    } else if (opts_.eviction == EvictionPolicy::kVolatileTTL) {
      victim = select_victim_ttl(candidates, meta_);
    }
    if (!victim) break;

    auto type_opt = meta_.GetKeyType(*victim);
    if (!type_opt || *type_opt == KeyType::kNone) continue;

    KeyType kt = *type_opt;
    switch (kt) {
      case KeyType::kBytes: {
        std::vector<uint8_t> val;
        if (rocksdb_ && bytes_.Get(*victim, &val)) {
          rocksdb_->Put(*victim, std::string(reinterpret_cast<const char *>(val.data()), val.size()));
        }
        bytes_.Remove(*victim);
        break;
      }
      case KeyType::kZSet: {
        std::shared_ptr<ZSetType> zs;
        if (zsets_.Get(*victim, &zs)) { if (rocksdb_) persist_zset(*victim, *zs); zsets_.Remove(*victim); }
        break;
      }
      case KeyType::kList: {
        std::shared_ptr<StringList> lst;
        if (lists_.Get(*victim, &lst)) { if (rocksdb_) persist_list(*victim, *lst); lists_.Remove(*victim); }
        break;
      }
      case KeyType::kHash: {
        std::shared_ptr<HashType> h;
        if (hashes_.Get(*victim, &h)) { if (rocksdb_) persist_hash(*victim, *h); hashes_.Remove(*victim); }
        break;
      }
      case KeyType::kSet: {
        std::shared_ptr<SetType> s;
        if (sets_.Get(*victim, &s)) { if (rocksdb_) persist_set(*victim, *s); sets_.Remove(*victim); }
        break;
      }
      case KeyType::kStream: {
        std::shared_ptr<StreamType> s;
        if (streams_.Get(*victim, &s)) { if (rocksdb_) persist_stream(*victim, *s); streams_.Remove(*victim); }
        break;
      }
      case KeyType::kJson: {
        std::shared_ptr<JsonType> j;
        if (jsons_.Get(*victim, &j)) { if (rocksdb_) persist_json(*victim, *j); jsons_.Remove(*victim); }
        break;
      }
      case KeyType::kVector: {
        std::shared_ptr<VectorType> v;
        if (vectors_.Get(*victim, &v)) { if (rocksdb_) persist_vector(*victim, *v); vectors_.Remove(*victim); }
        break;
      }
      default: break;
    }
    eviction_2q_.RemoveKey(*victim);
    meta_.UnregisterKey(*victim);
    evicted++;
  }
  evicted_ += evicted;
}

}  // namespace astrakv
