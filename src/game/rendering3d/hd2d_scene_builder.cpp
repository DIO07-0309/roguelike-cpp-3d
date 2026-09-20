// M6-HD2D: 场景构建器实现 — GameScene 只读状态 → HD2DDrawItem 列表
// 切片范围: 地板/墙 tile (纯色起步) + 玩家/怪 billboard (贴图) + 特效
// 红线: 只读 gs; 无 gameplay 副作用; 视觉随机只吃 visual_rng (本文件未用随机)
#include "hd2d_scene_builder.h"
#include "game/animation/player_avatar.h"
#include "scenes/game_scene.h"
#include "world/game_map.h"
#include "world/challenge_room.h"              // M6-v2a: ChallengePhase
#include "world/special_room.h"                // M6-v2h: SpecialRoom 图标 key
#include "entities/player.h"
#include "entities/monster.h"
#include "entities/item.h"                    // M6-v2a: item_icon_key
#include "systems/weapon_component.h"         // M6-v2b: WeaponType/range_indicator
#include "world/npc_system.h"                 // M6-v2a: npc_sprite_key
#include "world/biome.h"                      // A2.1: get_biome_for_floor (mote 风格)
#include "entities/boss.h"                    // M6-v2b: BossAI 技能预警只读
#include "resources/resource_manager.h"
#include "rendering/sprite_renderer.h"
#include "config.h"                 // TILE_SIZE
#include "core/logger.h"            // M6-i.1 DEBUG
#include <algorithm>
#include <cstring>
#include <cmath>

