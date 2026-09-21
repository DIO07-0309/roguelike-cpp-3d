#include "vfx_shader.h"
#include "config.h"

Shader VFXShader::s_bloom_shader = {};
Shader VFXShader::s_blur_shader = {};
Shader VFXShader::s_warp_shader = {};
VFXType VFXShader::s_current_type = VFXType::NONE;
bool VFXShader::s_enabled = false;

void VFXShader::init() {
    // 加载 shader（如果存在）
    // 由于 shader 文件可能不存在，这里只做初始化标记
    // 实际 shader 加载需要顶点着色器和片段着色器文件
    // 当前使用程序绘制作为 fallback
}

void VFXShader::set_type(VFXType type) {
    s_current_type = type;
}

void VFXShader::enable(bool enabled) {
    s_enabled = enabled;
}

bool VFXShader::is_active() {
    return s_enabled && s_current_type != VFXType::NONE;
}

VFXType VFXShader::current_type() {
    return s_current_type;
}

void VFXShader::apply(RenderTexture2D target, int screen_w, int screen_h) {
    if (!is_active()) return;
    
    // 检查 shader 是否已加载
    Shader shader;
    switch (s_current_type) {
        case VFXType::BLOOM:
            shader = s_bloom_shader;
            break;
        case VFXType::MOTION_BLUR:
            shader = s_blur_shader;
            break;
        case VFXType::SCREEN_WARP:
            shader = s_warp_shader;
            break;
        default:
            return;
    }
    
    // 如果 shader 未加载（id == 0），使用 fallback 程序绘制
    if (shader.id == 0) {
        // 直接绘制纹理到屏幕（无 shader 效果）
        DrawTextureRec(target.texture, 
                       (Rectangle){0, 0, (float)target.texture.width, (float)target.texture.height},
                       (Vector2){0, 0}, WHITE);
        return;
    }
    
    // 应用 shader
    BeginShaderMode(shader);
        DrawTextureRec(target.texture, 
                       (Rectangle){0, 0, (float)target.texture.width, (float)target.texture.height},
                       (Vector2){0, 0}, WHITE);
    EndShaderMode();
}
