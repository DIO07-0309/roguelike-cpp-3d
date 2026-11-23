// 回放系统回归 — 纯函数: action id 映射 / hash 链校验 / JSON round-trip
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "core/replay/replay_file.h"
#include "core/replay/state_hash.h"

TEST(Replay, ActionIdMappingRoundTrip) {
    int count = replay_action_count();
    EXPECT_GT(count, 0);
    for (int id = 0; id < count; id++) {
        const char* name = replay_action_name(id);
        ASSERT_NE(name, nullptr) << "id " << id;
        EXPECT_EQ(replay_action_id(name), id) << "name " << name;
    }
}

TEST(Replay, UnknownActionReturnsSentinel) {
    EXPECT_EQ(replay_action_id("__no_such_action__"), -1);
    EXPECT_STREQ(replay_action_name(-1), "?");  // 越界返回占位串
}

TEST(Replay, VerifyHashChain) {
    std::vector<uint64_t> expected = {1, 2, 3, 42, 0};
    EXPECT_TRUE(verify_hash_chain(expected, expected));
    auto mismatch = expected;
    mismatch[2] = 99;
    EXPECT_FALSE(verify_hash_chain(expected, mismatch));
    EXPECT_FALSE(verify_hash_chain({1, 2}, {1, 2, 3}));  // 长度不同
    EXPECT_TRUE(verify_hash_chain({}, {}));              // 空链
}

TEST(Replay, SaveLoadRoundTrip) {
    ReplayFile rf;
    rf.game_version = "1.14";
    rf.seed = 4242;
    rf.mods = {{"mod_a", "1.0"}, {"mod_b", "2.1"}};
    rf.actions = {{0, replay_action_id("move_up")}, {5, replay_action_id("attack")}};
    rf.hash_chain = {123, 456, 789};

    std::string json = save_replay(rf);
    EXPECT_FALSE(json.empty());
    const char* path = "tests/core/_tmp_replay_roundtrip.json";
    std::ofstream f(path, std::ios::binary);
    f << json;
    f.close();

    ReplayFile out;
    ASSERT_TRUE(load_replay(path, out));
    EXPECT_EQ(out.version, rf.version);
    EXPECT_EQ(out.game_version, rf.game_version);
    EXPECT_EQ(out.seed, rf.seed);
    ASSERT_EQ(out.mods.size(), rf.mods.size());
    ASSERT_EQ(out.actions.size(), rf.actions.size());
    ASSERT_EQ(out.hash_chain.size(), rf.hash_chain.size());
    for (std::size_t i = 0; i < rf.mods.size(); i++) {
        EXPECT_EQ(out.mods[i].id, rf.mods[i].id);
        EXPECT_EQ(out.mods[i].version, rf.mods[i].version);
    }
    for (std::size_t i = 0; i < rf.actions.size(); i++) {
        EXPECT_EQ(out.actions[i].frame, rf.actions[i].frame);
        EXPECT_EQ(out.actions[i].action_id, rf.actions[i].action_id);
    }
    for (std::size_t i = 0; i < rf.hash_chain.size(); i++)
        EXPECT_EQ(out.hash_chain[i], rf.hash_chain[i]);
    // 二次序列化应与首次逐字节一致 (规范化往返)
    EXPECT_EQ(save_replay(out), json);
    std::remove(path);
}