namespace hd2d {

// ── tile 颜色回退: 贴图缺席时的纯色地形 ──
static Color _tile_color(TileType t, bool visible_now) {
    Color base;
    switch (t) {
    case TileType::WALL:      base = {74, 78, 96, 255};  break;
    case TileType::FLOOR:     base = {40, 42, 58, 255};  break;
    case TileType::LAVA:      base = {200, 90, 30, 255}; break;
    case TileType::STAIRS_DOWN: base = {180, 160, 60, 255}; break;
    case TileType::DOOR:      base = {130, 90, 50, 255}; break;
    default:                  base = {46, 48, 64, 255};  break;
    }
    // fog: 已探索不可见 = 压暗 40%
    if (!visible_now) {
        base.r = base.r * 6 / 10; base.g = base.g * 6 / 10; base.b = base.b * 6 / 10;
    }
    return base;
}

// ── M6-i: biome_id → 材质风格映射 (biomes.json 三群系; 未知=通用) ──
static int _biome_material_style(const GameMap& map) {
    const char* id = map.biome_id();
    if (strcmp(id, "forgotten_prison") == 0) return (int)SpriteRenderer::BiomeStyle::PRISON;
    if (strcmp(id, "ash_volcano") == 0)      return (int)SpriteRenderer::BiomeStyle::VOLCANO;
    if (strcmp(id, "void_abyss") == 0)       return (int)SpriteRenderer::BiomeStyle::ABYSS;
    return (int)SpriteRenderer::BiomeStyle::GENERIC;
}

// ── M6-i.1: 群系贴图 key — 全 id → sprites.json 短名 (与 game_map 同修) ──
static const char* _biome_tile_key(const char* biome_id, const char* kind) {
    static char key[48];
    const char* short_id = "";
    if (biome_id && biome_id[0]) {
        if (strcmp(biome_id, "forgotten_prison") == 0) short_id = "prison";
        else if (strcmp(biome_id, "ash_volcano") == 0) short_id = "volcano";
        else if (strcmp(biome_id, "void_abyss") == 0) short_id = "abyss";
    }
    if (short_id[0]) {
        snprintf(key, sizeof(key), "%s_%s", kind, short_id);
        return key;
    }
    return kind;
}

// ── M6-v2a: 群系 tile 贴图解析 — 复用 2D 同源回退链 ──
// (群系 wall_<biome> → 通用 wall → 程序化 procedural_tile, 与 GameMap::draw 一致)
// M6-i: 程序化末端升级为群系风格化材质 (监狱/火山/深渊 各自画法)
struct TileTexPair {
    Texture2D tex = {};
    SpriteDef def;
};

static TileTexPair _resolve_tile_tex(const GameMap& map, const char* kind,
                                      const Color& fallback_color, bool wall) {
    TileTexPair out;
    auto& res = ResourceManager::inst();
    const char* biome_key = _biome_tile_key(map.biome_id(), kind);
    Texture2D biome_tex = res.sprite_by_key(biome_key, out.def);
    if (biome_tex.id > 0) { out.tex = biome_tex; return out; }
    out.def = SpriteDef{};
    Texture2D generic = res.sprite_by_key(kind, out.def);
    if (generic.id > 0) { out.tex = generic; return out; }
    // M6-i: 群系风格化程序材质 (accent = palette 苔藓/特征色; 未配置回退通用)
    char proc_key[56];
    int style = _biome_material_style(map);
    const auto& pal = map.palette();
    bool has_pal = map.has_palette();
    Color accent = has_pal ? pal.wall_moss : fallback_color;
    if (style != (int)SpriteRenderer::BiomeStyle::GENERIC) {
        snprintf(proc_key, sizeof(proc_key), "bio%d_%s_%02x%02x%02x",
                 style, kind, fallback_color.r, fallback_color.g,
                 fallback_color.b);
        out.tex = res.procedural_biome_tile(proc_key, fallback_color, accent,
                                            style, wall);
    } else {
        snprintf(proc_key, sizeof(proc_key), "%s_%02x%02x%02x",
                 kind, fallback_color.r, fallback_color.g, fallback_color.b);
        out.tex = res.procedural_tile(proc_key, fallback_color, wall);
    }
    out.def = SpriteDef{};  // procedural = 整图单帧
    return out;
}

// ── M6-v2h: 特殊房间地板 tint (2D game_map.draw 九色同源) ──
static Color _special_room_tint(const SpecialRoom* sr) {
    Color base;
    switch (sr->type) {
        case SpecialRoomType::ALTAR:      base = {60, 44, 22, 255}; break;
        case SpecialRoomType::TREASURE:   base = {34, 46, 76, 255}; break;
        case SpecialRoomType::FOUNTAIN:   base = {28, 56, 34, 255}; break;
        case SpecialRoomType::SHOP:       base = {62, 56, 24, 255}; break;
        case SpecialRoomType::BLACKSMITH: base = {68, 40, 28, 255}; break;
        case SpecialRoomType::LIBRARY:    base = {22, 38, 68, 255}; break;
        case SpecialRoomType::GAMBLER:    base = {56, 22, 60, 255}; break;
        case SpecialRoomType::SHRINE:     base = {52, 52, 16, 255}; break;
        case SpecialRoomType::SECRET:     base = {62, 16, 16, 255}; break;
        default:                          base = {25, 25, 35, 255}; break;
    }
    if (sr->triggered) {
        base.r = (unsigned char)(base.r * 0.55f);
        base.g = (unsigned char)(base.g * 0.55f);
        base.b = (unsigned char)(base.b * 0.55f);
    }
    return base;
}

// ── M6-v2h: 特殊房间中心图标 key (2D room_* 素材同源) ──
static const char* _special_room_icon_key(SpecialRoomType type) {
    switch (type) {
        case SpecialRoomType::ALTAR:      return "room_altar";
        case SpecialRoomType::TREASURE:   return "room_chest";
        case SpecialRoomType::FOUNTAIN:   return "room_spring";
        case SpecialRoomType::SHOP:       return "room_shop";
        case SpecialRoomType::BLACKSMITH: return "room_blacksmith";
        case SpecialRoomType::LIBRARY:    return "room_library";
        case SpecialRoomType::GAMBLER:    return "room_gambler";
        case SpecialRoomType::SHRINE:     return "room_shrine";
        case SpecialRoomType::SECRET:     return "room_secret";
        default:                          return nullptr;
    }
}

// ── M6-j: 地板装饰 — 坐标确定性哈希 (2D game_map.draw 同款, 零 RNG) ──
// tint 变体 (污渍 6% / 石块 4%) + decal 贴片 (10%: 裂缝 4% / 苔藓 3% / 符文 3%)
static void _apply_floor_decoration(const GameMap& map, int tx, int ty,
                                    bool has_pal, const TilePalette& pal,
                                    Texture2D floor_tex, HD2DDrawItem& item,
                                    std::vector<HD2DDrawItem>& out) {
    if (!has_pal || floor_tex.id <= 0) return;      // 程序化地板自带风格化
    unsigned int h = (unsigned int)tx * 73856093u
                   ^ (unsigned int)ty * 19349663u;
    unsigned int variant = (h ^ (h >> 13)) % 100u;
    if (variant < 6u)        item.tint = pal.floor_dirt;   // 污渍
    else if (variant < 10u)  item.tint = pal.floor_b;      // 石块变体
    unsigned int decal_roll = (h ^ (h >> 7)) % 100u;
    if (decal_roll >= 10u) return;                          // 90% 无装饰
    // decal 类型 + 群系配色 (2D dh 段同源: 裂缝 4% / 苔藓 3% / 符文 3%)
    int kind = (decal_roll < 4u) ? 0 : (decal_roll < 7u) ? 1 : 2;
    Color primary = pal.floor_joint, secondary = pal.wall_highlight;
    const char* biome = map.biome_id();
    if (strcmp(biome, "ash_volcano") == 0)       primary = pal.wall_highlight;
    else if (strcmp(biome, "void_abyss") == 0)    primary = pal.floor_b;
    char dkey[56];
    snprintf(dkey, sizeof(dkey), "decal%d_%02x%02x%02x", kind,
             primary.r, primary.g, primary.b);
    HD2DDrawItem decal;
    decal.kind = HD2DDrawItem::Kind::FLOOR_DECAL;
    decal.world_pos = {(float)tx * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                       (float)ty * TILE_SIZE + TILE_SIZE * 0.5f};
    decal.size = (float)TILE_SIZE;
    decal.texture = ResourceManager::inst().procedural_floor_decal(
        dkey, kind, primary, secondary);
    if (decal.texture.id <= 0) return;
    out.push_back(decal);
}

// ── 地形: 玩家周围可见 tile → 地板/墙 item (M6-v2a: 群系贴图接线) ──
// v1.6 实机反馈: 未探索区画成"暗色岩石块"会从俯视泄露走廊/房间轮廓
// (玩家可提前读出地图布局) → 回归战争迷雾语义: 未探索 = 不绘制 (虚空),
// 已探索不可见 = 压暗 40% 记忆显示。副作用: 未探索岩浆不再入点光表,
// 光不会从黑幕外漏出 (正确性提升)。

// ── A3.2-fix2: 顶面色调映射 — 2D 同源 wall_top/wall_face 通道比 ──
// 3D 顶面复用墙面贴图 (按 wall_face 生成), 乘通道比即得 2D wall_top 色调;
// 实机反馈: 深渊顶面暗同虚空 → 读作"镂空"。无 palette 回退固定提亮。
static Color _wall_top_tint(const GameMap& map, const Color& tint) {
    if (!map.has_palette()) {
        return Color{(unsigned char)std::min(tint.r * 1.4f, 255.0f),
                     (unsigned char)std::min(tint.g * 1.4f, 255.0f),
                     (unsigned char)std::min(tint.b * 1.4f, 255.0f), 255};
    }
    const auto& pal = map.palette();
    auto ch = [](unsigned char top, unsigned char face) {
        float ratio = face > 0 ? (float)top / (float)face : 1.0f;
        return (unsigned char)std::min(ratio * 255.0f, 255.0f);
    };
    return Color{ch(pal.wall_top.r, pal.wall_face.r),
                 ch(pal.wall_top.g, pal.wall_face.g),
                 ch(pal.wall_top.b, pal.wall_face.b), 255};
}

static void _build_terrain(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    const GameMap* map = gs.game_map.get();
    if (!map) return;

    int cx = 0, cy = 0;
    if (gs.player) {
        cx = (int)(gs.player->entity.rect.x / TILE_SIZE);
        cy = (int)(gs.player->entity.rect.y / TILE_SIZE);
    }
    // 扩大 build 范围: 相机 440 拉近后视野投影超出旧 ±16/±12 (实测 52% 屏幕是 ClearBackground)
    int x0 = std::max(0, cx - 40), x1 = std::min(map->width - 1, cx + 40);
    int y0 = std::max(0, cy - 30), y1 = std::min(map->height - 1, cy + 30);

    // v2a: 与 2D 同源的贴图回退链 (群系 → 通用 → 程序化)
    auto& res = ResourceManager::inst();
    const auto& pal = map->palette();
    bool has_pal = map->has_palette();
    Color wall_c  = has_pal ? pal.wall_face : Color{60, 60, 80, 255};
    Color floor_c = has_pal ? pal.floor_base : Color{25, 25, 35, 255};
    TileTexPair wall_tex  = _resolve_tile_tex(*map, "wall", wall_c, true);
    TileTexPair floor_tex = _resolve_tile_tex(*map, "floor", floor_c, false);

    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            TileType t = map->tile_at(tx, ty);
            HD2DDrawItem item;
            item.tile_x = tx; item.tile_y = ty;
            item.world_pos = {(float)tx * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                              (float)ty * TILE_SIZE + TILE_SIZE * 0.5f};
            item.size = (float)TILE_SIZE;
            // v1.6: 未探索 → 跳过 (虚空; 见函数头注释)
            if (!map->isExplored(tx, ty)) continue;
            item.tint = _tile_color(t, map->isVisible(tx, ty));
            if (t == TileType::WALL) {
                item.kind = HD2DDrawItem::Kind::WALL_BLOCK;
                item.height = TILE_SIZE * 1.25f;
                item.texture = wall_tex.tex;
                item.tex_src = wall_tex.def.frame_w > 0
                    ? SpriteRenderer::frame_rect(wall_tex.def, 0) : Rectangle{};
                // i.1-fix2: 贴图墙不再叠蓝灰回退色 (地板同规则; 压暗由探索态编码)
                if (item.texture.id > 0)
                    item.tint = map->isVisible(tx, ty) ? WHITE
                                                      : Color{153, 153, 153, 255};
                item.top_tint = _wall_top_tint(*map, item.tint);  // A3.2-fix2
            } else if (t == TileType::DOOR) {
                // M6-v2h: 门 → 竖立贴图面板 (四态纹理走 door.* manifest)
                item.kind = HD2DDrawItem::Kind::DOOR_PANEL;
                item.height = TILE_SIZE * 1.05f;
                item.door_state = (int)map->door_state_at(tx, ty);
                SpriteDef ddef;
                const char* door_id = "door.closed";
                switch ((DoorState)item.door_state) {
                    case DoorState::OPEN:   door_id = "door.open";   break;
                    case DoorState::LOCKED: door_id = "door.locked"; break;
                    case DoorState::SEALED: door_id = "door.sealed"; break;
                    default: break;
                }
                item.texture = res.tex_by_id(door_id);
                item.tint = map->isVisible(tx, ty) ? WHITE
                                                      : Color{153, 153, 153, 255};
                // M6-n: 探测门两侧墙走向 → 面板贴墙 (billboard 改 wall-aligned)
                // 左右是墙 = 门嵌在东西走向墙里 → 面板法线沿 Z (axis 0)
                bool wall_lr = map->tile_at(tx - 1, ty) == TileType::WALL ||
                               map->tile_at(tx + 1, ty) == TileType::WALL;
                item.door_axis = wall_lr ? 0 : 1;
            } else {
                item.kind = HD2DDrawItem::Kind::FLOOR_TILE;
                item.texture = floor_tex.tex;
                item.tex_src = floor_tex.def.frame_w > 0
                    ? SpriteRenderer::frame_rect(floor_tex.def, 0) : Rectangle{};
                // 贴图地板不再叠 tint (贴图自带配色; fog 由 renderer 压暗)
                if (item.texture.id > 0) item.tint = WHITE;
                // M6-v2c: LAVA tile 标记 → renderer 岩浆动画 shader 分流
                // tint 编码探索压暗: 可见=白(全亮) / 已探索不可见=60%灰
                // (与 _tile_color fog 压暗 40% 同语义; shader 端乘 fragColor.rgb)
                if (t == TileType::LAVA) {
                    item.is_lava = true;
                    item.texture = {};          // 岩浆走程序化噪声, 弃贴图
                    item.tint = map->isVisible(tx, ty) ? WHITE
                                                       : Color{153, 153, 153, 255};
                }
                // M6-v2h: 特殊房间地板 tint (2D 九色同源; triggered 压暗 55%)
                if (t == TileType::FLOOR || t == TileType::STAIRS_DOWN) {
                    const SpecialRoom* sr = map->get_special_room_at(tx, ty);
                    if (sr) {
                        item.tint = _special_room_tint(sr);
                        if (!map->isVisible(tx, ty))
                            item.tint = Color{
                                (unsigned char)(item.tint.r * 6 / 10),
                                (unsigned char)(item.tint.g * 6 / 10),
                                (unsigned char)(item.tint.b * 6 / 10), 255};
                        // N2: 特殊房间纯色覆盖 — 叠 FLOOR_DECAL 盖住底层贴图
                        // (2D 版 DrawRectangle 同效果: 直接替换地板外观)
                        HD2DDrawItem room_floor;
                        room_floor.kind = HD2DDrawItem::Kind::FLOOR_DECAL;
                        room_floor.world_pos = {
                            (float)tx * TILE_SIZE + TILE_SIZE * 0.5f,
                            0.02f,                      // 略高于 base floor (0.01f)
                            (float)ty * TILE_SIZE + TILE_SIZE * 0.5f
                        };
                        room_floor.size = (float)TILE_SIZE;
                        room_floor.tint = item.tint;
                        room_floor.texture = {};
                        room_floor.tex_src = {};
                        out.push_back(room_floor);
                    }
                }
                // M6-v2h: 楼梯 tint 换棕金阶调 (2D 60/48/26 系)
                if (t == TileType::STAIRS_DOWN && item.texture.id > 0
                    && !map->get_special_room_at(tx, ty))
                    item.tint = map->isVisible(tx, ty)
                        ? Color{150, 120, 70, 255} : Color{90, 72, 42, 255};
                // N5: 楼梯 4 级 3D 立方 (视觉 descending 沿 +Z 方向)
                if (t == TileType::STAIRS_DOWN && map->isVisible(tx, ty)) {
                    float cx = (float)tx * TILE_SIZE + TILE_SIZE * 0.5f;
                    float cz = (float)ty * TILE_SIZE + TILE_SIZE * 0.5f;
                    for (int s = 0; s < 4; s++) {
                        unsigned char cr = (unsigned char)(60 + s * 14);
                        unsigned char cg = (unsigned char)(48 + s * 12);
                        unsigned char cb = (unsigned char)(26 + s * 8);
                        HD2DDrawItem step;
                        step.kind = HD2DDrawItem::Kind::STAIR_STEP;
                        step.world_pos = {cx, 4.5f - (float)s * 1.8f,
                                          cz - 12.0f + (float)s * 8.0f};
                        step.size = TILE_SIZE - 4.0f - (float)s * 4.0f;
                        step.height = 8.0f;                        // Z 深度
                        step.tint = {cr, cg, cb, 255};
                        out.push_back(step);
                    }
                    // 金色箭头 — 3 个大号 FLOOR_DECAL (2D 570-575 chevron 语义)
                    for (int c = 0; c < 3; c++) {
                        HD2DDrawItem arrow;
                        arrow.kind = HD2DDrawItem::Kind::FLOOR_DECAL;
                        arrow.world_pos = {cx, 0.20f + (float)c * 0.02f,
                                           cz - 8.0f + (float)c * 8.0f};
                        arrow.size = 14.0f;
                        arrow.tint = {255, 200, 50, 240};
                        arrow.texture = {};
                        arrow.tex_src = {};
                        out.push_back(arrow);
                    }
                }
                // M6-j: 地板装饰 (2D 同款坐标哈希; 只在普通可见地板)
                if (t == TileType::FLOOR && map->isVisible(tx, ty)
                    && !map->get_special_room_at(tx, ty))
                    _apply_floor_decoration(*map, tx, ty, has_pal, pal,
                                            floor_tex.tex, item, out);
            }
            out.push_back(item);
            // M6-l: Boss FOV 红雾 — Boss 可见但玩家不可见的区域叠红色半透明
            // 2D: DrawRectangle({180,40,40,50}); 3D: FLOOR_DECAL 贴地红色 quad
            if (map->isBossVisible(tx, ty) && map->isExplored(tx, ty)
                && !map->isVisible(tx, ty)) {
                HD2DDrawItem fog;
                fog.kind = HD2DDrawItem::Kind::FLOOR_DECAL;
                fog.world_pos = item.world_pos;
                fog.world_pos.y = 0.06f;
                fog.size = TILE_SIZE;
                fog.tile_x = tx; fog.tile_y = ty;
                fog.tint = {180, 40, 40, 50};
                fog.texture = {};
                out.push_back(fog);
            }
        }
    }
}

