mod ffi;

use std::ffi::{c_char, CStr};
use std::fmt;
use std::ptr;
use std::time::Duration;

// Force linking against the C++ static library
#[link(name = "astrakv", kind = "static")]
extern "C" {
    fn astrakv_open(_: *const ffi::astrakv_options_t) -> *mut ffi::astrakv_t;
}

#[derive(Debug)]
pub enum Error {
    NotFound,
    IoError(String),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::NotFound => write!(f, "key not found"),
            Error::IoError(s) => write!(f, "{}", s),
        }
    }
}

impl std::error::Error for Error {}

pub type Result<T> = std::result::Result<T, Error>;

// ── Zero-copy batch result types (own the buffer, iterate without copying) ──

pub struct HashEntries {
    buf: Vec<u8>,
}

impl HashEntries {
    pub fn len(&self) -> usize {
        let mut count = 0usize;
        let mut pos = 0usize;
        while pos + 4 <= self.buf.len() {
            let flen = u32::from_le_bytes([self.buf[pos], self.buf[pos+1], self.buf[pos+2], self.buf[pos+3]]) as usize;
            pos += 4;
            if pos + flen + 4 > self.buf.len() { break; }
            pos += flen;
            let vlen = u32::from_le_bytes([self.buf[pos], self.buf[pos+1], self.buf[pos+2], self.buf[pos+3]]) as usize;
            pos += 4;
            if pos + vlen > self.buf.len() { break; }
            pos += vlen;
            count += 1;
        }
        count
    }

    pub fn iter(&self) -> HashEntriesIter { HashEntriesIter { buf: &self.buf, pos: 0 } }

    pub fn into_vec(self) -> Vec<(Vec<u8>, Vec<u8>)> {
        self.iter().map(|(f, v)| (f.to_vec(), v.to_vec())).collect()
    }
}

pub struct HashEntriesIter<'a> { buf: &'a [u8], pos: usize }

impl<'a> Iterator for HashEntriesIter<'a> {
    type Item = (&'a [u8], &'a [u8]);
    fn next(&mut self) -> Option<Self::Item> {
        if self.pos + 4 > self.buf.len() { return None; }
        let flen = u32::from_le_bytes([self.buf[self.pos], self.buf[self.pos+1], self.buf[self.pos+2], self.buf[self.pos+3]]) as usize;
        self.pos += 4;
        if self.pos + flen + 4 > self.buf.len() { return None; }
        let field = &self.buf[self.pos..self.pos + flen];
        self.pos += flen;
        let vlen = u32::from_le_bytes([self.buf[self.pos], self.buf[self.pos+1], self.buf[self.pos+2], self.buf[self.pos+3]]) as usize;
        self.pos += 4;
        if self.pos + vlen > self.buf.len() { return None; }
        let value = &self.buf[self.pos..self.pos + vlen];
        self.pos += vlen;
        Some((field, value))
    }
}

pub struct SetMembers {
    buf: Vec<u8>,
}

impl SetMembers {
    pub fn len(&self) -> usize {
        let mut count = 0usize;
        let mut pos = 0usize;
        while pos + 4 <= self.buf.len() {
            let mlen = u32::from_le_bytes([self.buf[pos], self.buf[pos+1], self.buf[pos+2], self.buf[pos+3]]) as usize;
            pos += 4;
            if pos + mlen > self.buf.len() { break; }
            pos += mlen;
            count += 1;
        }
        count
    }

    pub fn iter(&self) -> SetMembersIter { SetMembersIter { buf: &self.buf, pos: 0 } }

    pub fn into_vec(self) -> Vec<Vec<u8>> {
        self.iter().map(|m| m.to_vec()).collect()
    }
}

pub struct SetMembersIter<'a> { buf: &'a [u8], pos: usize }

impl<'a> Iterator for SetMembersIter<'a> {
    type Item = &'a [u8];
    fn next(&mut self) -> Option<Self::Item> {
        if self.pos + 4 > self.buf.len() { return None; }
        let mlen = u32::from_le_bytes([self.buf[self.pos], self.buf[self.pos+1], self.buf[self.pos+2], self.buf[self.pos+3]]) as usize;
        self.pos += 4;
        if self.pos + mlen > self.buf.len() { return None; }
        let member = &self.buf[self.pos..self.pos + mlen];
        self.pos += mlen;
        Some(member)
    }
}

