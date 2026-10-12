#include "game/director/theme_preview.h"
#include "core/logger.h"
#include "raylib.h"   // GetFrameTime (预览节奏; 纯表现层, 不走注入 dt)

// ═══════════════════════════════════════════════════════════════
// G41: 取证/预览全局量 — 定义放在本 TU 而非 main.cpp
//
// ★ 不能放 main.cpp: 测试目标 roguelike_lib 用 GLOB 收齐 src/**/*.cpp
//   但显式排除 main.cpp。定义放在那里会让所有链接 roguelike_lib 的
//   测试目标报 undefined reference (本次实测: hd2d_scene_builder_test)。
//   放本文件则主程序与测试两条链接路径都能解析。
// ═══════════════════════════════════════════════════════════════

// --theme-preview 开关 (main.cpp 解析命令行后写入)
bool g_theme_preview = false;

// autoshot 输出路径覆盖; 空 = "saves/screenshot.png" (原行为, 向后兼容)
std::string g_autoshot_path;

// --anim-time <秒>: 钉死渲染动画时间 (<0 = 不覆写, 走墙钟)
// G43: 渲染路径 13 处动画 (熔岩/传送门/特效脉冲/动画帧) 原先直调 GetTime(),
// 使每次运行截图都不同 → 渲染改动无法逐字节回归。覆写后截图可复现。
float g_anim_time_override = -1.0f;

namespace {

// 预览覆盖全部 12 个 BuildType (不含 NONE — 未成型主题=默认狂战, 单独说明)
const BuildType kBuilds[] = {
    BuildType::BERSERKER,       BuildType::FIRE_MAGE,      BuildType::POISON_MASTER,
    BuildType::TIME_MASTER,     BuildType::SUPPORT,        BuildType::PROJECTILE,
    BuildType::ICE_MAGE,        BuildType::LIGHTNING_MAGE, BuildType::BLEED_BLADE,
    BuildType::SHADOW_STRIKER,  BuildType::JUGGERNAUT,     BuildType::SUMMON_LORD,
};
constexpr int kBuildCount = (int)(sizeof(kBuilds) / sizeof(kBuilds[0]));

}  // namespace

void ThemePreviewController::tick(
                                  const std::function<void(BuildType, int)>& on_switch) {
    if (!_enabled || _finished) return;

    // 自行取帧时: 预览是纯表现层节奏, 不参与 gameplay 模拟,
    // 故不走注入的 dt — 也省得调用方为了传一个 dt 把 _process 撑过 40 行
    float dt = GetFrameTime();

    if (_first_frame) {          // 首帧立即拍第一张, 不等停留时间
        _first_frame = false;
        _timer = 0.0f;
        on_switch(kBuilds[_index], _index);
        return;
    }

    _timer += dt;
    if (_timer < kHoldSeconds) return;
    _timer = 0.0f;

    _index++;
    if (_index >= kBuildCount) { _finished = true; return; }
    on_switch(kBuilds[_index], _index);
}
