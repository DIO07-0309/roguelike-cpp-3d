// A5-T3: PlayerAvatar — A6-S1 起为 SkeletonAvatar 的玩家适配壳 (行为与原实现一致)
#include "game/animation/player_avatar.h"

#include <cmath>
#include "entities/player.h"
#include "world/game_map.h"

namespace {

constexpr float kAttackBaseDur = 0.36f;

float face_sign(Direction dir) {
    return (dir == Direction::LEFT || dir == Direction::UP) ? -1.f : 1.f;
}

// G10.6-B C1 同款朝墙探测 (视觉衰减, 不改判定)
bool wall_ahead(const GameMap* map, Vector2 center, Direction dir) {
    if (!map) return false;
    const float PROBE = 20.f, TS = 32.f;
    float px = center.x, py = center.y;
    if (dir == Direction::UP) py -= PROBE;
    else if (dir == Direction::DOWN) py += PROBE;
    else if (dir == Direction::LEFT) px -= PROBE;
    else px += PROBE;
    int tx = (int)(px / TS), ty = (int)(py / TS);
    if (tx < 0 || ty < 0) return true;
    return !map->is_walkable(tx, ty);
}

} // namespace

bool PlayerAvatar::try_init(const std::string& anim_dir, std::string& err) {
    return _core.try_init(anim_dir + "/player_skeleton.json",
                          anim_dir + "/player_anim.json", err);
}

void PlayerAvatar::update(float dt, const Player& pl) {
    if (!_core.active()) return;
    AnimInput in;
    in.hit_flash = _core.hp_hit_edge(pl.combat.current_hp);  // hp 下降沿 = 受击表现信号
    in.attacking = pl.weapon.is_attacking();
    in.moving = pl.is_moving;
    in.attack_recovery_ratio = pl.weapon.runtime().recovery_timer / kAttackBaseDur;
    _core.advance(dt, in);
}

OverlayTf PlayerAvatar::_overlay(const Player& pl) const {
    OverlayTf ov;
    float p = _core.time() / (kAttackBaseDur * _core.dur_scale());
    if (_core.current_clip() == "attack" && pl.combo.is_heavy() && p < 1.f)
        ov.rot_deg = 6.f * sinf(p * 6.2831853f);        // 重击脚底回弹 (原路径同款)
    ov.rot_deg += pl.dodge.tilt_deg();                  // B3 倾斜: 同脚底原点
    Vector2 sq = pl.dodge.squash_scale();
    float heavy = pl.combo.is_heavy() ? 1.25f : 1.0f;
    ov.sx = sq.x * heavy;
    ov.sy = sq.y * heavy;
    return ov;
}

// feet 复刻 draw_no_cam 坐标框架 (296-304): 重击/wall 衰减 + 翻滚压扁
static Vector2 _feet_of(const Player& pl, float cam_x, float cam_y,
                        const GameMap* view_map) {
    Rectangle dr = pl.entity.draw_rect(cam_x, cam_y);
    Vector2 center = { dr.x + dr.width / 2, dr.y + dr.height / 2 };
    float heavy = pl.combo.is_heavy() ? 1.25f : 1.0f;
    if (wall_ahead(view_map, center, pl.direction)) heavy = 1.08f;
    float hw = dr.width * heavy, hh = dr.height * heavy;
    float hx = dr.x - (hw - dr.width) / 2, hy = dr.y - (hh - dr.height) / 2;
    return { hx + hw / 2, hy + hh };
}

void PlayerAvatar::draw(const Player& pl, float cam_x, float cam_y, const GameMap* view_map) {
    if (!_core.active()) return;
    auto pose = _core.pose(_overlay(pl));
    Vector2 feet = _feet_of(pl, cam_x, cam_y, view_map);
    _core.draw_row(pose, feet, face_sign(pl.direction), 255);
    for (const auto& g : pl.dodge.ghosts()) {           // B3 残影: 同件重绘偏移回贴
        float ga = 120.f * (1.f - g.age / DodgeComponent::kGhostLife);
        if (ga <= 0.f) continue;
        Vector2 gf = { feet.x + (g.pos.x - pl.entity.position.x),
                       feet.y + (g.pos.y - pl.entity.position.y) };
        _core.draw_row(pose, gf, face_sign(pl.direction), (int)ga);
    }
}

std::vector<AvatarPartDraw> PlayerAvatar::worldParts(const Player& player) const {
    return _core.part_draws(_overlay(player), face_sign(player.direction) < 0);
}
