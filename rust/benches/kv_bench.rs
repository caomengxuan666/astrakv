use criterion::{black_box, criterion_group, criterion_main, Criterion};
use astrakv::Kv;

fn bench_pure_kv(c: &mut Criterion) {
    let kv = Kv::in_memory().unwrap();

    let key = b"bench_key";
    let val = b"bench_value_1234567890";

    c.bench_function("put", |b| {
        b.iter(|| kv.put(black_box(key), black_box(val)).unwrap())
    });

    c.bench_function("get_hit", |b| {
        kv.put(key, val).unwrap();
        b.iter(|| {
            let _ = black_box(kv.get(black_box(key)));
        })
    });

    c.bench_function("get_miss", |b| {
        b.iter(|| {
            let _ = black_box(kv.get(black_box(b"no_such_key")));
        })
    });

    c.bench_function("del", |b| {
        kv.put(key, val).unwrap();
        b.iter(|| {
            let _ = black_box(kv.delete(black_box(key)));
        })
    });
}

fn bench_zset(c: &mut Criterion) {
    let kv = Kv::in_memory().unwrap();
    let zkey = b"zset_bench";

    for i in 0..1000 {
        kv.zadd(zkey, format!("member_{}", i).as_bytes(), i as f64).unwrap();
    }

    c.bench_function("zadd", |b| {
        let mut i = 1000u64;
        b.iter(|| {
            i += 1;
            kv.zadd(
                black_box(zkey),
                black_box(format!("m_{}", i).as_bytes()),
                black_box(i as f64),
            )
            .unwrap()
        })
    });

    c.bench_function("zscore", |b| {
        b.iter(|| {
            let _ = black_box(kv.zscore(black_box(zkey), black_box("member_500".as_bytes())));
        })
    });

    c.bench_function("zrange_100", |b| {
        b.iter(|| {
            let _ = black_box(kv.zrange(black_box(zkey), 0, 99, false));
        })
    });

    c.bench_function("zcard", |b| {
        b.iter(|| {
            let _ = black_box(kv.zcard(black_box(zkey)));
        })
    });
}

fn bench_hash(c: &mut Criterion) {
    let kv = Kv::in_memory().unwrap();
    let hkey = b"hash_bench";

    for i in 0..1000 {
        kv.hset(
            hkey,
            format!("field_{}", i).as_bytes(),
            format!("val_{}", i).as_bytes(),
        )
        .unwrap();
    }

    c.bench_function("hset", |b| {
        let mut i = 1000u64;
        b.iter(|| {
            i += 1;
            kv.hset(
                black_box(hkey),
                black_box(format!("f_{}", i).as_bytes()),
                black_box(format!("v_{}", i).as_bytes()),
            )
            .unwrap()
        })
    });

    c.bench_function("hget", |b| {
        b.iter(|| {
            let _ = black_box(kv.hget(black_box(hkey), black_box("field_500".as_bytes())));
        })
    });

    c.bench_function("hgetall_1000", |b| {
        b.iter(|| {
            let entries = black_box(kv.hgetall(black_box(hkey)).unwrap());
            black_box(entries.iter().count())
        })
    });
}

fn bench_list(c: &mut Criterion) {
    let kv = Kv::in_memory().unwrap();
    let lkey = b"list_bench";

    for i in 0..1000 {
        kv.rpush(lkey, format!("item_{}", i).as_bytes()).unwrap();
    }

    c.bench_function("lpush", |b| {
        b.iter(|| kv.lpush(black_box(lkey), black_box(b"item")).unwrap())
    });

    c.bench_function("lpop", |b| {
        b.iter(|| {
            let _ = black_box(kv.lpop(black_box(lkey)));
        })
    });

    c.bench_function("llen", |b| {
        b.iter(|| {
            let _ = black_box(kv.llen(black_box(lkey)));
        })
    });
}

fn bench_set(c: &mut Criterion) {
    let kv = Kv::in_memory().unwrap();
    let skey = b"set_bench";

    for i in 0..1000 {
        kv.sadd(skey, format!("member_{}", i).as_bytes()).unwrap();
    }

    c.bench_function("sadd", |b| {
        let mut i = 1000u64;
        b.iter(|| {
            i += 1;
            kv.sadd(
                black_box(skey),
                black_box(format!("m_{}", i).as_bytes()),
            )
            .unwrap()
        })
    });

    c.bench_function("sismember", |b| {
        b.iter(|| {
            let _ = black_box(kv.sismember(black_box(skey), black_box("member_500".as_bytes())));
        })
    });

    c.bench_function("scard", |b| {
        b.iter(|| {
            let _ = black_box(kv.scard(black_box(skey)));
        })
    });
}

criterion_group!(benches, bench_pure_kv, bench_zset, bench_hash, bench_list, bench_set);
criterion_main!(benches);
