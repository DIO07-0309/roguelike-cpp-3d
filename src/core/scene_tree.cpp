#include "scene_tree.h"
#include "logger.h"
#include "win_center.h"
#include "audio_server.h"
#include <algorithm>
#include <cstring>
#include <cmath>

// M6-i.1: --hidwin 窗口移出屏幕 (后台取证; 定义于此以便测试目标也能链接)
bool g_window_hidden = false;
bool g_autocontinue = false;
int  g_goto_floor = 0;

SceneTree::SceneTree(int w, int h, const char* title) {
    // G10.9-fix2: 放弃 FLAG_WINDOW_HIGHDPI
    // 探针实证 (tools/dpi_probe v4-v7): raylib5.0+GLFW+HIGHDPI 下窗口/FBO/绘图
    // 三空间互不同步 (窗口物理 960x640, FBO 1440x960, TakeScreenshot 2160x1440,
    // GetScreenWidth 恒报 960x640 旧值) — blit 矩形无法正确计算, 只见左上。
    // 方案 B: 不开 HIGHDPI (DPI-unaware 窗口, OS 位图拉伸保证完整可见) +
    // RESIZABLE + letterbox blit (dst 全程 GetScreen* 逻辑坐标, 自洽一致)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(w, h, title);
    // DPI-unaware + 系统缩放 150%: SetWindowSize(1440,960) 系统坐标 → OS 拉伸
    // 到物理 2160x1440 显示; 960x640 逻辑纹理 blit 到 1440x960 绘图空间 (1.5x)
    {
        Vector2 dpi = GetWindowScaleDPI();
        if (dpi.x > 1.01f) SetWindowSize((int)(w * dpi.x), (int)(h * dpi.y));
    }
    SetWindowMinSize(w / 2, h / 2);
    // G10.7-B1: 窗口图标 — 与 exe 图标同源 (剑与门, assets/brand/roguelike.png)
    {
        Image icon = LoadImage("assets/brand/roguelike.png");
        if (icon.width > 0) {
            SetWindowIcon(icon);
            UnloadImage(icon);
        }
    }
    InitAudioDevice();
    SetExitKey(0);
    center_active_window();
    // M6-i.1: 取证静默 — 仅移出屏幕 (不最小化: 最小化会触发 OS 帧节流,
    // autoshot 帧计数 3600f 在 1-5fps 下永远到不了)
    if (g_window_hidden) {
        SetWindowPosition(-32000, -32000);
    }
    SetTargetFPS(60);
    _input.setup_defaults();
    _audio = std::make_unique<AudioServer>();
    _audio->init();
    _target = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT);
    // G10.9: blit 放大用双线性, 高分屏上文字平滑而非锯齿块
    SetTextureFilter(_target.texture, TEXTURE_FILTER_BILINEAR);
    _running = true;
}

SceneTree::~SceneTree() {
    if (_root) { _root->_propagate_exit_tree(); _root.reset(); }
    UnloadRenderTexture(_target);
    CloseWindow();
}

void SceneTree::change_scene(std::shared_ptr<Node> new_scene) {
    LOG_INFO("场景切换 -> %s", new_scene->name.c_str());
    // BGM 由场景自身声明 (组合优于硬编码映射表)
    const char* bgm = new_scene->get_bgm_name();
    if (bgm) {
        float vol = 0.38f;
        if (strcmp(bgm, "title") == 0) vol = 0.35f;
        else if (strcmp(bgm, "boss") == 0) vol = 0.45f;
        _audio->play_bgm(bgm, vol);
    }
    _pending_scene = new_scene;
    _scene_changed = true;
}

void SceneTree::process_frame(double delta) {
    if (_scene_changed && _pending_scene) {
        if (_root) _root->_propagate_exit_tree();
        _root = _pending_scene;
        _current_scene = _pending_scene.get();
        _root->_set_tree(this);
        _root->_propagate_enter_tree();
        _root->_propagate_ready();
        _pending_scene = nullptr;
        _scene_changed = false;
        _skip_input = 5;  // 跳过5帧防Esc穿透 (SetExitKey(0) 已彻底禁用默认Esc退出)
    }

    if (_root) {
        auto& children = _root->get_children();
        children.erase(std::remove_if(children.begin(), children.end(),
            [](auto& c) { return c->_queued_free; }), children.end());
    }

    if (_root) _root->_propagate_process(delta);
    _audio->update((float)delta);  // Q4.2: BGM 循环重播检测
    _time += delta;
}