/// Represents a member-score pair from a sorted set.
#[derive(Debug, Clone)]
pub struct ZSetEntry {
    pub member: String,
    pub score: f64,
}

// ── Kv ──

pub struct Kv {
    inner: *mut ffi::astrakv_t,
}

impl Kv {
    pub fn open(path: Option<&str>, shards: Option<usize>, max_memory_mb: Option<usize>) -> Result<Self> {
        let path_c = path.map(|s| std::ffi::CString::new(s).unwrap());
        let opts = ffi::astrakv_options_t {
            path: path_c.as_ref().map_or(ptr::null(), |s| s.as_ptr() as *const c_char),
            shards: shards.unwrap_or(16) as usize,
            max_memory_mb: max_memory_mb.unwrap_or(0) as usize,
        };

        let inner = unsafe { ffi::astrakv_open(&opts) };
        if inner.is_null() {
            return Err(Error::IoError("failed to open KV store".into()));
        }
        Ok(Kv { inner })
    }

    pub fn in_memory() -> Result<Self> {
        Self::open(None, None, None)
    }

    // ── Pure KV ──

    pub fn put(&self, key: &[u8], value: &[u8]) -> Result<()> {
        let ret = unsafe {
            ffi::astrakv_put(self.inner, key.as_ptr(), key.len(), value.as_ptr(), value.len())
        };
        if ret != 0 { Err(Error::IoError("put failed".into())) } else { Ok(()) }
    }

    pub fn get(&self, key: &[u8]) -> Result<Option<Vec<u8>>> {
        let mut val: *mut u8 = ptr::null_mut();
        let mut vlen: usize = 0;
        let ret = unsafe {
            ffi::astrakv_get(self.inner, key.as_ptr(), key.len(), &mut val, &mut vlen)
        };
        if ret != 0 {
            return Ok(None);  // key not found
        }
        if val.is_null() {
            return Ok(None);
        }
        let v = unsafe { std::slice::from_raw_parts(val, vlen).to_vec() };
        unsafe { libc::free(val as *mut libc::c_void) };
        Ok(Some(v))
    }

    pub fn delete(&self, key: &[u8]) -> bool {
        unsafe { ffi::astrakv_del(self.inner, key.as_ptr(), key.len()) == 0 }
    }

    pub fn exists(&self, key: &[u8]) -> bool {
        unsafe { ffi::astrakv_exists(self.inner, key.as_ptr(), key.len()) == 0 }
    }

    // ── String convenience ──

    pub fn put_str(&self, key: &str, value: &str) -> Result<()> {
        self.put(key.as_bytes(), value.as_bytes())
    }

    pub fn get_str(&self, key: &str) -> Result<Option<String>> {
        self.get(key.as_bytes()).map(|opt| {
            opt.map(|v| String::from_utf8_lossy(&v).into_owned())
        })
    }

    // ── TTL ──

    pub fn expire(&self, key: &[u8], ms: i64) -> Result<()> {
        let ret = unsafe { ffi::astrakv_expire(self.inner, key.as_ptr(), key.len(), ms) };
        if ret != 0 { Err(Error::NotFound) } else { Ok(()) }
    }

    pub fn ttl(&self, key: &[u8]) -> Result<Option<Duration>> {
        let mut ms: i64 = 0;
        let ret = unsafe { ffi::astrakv_ttl(self.inner, key.as_ptr(), key.len(), &mut ms) };
        if ret != 0 {
            Ok(None)
        } else {
            Ok(Some(Duration::from_millis(ms as u64)))
        }
    }

    // ── ZSet ──

    pub fn zadd(&self, key: &[u8], member: &[u8], score: f64) -> Result<()> {
        let ret = unsafe {
            ffi::astrakv_zadd(self.inner, key.as_ptr(), key.len(),
                              member.as_ptr(), member.len(), score)
        };
        if ret != 0 { Err(Error::IoError("zadd failed".into())) } else { Ok(()) }
    }

    pub fn zscore(&self, key: &[u8], member: &[u8]) -> Result<Option<f64>> {
        let mut score: f64 = 0.0;
        let ret = unsafe {
            ffi::astrakv_zscore(self.inner, key.as_ptr(), key.len(),
                                member.as_ptr(), member.len(), &mut score)
        };
        if ret != 0 { Ok(None) } else { Ok(Some(score)) }
    }

