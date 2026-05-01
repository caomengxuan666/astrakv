// AstrakV - C API implementation (wraps C++ Engine)
#include "astrakv/kv.h"
#include "engine.hpp"

#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

using astrakv::Engine;

// ── Internal helpers ──

static Engine *to_engine(astrakv_t *kv) {
  return reinterpret_cast<Engine *>(kv);
}

static std::string_view sv(const uint8_t *data, size_t len) {
  return std::string_view(reinterpret_cast<const char *>(data), len);
}

// ── Lifecycle ──

astrakv_t *astrakv_open(const astrakv_options_t *opts) {
  Engine::Options o;
  if (opts) {
    if (opts->path) o.path   = opts->path;
    if (opts->shards) o.shards = opts->shards;
    o.max_mb = opts->max_memory_mb;
  }
  try {
    auto *eng = new Engine(o);
    return reinterpret_cast<astrakv_t *>(eng);
  } catch (...) {
    return nullptr;
  }
}

void astrakv_close(astrakv_t *kv) {
  delete to_engine(kv);
}

const char *astrakv_last_error(astrakv_t *kv) {
  // Engine::last_error returns a std::string reference; we need a static buffer
  thread_local static std::string err;
  err = to_engine(kv)->last_error();
  return err.c_str();
}

// ── Pure KV ──

int astrakv_put(astrakv_t *kv, const uint8_t *key, size_t klen,
                                const uint8_t *val, size_t vlen) {
  return to_engine(kv)->put(sv(key, klen), {val, vlen}) ? 0 : -1;
}

int astrakv_get(astrakv_t *kv, const uint8_t *key, size_t klen,
                                uint8_t **val, size_t *vlen) {
  auto v = to_engine(kv)->get(sv(key, klen));
  if (!v) return -1;

  *vlen = v->size();
  *val  = static_cast<uint8_t *>(malloc(v->size()));
  if (!*val) return -1;
  std::memcpy(*val, v->data(), v->size());
  return 0;
}

int astrakv_del(astrakv_t *kv, const uint8_t *key, size_t klen) {
  return to_engine(kv)->del(sv(key, klen)) ? 0 : -1;
}

int astrakv_exists(astrakv_t *kv, const uint8_t *key, size_t klen) {
  return to_engine(kv)->exists(sv(key, klen)) ? 0 : -1;
}

int astrakv_type(astrakv_t *kv, const uint8_t *key, size_t klen,
                                 astrakv_value_type_t *type) {
  auto t = to_engine(kv)->type(sv(key, klen));
  switch (t) {
    case astrakv::KeyType::kNone:  *type = ASTRAKV_TYPE_NONE; break;
    case astrakv::KeyType::kBytes: *type = ASTRAKV_TYPE_BYTES; break;
    case astrakv::KeyType::kZSet:  *type = ASTRAKV_TYPE_ZSET; break;
    case astrakv::KeyType::kList:  *type = ASTRAKV_TYPE_LIST; break;
    case astrakv::KeyType::kHash:  *type = ASTRAKV_TYPE_HASH; break;
    case astrakv::KeyType::kSet:   *type = ASTRAKV_TYPE_SET;  break;
    case astrakv::KeyType::kStream: *type = ASTRAKV_TYPE_STREAM; break;
    case astrakv::KeyType::kJson:   *type = ASTRAKV_TYPE_JSON; break;
    case astrakv::KeyType::kVector: *type = ASTRAKV_TYPE_VECTOR; break;
    default: *type = ASTRAKV_TYPE_NONE; break;
  }
  return 0;
}

// ── TTL ──

int astrakv_expire(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t ms) {
  return to_engine(kv)->expire(sv(key, klen), ms) ? 0 : -1;
}

int astrakv_ttl(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *ms) {
  auto v = to_engine(kv)->ttl(sv(key, klen));
  if (!v) return -1;
  *ms = *v;
  return 0;
}

// ── ZSet ──

int astrakv_zadd(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *member, size_t mlen,
                                  double score) {
  return to_engine(kv)->zadd(sv(key, klen), sv(member, mlen), score) ? 0 : -1;
}

