#include <gtest/gtest.h>
#include "astrakv/kv.hpp"

using namespace astrakv;

TEST(KvTest, PutGetDelete) {
  Kv kv(Kv::Options{});
  EXPECT_TRUE(kv.put("hello", std::span<const uint8_t>(
    reinterpret_cast<const uint8_t*>("world"), 5)));
  auto v = kv.get("hello");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(std::string(v->begin(), v->end()), "world");
  EXPECT_TRUE(kv.exists("hello"));
  EXPECT_TRUE(kv.del("hello"));
  EXPECT_FALSE(kv.exists("hello"));
  EXPECT_FALSE(kv.get("hello").has_value());
}

TEST(KvTest, ZSet) {
  Kv kv(Kv::Options{});
  EXPECT_TRUE(kv.zadd("scores", "alice", 100));
  EXPECT_TRUE(kv.zadd("scores", "bob", 200));
  EXPECT_EQ(kv.zcard("scores"), 2);
  auto s = kv.zscore("scores", "alice");
  ASSERT_TRUE(s.has_value());
  EXPECT_DOUBLE_EQ(*s, 100);
  auto r = kv.zrange("scores", 0, -1, false);
  EXPECT_EQ(r.size(), 2);
  EXPECT_EQ(r[0].first, "alice");
  EXPECT_EQ(r[1].first, "bob");
}

TEST(KvTest, List) {
  Kv kv(Kv::Options{});
  EXPECT_TRUE(kv.lpush("mylist", std::span<const uint8_t>(
    reinterpret_cast<const uint8_t*>("b"), 1)));
  EXPECT_TRUE(kv.lpush("mylist", std::span<const uint8_t>(
    reinterpret_cast<const uint8_t*>("a"), 1)));
  EXPECT_EQ(kv.llen("mylist"), 2);
  auto v = kv.rpop("mylist");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(std::string(v->begin(), v->end()), "b");
}

TEST(KvTest, TTL) {
  Kv kv(Kv::Options{});
  kv.put("temp", std::span<const uint8_t>(
    reinterpret_cast<const uint8_t*>("x"), 1));
  EXPECT_TRUE(kv.expire("temp", 100000));  // 100s
  auto t = kv.ttl("temp");
  ASSERT_TRUE(t.has_value());
  EXPECT_GT(*t, 0);
  EXPECT_LE(*t, 100000);
}

TEST(KvTest, Iterator) {
  Kv kv(Kv::Options{});
  kv.put("k1", std::span<const uint8_t>(reinterpret_cast<const uint8_t*>("v1"), 2));
  kv.put("k2", std::span<const uint8_t>(reinterpret_cast<const uint8_t*>("v2"), 2));
  auto it = kv.iter();
  int count = 0;
  while (auto entry = it.next()) {
    count++;
  }
  EXPECT_EQ(count, 2);
}
