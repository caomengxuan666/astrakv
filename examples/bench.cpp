#include "engine.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>

using namespace astrakv;
using clock_ns = std::chrono::high_resolution_clock;

struct BenchResult { const char *name; double ns; };

template <typename F>
static double bench(F &&fn, size_t warmup_iters, size_t measure_iters) {
  for (size_t i = 0; i < warmup_iters; ++i) fn();
  auto t0 = clock_ns::now();
  for (size_t i = 0; i < measure_iters; ++i) fn();
  auto t1 = clock_ns::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count() / measure_iters;
}

int main() {
  Engine::Options opts;
  opts.max_mb = 0;
  Engine engine(opts);
  std::vector<BenchResult> results;

  // ── Pure KV ──
  const std::string pk = "bench";
  engine.put(pk, std::vector<uint8_t>{1,2,3,4,5});

  results.push_back({"put", bench([&]{ engine.put("tmp", std::vector<uint8_t>{1}); }, 1000, 100000)});

  results.push_back({"get_hit", bench([&]{ engine.get(pk); }, 1000, 100000)});

  results.push_back({"get_miss", bench([&]{ engine.get("no_key"); }, 1000, 100000)});

  results.push_back({"del", bench([&]{
    engine.put("d", std::vector<uint8_t>{1});
    engine.del("d");
  }, 100, 10000)});

  // ── ZSet ──
  engine.zadd("zbench", "m0", 1.0);
  for (int i = 1; i < 1000; ++i) engine.zadd("zbench", "m"+std::to_string(i), i*1.0);
  int zcnt = 1000;

  results.push_back({"zadd", bench([&]{
    engine.zadd("zbench", "m"+std::to_string(zcnt++), zcnt*1.0);
  }, 100, 10000)});

  results.push_back({"zscore", bench([&]{ engine.zscore("zbench", "m500"); }, 1000, 100000)});

  results.push_back({"zrange_100", bench([&]{ engine.zrange("zbench", 0, 99, false); }, 100, 1000)});

  results.push_back({"zcard", bench([&]{ engine.zcard("zbench"); }, 1000, 100000)});

  // ── Hash ──
  engine.hset("hbench", "f0", "v0");
  for (int i = 1; i < 1000; ++i) engine.hset("hbench", "f"+std::to_string(i), "v"+std::to_string(i));
  int hcnt = 1000;

  results.push_back({"hset", bench([&]{
    engine.hset("hbench", "f"+std::to_string(hcnt++), "vx");
  }, 100, 10000)});

  results.push_back({"hget", bench([&]{ engine.hget("hbench", "f500"); }, 1000, 100000)});

  results.push_back({"hgetall_1000", bench([&]{ engine.hgetall("hbench"); }, 10, 100)});

  // ── List ──
  engine.rpush("lbench", std::vector<uint8_t>{1});
  for (int i = 1; i < 1000; ++i) engine.rpush("lbench", std::vector<uint8_t>{1});

  results.push_back({"lpush", bench([&]{ engine.lpush("lbench", std::vector<uint8_t>{1}); }, 100, 100000)});

  results.push_back({"lpop", bench([&]{ engine.lpop("lbench"); }, 100, 100000)});

  results.push_back({"llen", bench([&]{ engine.llen("lbench"); }, 1000, 100000)});

  // ── Set ──
  engine.sadd("sbench", "m0");
  for (int i = 1; i < 1000; ++i) engine.sadd("sbench", "m"+std::to_string(i));
  int scnt = 1000;

  results.push_back({"sadd", bench([&]{
    engine.sadd("sbench", "m"+std::to_string(scnt++));
  }, 100, 10000)});

  results.push_back({"sismember", bench([&]{ engine.sismember("sbench", "m500"); }, 1000, 100000)});

  results.push_back({"scard", bench([&]{ engine.scard("sbench"); }, 1000, 100000)});

  // ── Output ──
  std::cout << std::left << std::setw(16) << "Operation" << "Time\n";
  std::cout << std::string(30, '-') << "\n";
  for (auto &r : results) {
    if (r.ns < 1000)
      std::cout << std::setw(16) << r.name << std::fixed << std::setprecision(0) << r.ns << " ns\n";
    else if (r.ns < 1000000)
      std::cout << std::setw(16) << r.name << std::fixed << std::setprecision(1) << r.ns/1000.0 << " µs\n";
    else
      std::cout << std::setw(16) << r.name << std::fixed << std::setprecision(1) << r.ns/1000000.0 << " ms\n";
  }
  return 0;
}
