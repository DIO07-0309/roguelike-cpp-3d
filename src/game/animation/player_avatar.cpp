// A5-T3: PlayerAvatar — A6-S1 起为 SkeletonAvatar 的玩家适配壳 (行为与原实现一致)
#include "game/animation/player_avatar.h"

#include <cmath>
#include "entities/player.h"
#include "data/voxel_part_defs.h"
#include "game/rendering3d/part_voxelizer.h"
#include "game/rendering3d/model_cache.h"   // A16-Player: build_gpu_mesh_asset
#include "data/mesh_asset.h"                // A16-Player: load_mesh_asset_file
#include "world/game_map.h"

namespace {

constexpr float kAttackBaseDur = 0.36f;

float face_sign(Direction dir) {
    return (dir == Direction::LEFT || dir == Direction::UP) ? -1.f : 1.f;
}

} // namespace

// A16-Player: 朝向角 — 绕广告牌 up 轴, 0=下(朝相机), +90=右, -90=左, 180=上(背对相机)
float direction_yaw(Direction dir) {
    switch (dir) {
        case Direction::RIGHT: return 90.f;
        case Direction::LEFT:  return -90.f;
        case Direction::UP:    return 180.f;
        case Direction::DOWN:  break;
    }
    return 0.f;
}

// G10.6-B C1 同款朝墙探测 (视觉衰减, 不改判定)
namespace {
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
    if (!_core.try_init(anim_dir + "/player_skeleton.json",
                        anim_dir + "/player_anim.json", err)) return false;
    if (voxel::g_voxel_avatar) {
        // A16-Player: 优先手工低模部件; 任一缺失/损坏 → 回退 A13 体素 (再失败 → 纸偶)
        if (!_setup_lowpoly_parts())
            _setup_voxel_parts();
    }
    return true;
}

// A16-Player: 逐部件加载手工低模 mesh (assets/models/player_part_<slot>.json),
// 按 _core.part_defs() 顺序 (8 个, 与骨骼一一对应) 生成 mesh; 任一失败 → 释放并返回 false
bool PlayerAvatar::_setup_lowpoly_parts() {
    std::vector<Mesh> meshes;
    meshes.reserve(_core.part_defs().size());
    for (const auto& part : _core.part_defs()) {
        const std::string slot = voxel_slot_from_path(part.file);
        if (slot.empty()) {
            for (auto& m : meshes)
                if (m.vertexCount > 0) UnloadMesh(m);
            return false;
        }
        std::string err;
        const std::string path = "assets/models/player_part_" + slot + ".json";
        const auto asset = load_mesh_asset_file(path, err);
        if (!asset) {
            for (auto& m : meshes)
                if (m.vertexCount > 0) UnloadMesh(m);
            return false;
        }
        meshes.push_back(build_gpu_mesh_asset(*asset));
    }
    _core.set_voxel_parts(std::move(meshes));
    return true;
}

// A13: 逐部件体素化 (贴图 px 网格, pivot = 贴图内左下原点锚点); 任一失败 → 放弃整批
void PlayerAvatar::_setup_voxel_parts() {
    std::string load_err;
    const auto config = load_voxel_parts_file("resources/voxel_parts.json", load_err);
    if (!config) return;
    std::vector<Mesh> meshes;
    for (const auto& part : _core.part_defs()) {
        Image image = LoadImage(part.file.c_str());
        if (image.width <= 0 || image.height <= 0) {
            for (auto& m : meshes)
                if (m.vertexCount > 0) UnloadMesh(m);
            return;
        }
        meshes.push_back(voxel::build_part_mesh(image, *config, voxel_slot_from_path(part.file),
                                                part.pivot_x, part.pivot_y));
        UnloadImage(image);
    }
    _core.set_voxel_parts(std::move(meshes));
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

void PlayerAvatar::draw(const Player& pl, float cam_x, float cam_y, const GameMap* view_map) const {
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
    auto parts = _core.part_draws(_overlay(player), false);
    // A16-Player: 朝向随 WASD — 绕广告牌 up 轴转向 (0=下/朝相机, 90=右, -90=左, 180=上)
    // face_sign 只做左右镜像不随 WASD 转向, 故此处按 direction 统一设 yaw
    const float yaw = direction_yaw(player.direction);
    for (auto& p : parts) p.yaw_deg = yaw;
    return parts;
}
