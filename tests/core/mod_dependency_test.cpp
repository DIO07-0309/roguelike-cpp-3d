#include <gtest/gtest.h>
#include <algorithm>
#include "core/mod_dependency.h"

// G13: DependencyResolver 拓扑排序/循环检测行为回归
// (重构前该逻辑无任何覆盖, 拆函数后必须有锁)
static int pos(const DepResult& r, const char* id) {
    for (int i = 0; i < (int)r.ordered_ids.size(); i++)
        if (r.ordered_ids[i] == id) return i;
    return -1;
}

TEST(DependencyResolverTest, EmptyInput) {
    const DepResult r = DependencyResolver::resolve({});
    EXPECT_TRUE(r.ordered_ids.empty());
    EXPECT_TRUE(r.skipped.empty());
    EXPECT_TRUE(r.cycle_info.empty());
}

TEST(DependencyResolverTest, RequiresOrdersDependencyFirst) {
    ModDepInfo a{"a"};
    ModDepInfo b{"b", {"a"}};
    ModDepInfo c{"c", {"b"}};
    const DepResult r = DependencyResolver::resolve({a, b, c});
    EXPECT_EQ(pos(r, "a"), 0);
    EXPECT_GT(pos(r, "b"), pos(r, "a"));
    EXPECT_GT(pos(r, "c"), pos(r, "b"));
    EXPECT_TRUE(r.skipped.empty());
    EXPECT_TRUE(r.cycle_info.empty());
}

TEST(DependencyResolverTest, LoadAfterOrdersWithoutHardDep) {
    ModDepInfo a{"a"};
    ModDepInfo b{"b", {}, {"a"}};
    const DepResult r = DependencyResolver::resolve({b, a});   // 故意乱序输入
    EXPECT_EQ(pos(r, "a"), 0);
    EXPECT_EQ(pos(r, "b"), 1);
    EXPECT_TRUE(r.cycle_info.empty());
}

TEST(DependencyResolverTest, MissingDependencySkipped) {
    ModDepInfo a{"a"};
    ModDepInfo b{"b", {"ghost"}};
    const DepResult r = DependencyResolver::resolve({a, b});
    EXPECT_EQ(pos(r, "a"), 0);
    EXPECT_EQ(pos(r, "b"), -1);
    ASSERT_EQ(r.skipped.size(), 1u);
    EXPECT_EQ(r.skipped[0], "b");
    EXPECT_TRUE(r.cycle_info.empty());
}

TEST(DependencyResolverTest, CyclicDependencyDetected) {
    ModDepInfo a{"a", {"b"}};
    ModDepInfo b{"b", {"a"}};
    const DepResult r = DependencyResolver::resolve({a, b});
    EXPECT_TRUE(r.ordered_ids.empty());
    EXPECT_TRUE(r.skipped.empty());
    ASSERT_EQ(r.cycle_info.size(), 2u);
    const auto cyc = r.cycle_info;
    EXPECT_NE(std::find(cyc.begin(), cyc.end(), "a"), cyc.end());
    EXPECT_NE(std::find(cyc.begin(), cyc.end(), "b"), cyc.end());
}

TEST(DependencyResolverTest, SkippedDepDoesNotCreateFalseCycle) {
    // b 因缺失依赖被跳过; c 依赖 b — 边在建图阶段被跳过, 不得误判为循环
    ModDepInfo b{"b", {"ghost"}};
    ModDepInfo c{"c", {"b"}};
    const DepResult r = DependencyResolver::resolve({b, c});
    EXPECT_TRUE(r.cycle_info.empty());
    EXPECT_EQ(pos(r, "b"), -1);
}