int astrakv_zscore(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    const uint8_t *member, size_t mlen,
                                    double *score) {
  auto v = to_engine(kv)->zscore(sv(key, klen), sv(member, mlen));
  if (!v) return -1;
  *score = *v;
  return 0;
}

int astrakv_zrem(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *member, size_t mlen) {
  return to_engine(kv)->zrem(sv(key, klen), sv(member, mlen)) ? 0 : -1;
}

int astrakv_zcard(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *count) {
  *count = to_engine(kv)->zcard(sv(key, klen));
  return 0;
}

// Packed format: [4-byte member_len LE][member bytes][8-byte score LE] repeated
int astrakv_zrange(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    int64_t start, int64_t stop, int reverse,
                                    uint8_t **out, size_t *out_len) {
  auto entries = to_engine(kv)->zrange(sv(key, klen), start, stop, reverse != 0);
  if (entries.empty()) {
    *out     = nullptr;
    *out_len = 0;
    return 0;
  }

  // Calculate total size
  size_t total = 0;
  for (auto &[mem, score] : entries) {
    total += 4 + mem.size() + 8;  // len(4) + member_bytes + score(8)
  }

  *out     = static_cast<uint8_t *>(malloc(total));
  *out_len = total;
  if (!*out) return -1;

  uint8_t *ptr = *out;
  for (auto &[mem, score] : entries) {
    uint32_t mlen = static_cast<uint32_t>(mem.size());
    std::memcpy(ptr, &mlen, 4);          ptr += 4;
    std::memcpy(ptr, mem.data(), mlen);  ptr += mlen;
    std::memcpy(ptr, &score, 8);         ptr += 8;
  }
  return 0;
}

int astrakv_zrangebyscore(astrakv_t *kv, const uint8_t *key, size_t klen,
                                           double min, double max,
                                           uint8_t **out, size_t *out_len) {
  auto entries = to_engine(kv)->zrangebyscore(sv(key, klen), min, max);
  if (entries.empty()) {
    *out     = nullptr;
    *out_len = 0;
    return 0;
  }

  size_t total = 0;
  for (auto &[mem, score] : entries) {
    total += 4 + mem.size() + 8;
  }

  *out     = static_cast<uint8_t *>(malloc(total));
  *out_len = total;
  if (!*out) return -1;

  uint8_t *ptr = *out;
  for (auto &[mem, score] : entries) {
    uint32_t mlen = static_cast<uint32_t>(mem.size());
    std::memcpy(ptr, &mlen, 4);          ptr += 4;
    std::memcpy(ptr, mem.data(), mlen);  ptr += mlen;
    std::memcpy(ptr, &score, 8);         ptr += 8;
  }
  return 0;
}

// ── List ──

int astrakv_lpush(astrakv_t *kv, const uint8_t *key, size_t klen,
                                   const uint8_t *val, size_t vlen) {
  return to_engine(kv)->lpush(sv(key, klen), {val, vlen}) ? 0 : -1;
}

int astrakv_rpush(astrakv_t *kv, const uint8_t *key, size_t klen,
                                   const uint8_t *val, size_t vlen) {
  return to_engine(kv)->rpush(sv(key, klen), {val, vlen}) ? 0 : -1;
}

int astrakv_lpop(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  uint8_t **val, size_t *vlen) {
  auto v = to_engine(kv)->lpop(sv(key, klen));
  if (!v) return -1;
  *vlen = v->size();
  *val  = static_cast<uint8_t *>(malloc(v->size()));
  if (!*val) return -1;
  std::memcpy(*val, v->data(), v->size());
  return 0;
}

int astrakv_rpop(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  uint8_t **val, size_t *vlen) {
  auto v = to_engine(kv)->rpop(sv(key, klen));
  if (!v) return -1;
  *vlen = v->size();
  *val  = static_cast<uint8_t *>(malloc(v->size()));
  if (!*val) return -1;
  std::memcpy(*val, v->data(), v->size());
  return 0;
}

int astrakv_llen(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *len) {
  *len = to_engine(kv)->llen(sv(key, klen));
  return 0;
}

// ── Hash ──

