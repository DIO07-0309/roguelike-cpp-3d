// G9: Weapon system unit tests
#include <gtest/gtest.h>
#include <cmath>
#include "data/weapon_defs.h"
#include "systems/weapon_component.h"
#include "systems/hit_detection.h"

// ═══════════════════════════════════════════════════
// WeaponDef loading tests
// ═══════════════════════════════════════════════════

TEST(WeaponDefLoad, FistBasicExists) {
    // Load from resources/weapons.json
    bool ok = load_weapon_defs("resources/weapons.json");
    EXPECT_TRUE(ok);
    const WeaponDef* fist = get_weapon_def("fist_basic");
    ASSERT_NE(fist, nullptr);
    EXPECT_EQ(fist->type, WeaponType::FIST);
    EXPECT_EQ(fist->stage_count, 1);
    EXPECT_FLOAT_EQ(fist->stages[0].damage_multiplier, 1.0f);
}

TEST(WeaponDefLoad, DaggerHas3Stages) {
    load_weapon_defs("resources/weapons.json");
    const WeaponDef* dagger = get_weapon_def("dagger_common");
    ASSERT_NE(dagger, nullptr);
    EXPECT_EQ(dagger->type, WeaponType::DAGGER);
    EXPECT_EQ(dagger->stage_count, 3);
    // Stage 3 should be thrust with 1.15x multiplier
    EXPECT_GT(dagger->stages[2].damage_multiplier, 1.0f);
    EXPECT_EQ(dagger->stages[2].hit_shape, HitShape::CAPSULE);
}

TEST(WeaponDefLoad, All6TypesLoad) {
    load_weapon_defs("resources/weapons.json");
    for (int i = 0; i < (int)WeaponType::COUNT; ++i) {
        auto wt = (WeaponType)i;
        auto list = get_weapon_defs_for_type(wt);
        EXPECT_FALSE(list.empty()) << "No weapons for type " << weapon_type_name(wt);
    }
}

TEST(WeaponDefLoad, QualityNamesPopulated) {
    load_weapon_defs("resources/weapons.json");
    const WeaponDef* sw = get_weapon_def("sword_legendary");
    ASSERT_NE(sw, nullptr);
    EXPECT_FALSE(sw->name.empty());
    EXPECT_FALSE(sw->legendary_name.empty());
    EXPECT_FALSE(sw->rare_names[0].empty());
    EXPECT_FALSE(sw->epic_names[0].empty());
}

// ═══════════════════════════════════════════════════
// WeaponComponent tests
// ═══════════════════════════════════════════════════

class WeaponComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        load_weapon_defs("resources/weapons.json");
    }
};

TEST_F(WeaponComponentTest, DefaultIsFist) {
    WeaponComponent wc;
    EXPECT_EQ(wc.weapon_type(), WeaponType::FIST);
    EXPECT_STREQ(wc.current_weapon_id(), "fist_basic");
}

TEST_F(WeaponComponentTest, EquipDagger) {
    WeaponComponent wc;
    wc.equip("dagger_common");
    EXPECT_EQ(wc.weapon_type(), WeaponType::DAGGER);
}

TEST_F(WeaponComponentTest, EquipAndUnequip) {
    WeaponComponent wc;
    wc.equip("sword_common");
    EXPECT_EQ(wc.weapon_type(), WeaponType::SWORD);
    wc.unequip();
    EXPECT_EQ(wc.weapon_type(), WeaponType::FIST);
}

TEST_F(WeaponComponentTest, ComboAdvances) {
    WeaponComponent wc;
    wc.equip("dagger_common");
    EXPECT_EQ(wc.combo_index(), 0);
    wc.execute_attack(1.0);
    EXPECT_EQ(wc.combo_index(), 1);
    wc.execute_attack(1.3);
    EXPECT_EQ(wc.combo_index(), 2);
}

TEST_F(WeaponComponentTest, ComboTimeoutResets) {
    WeaponComponent wc;
    wc.equip("dagger_common");
    wc.execute_attack(1.0);
    EXPECT_EQ(wc.combo_index(), 1);
    // Tick past timeout
    for (int i = 0; i < 60; ++i) wc.tick(0.05f); // 3 seconds
    EXPECT_EQ(wc.combo_index(), 0);
}

TEST_F(WeaponComponentTest, CannotAttackDuringRecovery) {
    WeaponComponent wc;
    wc.equip("sword_common");
    wc.execute_attack(1.0); // recovery ~0.35s
    EXPECT_FALSE(wc.can_attack(1.05)); // still in recovery
}

TEST_F(WeaponComponentTest, FistHasSingleStage) {
    WeaponComponent wc;
    EXPECT_EQ(wc.combo_index(), 0);
    wc.execute_attack(1.0);
    EXPECT_EQ(wc.combo_index(), 1); // advances but to 1
    EXPECT_FLOAT_EQ(wc.multiplier(), 1.0f);
}

// ═══════════════════════════════════════════════════
// Hit detection tests (shape geometry)
// ═══════════════════════════════════════════════════

// Minimal mock monster struct for hit detection testing