// ── M6-m: 怪物 sprite key 映射 (与 monster.cpp _monster_sprite_key 同源) ──
static const char* _monster_sprite_key_for_3d(const Monster& m) {
    if (m.is_boss) return nullptr;
    switch (m.monster_type) {
        case MonsterType::BOMBER:    return "mon_bomber";
        case MonsterType::TANK:      return "mon_tank";
        case MonsterType::CHARGER:   return "mon_charger";
        case MonsterType::SUMMONER:  return "mon_summoner";
        case MonsterType::SHAMAN:    return "mon_shaman";
        default: break;
    }
    const auto& name = m.name;
    if (name.find("史莱姆") != std::string::npos) return "mon_slime";
    if (name.find("骨") != std::string::npos || name.find("骷髅") != std::string::npos)
        return "mon_skeleton";
    if (name.find("萨满") != std::string::npos || name.find("法师") != std::string::npos)
        return "mon_shaman";
    if (name.find("潜伏") != std::string::npos || name.find("潜行者") != std::string::npos)
        return "mon_shadow_stalker";
    if (name.find("刺客") != std::string::npos) return "mon_shadow_assassin";
    if (name.find("火魔") != std::string::npos) return "mon_fire_imp";
    if (name.find("守卫") != std::string::npos) return "mon_tank";
    if (name.find("兽人") != std::string::npos)
        return (name.find("精英") != std::string::npos) ? "mon_elite_orc" : "mon_orc";
    return "mon_orc";
}

