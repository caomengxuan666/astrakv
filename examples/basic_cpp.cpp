#include <iostream>
#include "astrakv/kv.hpp"

int main() {
    using namespace astrakv;

    Kv kv(Kv::Options{});

    kv.put("hello", std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>("world"), 5));

    auto v = kv.get("hello");
    if (v) {
        std::cout << "got: " << std::string(v->begin(), v->end()) << "\n";
    }

    // ZSet
    kv.zadd("scores", "alice", 100);
    kv.zadd("scores", "bob", 200);
    std::cout << "zcard: " << kv.zcard("scores") << "\n";

    for (auto &[mem, score] : kv.zrange("scores", 0, -1, false)) {
        std::cout << "  " << mem << " = " << score << "\n";
    }

    kv.del("hello");
    std::cout << "exists: " << kv.exists("hello") << "\n";

    return 0;
}