    pub fn zrem(&self, key: &[u8], member: &[u8]) -> bool {
        unsafe { ffi::astrakv_zrem(self.inner, key.as_ptr(), key.len(), member.as_ptr(), member.len()) == 0 }
    }

    pub fn zcard(&self, key: &[u8]) -> i64 {
        let mut count: i64 = 0;
        unsafe { ffi::astrakv_zcard(self.inner, key.as_ptr(), key.len(), &mut count) };
        count
    }

    pub fn zrange(&self, key: &[u8], start: i64, stop: i64, reverse: bool) -> Result<Vec<ZSetEntry>> {
        let mut out: *mut u8 = ptr::null_mut();
        let mut out_len: usize = 0;
        let ret = unsafe {
            ffi::astrakv_zrange(self.inner, key.as_ptr(), key.len(),
                                start, stop, reverse as i32,
                                &mut out, &mut out_len)
        };
        if ret != 0 { return Err(Error::IoError("zrange failed".into())); }
        if out.is_null() { return Ok(Vec::new()); }

        let entries = parse_zset_entries(out, out_len);
        unsafe { libc::free(out as *mut libc::c_void) };
        Ok(entries)
    }

    pub fn zrangebyscore(&self, key: &[u8], min: f64, max: f64) -> Result<Vec<ZSetEntry>> {
        let mut out: *mut u8 = ptr::null_mut();
        let mut out_len: usize = 0;
        let ret = unsafe {
            ffi::astrakv_zrangebyscore(self.inner, key.as_ptr(), key.len(),
                                       min, max, &mut out, &mut out_len)
        };
        if ret != 0 { return Err(Error::IoError("zrangebyscore failed".into())); }
        if out.is_null() { return Ok(Vec::new()); }

        let entries = parse_zset_entries(out, out_len);
        unsafe { libc::free(out as *mut libc::c_void) };
        Ok(entries)
    }

    // ── List ──

    pub fn lpush(&self, key: &[u8], value: &[u8]) -> Result<()> {
        let ret = unsafe {
            ffi::astrakv_lpush(self.inner, key.as_ptr(), key.len(), value.as_ptr(), value.len())
        };
        if ret != 0 { Err(Error::IoError("lpush failed".into())) } else { Ok(()) }
    }

    pub fn rpush(&self, key: &[u8], value: &[u8]) -> Result<()> {
        let ret = unsafe {
            ffi::astrakv_rpush(self.inner, key.as_ptr(), key.len(), value.as_ptr(), value.len())
        };
        if ret != 0 { Err(Error::IoError("rpush failed".into())) } else { Ok(()) }
    }

    pub fn lpop(&self, key: &[u8]) -> Result<Option<Vec<u8>>> {
        let mut val: *mut u8 = ptr::null_mut();
        let mut vlen: usize = 0;
        let ret = unsafe {
            ffi::astrakv_lpop(self.inner, key.as_ptr(), key.len(), &mut val, &mut vlen)
        };
        if ret != 0 { return Ok(None); }
        let v = unsafe { std::slice::from_raw_parts(val, vlen).to_vec() };
        unsafe { libc::free(val as *mut libc::c_void) };
        Ok(Some(v))
    }

    pub fn rpop(&self, key: &[u8]) -> Result<Option<Vec<u8>>> {
        let mut val: *mut u8 = ptr::null_mut();
        let mut vlen: usize = 0;
        let ret = unsafe {
            ffi::astrakv_rpop(self.inner, key.as_ptr(), key.len(), &mut val, &mut vlen)
        };
        if ret != 0 { return Ok(None); }
        let v = unsafe { std::slice::from_raw_parts(val, vlen).to_vec() };
        unsafe { libc::free(val as *mut libc::c_void) };
        Ok(Some(v))
    }

    pub fn llen(&self, key: &[u8]) -> i64 {
        let mut len: i64 = 0;
        unsafe { ffi::astrakv_llen(self.inner, key.as_ptr(), key.len(), &mut len) };
        len
    }

    // ── Hash ──