int astrakv_hset(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *field, size_t flen,
                                  const uint8_t *val, size_t vlen) {
  return to_engine(kv)->hset(sv(key, klen), sv(field, flen), sv(val, vlen)) ? 0 : -1;
}

int astrakv_hget(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *field, size_t flen,
                                  uint8_t **val, size_t *vlen) {
  auto v = to_engine(kv)->hget(sv(key, klen), sv(field, flen));
  if (!v) return -1;
  *vlen = v->size();
  *val  = static_cast<uint8_t *>(malloc(v->size()));
  if (!*val) return -1;
  std::memcpy(*val, v->data(), v->size());
  return 0;
}

int astrakv_hdel(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *field, size_t flen) {
  return to_engine(kv)->hdel(sv(key, klen), sv(field, flen)) ? 0 : -1;
}

int astrakv_hexists(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     const uint8_t *field, size_t flen) {
  return to_engine(kv)->hexists(sv(key, klen), sv(field, flen)) ? 0 : -1;
}

int astrakv_hgetall(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     uint8_t **out, size_t *out_len) {
  auto *eng = to_engine(kv);
  std::string_view k = sv(key, klen);

  // Single pass: append to std::string buffer
  std::string buf;
  buf.reserve(1024);  // typical hash size hint
  eng->forEachHash(k, [&](const std::string &field, const std::string &value) {
    uint32_t flen = static_cast<uint32_t>(field.size());
    uint32_t vlen = static_cast<uint32_t>(value.size());
    buf.append(reinterpret_cast<const char *>(&flen), 4);
    buf.append(field);
    buf.append(reinterpret_cast<const char *>(&vlen), 4);
    buf.append(value);
  });
  if (buf.empty()) { *out = nullptr; *out_len = 0; return 0; }

  *out_len = buf.size();
  *out     = static_cast<uint8_t *>(malloc(*out_len));
  if (!*out) return -1;
  std::memcpy(*out, buf.data(), *out_len);
  return 0;
}

int astrakv_hlen(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *len) {
  *len = to_engine(kv)->hlen(sv(key, klen));
  return 0;
}

int astrakv_hincrby(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     const uint8_t *field, size_t flen,
                                     int64_t delta, int64_t *result) {
  auto v = to_engine(kv)->hincrby(sv(key, klen), sv(field, flen), delta);
  if (!v) return -1;
  *result = *v;
  return 0;
}

int astrakv_hscan(astrakv_t *kv, const uint8_t *key, size_t klen,
                  astrakv_hash_cb cb, void *userdata) {
  if (!cb) return -1;
  to_engine(kv)->forEachHash(sv(key, klen), [cb, userdata](const std::string &field, const std::string &value) {
    cb(reinterpret_cast<const uint8_t *>(field.data()), field.size(),
       reinterpret_cast<const uint8_t *>(value.data()), value.size(), userdata);
  });
  return 0;
}

// ── Set ──

int astrakv_sadd(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *member, size_t mlen) {
  return to_engine(kv)->sadd(sv(key, klen), sv(member, mlen)) ? 0 : -1;
}

int astrakv_srem(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *member, size_t mlen) {
  return to_engine(kv)->srem(sv(key, klen), sv(member, mlen)) ? 0 : -1;
}

int astrakv_sismember(astrakv_t *kv, const uint8_t *key, size_t klen,
                                       const uint8_t *member, size_t mlen) {
  return to_engine(kv)->sismember(sv(key, klen), sv(member, mlen)) ? 0 : -1;
}

int astrakv_smembers(astrakv_t *kv, const uint8_t *key, size_t klen,
                                      uint8_t **out, size_t *out_len) {
  auto *eng = to_engine(kv);
  std::string_view k = sv(key, klen);

  std::string buf;
  buf.reserve(1024);
  eng->forEachSet(k, [&](const std::string &m) {
    uint32_t mlen = static_cast<uint32_t>(m.size());
    buf.append(reinterpret_cast<const char *>(&mlen), 4);
    buf.append(m);
  });
  if (buf.empty()) { *out = nullptr; *out_len = 0; return 0; }

  *out_len = buf.size();
  *out     = static_cast<uint8_t *>(malloc(*out_len));
  if (!*out) return -1;
  std::memcpy(*out, buf.data(), *out_len);
  return 0;
}