static bool buildPlayerAvatar(const GameScene& scene, std::vector<HD2DDrawItem>& out) {
    const auto* avatar = scene.playerAvatar();
    if (!avatar || !avatar->active()) return false;
    const auto& player = *scene.player;
    const auto parts = avatar->worldParts(player);
    const auto& rect = player.entity.rect;
    const Vector3 feet = {rect.x + rect.width * 0.5f, 0, rect.y + rect.height * 0.5f};
    for (const auto& ghost : player.dodge.ghosts()) {
        const float alpha = 120.f * (1.f - ghost.age / DodgeComponent::kGhostLife);
        if (alpha <= 0) continue;
        const Vector3 ghost_feet = {feet.x + ghost.pos.x - player.entity.position.x,
                                   feet.y, feet.z + ghost.pos.y - player.entity.position.y};
        appendAvatarParts(parts, ghost_feet, ghost.pos.y,
                          static_cast<unsigned char>(alpha), 0, out);
    }
    // A5-T5-fix: avatar parts 不用 blob shadow (矩形 quad 可见), 完全依赖 depth shadow
    appendAvatarParts(parts, feet, rect.y, 255, 0.f, out);
    return true;
}

static void buildStaticPlayer(GameScene& gs, int anim_frame, std::vector<HD2DDrawItem>& out) {
    auto& res = ResourceManager::inst();
    HD2DDrawItem item;
    item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
    const auto& r = gs.player->entity.rect;
    item.world_pos = {r.x + r.width * 0.5f, 0, r.y + r.height * 0.5f};
    item.size = 36.0f;
    item.sort_y = r.y;
    item.outline = true;
    SpriteDef def;
    item.texture = res.sprite_by_key("player_default", def);
    if (item.texture.id > 0)
        item.tex_src = SpriteRenderer::frame_rect(def, anim_frame);
    else item.tint = {90, 160, 255, 255};
    item.flip_x = (gs.player->direction == Direction::LEFT);
    Vector2 sq = gs.player->dodge.squash_scale();
    item.scale_w = sq.x; item.scale_h = sq.y;
    for (const auto& g : gs.player->dodge.ghosts()) {
        float ga = 120.0f * (1.0f - g.age / DodgeComponent::kGhostLife);
        if (ga <= 0.0f) continue;
        HD2DDrawItem gh = item;
        gh.world_pos = {g.pos.x + r.width * 0.5f, 0, g.pos.y + r.height * 0.5f};
        gh.sort_y = g.pos.y;
        gh.tint = {255, 255, 255, (unsigned char)ga};
        gh.outline = false;
        out.push_back(gh);
    }
    out.push_back(item);
}

static void _build_entities(GameScene& gs, std::vector<HD2DDrawItem>& out,
                            bool part_color_ready) {
    auto& res = ResourceManager::inst();
    int anim_frame = ((int)(GetTime() * 4)) & 1;
    if (gs.player && gs.player->combat.is_alive
        && (!part_color_ready || !buildPlayerAvatar(gs, out)))
        buildStaticPlayer(gs, anim_frame, out);
    for (auto& m : gs.monsters) {
        if (!m || !m->combat.is_alive) continue;
        // A6-S1: 骨骼皮肤命中 → 逐件 pro 片 (无 blob shadow, 依赖 depth shadow), 否则旧 billboard 原样
        if (auto* skav = m->skeleton_avatar(); skav && skav->active()) {
            const auto& mr = m->entity.rect;
            const auto parts = skav->part_draws({}, skav->facing() < 0);
            if (!parts.empty()) {
                appendAvatarParts(parts, {mr.x + mr.width * 0.5f, 0, mr.y + mr.height * 0.5f},
                                  mr.y, 255, 0.f, out);
                continue;
            }
        }
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        const auto& r = m->entity.rect;
        item.world_pos = {r.x + r.width * 0.5f, 0, r.y + r.height * 0.5f};
        item.size = 34.0f;
        item.sort_y = r.y;
        item.outline = true;
        SpriteDef def;
        const char* skey = !m->sprite_override.empty() ? m->sprite_override.c_str()
                                                       : _monster_sprite_key_for_3d(*m);
        if (skey) item.texture = res.sprite_by_key(skey, def);
        if (item.texture.id > 0)
            item.tex_src = SpriteRenderer::frame_rect(def, anim_frame);
        else item.tint = {220, 80, 80, 255};
        out.push_back(item);
    }
}

