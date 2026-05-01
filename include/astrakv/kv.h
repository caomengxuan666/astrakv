// AstrakV - C API
// Thread-safe embedded key-value store
#ifndef ASTRAKV_KV_H
#define ASTRAKV_KV_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// === Opaque types ===
typedef struct astrakv_t      astrakv_t;
typedef struct astrakv_iter_t astrakv_iter_t;

// === Value types ===
typedef enum {
  ASTRAKV_TYPE_NONE   = 0,
  ASTRAKV_TYPE_BYTES  = 1,
  ASTRAKV_TYPE_ZSET   = 2,
  ASTRAKV_TYPE_LIST   = 3,
  ASTRAKV_TYPE_HASH   = 4,
  ASTRAKV_TYPE_SET    = 5,
  ASTRAKV_TYPE_STREAM = 6,
  ASTRAKV_TYPE_JSON   = 7,
  ASTRAKV_TYPE_VECTOR = 8,
} astrakv_value_type_t;

// === Options ===
typedef struct {
  const char *path;               // data directory (NULL = in-memory only)
  size_t      shards;             // concurrency shards (0 = default 16)
  size_t      max_memory_mb;      // max memory in MB (0 = unlimited)
} astrakv_options_t;

// === Lifecycle ===
astrakv_t*  astrakv_open(const astrakv_options_t *opts);
void        astrakv_close(astrakv_t *kv);
const char* astrakv_last_error(astrakv_t *kv);

// === Pure KV ===
int astrakv_put(astrakv_t *kv, const uint8_t *key, size_t klen,
                                const uint8_t *val, size_t vlen);
int astrakv_get(astrakv_t *kv, const uint8_t *key, size_t klen,
                                uint8_t **val, size_t *vlen);   // caller frees *val
int astrakv_del(astrakv_t *kv, const uint8_t *key, size_t klen);
int astrakv_exists(astrakv_t *kv, const uint8_t *key, size_t klen);
int astrakv_type(astrakv_t *kv, const uint8_t *key, size_t klen,
                                 astrakv_value_type_t *type);

// === TTL ===
int astrakv_expire(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t ms);
int astrakv_ttl(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *ms_remaining);

// === ZSet (sorted set) ===
int astrakv_zadd(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *member, size_t mlen,
                                  double score);
int astrakv_zscore(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    const uint8_t *member, size_t mlen,
                                    double *score);
int astrakv_zrem(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *member, size_t mlen);
int astrakv_zcard(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *count);

// ZRange: returns packed [member_len|member_bytes|score_double]... in *out
// out_len is the total byte count; caller frees *out
int astrakv_zrange(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    int64_t start, int64_t stop, int reverse,
                                    uint8_t **out, size_t *out_len);
int astrakv_zrangebyscore(astrakv_t *kv, const uint8_t *key, size_t klen,
                                           double min, double max,
                                           uint8_t **out, size_t *out_len);

// === List ===
int astrakv_lpush(astrakv_t *kv, const uint8_t *key, size_t klen,
                                   const uint8_t *val, size_t vlen);
int astrakv_rpush(astrakv_t *kv, const uint8_t *key, size_t klen,
                                   const uint8_t *val, size_t vlen);
int astrakv_lpop(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  uint8_t **val, size_t *vlen);
int astrakv_rpop(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  uint8_t **val, size_t *vlen);
int astrakv_llen(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *len);

// === Hash ===
int astrakv_hset(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *field, size_t flen,
                                  const uint8_t *val, size_t vlen);
int astrakv_hget(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *field, size_t flen,
                                  uint8_t **val, size_t *vlen);
int astrakv_hdel(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *field, size_t flen);
int astrakv_hexists(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     const uint8_t *field, size_t flen);
int astrakv_hgetall(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     uint8_t **out, size_t *out_len);
int astrakv_hlen(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *len);
int astrakv_hincrby(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     const uint8_t *field, size_t flen,
                                     int64_t delta, int64_t *result);
// Callback-based iteration: avoids intermediate binary buffer for bulk reads.
// Use astrakv_hlen first to pre-allocate capacity on the Rust side.
typedef void (*astrakv_hash_cb)(const uint8_t *field, size_t flen,
                                 const uint8_t *value, size_t vlen,
                                 void *userdata);
int astrakv_hscan(astrakv_t *kv, const uint8_t *key, size_t klen,
                  astrakv_hash_cb cb, void *userdata);

// === Set ===
int astrakv_sadd(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *member, size_t mlen);
int astrakv_srem(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *member, size_t mlen);
int astrakv_sismember(astrakv_t *kv, const uint8_t *key, size_t klen,
                                       const uint8_t *member, size_t mlen);
