#pragma once
#include "systems/relic_effect_processor.h"

class GameScene;
struct Monster;

class GameSceneCombat {
public:
    explicit GameSceneCombat(GameScene& scene) : _s(scene) {}

    void on_monster_killed(Monster* m);
    void cleanup_dead_monsters();
    void apply_pending_damage();

    RelicEffectProcessor& relic_fx() { return _relic_fx; }

private:
    void applyKillFeedback(Monster* m);
    void grantKillXp(Monster* m);
    void applyBossReward(Monster* m);
    // G53: applyBossReward 拆出的三段 (播报 / 世界flag / 掉落)
    void _play_boss_death_feedback(Monster* m);
    void _apply_boss_kill_flags();
    void _grant_boss_drops(Monster* m);
    void applyNormalLoot(Monster* m);
    void applyKillRelics(Monster* m);
    void applyNewRelicEffects(Monster* m);
    void refreshStairsIfCleared();
    GameScene& _s;
    RelicEffectProcessor _relic_fx;
};