int astrakv_scard(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *count) {
  *count = to_engine(kv)->scard(sv(key, klen));
  return 0;
}

int astrakv_sscan(astrakv_t *kv, const uint8_t *key, size_t klen,
                  astrakv_set_cb cb, void *userdata) {
  if (!cb) return -1;
  to_engine(kv)->forEachSet(sv(key, klen), [cb, userdata](const std::string &member) {
    cb(reinterpret_cast<const uint8_t *>(member.data()), member.size(), userdata);
  });
  return 0;
}

// ── Stream ──

int astrakv_xadd(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *id, size_t idlen,
                                  const uint8_t *fields, size_t fields_len,
                                  char **id_out) {
  std::string id_str(reinterpret_cast<const char *>(id), idlen);

  std::vector<std::pair<std::string, std::string>> fvec;
  const uint8_t *ptr = fields;
  const uint8_t *end = fields + fields_len;
  while (ptr + 4 <= end) {
    uint32_t flen;
    std::memcpy(&flen, ptr, 4); ptr += 4;
    if (ptr + flen > end) break;
    std::string field(reinterpret_cast<const char *>(ptr), flen); ptr += flen;
    if (ptr + 4 > end) break;
    uint32_t vlen;
    std::memcpy(&vlen, ptr, 4); ptr += 4;
    if (ptr + vlen > end) break;
    std::string value(reinterpret_cast<const char *>(ptr), vlen); ptr += vlen;
    fvec.emplace_back(std::move(field), std::move(value));
  }

  std::string result_id = to_engine(kv)->xadd(sv(key, klen), id_str, fvec);
  if (result_id.empty()) return -1;

  *id_out = static_cast<char *>(malloc(result_id.size() + 1));
  if (!*id_out) return -1;
  std::memcpy(*id_out, result_id.data(), result_id.size());
  (*id_out)[result_id.size()] = '\0';
  return 0;
}

int astrakv_xread(astrakv_t *kv, const uint8_t *key, size_t klen,
                                   const uint8_t *start_id, size_t start_id_len,
                                   int64_t count,
                                   uint8_t **out, size_t *out_len) {
  std::string sid(reinterpret_cast<const char *>(start_id), start_id_len);
  auto entries = to_engine(kv)->xread(sv(key, klen), sid, count);
  if (entries.empty()) {
    *out     = nullptr;
    *out_len = 0;
    return 0;
  }

  size_t total = 4;
  for (auto &e : entries) {
    total += 4 + e.id.size() + 4;
    for (auto &[f, v] : e.fields) {
      total += 4 + f.size() + 4 + v.size();
    }
  }

  *out     = static_cast<uint8_t *>(malloc(total));
  *out_len = total;
  if (!*out) return -1;

  uint8_t *wptr = *out;
  uint32_t ecount = static_cast<uint32_t>(entries.size());
  std::memcpy(wptr, &ecount, 4); wptr += 4;
  for (auto &e : entries) {
    uint32_t ilen = static_cast<uint32_t>(e.id.size());
    std::memcpy(wptr, &ilen, 4); wptr += 4;
    std::memcpy(wptr, e.id.data(), ilen); wptr += ilen;
    uint32_t fcount = static_cast<uint32_t>(e.fields.size());
    std::memcpy(wptr, &fcount, 4); wptr += 4;
    for (auto &[f, v] : e.fields) {
      uint32_t flen = static_cast<uint32_t>(f.size());
      std::memcpy(wptr, &flen, 4); wptr += 4;
      std::memcpy(wptr, f.data(), flen); wptr += flen;
      uint32_t vlen = static_cast<uint32_t>(v.size());
      std::memcpy(wptr, &vlen, 4); wptr += 4;
      std::memcpy(wptr, v.data(), vlen); wptr += vlen;
    }
  }
  return 0;
}