// ── 特效: active_effects 存活项 → 发光片 ──
static void _build_effects(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (const auto& e : gs.active_effects) {
        if (e.elapsed >= e.duration) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::FX_QUAD;
        item.world_pos = {e.world_x, 0, e.world_y};
        item.size = e.radius;
        item.tint = e.color;
        out.push_back(item);
    }
}

// ── M6-v2a: 地面物品 → 贴地小 billboard (图标与 2D 同源) ──
static void _build_ground_items(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    auto& res = ResourceManager::inst();
    for (const auto& d : gs.dropped_items()) {
        if (gs.game_map && !gs.game_map->isVisible(d.tile_x, d.tile_y)) continue;
        const char* ikey = item_icon_key(d.item.get());
        if (!ikey) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        item.world_pos = {(float)d.tile_x * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                          (float)d.tile_y * TILE_SIZE + TILE_SIZE * 0.5f};
        item.size = 24.0f;                     // 拾取物小一号
        item.sort_y = (float)d.tile_y * TILE_SIZE;
        SpriteDef def;
        item.texture = res.sprite_by_key(ikey, def);
        if (item.texture.id <= 0) continue;     // 图标缺素材不画 (2D 有几何回退, 3D 跳过)
        item.tex_src = SpriteRenderer::frame_rect(def, 0);
        item.tint = WHITE;
        out.push_back(item);
    }
}

// ── M6-k: Arena 物件 → billboard/贴地 (爆炸桶/图腾/毒池/岩石/尖刺) ──
static void _build_arena_objects(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    const GameMap* map = gs.game_map.get();
    if (!map) return;
    auto& res = ResourceManager::inst();
    static bool logged = false;
    if (!logged) { logged = true; LOG_INFO("[M6k] arena_objects count=%d", (int)map->arena_objects.size()); }
    for (const auto& ao : map->arena_objects) {
        if (!ao.active) continue;
        if (!map->isVisible(ao.tile_x, ao.tile_y)) continue;
        int type_idx = static_cast<int>(ao.type);
        char key[32];
        snprintf(key, sizeof(key), "arena_prop_%d", type_idx);
        Color base = {100, 100, 100, 255};
        Texture2D tex = res.procedural_arena_prop(key, type_idx, base);
        if (tex.id <= 0) continue;
        HD2DDrawItem item;
        item.world_pos = {(float)ao.tile_x * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                          (float)ao.tile_y * TILE_SIZE + TILE_SIZE * 0.5f};
        item.tile_x = ao.tile_x;
        item.tile_y = ao.tile_y;
        item.size = 28.0f;
        item.sort_y = (float)ao.tile_y * TILE_SIZE;
        item.texture = tex;
        item.tex_src = {0, 0, 32, 32};
        if (ao.type == ArenaObjectType::POISON_POOL ||
            ao.type == ArenaObjectType::SPIKE) {
            item.kind = HD2DDrawItem::Kind::FLOOR_DECAL;
            item.world_pos.y = 0.08f;
            item.size = TILE_SIZE;
        } else {
            item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        }
        item.tint = WHITE;
        if (ao.type == ArenaObjectType::EXPLOSIVE_BARREL && ao.timer > 0.0f) {
            float pulse = 0.6f + 0.4f * sinf((float)GetTime() * 14.0f);
            item.tint = {255, (unsigned char)(150 * pulse), (unsigned char)(100 * pulse), 255};
        }
        out.push_back(item);
    }
}

// ── M6-v2a: 未完成 NPC → billboard (npc_sprite_key 楼层映射) ──
static void _build_npcs(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    auto& res = ResourceManager::inst();
    const char* skey = npc_sprite_key(gs.current_floor);
    for (const auto& npc : gs.npc_views()) {
        if (gs.game_map && !gs.game_map->isVisible(npc.tile_x, npc.tile_y)) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        item.world_pos = {(float)npc.tile_x * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                          (float)npc.tile_y * TILE_SIZE + TILE_SIZE * 0.5f};
        item.size = 34.0f;
        item.sort_y = (float)npc.tile_y * TILE_SIZE;
        item.outline = true;                   // A1.1: NPC 与玩家/怪同等待遇描边
        SpriteDef def;
        item.texture = res.sprite_by_key(skey, def);
        if (item.texture.id <= 0) continue;     // 缺素材回退: 2D 有绿点, 3D 跳过
        item.tex_src = SpriteRenderer::frame_rect(def, 0);
        item.tint = WHITE;
        out.push_back(item);
    }
}

// ── M6-v2h: 特殊房间中心图标 → 贴地小 billboard (2D room_* 素材同源) ──
// triggered 后不画 (2D 同条件); 缺素材跳过
static void _build_special_rooms(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    const GameMap* map = gs.game_map.get();
    if (!map) return;
    auto& res = ResourceManager::inst();
    for (const auto& sr : map->special_rooms) {
        if (sr.type == SpecialRoomType::CHALLENGE) continue;   // 传送门另有绘制
        if (sr.triggered) continue;

        // N2: 菱形嵌纹 — 房间内每个地板 tile 中心添加亮色 diamond decal
        Color inlay_base = _special_room_tint(&sr);
        Color inlay = { (unsigned char)(inlay_base.r * 2 > 255 ? 255 : inlay_base.r * 2),
                        (unsigned char)(inlay_base.g * 2 > 255 ? 255 : inlay_base.g * 2),
                        (unsigned char)(inlay_base.b * 2 > 255 ? 255 : inlay_base.b * 2),
                        150 };
        int sx0 = std::max(0, sr.rx), sx1 = std::min(map->width, sr.rx + sr.rw);
        int sy0 = std::max(0, sr.ry), sy1 = std::min(map->height, sr.ry + sr.rh);
        for (int ty = sy0; ty < sy1; ty++) {
            for (int tx = sx0; tx < sx1; tx++) {
                if (map->tile_at(tx, ty) != TileType::FLOOR) continue;
                HD2DDrawItem decal;
                decal.kind = HD2DDrawItem::Kind::FLOOR_DECAL;
                decal.world_pos = {
                    (float)tx * TILE_SIZE + TILE_SIZE * 0.5f,
                    0.12f,                          // 高于地板+tint，避免 z-fight
                    (float)ty * TILE_SIZE + TILE_SIZE * 0.5f
                };
                decal.size = TILE_SIZE * 0.35f;     // 菱形尺寸 (tile 中心小 diamond)
                decal.tint = inlay;
                decal.texture = {};
                decal.tex_src = {};
                out.push_back(decal);
            }
        }

        // 中心图标 (2D room_* 素材同源)
        const char* ikey = _special_room_icon_key(sr.type);
        if (!ikey) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ROOM_ICON;
        item.world_pos = {(float)sr.cx * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                          (float)sr.cy * TILE_SIZE + TILE_SIZE * 0.5f};
        item.size = TILE_SIZE * 0.75f;
        item.sort_y = (float)sr.cy * TILE_SIZE;
        SpriteDef def;
        item.texture = res.sprite_by_key(ikey, def);
        if (item.texture.id <= 0) continue;
        item.tex_src = SpriteRenderer::frame_rect(def, 0);
        item.tint = WHITE;
        out.push_back(item);
    }
}

