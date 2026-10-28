// ═══════════════════════════════════════════════════════════════
// G45: 黄金基线回归 — 把核心玩法结果钉死, 行为漂移立刻被抓
//
// 动机: 809 个既有测试全部是单元/模块级契约, 集成层只有 2 个冒烟测试
// (且只验证"不崩 + 时钟推进")。G44 拆 main 时漏掉 20+ 行初始化, 症状是
// "游戏启动后首帧前静默退出" — 这类**静默行为改变**单元测试抓不到,
// 冒烟测试也抓不到 (它还真跑了 60 tick 不崩)。
//
// 实现方式: 调用 sim CLI 跑固定 seed 的对局, 读 balance_report.json。
//   · sim 完全确定 (同 seed 两次运行 report 哈希一致, 已验证)
//   · 走真实主循环 (含 SceneTree 驱动/看门狗/结算), 不桩任何东西 —
//     进程内方案的尝试记录见下方注释 (缺 get_tree 导致永不结算)
//   · 代价: ~1s/次, 可接受 (回归测试本来就该慢)
//
// ⚠ 维护约定: 故意修改玩法后基线值必须同步更新, 且 commit message 必须
// 说明"玩法为什么变" — 不允许悄悄改数让测试变绿。
// ═══════════════════════════════════════════════════════════════
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {

// 跑 N 局固定 seed 的 sim, 返回解析后的 report
nlohmann::json run_sim_report(int runs, unsigned int seed) {
    // sim CLI 只写固定路径 reports/balance_report.json (无 --report 参数)。
    // 先删旧文件再跑 — 否则若本次跑失败, 会读到上一次的残留而假绿。
    std::remove("reports/balance_report.json");
    std::string cmd = "build\\roguelike_cpp.exe --sim " + std::to_string(runs)
                    + " --sim-seed " + std::to_string(seed)
                    + " --sim-build none";
    const int rc = std::system(cmd.c_str());
    EXPECT_EQ(rc, 0) << "sim CLI 退出码非 0: " << cmd;

    std::ifstream f("reports/balance_report.json");
    if (!f.is_open()) return nlohmann::json::object();
    nlohmann::json j;
    try { f >> j; } catch (...) { return nlohmann::json::object(); }
    return j;
}

// 从 report 里取出第 i 局的字段 (顺序即 RNG 消耗顺序, 必须确定)
const nlohmann::json* run_at(const nlohmann::json& report, size_t i) {
    if (!report.contains("runs") || !report["runs"].is_array()) return nullptr;
    if (i >= report["runs"].size()) return nullptr;
    return &report["runs"][i];
}

}  // namespace