// G10.9-fix: letterbox blit 矩形 — 全程逻辑坐标空间
// (HIGHDPI 下 raylib 绘图空间是逻辑 screen 坐标: 窗口 960x640 / 全屏 1024x768 均由
//  GetScreenWidth 返回; GetMonitorWidth 返回物理像素, 混用会溢出绘图空间 → 只见左上)
Rectangle SceneTree::_blit_dst(Rectangle window_rect, bool fullscreen) const {
    (void)fullscreen;  // 统一走 GetScreen* — raylib 全屏时同样反映当前绘图空间
    float vw = (float)GetScreenWidth();
    float vh = (float)GetScreenHeight();
    float scale = fminf(vw / WINDOW_WIDTH, vh / WINDOW_HEIGHT);
    float dw = WINDOW_WIDTH * scale, dh = WINDOW_HEIGHT * scale;
    return {(vw - dw) / 2, (vh - dh) / 2, dw, dh};
}

// G10.9: 鼠标物理坐标 → 960×640 逻辑坐标
// (GetMousePosition 已是逻辑坐标, 再经 blit 矩阵逆映射到游戏缓冲)
Vector2 SceneTree::get_mouse_logical() const {
    Vector2 raw = GetMousePosition();
    Rectangle dst = _blit_dst({}, IsWindowFullscreen());
    if (dst.width <= 0 || dst.height <= 0) return raw;
    float lx = (raw.x - dst.x) * (WINDOW_WIDTH / dst.width);
    float ly = (raw.y - dst.y) * (WINDOW_HEIGHT / dst.height);
    return {lx, ly};
}

void SceneTree::_handle_input() {
    if (_skip_input > 0) { _skip_input--; return; }
    if (_input.is_action_just_pressed("fullscreen")) ToggleFullscreen();
    if (_current_scene) _current_scene->_input(_input);
}

void SceneTree::run() {
    double last = GetTime();
    // P1-C9 诊断: 输入心跳 (每2秒) — GLFW 原始按键队列/焦点/鼠标三态
    // 定位 "进层后键盘失灵": 区分 (a)窗口失焦 (b)键盘消息不达 (c)状态卡死
    double input_diag_timer = 0.0;
    long input_diag_frames = 0;
    while (_running && !WindowShouldClose()) {
        double now = GetTime();
        double dt = now - last;
        last = now;
        if (dt > 0.1) dt = 0.1;
        _handle_input();
        process_frame(dt);

        // ── P1-C9: 输入心跳诊断 (--input-diag 时每2秒; 区分失焦/消息不达/状态卡死) ──
        if (_input_diag) {
            input_diag_frames++;
            input_diag_timer += dt;
            if (input_diag_timer >= 2.0) {
                int drained = 0;
                while (GetKeyPressed() != 0) drained++;   // 排空 GLFW 按键队列
                int mx = GetMouseX(), my = GetMouseY();
                LOG_INFO("[INPUT-DIAG] f=%lld keys=%d focus=%d cursor=%d mouse=(%d,%d)",
                         (long long)input_diag_frames, drained,
                         (int)IsWindowFocused(), (int)IsCursorOnScreen(), mx, my);
                input_diag_timer = 0.0;
                input_diag_frames = 0;
            }
        }

        // Batch 3H + G10.9: 渲染到 960×640 逻辑缓冲, blit 到客户区
        // letterbox 保持 3:2 (消除任意拉伸变形), BILINEAR 平滑放大
        BeginTextureMode(_target);
        ClearBackground(BLACK);
        if (_root) _root->_render();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        // Source: entire texture (flipped vertically because OpenGL)
        Rectangle src = {0, 0, (float)_target.texture.width, -(float)_target.texture.height};
        Rectangle dst = _blit_dst({}, IsWindowFullscreen());
        DrawTexturePro(_target.texture, src, dst, {0, 0}, 0.0f, WHITE);
        // M6-i.1: 自动截图取证 (--autoshot N — 第 N 帧把主 RT 导出 PNG;
        // 绝对路径 saves/ 由 SaveManager mkdir 保证存在, 不依赖进程 CWD)
        extern int g_hd2d_autoshot;
        if (g_hd2d_autoshot > 0 && --g_hd2d_autoshot == 0) {
            Image shot = LoadImageFromTexture(_target.texture);
            ExportImage(shot, "saves/screenshot.png");
            UnloadImage(shot);
            // M6-n: 取证帧同时记录 FPS (性能回归基线; --autoshot 专用)
            LOG_INFO("[autoshot] 主 RT 已导出 -> saves/screenshot.png (%dx%d) FPS=%d",
                     _target.texture.width, _target.texture.height, GetFPS());
        }
        EndDrawing();
    }
}
