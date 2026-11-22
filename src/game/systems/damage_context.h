#pragma once
#include <string>

// 元素伤害语义由 ElementType 承担; POISON/FIRE/ICE/LIGHTNING 四值删除.
// 显式保留原数值, 确保无任何隐式漂移 (存档里 damage_type 以 int 落盘).
enum class DamageType {
    PHYSICAL = 0,
    MAGICAL  = 1,
    TRUE     = 6,
};

struct DamageContext {
    void* source = nullptr;
    void* target = nullptr;
    int raw_damage = 0;
    int final_damage = 0;
    DamageType damage_type = DamageType::PHYSICAL;
    bool cancelled = false;
};