// ── M6-v2a: 挑战传送门 → 竖立脉冲光环 (2D 双层圆的 3D 对应物) ──
// 与 2D 分支 (game_scene._render 2222-2237) 同条件: DUNGEON 入口 / ARENA 返回
static void _build_portals(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    const auto& challenge = gs.challenge_ctrl();
    const GameMap* map = gs.game_map.get();
    if (challenge.phase() == ChallengePhase::PORTAL_ACTIVE && map) {
        for (const auto& sr : map->special_rooms) {
            if (sr.type != SpecialRoomType::CHALLENGE) continue;
            HD2DDrawItem item;
            item.kind = HD2DDrawItem::Kind::PORTAL_RING;
            item.world_pos = {(float)sr.portal_tx * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                              (float)sr.portal_ty * TILE_SIZE + TILE_SIZE * 0.5f};
            item.height = 18.0f;               // 门环半径 (贴地圆心)
            item.size = 0.16f;                 // 环管粗细比例
            item.portal_entry = true;
            out.push_back(item);
            break;                             // 与 2D 同: 只画第一个挑战房
        }
    }
    if (challenge.phase() == ChallengePhase::CLEARED && gs.in_challenge_arena() &&
        challenge.return_portal_tx() >= 0) {
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::PORTAL_RING;
        item.world_pos = {(float)challenge.return_portal_tx() * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                          (float)challenge.return_portal_ty() * TILE_SIZE + TILE_SIZE * 0.5f};
        item.height = 18.0f;
        item.size = 0.16f;
        item.portal_entry = false;
        out.push_back(item);
    }
}

