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
//   当前实际值 (P3-redesign 卡死系统性修复后, 见下方基线更新):
//   run0 floor=9  kills=40  dmg=2254 weapon=crossbow_legendary
//   run1 floor=9  kills=37  dmg=1155 weapon=spear_common
//   run2 floor=3  kills=9   dmg=78  weapon=dagger_common
//   run3 floor=13 kills=61  dmg=3745 weapon=sword_common
//   (历史值 A15-Guard 时代曾为 run1 spear_legendary / run2 floor5 / run3 floor2, 已漂移)
// ⚠ 注: `--sim-build none` 实际会跑成"1 局指定 + 3 局随机 build"
//   (SimRunner 的 all_builds/随机分支行为), 故 4 局 build 不同 —
//   这不影响确定性, 但意味着改 build 相关逻辑会让前 3 局结果变化。
// ═ P3-redesign 卡死系统性修复基线更新 (2026-11-28) ═
//   5 个 per-change 改动 (standstill/loot-suppress/progress-tighten/hunt-lock/numeric)
//   协同: 清怪优先 + sim 数值/预算放宽。玩法显著改变 — 4 局全部漂移:
//   floor [2,3,4,4]→[9,9,3,13], kills/dmg 大幅上升 (清怪不再卡死, 能推进深层)。
//   20-30 局实测 avg_floor 4.2→8~9.5, F5 Boss 击杀 3→25, F10 首次可击杀。
//   归因: 推测 loot-suppress/progress-tighten 为主贡献 (env SIM_RD_*=0 可逐个关闭复现 A/B 复核)。
//   注: 中途曾采集 [8,8,3,12] 基线, 后修正 SIM_CURVES 第5-11列 (exp_scale 等恢复原表) 重采为当前值。
//   确定性: GOLDEN-003 同 seed 一致; 所有改动在 sim_redesign_active 开关内, 真实游戏不变。
// ═ P3-redesign 真实方位角基线更新 (2026-11-28) ═
//   WeaponExecutor 命中判定在 sim 下传玩家→最近怪的真实 atan2 方位角,
//   修正 _face_nearest_target 四方向量化造成的 30-60° 偏差 (命中瞬间诊断:
//   probe 空挥率 91.8%→17.4%, 方位角 30-60° 集中→<30°).
//   玩法有意改变: 近战 run1/2/3 的 hits/dmg 漂移 (run0 弩走弹体不受影响, 保持不变)。
//   归因验证: 该改动仅 sim_hit_aim_enabled 开启时生效, 真实游戏走默认四向逐字节不变;
//   GOLDEN-003 同 seed 确定性仍 PASS。
//   当前实测值: run2 kills 16→15, dmg run1 127→84 / run2 51→72 / run3 291→331。
// ═ P6 清怪模式基线更新 (2026-11-19) ═
//   CombatFocusTracker: 僵持≥8s 无怪掉血 → 临时禁 loot/room 评分强制追怪。
//   玩法有意改变: 清怪尾段不再被 loot/room 压制, run1/2/3 kills/dmg 漂移,
//   卡死率 254→239 (300 局)。run0 未触及僵持分支, 保持不变。
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

    const int    base_floor[] = {9, 9, 3, 13};
    const int    base_kills[] = {40, 37, 10, 61};
    const long   base_dmg[]   = {2254, 1155, 78, 3745};
    const char*  base_weapon[] = {"crossbow_legendary", "spear_common",
                                  "dagger_common", "sword_common"};

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