    pub fn hset(&self, key: &[u8], field: &[u8], value: &[u8]) -> Result<()> {
        let ret = unsafe {
            ffi::astrakv_hset(self.inner, key.as_ptr(), key.len(),
                              field.as_ptr(), field.len(),
                              value.as_ptr(), value.len())
        };
        if ret != 0 { Err(Error::IoError("hset failed".into())) } else { Ok(()) }
    }

    pub fn hget(&self, key: &[u8], field: &[u8]) -> Result<Option<Vec<u8>>> {
        let mut val: *mut u8 = ptr::null_mut();
        let mut vlen: usize = 0;
        let ret = unsafe {
            ffi::astrakv_hget(self.inner, key.as_ptr(), key.len(),
                              field.as_ptr(), field.len(),
                              &mut val, &mut vlen)
        };
        if ret != 0 { return Ok(None); }
        if val.is_null() { return Ok(None); }
        let v = unsafe { std::slice::from_raw_parts(val, vlen).to_vec() };
        unsafe { libc::free(val as *mut libc::c_void) };
        Ok(Some(v))
    }

    pub fn hdel(&self, key: &[u8], field: &[u8]) -> bool {
        unsafe { ffi::astrakv_hdel(self.inner, key.as_ptr(), key.len(), field.as_ptr(), field.len()) == 0 }
    }

    pub fn hexists(&self, key: &[u8], field: &[u8]) -> bool {
        unsafe { ffi::astrakv_hexists(self.inner, key.as_ptr(), key.len(), field.as_ptr(), field.len()) == 0 }
    }

    pub fn hgetall(&self, key: &[u8]) -> Result<HashEntries> {
        let mut out: *mut u8 = ptr::null_mut();
        let mut out_len: usize = 0;
        let ret = unsafe {
            ffi::astrakv_hgetall(self.inner, key.as_ptr(), key.len(), &mut out, &mut out_len)
        };
        if ret != 0 { return Err(Error::IoError("hgetall failed".into())); }
        let buf = if out.is_null() { Vec::new() } else {
            let v = unsafe { std::slice::from_raw_parts(out, out_len).to_vec() };
            unsafe { libc::free(out as *mut libc::c_void) };
            v
        };
        Ok(HashEntries { buf })
    }

    pub fn hlen(&self, key: &[u8]) -> i64 {
        let mut len: i64 = 0;
        unsafe { ffi::astrakv_hlen(self.inner, key.as_ptr(), key.len(), &mut len) };
        len
    }

    pub fn hincrby(&self, key: &[u8], field: &[u8], delta: i64) -> Result<i64> {
        let mut result: i64 = 0;
        let ret = unsafe {
            ffi::astrakv_hincrby(self.inner, key.as_ptr(), key.len(),
                                 field.as_ptr(), field.len(), delta, &mut result)
        };
        if ret != 0 { Err(Error::IoError("hincrby failed".into())) } else { Ok(result) }
    }

    // ── Set ──

    pub fn sadd(&self, key: &[u8], member: &[u8]) -> Result<()> {
        let ret = unsafe {
            ffi::astrakv_sadd(self.inner, key.as_ptr(), key.len(), member.as_ptr(), member.len())
        };
        if ret != 0 { Err(Error::IoError("sadd failed".into())) } else { Ok(()) }
    }

    pub fn srem(&self, key: &[u8], member: &[u8]) -> bool {
        unsafe { ffi::astrakv_srem(self.inner, key.as_ptr(), key.len(), member.as_ptr(), member.len()) == 0 }
    }

    pub fn sismember(&self, key: &[u8], member: &[u8]) -> bool {
        unsafe { ffi::astrakv_sismember(self.inner, key.as_ptr(), key.len(), member.as_ptr(), member.len()) == 0 }
    }

    pub fn smembers(&self, key: &[u8]) -> Result<SetMembers> {
        let mut out: *mut u8 = ptr::null_mut();
        let mut out_len: usize = 0;
        let ret = unsafe {
            ffi::astrakv_smembers(self.inner, key.as_ptr(), key.len(), &mut out, &mut out_len)
        };
        if ret != 0 { return Err(Error::IoError("smembers failed".into())); }
        let buf = if out.is_null() { Vec::new() } else {
            let v = unsafe { std::slice::from_raw_parts(out, out_len).to_vec() };
            unsafe { libc::free(out as *mut libc::c_void) };
            v
        };
        Ok(SetMembers { buf })
    }

