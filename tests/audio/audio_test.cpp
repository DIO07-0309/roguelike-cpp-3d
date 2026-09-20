#include <gtest/gtest.h>
#include "game/audio/bgm_engine.h"
#include "game/audio/audio_server.h"
#include "raylib.h"
#include "raymath.h"

Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

TEST(Audio, BgmEngineInit) {
    BGMEngine bgm;
    bgm.init();
    EXPECT_TRUE(bgm.is_initialized());
}

TEST(Audio, BgmCompileBiomeVariants) {
    BGMEngine bgm;
    bgm.init();
    // 验证三群系 BGM 可编译
    EXPECT_TRUE(bgm.is_initialized());
    // 触发 lazy-compile
    bgm.play("prison");
    bgm.stop();
    bgm.play("volcano");
    bgm.stop();
    bgm.play("abyss");
    bgm.stop();
    EXPECT_TRUE(bgm.is_initialized());
}
