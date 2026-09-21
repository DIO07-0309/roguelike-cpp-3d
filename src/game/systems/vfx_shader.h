#pragma once
#include "raylib.h"

// ============================================================
// VFXShader - 特效 Shader（发光/模糊/扭曲）
// ============================================================

enum class VFXType {
    NONE,
    BLOOM,      // 发光
    MOTION_BLUR,// 模糊
    SCREEN_WARP // 扭曲
};

class VFXShader {
public:
    // 初始化
    static void init();
    
    // 设置当前效果
    static void set_type(VFXType type);
    
    // 启用/禁用
    static void enable(bool enabled);
    
    // 是否活跃
    static bool is_active();
    
    // 获取当前类型
    static VFXType current_type();
    
    // 应用效果（在绘制场景后调用）
    static void apply(RenderTexture2D target, int screen_w, int screen_h);
    
private:
    static Shader s_bloom_shader;
    static Shader s_blur_shader;
    static Shader s_warp_shader;
    static VFXType s_current_type;
    static bool s_enabled;
};
