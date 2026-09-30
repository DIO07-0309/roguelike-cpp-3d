// B3-M: MirrorMemoryStore — CloneTable 跨局持久化闭环测试
#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "ai/mirror/behavior_clone_table.h"
#include "ai/mirror/mirror_memory_store.h"

namespace {
const char* kTestPath = "saves/test_mirror_memory.json";
const char* kTmpPath = "saves/test_mirror_memory.json.tmp";

struct TestPathGuard {
    TestPathGuard() { mirror::MirrorMemoryStore::set_path_for_test(kTestPath); }
    ~TestPathGuard() {
        std::remove(kTestPath);
        std::remove(kTmpPath);
        mirror::MirrorMemoryStore::set_path_for_test(
            "saves/mirror_memory.json");   // 还原生产默认
    }
};

CloneContext lowHpClose() { return CloneContext::from_state(1.5f, 0.2f, 3); }

void bump(BehaviorCloneTable& t, PlayerIntention intent, int n) {
    for (int i = 0; i < n; i++) t.record_decision(lowHpClose(), intent);
}

const BehaviorCloneTable::Counts* find_entry(const BehaviorCloneTable& t) {
    auto it = t.table().find(lowHpClose().key());
    return it == t.table().end() ? nullptr : &it->second;
}
}  // namespace

TEST(MirrorMemoryStore, SaveLoadRoundTripKeepsPrediction) {
    TestPathGuard guard;
    BehaviorCloneTable src;
    bump(src, PlayerIntention::HEAL, 40);
    bump(src, PlayerIntention::ATTACK, 10);
    ASSERT_TRUE(mirror::MirrorMemoryStore::save_from(src));

    BehaviorCloneTable dst;
    ASSERT_TRUE(mirror::MirrorMemoryStore::load_into(dst));
    ClonePrediction p = dst.predict(1.5f, 0.2f, 3);
    EXPECT_EQ(p.best, PlayerIntention::HEAL);
    EXPECT_EQ(p.level, 0);                        // exact 命中
}

TEST(MirrorMemoryStore, LoadAppliesOncePerRunDecay) {
    TestPathGuard guard;
    BehaviorCloneTable src;
    bump(src, PlayerIntention::HEAL, 100);
    bump(src, PlayerIntention::ATTACK, 50);
    ASSERT_TRUE(mirror::MirrorMemoryStore::save_from(src));

    BehaviorCloneTable dst;
    ASSERT_TRUE(mirror::MirrorMemoryStore::load_into(dst));
    const auto* c = find_entry(dst);              // 100→99, 50→49 (×0.99 截断)
    ASSERT_NE(c, nullptr);
    EXPECT_EQ((*c)[(int)PlayerIntention::HEAL], 99);
    EXPECT_EQ((*c)[(int)PlayerIntention::ATTACK], 49);
}

TEST(MirrorMemoryStore, MergeAccumulatesOntoCurrentRun) {
    TestPathGuard guard;
    BehaviorCloneTable src;
    bump(src, PlayerIntention::HEAL, 100);
    ASSERT_TRUE(mirror::MirrorMemoryStore::save_from(src));

    BehaviorCloneTable dst;                       // 本局先学了 2 次
    bump(dst, PlayerIntention::HEAL, 2);
    ASSERT_TRUE(mirror::MirrorMemoryStore::load_into(dst));
    const auto* c = find_entry(dst);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ((*c)[(int)PlayerIntention::HEAL], 101);  // 2 + 99
}

TEST(MirrorMemoryStore, TinyOldMemoryDecaysOut) {
    TestPathGuard guard;
    BehaviorCloneTable src;
    bump(src, PlayerIntention::HEAL, 1);          // 单次证据
    ASSERT_TRUE(mirror::MirrorMemoryStore::save_from(src));

    BehaviorCloneTable dst;
    EXPECT_FALSE(mirror::MirrorMemoryStore::load_into(dst));  // 1×0.99→0
    EXPECT_EQ(dst.entries(), 0u);
}

TEST(MirrorMemoryStore, MissingAndCorruptFilesAreSafe) {
    TestPathGuard guard;
    BehaviorCloneTable dst;
    std::remove(kTestPath);
    EXPECT_FALSE(mirror::MirrorMemoryStore::load_into(dst));

    FILE* f = fopen(kTestPath, "wb");             // 垃圾内容不得崩溃
    ASSERT_NE(f, nullptr);
    fputs("{not json", f);
    fclose(f);
    EXPECT_FALSE(mirror::MirrorMemoryStore::load_into(dst));
    EXPECT_EQ(dst.entries(), 0u);
}

// G22d: 成功路径不得留下永不读取的 .tmp 暂存残留
TEST(MirrorMemoryStore, SaveSucceedsAndLeavesNoTmpResidue) {
    TestPathGuard guard;
    BehaviorCloneTable src;
    bump(src, PlayerIntention::HEAL, 10);
    ASSERT_TRUE(mirror::MirrorMemoryStore::save_from(src));
    EXPECT_TRUE(std::filesystem::exists(kTestPath));
    EXPECT_FALSE(std::filesystem::exists(kTmpPath));
}

// G22d: 伪原子替换失败时返回 false 并清理 .tmp
// 注入方式: 把目标路径做成目录, 文件不得替换目录, std::rename 必失败
TEST(MirrorMemoryStore, RenameFailureReturnsFalseAndCleansTmp) {
    TestPathGuard guard;
    std::remove(kTestPath);
    ASSERT_TRUE(std::filesystem::create_directory(kTestPath));

    BehaviorCloneTable src;
    bump(src, PlayerIntention::HEAL, 10);
    EXPECT_FALSE(mirror::MirrorMemoryStore::save_from(src));
    EXPECT_FALSE(std::filesystem::exists(kTmpPath));   // 残留已清理

    // 正式档未被半成品覆盖: 目录还在, 说明 rename 没有发生
    EXPECT_TRUE(std::filesystem::is_directory(kTestPath));
    std::filesystem::remove(kTestPath);
}