int astrakv_xrange(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    const uint8_t *start_id, size_t start_id_len,
                                    const uint8_t *end_id, size_t end_id_len,
                                    int64_t count,
                                    uint8_t **out, size_t *out_len) {
  std::string sid(reinterpret_cast<const char *>(start_id), start_id_len);
  std::string eid(reinterpret_cast<const char *>(end_id), end_id_len);
  auto entries = to_engine(kv)->xrange(sv(key, klen), sid, eid, count);
  if (entries.empty()) {
    *out     = nullptr;
    *out_len = 0;
    return 0;
  }

  size_t total = 4;
  for (auto &e : entries) {
    total += 4 + e.id.size() + 4;
    for (auto &[f, v] : e.fields) {
      total += 4 + f.size() + 4 + v.size();
    }
  }

  *out     = static_cast<uint8_t *>(malloc(total));
  *out_len = total;
  if (!*out) return -1;

  uint8_t *wptr = *out;
  uint32_t ecount = static_cast<uint32_t>(entries.size());
  std::memcpy(wptr, &ecount, 4); wptr += 4;
  for (auto &e : entries) {
    uint32_t ilen = static_cast<uint32_t>(e.id.size());
    std::memcpy(wptr, &ilen, 4); wptr += 4;
    std::memcpy(wptr, e.id.data(), ilen); wptr += ilen;
    uint32_t fcount = static_cast<uint32_t>(e.fields.size());
    std::memcpy(wptr, &fcount, 4); wptr += 4;
    for (auto &[f, v] : e.fields) {
      uint32_t flen = static_cast<uint32_t>(f.size());
      std::memcpy(wptr, &flen, 4); wptr += 4;
      std::memcpy(wptr, f.data(), flen); wptr += flen;
      uint32_t vlen = static_cast<uint32_t>(v.size());
      std::memcpy(wptr, &vlen, 4); wptr += 4;
      std::memcpy(wptr, v.data(), vlen); wptr += vlen;
    }
  }
  return 0;
}

int astrakv_xlen(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *len) {
  *len = to_engine(kv)->xlen(sv(key, klen));
  return 0;
}

int astrakv_xdel(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *id, size_t idlen) {
  std::string sid(reinterpret_cast<const char *>(id), idlen);
  return to_engine(kv)->xdel(sv(key, klen), sid) ? 0 : -1;
}

int astrakv_xtrim(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t maxlen, int64_t *removed) {
  *removed = to_engine(kv)->xtrim(sv(key, klen), maxlen);
  return 0;
}

// ── JSON ──

int astrakv_jsonset(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     const uint8_t *path, size_t plen,
                                     const uint8_t *json_val, size_t jvlen) {
  std::string_view path_sv(reinterpret_cast<const char *>(path), plen);
  std::string jval(reinterpret_cast<const char *>(json_val), jvlen);
  return to_engine(kv)->jsonset(sv(key, klen), path_sv, jval) ? 0 : -1;
}

int astrakv_jsonget(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     const uint8_t *path, size_t plen,
                                     uint8_t **json_out, size_t *jout_len) {
  std::string_view path_sv(reinterpret_cast<const char *>(path), plen);
  auto v = to_engine(kv)->jsonget(sv(key, klen), path_sv);
  if (!v) return -1;
  *jout_len = v->size();
  *json_out = static_cast<uint8_t *>(malloc(v->size() + 1));
  if (!*json_out) return -1;
  std::memcpy(*json_out, v->data(), v->size());
  (*json_out)[v->size()] = '\0';
  return 0;
}

int astrakv_jsondel(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     const uint8_t *path, size_t plen) {
  std::string_view path_sv(reinterpret_cast<const char *>(path), plen);
  return to_engine(kv)->jsondel(sv(key, klen), path_sv) ? 0 : -1;
}

// ── Vector ──

int astrakv_vecadd(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    const uint8_t *id, size_t idlen,
                                    const float *vec, size_t veclen) {
  std::string id_str(reinterpret_cast<const char *>(id), idlen);
  return to_engine(kv)->vecadd(sv(key, klen), id_str, {vec, veclen}) ? 0 : -1;
}