    pub fn scard(&self, key: &[u8]) -> i64 {
        let mut count: i64 = 0;
        unsafe { ffi::astrakv_scard(self.inner, key.as_ptr(), key.len(), &mut count) };
        count
    }

    // ── Stream ──

    pub fn xadd(&self, key: &[u8], id: &[u8], fields: &[u8]) -> Result<String> {
        let mut id_out: *mut c_char = ptr::null_mut();
        let ret = unsafe {
            ffi::astrakv_xadd(self.inner, key.as_ptr(), key.len(),
                              id.as_ptr(), id.len(),
                              fields.as_ptr(), fields.len(),
                              &mut id_out)
        };
        if ret != 0 { return Err(Error::IoError("xadd failed".into())); }
        let s = unsafe { CStr::from_ptr(id_out).to_string_lossy().into_owned() };
        unsafe { libc::free(id_out as *mut libc::c_void) };
        Ok(s)
    }

    pub fn xread(&self, key: &[u8], start_id: &[u8], count: i64) -> Result<Vec<u8>> {
        let mut out: *mut u8 = ptr::null_mut();
        let mut out_len: usize = 0;
        let ret = unsafe {
            ffi::astrakv_xread(self.inner, key.as_ptr(), key.len(),
                               start_id.as_ptr(), start_id.len(),
                               count,
                               &mut out, &mut out_len)
        };
        if ret != 0 { return Err(Error::IoError("xread failed".into())); }
        if out.is_null() { return Ok(Vec::new()); }
        let data = unsafe { std::slice::from_raw_parts(out, out_len).to_vec() };
        unsafe { libc::free(out as *mut libc::c_void) };
        Ok(data)
    }

    pub fn xlen(&self, key: &[u8]) -> i64 {
        let mut len: i64 = 0;
        unsafe { ffi::astrakv_xlen(self.inner, key.as_ptr(), key.len(), &mut len) };
        len
    }

    // ── JSON ──

    pub fn jsonset(&self, key: &[u8], path: &[u8], json_val: &[u8]) -> Result<()> {
        let ret = unsafe {
            ffi::astrakv_jsonset(self.inner, key.as_ptr(), key.len(),
                                 path.as_ptr(), path.len(),
                                 json_val.as_ptr(), json_val.len())
        };
        if ret != 0 { Err(Error::IoError("jsonset failed".into())) } else { Ok(()) }
    }

    pub fn jsonget(&self, key: &[u8], path: &[u8]) -> Result<Option<Vec<u8>>> {
        let mut json_out: *mut u8 = ptr::null_mut();
        let mut jout_len: usize = 0;
        let ret = unsafe {
            ffi::astrakv_jsonget(self.inner, key.as_ptr(), key.len(),
                                 path.as_ptr(), path.len(),
                                 &mut json_out, &mut jout_len)
        };
        if ret != 0 { return Ok(None); }
        if json_out.is_null() { return Ok(None); }
        let v = unsafe { std::slice::from_raw_parts(json_out, jout_len).to_vec() };
        unsafe { libc::free(json_out as *mut libc::c_void) };
        Ok(Some(v))
    }

    // ── Vector ──

    pub fn vecadd(&self, key: &[u8], id: &[u8], vec: &[f32]) -> Result<()> {
        let ret = unsafe {
            ffi::astrakv_vecadd(self.inner, key.as_ptr(), key.len(),
                                id.as_ptr(), id.len(),
                                vec.as_ptr(), vec.len())
        };
        if ret != 0 { Err(Error::IoError("vecadd failed".into())) } else { Ok(()) }
    }

    pub fn vecsearch(&self, key: &[u8], query: &[f32], k: usize) -> Result<Vec<u8>> {
        let mut out: *mut u8 = ptr::null_mut();
        let mut out_len: usize = 0;
        let ret = unsafe {
            ffi::astrakv_vecsearch(self.inner, key.as_ptr(), key.len(),
                                   query.as_ptr(), query.len(), k,
                                   &mut out, &mut out_len)
        };
        if ret != 0 { return Err(Error::IoError("vecsearch failed".into())); }
        if out.is_null() { return Ok(Vec::new()); }
        let data = unsafe { std::slice::from_raw_parts(out, out_len).to_vec() };
        unsafe { libc::free(out as *mut libc::c_void) };
        Ok(data)
    }

