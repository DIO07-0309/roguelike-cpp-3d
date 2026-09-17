// A5-T3: PlayerAvatar 实现 — 懒加载 + 2D 逐件绘制 + B3 overlay 通道
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

void PlayerAvatar::_draw_row(const std::vector<WorldBone>& pose, Vector2 feet,
                             float face, int alpha) const {
    for (size_t index = 0; index < _sk.parts.size(); ++index) {
        const auto& definition = _sk.parts[index];
        const auto part = buildAvatarPart(definition, pose[definition.bone], _tex[index],
                                          _sk.pixels_per_unit, face < 0);
        const Rectangle source{0, 0, part.flip_x ? -float(part.tex.width) : float(part.tex.width),
                               float(part.tex.height)};
        const Rectangle destination{feet.x + part.offset.x, feet.y - part.offset.y,
                                    part.size.x, part.size.y};
        DrawTexturePro(part.tex, source, destination, part.pivot, part.rot_deg,
                       {255, 255, 255, static_cast<unsigned char>(alpha)});
    }
}

PlayerAvatar::~PlayerAvatar() { release_textures(); }

void PlayerAvatar::release_textures() {
    for (auto& t : _tex)
        if (t.id > 0) UnloadTexture(t);
    _tex.clear();
}

bool PlayerAvatar::try_init(const std::string& anim_dir, std::string& err) {
    _active = false;
    auto sk = load_skeleton_file(anim_dir + "/player_skeleton.json", err);
    if (!sk) return false;
    auto set = load_anim_file(anim_dir + "/player_anim.json", *sk, err);
    if (!set) return false;
    std::vector<Texture2D> tex;
    for (const auto& part : sk->parts) {
        Texture2D t = LoadTexture(part.file.c_str());
        if (t.id <= 0) {
            for (auto& ok : tex) UnloadTexture(ok);
            err = "avatar: texture load failed: " + part.file;
            return false;
        }
        tex.push_back(t);
    }
    _sk = std::move(*sk);
    _set = std::move(*set);
    _tex = std::move(tex);
    _an.set_durations(_set);
    _active = true;
    return true;
}

void PlayerAvatar::update(float dt, const Player& pl) {
    if (!_active) return;
    AnimInput in;
    int hp = pl.combat.current_hp;
    in.hit_flash = (_last_hp >= 0 && hp < _last_hp);   // hp 下降沿 = 受击表现信号
    _last_hp = hp;
    in.attacking = pl.weapon.is_attacking();
    in.moving = pl.is_moving;
    in.attack_recovery_ratio = pl.weapon.runtime().recovery_timer / kAttackBaseDur;
    _an.advance(dt, in);
}

OverlayTf PlayerAvatar::_overlay(const Player& pl) const {
    OverlayTf ov;
    float p = _an.time() / (kAttackBaseDur * _an.dur_scale());
    if (_an.current_name() == "attack" && pl.combo.is_heavy() && p < 1.f)
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
    if (!_active) return;
    OverlayTf ov = _overlay(pl);
    auto pose = compute_pose(_sk, _an.clip(_set), _an.pose_time(), ov);
    Vector2 feet = _feet_of(pl, cam_x, cam_y, view_map);
    _draw_row(pose, feet, face_sign(pl.direction), 255);
    for (const auto& g : pl.dodge.ghosts()) {           // B3 残影: 同件重绘偏移回贴
        float ga = 120.f * (1.f - g.age / DodgeComponent::kGhostLife);
        if (ga <= 0.f) continue;
        Vector2 gf = { feet.x + (g.pos.x - pl.entity.position.x),
                       feet.y + (g.pos.y - pl.entity.position.y) };
        _draw_row(pose, gf, face_sign(pl.direction), (int)ga);
    }
}

AvatarPartDraw buildAvatarPart(const PartDef& part, const WorldBone& bone,
                               Texture2D texture, float pixels_per_unit, bool flip_x) {
    const float radians = bone.rot_deg * 3.14159265f / 180.f;
    const float cosine = std::cos(radians), sine = std::sin(radians);
    const float local_x = part.dx * bone.sx, local_y = part.dy * bone.sy;
    const float face = flip_x ? -1.f : 1.f;
    AvatarPartDraw result;
    result.tex = texture;
    result.offset = {face * (bone.x + local_x * cosine - local_y * sine) * pixels_per_unit,
                     (bone.y + local_x * sine + local_y * cosine) * pixels_per_unit};
    result.size = {texture.width * pixels_per_unit * bone.sx,
                   texture.height * pixels_per_unit * bone.sy};
    result.pivot = {part.pivot_x * pixels_per_unit * bone.sx,
                    (texture.height - part.pivot_y) * pixels_per_unit * bone.sy};
    if (flip_x) result.pivot.x = result.size.x - result.pivot.x;
    result.rot_deg = -face * bone.rot_deg;
    result.flip_x = flip_x;
    return result;
}

std::vector<AvatarPartDraw> PlayerAvatar::worldParts(const Player& player) const {
    std::vector<AvatarPartDraw> parts;
    if (!_active) return parts;
    const auto pose = compute_pose(_sk, _an.clip(_set), _an.pose_time(), _overlay(player));
    parts.reserve(_sk.parts.size());
    for (size_t index = 0; index < _sk.parts.size(); ++index) {
        const auto& part = _sk.parts[index];
        parts.push_back(buildAvatarPart(part, pose[part.bone], _tex[index],
                                        _sk.pixels_per_unit, face_sign(player.direction) < 0));
    }
    return parts;
}
