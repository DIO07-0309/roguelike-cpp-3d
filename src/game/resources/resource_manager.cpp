#include "resource_manager.h"
#include "core/logger.h"
#include "combat_system.h"
#include "game/rendering/sprite_renderer.h"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <set>
#ifdef FONT_USE_RUNTIME_SCAN
#  include <dirent.h>
#  include <sys/stat.h>
#endif

// ═══════════════════════════════════════════════════════════════
// 字体码位: 优先编译期生成的头文件 (release 模式)
// 回退到运行时扫描 (dev 模式, 定义 FONT_USE_RUNTIME_SCAN)
// ═══════════════════════════════════════════════════════════════

#if !defined(FONT_USE_RUNTIME_SCAN)
#  include "core/font_codepoints.h"
#else
// ── 运行时扫描 (仅 dev 模式, 新增中文后重新生成头文件) ──
static void _extract_codepoints(const char* s, std::set<int>& out) {
    while (*s) {
        unsigned char b = (unsigned char)*s;
        int cp = 0;
        if ((b & 0x80) == 0)        { cp = b; s++; }
        else if ((b & 0xE0) == 0xC0) { cp = (b&0x1F)<<6|(s[1]&0x3F); s+=2; }
        else if ((b & 0xF0) == 0xE0) { cp = (b&0x0F)<<12|(s[1]&0x3F)<<6|(s[2]&0x3F); s+=3; }
        else { s++; continue; }
        if (cp >= 0x2000) out.insert(cp);
    }
}

static void _scan_file(const char* path, std::set<int>& out) {
    FILE* f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf(sz + 1);
    size_t n = fread(buf.data(), 1, sz, f);
    fclose(f);
    if (n == 0 || n > sz) return;
    buf[n] = '\0';
    _extract_codepoints(buf.data(), out);
}

static void _scan_dir(const char* dir, const char* ext, std::set<int>& out) {
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        if (de->d_name[0] == '.') continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            _scan_dir(full, ext, out);
        } else {
            const char* e = strrchr(de->d_name, '.');
            if (e && strcmp(e, ext) == 0) _scan_file(full, out);
        }
    }
    closedir(d);
}

static int _build_font_cp(std::vector<int>& out) {
    for (int i = 0x20; i <= 0x7E; i++) out.push_back(i);
    static int _extras[] = {
        0x00B7, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026,
        0x2190, 0x2191, 0x2192, 0x2193, 0x2194, 0x2208, 0x2260, 0x2265,
        0x2500, 0x2550, 0x25A0, 0x25B2, 0x25B6, 0x25CB, 0x2605, 0x2620,
        0x2713, 0x2717, 0x2744, 0x3001, 0x3002, 0x3010, 0x3011,
        0xFEFF, 0xFF01, 0xFF08, 0xFF09, 0xFF0C, 0xFF1A, 0xFF1F,
    };
    for (int cp : _extras) out.push_back(cp);
    std::set<int> cjk;
    _scan_dir("src", ".cpp", cjk);
    _scan_dir("src", ".h", cjk);
    _scan_dir("resources", ".json", cjk);
    for (int cp : cjk) out.push_back(cp);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return (int)out.size();
}
#endif

// ═══════════════════════════════════════════════════════════════
// 单例
// ═══════════════════════════════════════════════════════════════
ResourceManager& ResourceManager::inst() {
    static ResourceManager rm;
    return rm;
}

ResourceManager::~ResourceManager() { unload_all(); }

void ResourceManager::load_all() {
    if (_loaded) return;
    _loaded = true;
    _init_font();
    load_sprite_config();   // M4f.4: 数据驱动精灵定义
    LOG_INFO("ResourceManager: fonts loaded");
}

void ResourceManager::unload_all() {
    if (!_loaded) return;   // 防重入: main 显式卸载 + 静态析构双调 (double-free 根因)
    _loaded = false;
    _free_font();
    for (auto& [_, s] : _sound_cache) UnloadSound(s);
    _sound_cache.clear();
    for (auto& [_, t] : _texture_cache) UnloadTexture(t);
    _texture_cache.clear();
    _json_cache.clear();
    _loaded = false;
}

