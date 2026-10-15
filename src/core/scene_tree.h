#pragma once
#include <memory>
#include <vector>
#include "raylib.h"
#include "node.h"
#include "input_map.h"
#include "config.h"

class AudioServer;
class VFXServer;

class SceneTree {
public:
    SceneTree(int width, int height, const char* title);
    ~SceneTree();

    void run();
    void change_scene(std::shared_ptr<Node> new_scene);
    Node* get_current_scene() const { return _current_scene; }
    void quit() { _running = false; }

    InputMap& get_input() { return _input; }
    AudioServer* get_audio() { return _audio.get(); }

    // P1-C9: 输入诊断开关 (--input-diag 启动参数; 默认关, 零日志噪声)
    void set_input_diag(bool on) { _input_diag = on; }
    bool is_input_diag() const { return _input_diag; }

    // 逻辑分辨率 (始终 960×640，全屏/缩放时由 RenderTexture blit 保持比例)
    int width()  const { return WINDOW_WIDTH; }
    int height() const { return WINDOW_HEIGHT; }
    int get_width()  const { return width(); }
    int get_height() const { return height(); }

    // G10.9: 鼠标物理坐标 → 960×640 逻辑坐标 (经 blit 矩阵逆映射)
    Vector2 get_mouse_logical() const;

    void process_frame(double delta);
    bool is_running() const { return _running; }  // Q3.1: headless sim 循环
    void process_input() { _handle_input(); }    // Q3.1: headless sim 输入分发

    // M6-v2c: 主渲染 RT 只读访问 (HD2D 后处理链嵌套 FBO 恢复用;
    // raylib 5.0 EndTextureMode 会盲绑 FBO 0, 嵌套 pass 后需手动恢复)
    const RenderTexture2D& main_target() const { return _target; }

private:
    void _handle_input();
    // G53: run 拆出的三段帧后处理
    void _input_diag_tick(float dt);
    void _render_to_target();
    void _maybe_export_frame();
    void _blit_to_screen();
    // G53: 构造函数拆出的窗口图标设置
    void _set_window_icon();
    // G53: 输入心跳诊断的累计状态 (原为 run 的局部变量, 提升为成员才能跨帧累计)
    float _input_diag_timer = 0.0f;
    long  _input_diag_frames = 0;

    // G10.9: 当前 blit 目标矩形 (窗口客户区 letterbox 矩形, 鼠标逆映射共用)
    Rectangle _blit_dst(Rectangle window_rect, bool fullscreen) const;

    bool _running = false;
    double _time = 0.0;

    InputMap _input;
    std::shared_ptr<Node> _root;
    Node* _current_scene = nullptr;

    std::unique_ptr<AudioServer> _audio;

    RenderTexture2D _target = {0};  // Batch 3H: 960×640 render target

    std::shared_ptr<Node> _pending_scene;
    bool _scene_changed = false;
    int  _skip_input = 0;  // 场景切换后跳过N帧输入(防Esc穿透)
    bool _input_diag = false;  // P1-C9: --input-diag 输入心跳诊断
};
