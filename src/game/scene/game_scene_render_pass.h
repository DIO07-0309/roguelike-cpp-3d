#pragma once

class GameScene;

// 从 GameScene::_render 提取的全屏状态绘制模块 (G12-2)
// 组合而非继承: 持有 GameScene& , 通过 friend 读取/绘制私有状态
class GameSceneRenderPass {
public:
    explicit GameSceneRenderPass(GameScene& scene) : _s(scene) {}

    // G10.1: 元素核心选择界面 (整屏覆盖)
    // 返回 true 表示本帧已由该界面处理, 调用方应直接 return
    bool draw_element_select(int sw, int sh);

    // BOSS_INTRO: Boss 出场介绍屏 (整屏覆盖)
    // 返回 true 表示本帧已由该界面处理, 调用方应直接 return
    bool draw_boss_intro_screen(int sw, int sh);

private:
    // G10.1: 单张元素核心卡片
    void draw_element_card(int index, float cx, float cy,
                           float card_w, float card_h, bool selected);
    // G10.1: 卡片内换行描述文字 (13px 居中)
    void draw_card_description(const char* text, float cx, float top_y, float card_w);

    GameScene& _s;
};