// ── M6-v2b: 投射物 — WARNING 相 (AOE圈/轨迹线) + ACTIVE 相 (发光弹体) ──
// 条件与配色逐条对齐 2D 分支 (game_scene._render 2234-2275)
static void _build_projectiles(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (const auto& p : gs.projectiles) {
        if (!p.alive) continue;
        HD2DDrawItem item;
        item.world_pos = {p.pos.x, 12.0f, p.pos.y};   // 弹道离地 12px

        if (p.active_time < 0.0f) {
            // WARNING 相: 2D 配色 (红/橙/黄 三级) + 1-fade 递增警示
            float fade = 1.0f - (-p.active_time / p.warning_time);
            Color wc = (p.warning_level >= 2) ? Color{255,40,20,120}
                     : (p.warning_level >= 1) ? Color{255,160,30,120}
                     : Color{255,200,60,110};
            if (p.warning_radius > 0.0f) {
                // AOE 危险圈 → 贴地预警 ring
                item.kind = HD2DDrawItem::Kind::WARNING_RING;
                item.world_pos.y = 0.1f;
                item.size = p.warning_radius;
                item.tint = wc;
                item.height = fade;                    // renderer 递增脉冲
                out.push_back(item);
            } else {
                // 点弹 → 轨迹线 (终点 = 撞墙/寿命终点, 与 2D _preview 同算法)
                float speed = sqrtf(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
                if (speed < 1.0f) continue;
                float ux = p.vel.x / speed, uy = p.vel.y / speed;
                float len = speed * p.lifetime;
                if (gs.game_map) {
                    for (float d = TILE_SIZE; d < len; d += TILE_SIZE) {
                        auto [tx, ty] = gs.game_map->pixel_to_tile(
                            p.pos.x + ux * d, p.pos.y + uy * d);
                        if (!gs.game_map->is_walkable(tx, ty)) { len = d; break; }
                    }
                }
                item.kind = HD2DDrawItem::Kind::TRAJECTORY_LINE;
                item.end_pos = {p.pos.x + ux * len, 12.0f, p.pos.y + uy * len};
                item.tint = wc;
                item.height = fade;
                out.push_back(item);
            }
            continue;
        }
        // ACTIVE 相: 弹体 (穿透金 / 敌元素色 / 玩家土色; 2D 同源)
        item.kind = HD2DDrawItem::Kind::PROJECTILE_BODY;
        item.piercing = p.piercing;
        item.element = p.element;
        item.tint = p.owner != 0 ? Color{255, 80, 40, 255}
                   : Color{200, 160, 100, 255};
        item.size = 6.0f;
        item.trail_dir = p.vel;                 // M6-v2d: 拖尾方向 (px/s)
        out.push_back(item);
    }
}

// ── M6-v2b: 远程武器射程指示环 (玩家 range_indicator_timer 激活时) ──
// NUNCHAKU=双环带 / SPEAR,CROSSBOW=单环 (对齐 2D 2277-2319)
static void _build_range_indicator(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    if (!gs.player || gs.player->weapon.range_indicator_timer <= 0.0f) return;
    auto wt = gs.player->weapon.weapon_type();
    if (wt != WeaponType::SPEAR && wt != WeaponType::CROSSBOW
        && wt != WeaponType::NUNCHAKU) return;
    const auto& r = gs.player->entity.rect;
    HD2DDrawItem item;
    item.kind = HD2DDrawItem::Kind::WARNING_RING;
    item.world_pos = {r.x + r.width * 0.5f, 0.1f, r.y + r.height * 0.5f};
    item.tint = {235, 175, 95, 200};                 // 2D 同款暖金
    item.height = gs.player->weapon.range_indicator_timer / 0.25f;  // fade
    if (wt == WeaponType::NUNCHAKU) {
        const WeaponDef* def = gs.player->weapon.current_def();
        item.size = (def ? def->max_range : 5.0f) * TILE_SIZE;      // 外环
        item.element = (def ? def->min_range : 2.0f) * TILE_SIZE;    // 内环(复用)
    } else {
        item.size = gs.player->weapon.range_indicator_px;
        item.element = -1.0f;                        // -1 = 单环
    }
    out.push_back(item);
}

// ── M6-v2b: Boss 技能预警 — 弹幕弹道/扇形面/瞬移落点/旋风圈 (只读 BossAI) ──
// 条件对齐 2D 分支 (game_scene._render 2754-2762: is_boss && ai)
static void _build_boss_skill_warnings(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (auto& m : gs.monsters) {
        if (!m || !m->is_boss || !m->ai || !m->combat.is_alive) continue;
        auto* bai = dynamic_cast<BossAI*>(m->ai);
        if (!bai) continue;
        const auto& r = m->entity.rect;
        Vector3 bpos = {r.x + r.width * 0.5f, 0, r.y + r.height * 0.5f};

        // 弹幕: 每颗在飞 shot → 轨迹线 (2D BarrageSkill::draw 同源)
        if (auto* sk = bai->barrage_skill()) {
            for (const auto& s : sk->shots) {
                if (s.life <= 0.0f) continue;
                float speed = sqrtf(s.vx * s.vx + s.vy * s.vy);
                if (speed < 1.0f) continue;
                HD2DDrawItem item;
                item.kind = HD2DDrawItem::Kind::TRAJECTORY_LINE;
                item.world_pos = {s.x, 10.0f, s.y};
                item.end_pos = {s.x + s.vx / speed * 60.0f, 10.0f,
                                s.y + s.vy / speed * 60.0f};
                item.tint = {255, 120, 60, 130};
                item.height = 1.0f;
                out.push_back(item);
            }
            // 蓄力期: 风扇形预警 (朝玩家; half=spread/2)
            if (sk->windup_left > 0.0f && gs.player) {
                const auto& pr = gs.player->entity.rect;
                float ang = atan2f(pr.y + pr.height*0.5f - bpos.z,
                                   pr.x + pr.width*0.5f - bpos.x);
                HD2DDrawItem item;
                item.kind = HD2DDrawItem::Kind::CONE_FAN;
                item.world_pos = bpos;
                item.size = 110.0f;
                item.fan_angle = ang;
                item.fan_half_deg = sk->spread_deg * 0.5f;
                item.tint = {255, 160, 40, 150};
                out.push_back(item);
            }
        }
        // 扇形斩: 蓄力期面预警 (对齐 2D cone_skill().draw windup)
        if (auto* sk = bai->cone_skill()) {
            if (sk->windup_left > 0.0f && gs.player) {
                const auto& pr = gs.player->entity.rect;
                float ang = atan2f(pr.y + pr.height*0.5f - bpos.z,
                                   pr.x + pr.width*0.5f - bpos.x);
                HD2DDrawItem item;
                item.kind = HD2DDrawItem::Kind::CONE_FAN;
                item.world_pos = bpos;
                item.size = sk->reach;
                item.fan_angle = ang;
                item.fan_half_deg = sk->half_angle;
                item.tint = {255, 60, 40, 160};
                out.push_back(item);
            }
        }
        // 瞬移: 蓄力期落点圈 (2D BlinkSkill::draw pending 位置)
        if (auto* sk = bai->blink_skill()) {
            if (sk->windup_left > 0.0f) {
                HD2DDrawItem item;
                item.kind = HD2DDrawItem::Kind::WARNING_RING;
                item.world_pos = {sk->pending_x, 0.1f, sk->pending_y};
                item.size = 16.0f;
                item.tint = {180, 120, 255, 170};
                item.height = 1.0f;             // fade=1
                item.element = -1.0f;
                out.push_back(item);
            }
        }
        // 旋风: 蓄力白环 / 旋转期紫圈 (半径 = 范围 2.2 tile)
        if (auto* sk = bai->whirlwind_skill()) {
            if (sk->windup_left > 0.0f || sk->spin_duration > 0.0f) {
                bool spinning = sk->spin_duration > 0.0f;
                HD2DDrawItem item;
                item.kind = HD2DDrawItem::Kind::WARNING_RING;
                item.world_pos = bpos;
                item.world_pos.y = 0.1f;
                item.size = spinning ? 80.0f : 70.0f;
                item.tint = spinning ? Color{170, 90, 255, 170}
                                     : Color{240, 240, 255, 170};
                item.height = 1.0f;
                item.element = -1.0f;
                out.push_back(item);
            }
        }
    }
}

// ── M6-v2b: Boss 战场危险区 — 岩浆/影墙/虚空 贴地危险圈 (2D arena.draw 同源) ──
static void _build_danger_zones(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (const auto& z : gs.boss_ctrl().arena.zones()) {
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::WARNING_RING;
        item.world_pos = {z.world_x, 0.1f, z.world_y};
        item.size = z.radius;
        // 警戒期=橙黄脉冲 / 激活期=红; 2D is_warning 同条件
        item.tint = z.is_warning() ? z.warn_color : z.active_color;
        item.height = z.is_warning() ? 0.7f : 1.0f;   // fade
        item.element = -1.0f;                          // 单环
        out.push_back(item);
    }
}

// ── M6-v2b: 弱点光环 (F10.2 pulse ring) + Tank 守护连线 (2D 2743-2781 同源) ──
static void _build_monster_overlays(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (auto& m : gs.monsters) {
        if (!m || !m->combat.is_alive) continue;
        const auto& r = m->entity.rect;
        Vector3 c = {r.x + r.width * 0.5f, 0, r.y + r.height * 0.5f};
        // F10.2: 弱点脉冲环 (橙)
        if (m->is_weak_point) {
            HD2DDrawItem item;
            item.kind = HD2DDrawItem::Kind::WARNING_RING;
            item.world_pos = {c.x, 0.1f, c.z};
            item.size = 14.0f;
            item.tint = {255, 120, 30, 180};
            item.height = 1.0f;
            item.element = -1.0f;
            out.push_back(item);
        }
        // D2 Step4: Tank 守护连线 (淡蓝; 前排 → 保护目标)
        if (m->ai && m->team_role == TeamRole::FRONTLINE && m->ai->_protect_target) {
            auto* t = m->ai->_protect_target;
            HD2DDrawItem item;
            item.kind = HD2DDrawItem::Kind::ENTITY_LINK;
            item.world_pos = {c.x, 10.0f, c.z};
            const auto& tr = t->entity.rect;
            item.end_pos = {tr.x + tr.width * 0.5f, 10.0f, tr.y + tr.height * 0.5f};
            item.tint = {60, 140, 255, 100};
            out.push_back(item);
        }
    }
}

// ── A2.1: 氛围粒子 → 群系性格微光 (dust 尘埃 / ember 余烬 / firefly 幽光) ──
// 数据仍与 2D 共享 AmbientLayer (零逻辑改动); 本层只做"观感"翻译:
// 高度分层 + 风格摆动脉络全部用 GetTime 与 spawn 稳定字段推相位
// (与 v2a 呼吸帧同源, 非随机; sim 无头不跑本层, 不触 RNG 红线)
// A2.2: 风格优先读 biomes.json ambient.style (数据驱动), 缺省按 id 回退
static MoteStyle _mote_style_for_floor(int floor) {
    const BiomeDef* b = get_biome_for_floor(floor);
    if (!b) return MoteStyle::DUST;
    if (b->id == "ash_volcano") return MoteStyle::EMBER;
    if (b->id == "void_abyss")  return MoteStyle::FIREFLY;
    return MoteStyle::DUST;
}

static MoteStyle _mote_style_from_cfg(const AmbientCfg& cfg, int floor) {
    if (cfg.style == "ember")   return MoteStyle::EMBER;
    if (cfg.style == "firefly") return MoteStyle::FIREFLY;
    if (cfg.style == "dust")    return MoteStyle::DUST;
    return _mote_style_for_floor(floor);
}

// 稳定相位源: vx/size 在同一次 spawn 生命周期内不变 → 哈希成 [0,2π)
static float _mote_phase(const AmbientParticle& p) {
    unsigned int hx = (unsigned int)(int)(p.vx * 4096.0f);
    unsigned int hs = (unsigned int)(int)(p.size * 1000.0f);
    return (float)((hx ^ (hs * 2654435761u)) & 0xFFFFu) / 65535.0f * 6.28318f;
}

// 风格运动参数 (偏移/高度/尺寸倍率/脉搏) — 纯三角函数, 无状态无随机
struct MoteMotion { float dx, y, dz, size_mul, pulse; };

static MoteMotion _mote_motion(const MoteStyle style, float t, float ph,
                               float fade) {
    MoteMotion m{0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    switch (style) {
    case MoteStyle::DUST:                              // 尘埃: 贴地慢摆, 弱闪
        m.y = 4.0f + 16.0f * (1.0f - fade);
        m.dx = sinf(t * 0.7f + ph) * 2.0f;
        m.dz = cosf(t * 0.5f + ph) * 1.5f;
        m.size_mul = 0.9f;
        m.pulse = 0.55f + 0.2f * sinf(t * 1.3f + ph * 2.0f);
        break;
    case MoteStyle::EMBER:                             // 余烬: 急升蜿蜒, 熄灭渐暗
        m.y = 8.0f + 64.0f * (1.0f - fade);
        m.dx = sinf(t * 2.2f + ph) * 3.5f;
        m.dz = cosf(t * 1.8f + ph * 1.3f) * 2.5f;
        m.size_mul = 0.8f;
        m.pulse = fade * (0.6f + 0.4f * sinf(t * 6.0f + ph));
        break;
    case MoteStyle::FIREFLY: {                         // 幽光: 悬浮 bob, 强明灭
        m.y = 30.0f + 15.0f * sinf(t * 0.8f + ph);
        m.dz = sinf(t * 0.45f + ph * 1.7f) * 6.0f;
        m.size_mul = 1.15f;
        float blink = 0.5f + 0.5f * sinf(t * 2.4f + ph);
        m.pulse = 0.3f + 0.7f * blink * blink;
        break;
    }
    }
    return m;
}

static void _build_ambient(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    const auto& ambient = gs.ambient_layer();
    const auto& cfg = ambient.config();
    const MoteStyle style = _mote_style_from_cfg(cfg, gs.current_floor);
    // A2.2: 群系粒子贴图 (缺失→id=0→renderer 程序化软光回退)
    Texture2D mote_tex = cfg.texture.empty()
        ? Texture2D{} : ResourceManager::inst().load_texture(cfg.texture.c_str());
    const float t = (float)GetTime();
    for (const auto& p : ambient.particles()) {
        if (p.life <= 0.0f) continue;
        const float fade = p.life / p.max_life;            // 1→0 生命比
        const MoteMotion m = _mote_motion(style, t, _mote_phase(p), fade);
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::AMBIENT_MOTE;
        item.mote_style = style;
        item.texture = mote_tex;
        item.size = p.size * 2.0f * m.size_mul;            // 半径→直径感
        item.world_pos = {p.x + m.dx, m.y, p.y + m.dz};
        item.tint = cfg.color;
        item.height = fade;                                // 信息保留 (渲染端已含)
        // 首尾渐隐包络 (与 2D draw 同式) × 风格脉搏
        float env = std::min(1.0f, fade * 2.0f)
                  * std::min(1.0f, (p.max_life - p.life) * 2.0f + 0.3f);
        int a = (int)((float)p.alpha * env * m.pulse);
        item.tint.a = (unsigned char)(a > 255 ? 255 : (a < 0 ? 0 : a));
        out.push_back(item);
    }
}

// M6-n N1: 脚印 — 复用 2D 版 GameMap::_footsteps 数据，HD2D 表现层贴地 decal
static void _build_footsteps(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    const GameMap* map = gs.game_map.get();
    if (!map) return;
    const auto* fs_arr = map->get_footsteps();
    int head = map->get_footstep_head();
    int max_steps = map->get_footstep_max();

    for (int i = 0; i < max_steps; i++) {
        const auto& fs = fs_arr[i];
        if (fs.life <= 0.0f) continue;           // 生命周期结束
        if (!map->isExplored(fs.tx, fs.ty)) continue;  // 未探索不显示

        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::FLOOR_DECAL;
        item.world_pos = {
            (float)fs.tx * TILE_SIZE + TILE_SIZE * 0.5f,
            0.10f,                                  // 高于地板+decal，避免 z-fight
            (float)fs.ty * TILE_SIZE + TILE_SIZE * 0.5f
        };
        item.size = TILE_SIZE * 0.55f;              // 脚印尺寸 (椭圆短轴)
        // alpha = life / 2.5s × 140 (提高可见度，暗色地板上清晰)
        float fade = fs.life / 2.5f;
        item.tint = {220, 200, 130, (unsigned char)(140.0f * fade)};
        item.texture = {};                          // 纯色椭圆 (2D 版回退方案)
        item.tex_src = {};
        out.push_back(item);
    }
}

void build_scene(GameScene& gs, std::vector<HD2DDrawItem>& out_items,
                 bool part_color_ready) {
    _build_terrain(gs, out_items);
    _build_footsteps(gs, out_items);
    _build_entities(gs, out_items, part_color_ready);
    _build_effects(gs, out_items);
    _build_ground_items(gs, out_items);   // M6-v2a
    _build_arena_objects(gs, out_items);  // M6-k
    _build_special_rooms(gs, out_items);   // M6-v2h
    _build_npcs(gs, out_items);           // M6-v2a
    _build_portals(gs, out_items);        // M6-v2a
    _build_projectiles(gs, out_items);    // M6-v2b
    _build_range_indicator(gs, out_items);// M6-v2b
    _build_boss_skill_warnings(gs, out_items);  // M6-v2b
    _build_danger_zones(gs, out_items);         // M6-v2b
    _build_monster_overlays(gs, out_items);     // M6-v2b
    _build_ambient(gs, out_items);              // M6-v2e
}

} // namespace hd2d
