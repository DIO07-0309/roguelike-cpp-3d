#include "door_renderer.h"
#include "resource_manager.h"
#include "config.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// G10.2-B2: 门贴图路径已移除 — 统一经 Asset ID (door.open/closed/locked/sealed) 走 manifest。
// 唯一映射: DoorRenderer::asset_id_for(DoorState)。

// G10.2-B2: Door Visual Asset Mapping — 四态 ID 映射 + LOCKED overlay
// Asset ID 唯一来源 = manifest (resources/sprites.json §assets.door); 无硬编码路径。
const char* DoorRenderer::asset_id_for(DoorState s) {
    switch (s) {
        case DoorState::OPEN:   return "door.open";
        case DoorState::CLOSED: return "door.closed";
        case DoorState::LOCKED: return "door.locked";
        case DoorState::SEALED: return "door.sealed";
        default:                return "door.closed";
    }
}

Color DoorRenderer::overlay_for(DoorState s) {
    if (s != DoorState::LOCKED) return {0, 0, 0, 0};
    const AssetDef* d = ResourceManager::inst().asset_by_id("door.locked");
    if (!d || d->overlay.empty()) return {0, 0, 0, 0};
    unsigned r = 0, g = 0, b = 0;
    std::sscanf(d->overlay.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
    float alpha = (d->overlay_alpha > 0.0f) ? d->overlay_alpha : 0.35f;
    return {(unsigned char)r, (unsigned char)g, (unsigned char)b,
            (unsigned char)(alpha * 255.0f)};
}

DoorRenderer& DoorRenderer::inst() { static DoorRenderer r; return r; }

uint64_t DoorRenderer::_key(int tx, int ty) {
    return (uint64_t)((uint32_t)tx << 16 | (uint32_t)(uint16_t)ty);
}

Texture2D DoorRenderer::_tex_for(DoorState s) const {
    switch (s) {
        case DoorState::OPEN:   return _tex_open;
        case DoorState::CLOSED: return _tex_closed;
        case DoorState::LOCKED: return _tex_locked;
        case DoorState::SEALED: return _tex_sealed;
        default:                return _tex_closed;
    }
}

void DoorRenderer::init() {
    if (!IsWindowReady()) return;  // headless test — skip texture load
    auto load = [](const char* id) -> Texture2D {
        Texture2D t = ResourceManager::inst().tex_by_id(id);
        if (t.id > 0) SetTextureFilter(t, TEXTURE_FILTER_POINT);
        return t;
    };
    _tex_open   = load(asset_id_for(DoorState::OPEN));
    _tex_closed = load(asset_id_for(DoorState::CLOSED));
    _tex_locked = load(asset_id_for(DoorState::LOCKED));
    _tex_sealed = load(asset_id_for(DoorState::SEALED));
    _loaded = (_tex_open.id > 0 && _tex_closed.id > 0);
}

void DoorRenderer::update(float dt) {
    for (auto it = _anims.begin(); it != _anims.end(); ) {
        auto& a = it->second;
        if (a.progress < 1.0f) {
            a.progress = std::min(1.0f, a.progress + dt / _transition_dur);
            ++it;
        } else {
            // animation done — keep entry for draw, no need to advance
            ++it;
        }
    }
}

void DoorRenderer::on_state_change(int tx, int ty, DoorState from, DoorState to) {
    if (from == to) return;
    auto& a = _anims[_key(tx, ty)];
    a.previous = from;
    a.current  = to;
    a.progress = 0.0f;
}

void DoorRenderer::draw_door(int tx, int ty, DoorState state, bool bright,
                              float cam_x, float cam_y) {
    if (!_loaded) return;

    float dx = (float)(tx * TILE_SIZE) - cam_x;
    float dy = (float)(ty * TILE_SIZE) - cam_y;
    Rectangle dst = { dx, dy, (float)TILE_SIZE, (float)TILE_SIZE };

    auto it = _anims.find(_key(tx, ty));
    bool animating = (it != _anims.end() && it->second.progress < 1.0f);
    if (animating)
        _draw_door_transition(it->second, dst);
    else
        _draw_door_static(state, bright, dst);
}

// G42: 门切换动画 (旧贴图淡出 + 新贴图缩放淡入)
void DoorRenderer::_draw_door_transition(const DoorAnimState& a, Rectangle dst) {
    float p = a.progress;
    // old tile fades out
    Texture2D old_tex = _tex_for(a.previous);
    if (old_tex.id > 0) {
        Color fade_out = { 255, 255, 255, (unsigned char)((1.0f - p) * 255) };
        DrawTexturePro(old_tex, { 0, 0, (float)old_tex.width, (float)old_tex.height },
                       dst, { 0, 0 }, 0.0f, fade_out);
    }
    // new tile fades in + scale
    Texture2D new_tex = _tex_for(a.current);
    if (new_tex.id > 0) {
        float sc = 0.8f + 0.2f * p;
        float off = (float)TILE_SIZE * (1.0f - sc) * 0.5f;
        Rectangle scaled_dst = { dst.x + off, dst.y + off,
                                 (float)TILE_SIZE * sc, (float)TILE_SIZE * sc };
        Color fade_in = { 255, 255, 255, (unsigned char)(p * 255) };
        DrawTexturePro(new_tex, { 0, 0, (float)new_tex.width, (float)new_tex.height },
                       scaled_dst, { 0, 0 }, 0.0f, fade_in);
    }
}

// G42: 静态门 (贴图 + LOCKED 色罩 + 锁/封图标)
void DoorRenderer::_draw_door_static(DoorState state, bool bright, Rectangle dst) {
    // static draw
    Texture2D tex = _tex_for(state);
    if (tex.id > 0) {
        Color tint = bright ? WHITE : Color{ 160, 160, 160, 255 };
        DrawTexturePro(tex, { 0, 0, (float)tex.width, (float)tex.height },
                       dst, { 0, 0 }, 0.0f, tint);
    }
    // G10.2-B2 (D3): LOCKED 色罩 — 视觉明确区分于 CLOSED, 规则来自 manifest overlay
    Color ov = overlay_for(state);
    if (ov.a > 0) DrawRectangleRec(dst, ov);
    // overlay icons (only when static)
    float cx = dst.x + TILE_SIZE * 0.5f;
    float cy = dst.y + TILE_SIZE * 0.5f;
    if (state == DoorState::LOCKED) _draw_lock_icon(cx, cy, bright);
    if (state == DoorState::SEALED) _draw_seal_icon(cx, cy, bright);
}

void DoorRenderer::_draw_lock_icon(float cx, float cy, bool bright) {
    // Red lock body (small rectangle)
    Color body = bright ? Color{ 200, 50, 50, 220 } : Color{ 140, 35, 35, 200 };
    float bw = 8, bh = 6;
    DrawRectangle((int)(cx - bw/2), (int)(cy + 2), (int)bw, (int)bh, body);
    // Yellow lock shackle (arc above body)
    Color shackle = bright ? Color{ 230, 190, 60, 220 } : Color{ 160, 130, 40, 200 };
    DrawRectangle((int)(cx - 3), (int)(cy - 2), 6, 4, shackle);
    DrawRectangle((int)(cx - 4), (int)(cy - 2), 8, 2, shackle);
}

void DoorRenderer::_draw_seal_icon(float cx, float cy, bool bright) {
    // Purple pulsing cross + ring
    float pulse = 0.7f + 0.3f * sinf((float)GetTime() * 3.0f);
    Color seal = bright
        ? Color{ (unsigned char)(160 * pulse), 50, (unsigned char)(220 * pulse), 200 }
        : Color{ (unsigned char)(110 * pulse), 35, (unsigned char)(155 * pulse), 180 };
    // Cross lines
    DrawRectangle((int)(cx - 1), (int)(cy - 5), 2, 10, seal);
    DrawRectangle((int)(cx - 5), (int)(cy - 1), 10, 2, seal);
    // Outer ring
    DrawCircleLines((int)cx, (int)cy, 6, seal);
}
