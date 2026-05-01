#include "engine.hpp"
#include <chrono>
#include <iostream>

using namespace astrakv;

int main() {
  Engine::Options opts;
  Engine engine(opts);

  const std::string key = "hash_bench";

  // Pre-populate
  auto t0 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 1000; ++i) {
    engine.hset(key, "field_" + std::to_string(i), "val_" + std::to_string(i));
  }
  auto t1 = std::chrono::high_resolution_clock::now();
  auto hset_ms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
  std::cout << "hset 1000: " << hset_ms << " us\n";

  // Benchmark hgetall
  auto t2 = std::chrono::high_resolution_clock::now();
  auto result = engine.hgetall(key);
  auto t3 = std::chrono::high_resolution_clock::now();
  auto hgetall_us = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
  std::cout << "hgetall 1000: " << hgetall_us << " us, " << result.size() << " entries\n";

  // Benchmark forEachHash
  std::string buf;
  auto t4 = std::chrono::high_resolution_clock::now();
  engine.forEachHash(key, [&](const std::string &f, const std::string &v) {
    uint32_t fl = f.size(), vl = v.size();
    buf.append((char*)&fl, 4).append(f).append((char*)&vl, 4).append(v);
  });
  auto t5 = std::chrono::high_resolution_clock::now();
  auto forEach_us = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();
  std::cout << "forEachHash 1000: " << forEach_us << " us, buf=" << buf.size() << " bytes\n";

  return 0;
}
