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
    void applyNormalLoot(Monster* m);
    void applyKillRelics(Monster* m);
    void applyNewRelicEffects(Monster* m);
    void refreshStairsIfCleared();
    GameScene& _s;
    RelicEffectProcessor _relic_fx;
};