int astrakv_smembers(astrakv_t *kv, const uint8_t *key, size_t klen,
                                      uint8_t **out, size_t *out_len);
int astrakv_scard(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *count);
typedef void (*astrakv_set_cb)(const uint8_t *member, size_t mlen, void *userdata);
int astrakv_sscan(astrakv_t *kv, const uint8_t *key, size_t klen,
                  astrakv_set_cb cb, void *userdata);

// === Stream ===
// fields format: [4-byte field_len LE][field bytes][4-byte value_len LE][value bytes] repeated
// returns allocated id string via *id_out (caller frees)
int astrakv_xadd(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *id, size_t idlen,
                                  const uint8_t *fields, size_t fields_len,
                                  char **id_out);
// Returns packed entries: [4b entry_count][for each: 4b id_len|id|4b field_count|4b field_len|field|4b value_len|value...]
int astrakv_xread(astrakv_t *kv, const uint8_t *key, size_t klen,
                                   const uint8_t *start_id, size_t start_id_len,
                                   int64_t count,
                                   uint8_t **out, size_t *out_len);
int astrakv_xrange(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    const uint8_t *start_id, size_t start_id_len,
                                    const uint8_t *end_id, size_t end_id_len,
                                    int64_t count,
                                    uint8_t **out, size_t *out_len);
int astrakv_xlen(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *len);
int astrakv_xdel(astrakv_t *kv, const uint8_t *key, size_t klen,
                                  const uint8_t *id, size_t idlen);
int astrakv_xtrim(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t maxlen, int64_t *removed);

// === JSON ===
int astrakv_jsonset(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     const uint8_t *path, size_t plen,
                                     const uint8_t *json_val, size_t jvlen);
int astrakv_jsonget(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     const uint8_t *path, size_t plen,
                                     uint8_t **json_out, size_t *jout_len);
int astrakv_jsondel(astrakv_t *kv, const uint8_t *key, size_t klen,
                                     const uint8_t *path, size_t plen);

// === Vector ===
int astrakv_vecadd(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    const uint8_t *id, size_t idlen,
                                    const float *vec, size_t veclen);
int astrakv_vecget(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    const uint8_t *id, size_t idlen,
                                    float **vec_out, size_t *veclen_out);
int astrakv_vecdel(astrakv_t *kv, const uint8_t *key, size_t klen,
                                    const uint8_t *id, size_t idlen);
// Returns packed: [4-byte result_count LE][for each: 4-byte id_len LE|id|4-byte float distance LE]
int astrakv_vecsearch(astrakv_t *kv, const uint8_t *key, size_t klen,
                                       const float *query, size_t querylen, size_t k,
                                       uint8_t **out, size_t *out_len);
int astrakv_vecsize(astrakv_t *kv, const uint8_t *key, size_t klen, int64_t *size);

// === Iterator ===
astrakv_iter_t* astrakv_iter_create(astrakv_t *kv);
int  astrakv_iter_next(astrakv_iter_t *it, uint8_t **key, size_t *klen,
                                           uint8_t **val, size_t *vlen);
void astrakv_iter_destroy(astrakv_iter_t *it);

// === Stats ===
typedef struct {
  int64_t put_count;
  int64_t get_count;
  int64_t hits;
  int64_t misses;
  int64_t evicted;
  int64_t expired;
  int64_t key_count;
  int64_t memory_bytes;
} astrakv_stats_t;

int     astrakv_stats(astrakv_t *kv, astrakv_stats_t *out);
int64_t astrakv_count(astrakv_t *kv);
int64_t astrakv_memory_usage(astrakv_t *kv);

// === Eviction Policy ===
typedef enum {
  ASTRAKV_EVICTION_NOEVICTION       = 0,
  ASTRAKV_EVICTION_ALLKEYS_LRU      = 1,
  ASTRAKV_EVICTION_VOLATILE_LRU     = 2,
  ASTRAKV_EVICTION_ALLKEYS_LFU      = 3,
  ASTRAKV_EVICTION_VOLATILE_LFU     = 4,
  ASTRAKV_EVICTION_ALLKEYS_RANDOM   = 5,
  ASTRAKV_EVICTION_VOLATILE_RANDOM  = 6,
  ASTRAKV_EVICTION_VOLATILE_TTL     = 7,
  ASTRAKV_EVICTION_2Q               = 8,
} astrakv_eviction_policy_t;

int astrakv_set_eviction(astrakv_t *kv, astrakv_eviction_policy_t policy);
int astrakv_get_eviction(astrakv_t *kv, astrakv_eviction_policy_t *policy);

// === Maintenance ===
int64_t astrakv_cleanup_expired(astrakv_t *kv);

#ifdef __cplusplus
}
#endif

#endif