// ── GOLDEN-001: 固定 seed 的战斗结果必须与基线逐字段一致 ──
// 覆盖: 地牢生成 / 刷怪 / 战斗判定 / 掉落 / 楼层推进 / 结算
//
// 基线采集: `roguelike_cpp.exe --sim 4 --sim-seed 4242 --sim-build none`
//   run0 floor=2  kills=8   dmg=183 turns=10533 weapon=crossbow_rare
//   run1 floor=4  kills=14  dmg=196 turns=2     weapon=spear_legendary
//   run2 floor=5  kills=18  dmg=145 turns=323   weapon=dagger_rare
//   run3 floor=2  kills=4   dmg=45  turns=6478  weapon=sword_common
// ⚠ 注: `--sim-build none` 实际会跑成"1 局指定 + 3 局随机 build"
//   (SimRunner 的 all_builds/随机分支行为), 故 4 局 build 不同 —
//   这不影响确定性, 但意味着改 build 相关逻辑会让前 3 局结果变化。
// ═ A15-Guard 基线更新 (2026-10-28) ═
//   F1 新增守卫互动 NPC (可对话), spawn_floor_npcs 在 sim 主循环同样执行,
//   守卫占位使固定 seed 的随机 build 局 (run1-3) 结果漂移 — 玩法被有意改变。
//   归因验证: 临时移除守卫配置后 seed 4242 完全恢复旧基线 (run1=4/14/196/spear,
//   run2=5/18/145/dagger, run3=2/4/45/sword), 故确系守卫接入所致, 非环境漂移。
//   run0 (指定 build 局) 不受影响。
TEST(GoldenBaseline, FixedSeedOutcomeIsMatchesBaseline) {
    const nlohmann::json r = run_sim_report(4, 4242u);
    ASSERT_TRUE(r.contains("runs")) << "report 缺少 runs 字段";
    ASSERT_GE(r["runs"].size(), 4u) << "sim 未产出 4 局";

    const int    base_floor[] = {2, 3, 4, 4};
    const int    base_kills[] = {8, 13, 15, 14};
    const long   base_dmg[]   = {183, 59, 72, 363};
    const char*  base_weapon[] = {"crossbow_rare", "sword_common",
                                  "dagger_rare", "sword_common"};

    for (size_t i = 0; i < 4u; ++i) {
        const nlohmann::json* run = run_at(r, i);
        ASSERT_NE(run, nullptr) << "第 " << i << " 局缺失";
        EXPECT_EQ(run->value("floor", -1), base_floor[i])
            << "第 " << i << " 局 floor 与基线不符 — 玩法流程被改变";
        EXPECT_EQ(run->value("kills", -1), base_kills[i])
            << "第 " << i << " 局 kills 与基线不符 — 战斗/刷怪被改变";
        EXPECT_EQ((long)run->value("damage_dealt", -1L), base_dmg[i])
            << "第 " << i << " 局 damage_dealt 与基线不符 — 伤害公式被改变";
        EXPECT_EQ(run->value("weapon", ""), std::string(base_weapon[i]))
            << "第 " << i << " 局 weapon 与基线不符 — 掉落/装备链被改变";
    }
}

// ── GOLDEN-002: 换 seed 必须得到不同结果 (证明基线不是恒真) ──
// 没有这条, GOLDEN-001 可能因为"所有输入都返回同值"而假绿 —
// G44 事故正是"静默无输出"型缺陷, 恒真测试对它零防御
TEST(GoldenBaseline, DifferentSeedChangesOutcome) {
    const nlohmann::json a = run_sim_report(4, 1u);
    const nlohmann::json b = run_sim_report(4, 777777u);
    ASSERT_GE(a["runs"].size(), 1u);
    ASSERT_GE(b["runs"].size(), 1u);

    bool any_diff = false;
    for (size_t i = 0; i < a["runs"].size() && i < b["runs"].size(); ++i) {
        const nlohmann::json& ra = a["runs"][i];
        const nlohmann::json& rb = b["runs"][i];
        if (ra.value("floor", -1) != rb.value("floor", -2)
            || ra.value("kills", -1) != rb.value("kills", -2)
            || ra.value("damage_dealt", -1L) != rb.value("damage_dealt", -2L)) {
            any_diff = true;
            break;
        }
    }
    EXPECT_TRUE(any_diff)
        << "两个 seed 产出完全相同 — 基线测试退化为恒真, 失去回归能力";
}

// ── GOLDEN-003: 同 seed 两次运行必须一致 (确定性契约) ──
// 这是整个黄金基线的地基: 若 sim 不确定, 上面两条都无意义
TEST(GoldenBaseline, SameSeedIsDeterministic) {
    const nlohmann::json a = run_sim_report(3, 2024u);
    const nlohmann::json b = run_sim_report(3, 2024u);
    ASSERT_GE(a["runs"].size(), 1u);
    ASSERT_GE(b["runs"].size(), 1u);
    for (size_t i = 0; i < a["runs"].size() && i < b["runs"].size(); ++i) {
        EXPECT_EQ(a["runs"][i].value("floor", -1),
                  b["runs"][i].value("floor", -1))
            << "第 " << i << " 局 floor 不稳定 — sim 非确定";
        EXPECT_EQ(a["runs"][i].value("damage_dealt", -1L),
                  b["runs"][i].value("damage_dealt", -1L))
            << "第 " << i << " 局 damage_dealt 不稳定 — sim 非确定";
    }
}