void ResourceManager::_init_font() {
    // G10.9: 用户提供的思源黑体优先 (Bold 标题更清晰), 多级后备
    const char* font_paths[] = {
        "assets/fonts/source_hans_bold.otf",
        "assets/fonts/source_hans_regular.otf",
        "assets/fonts/game_regular.ttf",
        "assets/fonts/NotoSansCJKsc-Regular.otf",
        nullptr
    };

#ifdef FONT_USE_RUNTIME_SCAN
    static std::vector<int> cp_vec;
    static int cp_count = 0;
    static bool cp_built = false;
    if (!cp_built) {
        cp_count = _build_font_cp(cp_vec);
        cp_built = true;
        LOG_INFO("ResourceManager: 运行时扫描 %d 个码位", cp_count);
    }
    int* cp_data = cp_vec.data();
    int  cp_count_val = cp_count;
#else
    int* cp_data = const_cast<int*>(FONT_CP_DATA);
    const int  cp_count_val = FONT_CP_COUNT;
    LOG_INFO("ResourceManager: 编译期 %d 个码位", cp_count_val);
#endif

    for (int i = 0; font_paths[i]; i++) {
        if (!FileExists(font_paths[i])) { LOG_INFO("  跳过: %s", font_paths[i]); continue; }
        _font = LoadFontEx(font_paths[i], 40, cp_data, cp_count_val);
        _font_small_ = LoadFontEx(font_paths[i], 24, cp_data, cp_count_val);
        LOG_INFO("  尝试: %s -> 字形:%d/%d", font_paths[i], _font.glyphCount, cp_count_val);
        if (_font.texture.id > 0 && _font.glyphCount > 200) {
            _font_ok = true;
            SetTextureFilter(_font.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureFilter(_font_small_.texture, TEXTURE_FILTER_BILINEAR);
            LOG_INFO("字体: %s (atlas OK)", font_paths[i]);
            return;
        }
        UnloadFont(_font); UnloadFont(_font_small_);
    }
    _font = GetFontDefault(); _font_small_ = GetFontDefault(); _font_ok = false;
    LOG_WARN("无中文字体 — 英文界面");
}

void ResourceManager::_free_font() {
    if (_font.texture.id != GetFontDefault().texture.id && _font.texture.id > 0) {
        UnloadFont(_font); UnloadFont(_font_small_);
    }
}

Font& ResourceManager::font(int size) { return (size <= 20) ? _font_small_ : _font; }
Font& ResourceManager::font_small() { return _font_small_; }

// ═══════════════════════════════════════════════════════════════
// JSON / Sound cache (unchanged)
// ═══════════════════════════════════════════════════════════════

std::string ResourceManager::load_json_text(const char* path) {
    auto it = _json_cache.find(path);
    if (it != _json_cache.end()) return it->second;
    FILE* f = fopen(path, "rb");
    if (!f) { LOG_ERROR("Resource: cannot open %s", path); return ""; }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string buf(sz + 1, '\0');
    fread(&buf[0], 1, sz, f);
    fclose(f);
    _json_cache[path] = buf;
    return buf;
}

Sound ResourceManager::load_sound(const char* path) {
    auto it = _sound_cache.find(path);
    if (it != _sound_cache.end()) return it->second;
    if (!FileExists(path)) { Sound dummy = {0}; return dummy; }
    Sound s = LoadSound(path);
    _sound_cache[path] = s;
    return s;
}

// ═══════════════════════════════════════════════════════════════
// M4f: Texture 缓存 (文件加载 / 程序化生成)
// ═══════════════════════════════════════════════════════════════

Texture2D ResourceManager::load_texture(const char* path) {
    auto it = _texture_cache.find(path);
    if (it != _texture_cache.end()) return it->second;
    Texture2D tex = {0};
    if (FileExists(path)) tex = LoadTexture(path);
    if (tex.id > 0)
        _texture_cache[path] = tex;
    else
        LOG_WARN("Texture: 缺失 %s (回退占位)", path);
    return tex;
}

Texture2D ResourceManager::procedural_tile(const char* key, Color base,
                                            bool wall) {
    auto it = _texture_cache.find(key);
    if (it != _texture_cache.end()) return it->second;
    Texture2D tex = SpriteRenderer::gen_pixel_tile(base, wall);
    _texture_cache[key] = tex;
    return tex;
}

// M6-i: 群系风格化程序材质 (风格分歧在 SpriteRenderer; 这里只做缓存)
Texture2D ResourceManager::procedural_biome_tile(const char* key, Color base,
                                                 Color accent, int style,
                                                 bool wall) {
    auto it = _texture_cache.find(key);
    if (it != _texture_cache.end()) return it->second;
    auto s = (SpriteRenderer::BiomeStyle)style;
    Texture2D tex = SpriteRenderer::gen_biome_tile(base, accent, base, s, wall);
    _texture_cache[key] = tex;
    return tex;
}

// M6-j: 地板装饰片缓存
Texture2D ResourceManager::procedural_floor_decal(const char* key, int kind,
                                                  Color primary,
                                                  Color secondary) {
    auto it = _texture_cache.find(key);
    if (it != _texture_cache.end()) return it->second;
    Texture2D tex = SpriteRenderer::gen_floor_decal(kind, primary, secondary);
    _texture_cache[key] = tex;
    return tex;
}

Texture2D ResourceManager::procedural_sprite(const char* key, Color body,
                                             Color accent, int variant,
                                             int eye_dir) {
    auto it = _texture_cache.find(key);
    if (it != _texture_cache.end()) return it->second;
    Texture2D tex = SpriteRenderer::gen_pixel_sprite(body, accent, variant,
                                                      eye_dir);
    _texture_cache[key] = tex;
    return tex;
}

Texture2D ResourceManager::procedural_fx(const char* key, Color c) {
    auto it = _texture_cache.find(key);
    if (it != _texture_cache.end()) return it->second;
    Texture2D tex = SpriteRenderer::gen_pixel_blast(c);
    _texture_cache[key] = tex;
    return tex;
}

Texture2D ResourceManager::procedural_arena_prop(const char* key, int type, Color base) {
    auto it = _texture_cache.find(key);
    if (it != _texture_cache.end()) return it->second;
    Texture2D tex = SpriteRenderer::gen_arena_prop(type, base);
    _texture_cache[key] = tex;
    return tex;
}

// ═══════════════════════════════════════════════════════════════
// M4f.4: 数据驱动精灵 (resources/sprites.json)
// ═══════════════════════════════════════════════════════════════

bool ResourceManager::load_sprite_config() {
    std::string text = load_json_text("resources/sprites.json");
    if (text.empty()) { LOG_WARN("sprites.json 缺失, 使用程序化占位"); return false; }
    try {
        auto j = nlohmann::json::parse(text);
        for (auto& [key, def] : j.at("sprites").items()) {
            SpriteDef sd;
            sd.path = def.value("file", "");
            sd.frame_w = def.value("frame_w", 16);
            sd.frame_h = def.value("frame_h", 16);
            sd.frame_count = def.value("frame_count", 1);
            _sprite_defs[key] = sd;
        }
        _sprite_cfg_ok = true;
    } catch (const std::exception& e) {
        LOG_ERROR("sprites.json 解析失败: %s", e.what());
        return false;
    }
    LOG_INFO("sprites.json: %d 个精灵定义", (int)_sprite_defs.size());
    return _sprite_cfg_ok;
}

Texture2D ResourceManager::sprite_by_key(const char* key, SpriteDef& out_def) {
    auto it = _sprite_defs.find(key);
    if (it == _sprite_defs.end() || it->second.path.empty())
        return Texture2D{0};
    out_def = it->second;
    return load_texture(out_def.path.c_str());
}

// ── G10.2-B1: Asset Manifest (schema v2 "assets" 段) ─────────
bool ResourceManager::load_assets_config() {
    if (_assets_cfg_ok) return true;                 // 幂等
    std::string text = load_json_text("resources/sprites.json");
    if (text.empty()) { LOG_WARN("sprites.json 缺失, Asset Manifest 未装载"); return false; }
    try {
        auto j = nlohmann::json::parse(text);
        if (!j.contains("assets")) return false;
        for (auto& [cat, entries] : j["assets"].items())
            for (auto& [name, def] : entries.items()) {
                AssetDef ad;
                ad.path     = def.value("file", "");
                ad.source   = def.value("source", "");
                ad.fallback = def.value("fallback", "");
                if (def.contains("render")) {
                    auto& r = def["render"];
                    ad.frame_w = r.value("frame_w", 16);
                    ad.frame_h = r.value("frame_h", 16);
                    ad.overlay = r.value("overlay", "");
                    ad.overlay_alpha = r.value("overlay_alpha", 0.0f);
                }
                _asset_defs[cat + "." + name] = std::move(ad);
            }
        _assets_cfg_ok = true;
        LOG_INFO("Asset Manifest: %d 个资产定义", (int)_asset_defs.size());
    } catch (const std::exception& e) {
        LOG_ERROR("Asset Manifest 解析失败: %s", e.what());
        return false;
    }
    return _assets_cfg_ok;
}

const AssetDef* ResourceManager::asset_by_id(const char* id) {
    if (!_assets_cfg_ok && !load_assets_config()) return nullptr;
    auto it = _asset_defs.find(id ? id : "");
    return (it != _asset_defs.end()) ? &it->second : nullptr;
}

Texture2D ResourceManager::tex_by_id(const char* id) {
    const AssetDef* d = asset_by_id(id);
    return d ? load_texture(d->path.c_str()) : Texture2D{0};
}
