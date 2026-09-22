// G11: ObjectPool test — 槽位复用 / 回收重置 / 条件回收 / 扩容安全性
#include <gtest/gtest.h>
#include <cstdint>
#include "object_pool.h"
#include "types/weapon_types.h"

namespace {

// 带标记的探测类型: 用于验证回收后是否残留上一次使用的数据
struct Probe {
    int32_t id = 0;
    float   value = 0.0f;
    bool    alive = true;
};

TEST(ObjectPoolTest, InsertStoresValue) {
    Game::ObjectPool<Probe> pool{8};
    Probe in;
    in.id = 42;
    in.value = 3.25f;
    pool.insert(in);
    ASSERT_EQ(pool.size(), 1);
    EXPECT_EQ(pool.capacity(), 1);
    EXPECT_EQ(pool.at(0).id, 42);
    EXPECT_FLOAT_EQ(pool.at(0).value, 3.25f);
}

TEST(ObjectPoolTest, ReleaseResetsToDefaultState) {
    Game::ObjectPool<Probe> pool{8};
    Probe dirty;
    dirty.id = 7;
    dirty.value = 100.0f;
    dirty.alive = false;
    pool.insert(dirty);
    pool.release(0);
    ASSERT_EQ(pool.size(), 0);
    Probe* fresh = pool.acquire();
    EXPECT_EQ(fresh->id, 0);
    EXPECT_FLOAT_EQ(fresh->value, 0.0f);
    EXPECT_TRUE(fresh->alive);
}

TEST(ObjectPoolTest, ReleaseReusesSlotWithoutGrowing) {
    Game::ObjectPool<Probe> pool{8};
    Probe a; pool.insert(a);
    Probe b; pool.insert(b);
    EXPECT_EQ(pool.capacity(), 2);
    pool.release(0);
    pool.release(1);
    EXPECT_EQ(pool.size(), 0);
    EXPECT_EQ(pool.idle(), 2);
    Probe c; pool.insert(c);
    EXPECT_EQ(pool.capacity(), 2);      // 复用空闲槽位, 未扩容
    EXPECT_EQ(pool.size(), 1);
}

TEST(ObjectPoolTest, ReleaseIsIdempotentAndIgnoresOutOfRange) {
    Game::ObjectPool<Probe> pool{8};
    Probe p; pool.insert(p);
    pool.release(0);
    pool.release(0);      // 重复回收
    pool.release(-1);     // 下界越界
    pool.release(999);    // 上界越界
    EXPECT_EQ(pool.size(), 0);
    EXPECT_EQ(pool.idle(), 1);          // 仅入栈一次
}

TEST(ObjectPoolTest, ReleaseIfRemovesOnlyMatching) {
    Game::ObjectPool<Probe> pool{8};
    for (int i = 0; i < 5; ++i) {
        Probe p;
        p.id = i;
        p.alive = (i % 2 == 0);
        pool.insert(p);
    }
    pool.release_if([](const Probe& p) { return !p.alive; });
    EXPECT_EQ(pool.size(), 3);
    int id_sum = 0;
    pool.for_each([&](const Probe& p, int) {
        EXPECT_TRUE(p.alive);
        id_sum += p.id;
    });
    EXPECT_EQ(id_sum, 0 + 2 + 4);
}

TEST(ObjectPoolTest, ForEachSkipsIdleSlots) {
    Game::ObjectPool<Probe> pool{8};
    for (int i = 0; i < 4; ++i) {
        Probe p;
        p.id = i;
        pool.insert(p);
    }
    pool.release(1);
    pool.release(2);
    int visits = 0;
    pool.for_each([&](const Probe&, int) { ++visits; });
    EXPECT_EQ(visits, 2);
    EXPECT_EQ(pool.idle(), 2);
}

TEST(ObjectPoolTest, ClearRecyclesAllSlots) {
    Game::ObjectPool<Probe> pool{8};
    for (int i = 0; i < 6; ++i) {
        Probe p;
        p.id = i;
        pool.insert(p);
    }
    pool.clear();
    EXPECT_TRUE(pool.empty());
    EXPECT_EQ(pool.capacity(), 6);      // 不缩容
    EXPECT_EQ(pool.idle(), 6);
    Probe p; pool.insert(p);
    EXPECT_EQ(pool.capacity(), 6);      // 复用
    EXPECT_EQ(pool.size(), 1);
}

// 回归: 旧实现用 vector<Slot>::emplace_back 扩容, 会使此前 acquire 出的
// 裸指针全部悬垂。现在 deque 存储, 扩容不影响既有槽位地址。
TEST(ObjectPoolTest, GrowthKeepsExistingSlotsReadable) {
    Game::ObjectPool<Probe> pool{4};
    Probe* seed = pool.acquire();
    seed->id = 99;
    seed->value = -1.5f;
    for (int i = 0; i < 40; ++i) {
        Probe p;
        p.id = i;
        pool.insert(p);
    }
    EXPECT_EQ(pool.capacity(), 41);
    EXPECT_EQ(pool.at(0).id, 99);
    EXPECT_EQ(pool.at(0).value, seed->value);
    EXPECT_EQ(seed->id, 99);            // 扩容前取得的指针依然有效
    EXPECT_FLOAT_EQ(seed->value, -1.5f);
    bool found = false;
    pool.for_each([&](const Probe& p, int) { if (p.id == 99) found = true; });
    EXPECT_TRUE(found);
}

// 集成验证: 与真实弹体类型配合 (GameScene::projectiles 的元素类型)
TEST(ObjectPoolTest, WorksWithRealProjectile) {
    Game::ObjectPool<Projectile> pool{512};
    Projectile bolt;
    bolt.owner = (int)ProjectileOwner::PLAYER;
    bolt.pos = {10.0f, 20.0f};
    bolt.lifetime = 1.2f;
    pool.insert(bolt);
    ASSERT_EQ(pool.size(), 1);
    pool.release_if([](const Projectile& p) {
        return p.owner == (int)ProjectileOwner::PLAYER;
    });
    EXPECT_TRUE(pool.empty());
    EXPECT_EQ(pool.idle(), 1);
}

} // namespace
