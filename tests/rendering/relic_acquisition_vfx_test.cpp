// 圣物获得演出测试 — 31 映射完整性 / recipe 合法性 / 两两不同 / hook 副作用隔离
#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "data/vfx_recipe.h"
#include "systems/relic_acquisition_vfx.h"
#include "systems/relic_vfx_hook.h"
#include "types/combat_types.h"

namespace {

const char* kMapPath = "resources/relic_acquisition_vfx.json";
const char* kRecipePath = "resources/vfx_recipes.json";

std::set<std::string> valid_kinds() {
    return {"ring", "beam", "bolt", "lightning", "explosion", "shockwave",
            "slash_arc", "cone", "smoke", "spark", "aura", "flash"};
}

}  // namespace

TEST(RelicAcquisitionVfx, MappingLoads31) {
    EXPECT_TRUE(RelicAcquisitionVfx::load(kMapPath));
    EXPECT_EQ(RelicAcquisitionVfx::mapping_count(), 31u);
}

TEST(RelicAcquisitionVfx, EveryRecipeExistsAndUsesValidKinds) {
    ASSERT_TRUE(RelicAcquisitionVfx::load(kMapPath));
    ASSERT_TRUE(load_vfx_recipes(kRecipePath));
    auto kinds = valid_kinds();
    // 31 个已知圣物 id（映射表内容）逐一验证 recipe 存在且全步合法
    const char* ids[] = {
        "frozen_crystal", "glacier_shard", "winter_crown", "absolute_zero",
        "storm_caller", "lightning_rod", "divine_storm", "crimson_blade",
        "sacrificial_dagger", "hemoplague", "shadow_cloak", "assassin_mark",
        "night_veil", "chrono_stone", "paradox_orb", "chaos_dice", "magic_quill",
        "god_hand", "bastion_shield", "unyielding", "iron_determination",
        "phoenix_rebirth", "phoenix_feather", "soul_chain", "necromancer_tome",
        "swarm_queen", "spirit_army", "ignition_ring", "inferno_core",
        "venom_vein", "plague_heart"
    };
    for (const char* id : ids) {
        ASSERT_TRUE(RelicAcquisitionVfx::has_mapping(id)) << id;
        const VFXRecipe* r = get_vfx_recipe(std::string("relic_gain_") + id);
        ASSERT_NE(r, nullptr) << id;
        EXPECT_FALSE(r->steps.empty()) << id;
        for (const auto& s : r->steps)
            EXPECT_TRUE(kinds.count(s.type)) << id << " kind=" << s.type;
    }
}

TEST(RelicAcquisitionVfx, RecipesArePairwiseDistinct) {
    ASSERT_TRUE(load_vfx_recipes(kRecipePath));
    std::set<std::string> signatures;
    for (const auto& [id, recipe] : get_all_vfx_recipes()) {
        if (id.rfind("relic_gain_", 0) != 0) continue;
        std::string sig;
        for (const auto& s : recipe.steps) {
            sig += s.type + ":" + std::to_string((int)s.radius) + "/" +
                   std::to_string(s.count) + "/" + std::to_string(s.layers) + "/";
            sig += std::to_string((int)(s.delay * 1000)) + "|";
        }
        EXPECT_TRUE(signatures.insert(sig).second) << "duplicate recipe " << id;
    }
    EXPECT_EQ(signatures.size(), 31u);
}

TEST(RelicAcquisitionVfx, NoOpHookHasNoPending) {
    RelicAcquisitionVfx::uninstall();
    fire_relic_vfx_hook("frozen_crystal", 100.f, 200.f);  // no hook → no-op
    EXPECT_EQ(RelicAcquisitionVfx::pending_count(), 0u);
}

TEST(RelicAcquisitionVfx, HookQueuesAndDrainsIntoEffects) {
    ASSERT_TRUE(RelicAcquisitionVfx::load(kMapPath));
    ASSERT_TRUE(load_vfx_recipes(kRecipePath));
    RelicAcquisitionVfx::uninstall();
    RelicAcquisitionVfx::install();
    fire_relic_vfx_hook("absolute_zero", 100.f, 200.f);
    EXPECT_EQ(RelicAcquisitionVfx::pending_count(), 1u);
    std::vector<Effect> out;
    RelicAcquisitionVfx::process_queue(out);
    EXPECT_EQ(RelicAcquisitionVfx::pending_count(), 0u);
    EXPECT_FALSE(out.empty());
    RelicAcquisitionVfx::uninstall();
}

TEST(RelicAcquisitionVfx, UnmappedRelicProducesNoEffect) {
    ASSERT_TRUE(RelicAcquisitionVfx::load(kMapPath));
    RelicAcquisitionVfx::uninstall();
    RelicAcquisitionVfx::install();
    fire_relic_vfx_hook("__not_a_relic__", 0.f, 0.f);
    std::vector<Effect> out;
    RelicAcquisitionVfx::process_queue(out);
    EXPECT_TRUE(out.empty());
    RelicAcquisitionVfx::uninstall();
}

TEST(RelicAcquisitionVfx, QueueAccumulatesAndUninstallClears) {
    ASSERT_TRUE(RelicAcquisitionVfx::load(kMapPath));
    RelicAcquisitionVfx::uninstall();
    RelicAcquisitionVfx::install();
    fire_relic_vfx_hook("frozen_crystal", 0.f, 0.f);
    fire_relic_vfx_hook("glacier_shard", 0.f, 0.f);
    EXPECT_EQ(RelicAcquisitionVfx::pending_count(), 2u);  // 两次获得各入队一次
    RelicAcquisitionVfx::uninstall();
    EXPECT_EQ(RelicAcquisitionVfx::pending_count(), 0u);  // uninstall 清空
}

TEST(RelicAcquisitionVfx, NoHookRegistrationMeansZeroPending) {
    // 模拟 sim: 不调用 install → hook 槽为空 → fire 完全 no-op
    RelicAcquisitionVfx::uninstall();
    for (int i = 0; i < 5; i++) fire_relic_vfx_hook("absolute_zero", 10.f, 10.f);
    EXPECT_EQ(RelicAcquisitionVfx::pending_count(), 0u);
}
