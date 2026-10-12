#pragma once
#include "raylib.h"
#include "game/world/game_map.h"
#include <map>
#include <cstdint>

// ============================================================
// DoorRenderer — Kenney tile-based door sprites + transition anim
// 4 states: OPEN(archway) / CLOSED(wood door) / LOCKED(door+lock) / SEALED(door+seal)
// ============================================================

struct DoorAnimState {
    DoorState current  = DoorState::OPEN;
    DoorState previous = DoorState::OPEN;
    float progress     = 1.0f;  // 0→1
};

class DoorRenderer {
public:
    static DoorRenderer& inst();

    void init();
    void update(float dt);
    void on_state_change(int tx, int ty, DoorState from, DoorState to);
    void draw_door(int tx, int ty, DoorState state, bool bright,
                   float cam_x = 0, float cam_y = 0);

    bool is_loaded() const { return _loaded; }

private:
    DoorRenderer() = default;

    static uint64_t _key(int tx, int ty);
    Texture2D _tex_for(DoorState s) const;
    void _draw_lock_icon(float cx, float cy, bool bright);
    void _draw_seal_icon(float cx, float cy, bool bright);
    // G42: draw_door 51->16 行, 拆静态/动画两条独立路径
    void _draw_door_static(DoorState state, bool bright, Rectangle dst);
    void _draw_door_transition(const DoorAnimState& a, Rectangle dst);

public:
    // ── G10.2-B2: Door Visual Asset Mapping (静态, headless 可测) ──
    // DoorState → Asset ID (DoorRenderer 唯一映射依据; 硬编码路径已移除)
    static const char* asset_id_for(DoorState s);
    // DoorState → overlay 色 (LOCKED 按 manifest 规则; 其他透明 = 无色罩)
    static Color overlay_for(DoorState s);

    Texture2D _tex_open   = {0};
    Texture2D _tex_closed = {0};
    Texture2D _tex_locked = {0};
    Texture2D _tex_sealed = {0};
    bool _loaded = false;
    float _transition_dur = 0.3f;
    std::map<uint64_t, DoorAnimState> _anims;
};
