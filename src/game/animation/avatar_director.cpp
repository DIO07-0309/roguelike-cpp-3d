// G12-6: AvatarDirector 实现 —— 从 game_scene.cpp 原样搬出, 主游戏/教程共用
#include "game/animation/avatar_director.h"

#include <string>
#include "core/logger.h"
#include "game/entities/player.h"
#include "game/entities/monster.h"
#include "raylib.h"

void AvatarDirector::ensure_player(Player* player) {
    if (!player || _player_avatar) return;
    auto avatar = std::make_unique<PlayerAvatar>();
    std::string avatar_err;
    if (avatar->try_init("resources/animations", avatar_err))
        LOG_INFO("A5: player avatar active (skeletal)");
    else
        LOG_WARN("A5: avatar inactive, fallback static (%s)", avatar_err.c_str());
    _player_avatar = std::move(avatar);
}

void AvatarDirector::tick_player(const Player* player) {
    if (_player_avatar && _player_avatar->active() && player)
        _player_avatar->update(GetFrameTime(), *player);
}

// actor_avatars.json 白名单 —— 怪物/NPC 共用同一份, 只载一次
void AvatarDirector::ensure_defs() {
    if (_actor_avatars_loaded) return;
    _actor_avatars_loaded = true;
    std::string conf_err;
    auto conf = load_actor_avatars_file("resources/animations/actor_avatars.json", conf_err);
    if (conf) _actor_avatars = std::move(*conf);
    else LOG_WARN("A6: actor_avatars.json invalid, all fallback (%s)", conf_err.c_str());
}

void AvatarDirector::tick_monsters(const std::vector<std::unique_ptr<Monster>>& monsters) {
    ensure_defs();
    if (_actor_avatars.empty()) return;    // 白名单空 = 零开销全回退
    const float dt = GetFrameTime();
    const float now_wall = (float)GetTime();
    for (auto& m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        if (!m->skeleton_avatar()) {
            auto it = _actor_avatars.find(monster_actor_key(*m));
            if (it == _actor_avatars.end()) continue;
            auto avatar = std::make_unique<SkeletonAvatar>();
            std::string avatar_err;
            if (avatar->try_init(it->second.skeleton, it->second.anim, avatar_err))
                LOG_INFO("A6: monster avatar active (%s)", it->first.c_str());
            else
                LOG_WARN("A6: monster avatar inactive (%s): %s",
                         it->first.c_str(), avatar_err.c_str());
            m->set_skeleton_avatar(std::move(avatar));   // 成功/失败都缓存不重试
        }
        auto* avatar = m->skeleton_avatar();
        if (!avatar || !avatar->active()) continue;
        avatar->advance(dt, monster_anim_input(*m, avatar->hp_state(), now_wall));
        avatar->track_facing(m->entity.position);        // 渲染层朝向镜像
    }
}