    // ── Stats ──

    pub fn count(&self) -> i64 {
        unsafe { ffi::astrakv_count(self.inner) }
    }

    pub fn memory_usage(&self) -> i64 {
        unsafe { ffi::astrakv_memory_usage(self.inner) }
    }
}

impl Drop for Kv {
    fn drop(&mut self) {
        unsafe { ffi::astrakv_close(self.inner) };
    }
}

unsafe impl Send for Kv {}
unsafe impl Sync for Kv {}

// ── Iter ──

pub struct Iter {
    inner: *mut ffi::astrakv_iter_t,
}

impl Kv {
    pub fn iter(&self) -> Iter {
        let inner = unsafe { ffi::astrakv_iter_create(self.inner) };
        Iter { inner }
    }
}

impl Iterator for Iter {
    type Item = (Vec<u8>, Vec<u8>);

    fn next(&mut self) -> Option<Self::Item> {
        let mut key: *mut u8 = ptr::null_mut();
        let mut klen: usize = 0;
        let mut val: *mut u8 = ptr::null_mut();
        let mut vlen: usize = 0;
        let ret = unsafe {
            ffi::astrakv_iter_next(self.inner, &mut key, &mut klen, &mut val, &mut vlen)
        };
        if ret != 0 { return None; }

        let k = unsafe { std::slice::from_raw_parts(key, klen).to_vec() };
        let v = unsafe { std::slice::from_raw_parts(val, vlen).to_vec() };
        unsafe {
            libc::free(key as *mut libc::c_void);
            libc::free(val as *mut libc::c_void);
        }
        Some((k, v))
    }
}

impl Drop for Iter {
    fn drop(&mut self) {
        unsafe { ffi::astrakv_iter_destroy(self.inner) };
    }
}

// ── Helper ──

fn parse_zset_entries(data: *const u8, len: usize) -> Vec<ZSetEntry> {
    let mut entries = Vec::new();
    let buf = unsafe { std::slice::from_raw_parts(data, len) };
    let mut pos: usize = 0;

    while pos + 4 <= buf.len() {
        let mlen = u32::from_le_bytes([buf[pos], buf[pos+1], buf[pos+2], buf[pos+3]]) as usize;
        pos += 4;
        if pos + mlen + 8 > buf.len() { break; }

        let member = String::from_utf8_lossy(&buf[pos..pos + mlen]).into_owned();
        pos += mlen;

        let score = f64::from_le_bytes([
            buf[pos], buf[pos+1], buf[pos+2], buf[pos+3],
            buf[pos+4], buf[pos+5], buf[pos+6], buf[pos+7],
        ]);
        pos += 8;

        entries.push(ZSetEntry { member, score });
    }
    entries
}

fn parse_hash_entries(data: *const u8, len: usize) -> Vec<(Vec<u8>, Vec<u8>)> {
    let mut entries = Vec::new();
    let buf = unsafe { std::slice::from_raw_parts(data, len) };
    let mut pos: usize = 0;

    while pos + 4 <= buf.len() {
        let flen = u32::from_le_bytes([buf[pos], buf[pos+1], buf[pos+2], buf[pos+3]]) as usize;
        pos += 4;
        if pos + flen + 4 > buf.len() { break; }

        let field = buf[pos..pos + flen].to_vec();
        pos += flen;

        let vlen = u32::from_le_bytes([buf[pos], buf[pos+1], buf[pos+2], buf[pos+3]]) as usize;
        pos += 4;
        if pos + vlen > buf.len() { break; }

        let value = buf[pos..pos + vlen].to_vec();
        pos += vlen;

        entries.push((field, value));
    }
    entries
}

fn parse_set_members(data: *const u8, len: usize) -> Vec<Vec<u8>> {
    let mut members = Vec::new();
    let buf = unsafe { std::slice::from_raw_parts(data, len) };
    let mut pos: usize = 0;

    while pos + 4 <= buf.len() {
        let mlen = u32::from_le_bytes([buf[pos], buf[pos+1], buf[pos+2], buf[pos+3]]) as usize;
        pos += 4;
        if pos + mlen > buf.len() { break; }

        members.push(buf[pos..pos + mlen].to_vec());
        pos += mlen;
    }
    members
}