int astrakv_vecget(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    const uint8_t *id, size_t idlen,
                                    float **vec_out, size_t *veclen_out) {
  std::string id_str(reinterpret_cast<const char *>(id), idlen);
  auto v = to_engine(kv)->vecget(sv(key, klen), id_str);
  if (v.empty()) return -1;
  *veclen_out = v.size();
  *vec_out = static_cast<float *>(malloc(v.size() * sizeof(float)));
  if (!*vec_out) return -1;
  std::memcpy(*vec_out, v.data(), v.size() * sizeof(float));
  return 0;
}

int astrakv_vecdel(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    const uint8_t *id, size_t idlen) {
  std::string id_str(reinterpret_cast<const char *>(id), idlen);
  return to_engine(kv)->vecdel(sv(key, klen), id_str) ? 0 : -1;
}

int astrakv_vecsearch(astrakv_t *kv, const uint8_t *key, size_t klen,
                                       const float *query, size_t querylen, size_t k,
                                       uint8_t **out, size_t *out_len) {
  auto results = to_engine(kv)->vecsearch(sv(key, klen), {query, querylen}, k);
  if (results.empty()) {
    *out = nullptr;
    *out_len = 0;
    return 0;
  }

  size_t total = 4; // result_count
  for (auto &[id, distance] : results) {
    total += 4 + id.size() + 4;
  }

  *out = static_cast<uint8_t *>(malloc(total));
  *out_len = total;
  if (!*out) return -1;

  uint8_t *ptr = *out;
  uint32_t rcount = static_cast<uint32_t>(results.size());
  std::memcpy(ptr, &rcount, 4); ptr += 4;
  for (auto &[id, distance] : results) {
    uint32_t ilen = static_cast<uint32_t>(id.size());
    std::memcpy(ptr, &ilen, 4); ptr += 4;
    std::memcpy(ptr, id.data(), ilen); ptr += ilen;
    std::memcpy(ptr, &distance, 4); ptr += 4;
  }
  return 0;
}

int astrakv_vecsize(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *size) {
  *size = to_engine(kv)->vecsize(sv(key, klen));
  return 0;
}

// ── Iterator ──

struct astrakv_iter_t {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
  size_t pos = 0;
};

astrakv_iter_t *astrakv_iter_create(astrakv_t *kv) {
  auto *it  = new astrakv_iter_t;
  it->entries = to_engine(kv)->all_entries();
  it->pos     = 0;
  return it;
}

int astrakv_iter_next(astrakv_iter_t *it, uint8_t **key, size_t *klen,
                                           uint8_t **val, size_t *vlen) {
  if (it->pos >= it->entries.size()) return -1;
  auto &[k, v] = it->entries[it->pos++];

  *klen = k.size();
  *key  = static_cast<uint8_t *>(malloc(k.size()));
  std::memcpy(*key, k.data(), k.size());

  *vlen = v.size();
  *val  = static_cast<uint8_t *>(malloc(v.size()));
  std::memcpy(*val, v.data(), v.size());
  return 0;
}

void astrakv_iter_destroy(astrakv_iter_t *it) {
  delete it;
}

// ── Stats ──

int64_t astrakv_count(astrakv_t *kv) {
  return to_engine(kv)->count();
}

int64_t astrakv_memory_usage(astrakv_t *kv) {
  return to_engine(kv)->memory_usage();
}

int astrakv_stats(astrakv_t *kv, astrakv_stats_t *out) {
  auto s = to_engine(kv)->stats();
  out->put_count    = s.put_count;
  out->get_count    = s.get_count;
  out->hits         = s.hits;
  out->misses       = s.misses;
  out->evicted      = s.evicted;
  out->expired      = s.expired;
  out->key_count    = s.key_count;
  out->memory_bytes = s.memory_bytes;
  return 0;
}

int astrakv_set_eviction(astrakv_t *kv, astrakv_eviction_policy_t policy) {
  to_engine(kv)->set_eviction(static_cast<astrakv::EvictionPolicy>(policy));
  return 0;
}

int astrakv_get_eviction(astrakv_t *kv, astrakv_eviction_policy_t *policy) {
  *policy = static_cast<astrakv_eviction_policy_t>(to_engine(kv)->get_eviction());
  return 0;
}

// ── Maintenance ──

int64_t astrakv_cleanup_expired(astrakv_t *kv) {
  return to_engine(kv)->CleanupExpired();
}
