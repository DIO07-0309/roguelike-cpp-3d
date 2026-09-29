#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "raylib.h"
#include "game/rendering/sprite_renderer.h"   // M4f.4: SpriteDef

// ============================================================
// D7 Step4: ResourceManager — 统一资源管理层
// 所有 Font/JSON/Sound 加载/缓存/释放集中于此
// ============================================================

// G10.2-B1: Asset Manifest 条目 (schema v2 "assets" 段, 类别化)
// 六要素: Source/ID/Path/RenderRule/PixelScale/Fallback → 字段映射
struct AssetDef {
    std::string path;             // Asset Path
    std::string source;           // Asset Source (kenney_* / self / procedural)
    std::string fallback;         // Fallback Policy ("procedural" / "synth:key" / "" )
    int  frame_w = 16, frame_h = 16;   // Pixel Scale 基数 (16 → 2×=32)
    std::string overlay;          // Render Rule: 色罩 "#RRGGBB" (空=无, D3)
    float overlay_alpha = 0.0f;
};

class ResourceManager {
public:
    // ── 单例 ──
    static ResourceManager& inst();

    // ── 生命周期 ──
    void load_all();
    void unload_all();

    // ── Font ──
    Font& font(int size = 32);          // 返回主字体 (32px)
    Font& font_small();                 // 小字体 (18px)
    bool  font_loaded() const { return _font_ok; }

    // ── JSON (返回文件内容字符串, 由各系统自行解析) ──
    std::string load_json_text(const char* path);

    // ── Sound (文件路径 → Sound) ──
    Sound load_sound(const char* path); // MP3/WAV, 自动缓存

    // ── M4f: Texture (文件路径 → Texture, 自动缓存; 失败返回 {0}) ──
    Texture2D load_texture(const char* path);
    // 程序化像素纹理缓存 (key 唯一, 如 "wall_1a2a2e"); 同 key 复用
    Texture2D procedural_tile(const char* key, Color base, bool wall);
    // M6-i: 群系风格化程序材质缓存 (key 如 "biome_prison_wall_1a2a2e")
    // style 见 SpriteRenderer::BiomeStyle; accent = 群系特征色
    Texture2D procedural_biome_tile(const char* key, Color base, Color accent,
                                    int style, bool wall);
    // M6-j: 程序化地板装饰片缓存 (kind 0-3; 坐标哈希消费)
    Texture2D procedural_floor_decal(const char* key, int kind,
                                     Color primary, Color secondary);
    // M4f.2: 程序化角色/怪物占位精灵缓存
    Texture2D procedural_sprite(const char* key, Color body, Color accent,
                                int variant, int eye_dir = 0);
    // M4f.2: 程序化 VFX 爆点缓存
    Texture2D procedural_fx(const char* key, Color c);
    // M6-k: 程序化 Arena 物件缓存 (type 0-4; 零 RNG)
    Texture2D procedural_arena_prop(const char* key, int type, Color base);

    // ── M4f.4: 数据驱动精灵 (resources/sprites.json → 文件纹理) ──
    bool load_sprite_config();
    // 命中 sprites.json 返回纹理并填出参 def; 未命中返回 {0} (调用方回退程序化)
    Texture2D sprite_by_key(const char* key, SpriteDef& out_def);

    // ── G10.2-B1: Asset Manifest (schema v2 "assets" 段, 类别化) ──
    // Asset ID = "<category>.<name>" (如 door.closed); Asset Path 只允许存在于清单。
    // 解析 "assets" 段 (类别化: doors/sprites/audio/fonts); 幂等
    bool load_assets_config();
    // ID → 定义 (未装载/未命中返回 nullptr; 纯解析层, 不触发纹理加载 — headless 可测)
    const AssetDef* asset_by_id(const char* id);
    // ID → 纹理 (asset_by_id + load_texture; B2 起供渲染器迁移使用)
    Texture2D tex_by_id(const char* id);

private:
    ResourceManager() = default;
    ~ResourceManager();

    void _init_font();
    void _free_font();

    Font  _font = {0}, _font_small_{0};
    bool  _font_ok = false;
    bool  _loaded = false;

    // JSON 缓存 (path → content)
    std::unordered_map<std::string, std::string> _json_cache;

    // Sound 缓存 (path → Sound)
    std::unordered_map<std::string, Sound> _sound_cache;
    // M4f: 纹理缓存 (path/key → Texture)
    std::unordered_map<std::string, Texture2D> _texture_cache;
    // M4f.4: 数据驱动精灵定义 (key → SpriteDef)
    std::unordered_map<std::string, SpriteDef> _sprite_defs;
    bool _sprite_cfg_ok = false;
    // G10.2-B1: Asset Manifest (ID → AssetDef)
    std::unordered_map<std::string, AssetDef> _asset_defs;
    bool _assets_cfg_ok = false;
};
