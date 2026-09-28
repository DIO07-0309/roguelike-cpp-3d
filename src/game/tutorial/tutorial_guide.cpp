#include "tutorial_guide.h"
#include "player.h"
#include "monster.h"
#include "item.h"
#include "skill.h"
#include "game_map.h"
#include "ai.h"
#include "config.h"
#include <cmath>
#include <algorithm>

// 多行文案用 \n 分隔的单个字面量 —— 避免 switch 每 case 一个块把函数撑到 80+ 行
static std::vector<std::string> split_lines(const char* text) {
    std::vector<std::string> out;
    std::string cur;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') { out.push_back(cur); cur.clear(); }
        else cur += *p;
    }
    out.push_back(cur);
    return out;
}

static const char* tutorial_stage_text(TutorialStage stage) {
    switch (stage) {
    case TutorialStage::WELCOME:
        return "欢迎来到 Roguelike 新手教程！\n\nWASD 移动  空格 攻击  E 交互  B 背包\n1-4 技能  X 装备  U 使用  D 丢弃\nT/Esc 退出教程\n\n按 Enter 开始练习！";
    case TutorialStage::ELEMENT:
        return "第1步：选择元素\n开局三选一（游戏开始时也会出现）\n按 1 选火（暴击）· 按 2 选冰（冻结）· 按 3 选毒（持续伤害）\n元素影响你整个冒险的战斗方式";
    case TutorialStage::MOVE:
        return "第2步：移动\nW 上  S 下  A 左  D 右\n试试走几步吧！";
    case TutorialStage::PICKUP:
        return "第3步：拾取物品\n走过去按 E 键拾取药水和训练短剑\n（两件都要捡起来哦）";
    case TutorialStage::INVENTORY:
        return "第4步：背包与使用物品\n按 B 打开背包 → 上下选择药水\n→ 按 U 使用它来回复血量！";
    case TutorialStage::EQUIP:
        return "第5步：装备武器\n按 B 打开背包 → 选中训练用短剑\n→ 按 X 装备它！\n装备后才能发挥完整连击威力";
    case TutorialStage::ATTACK_COMBO:
        return "第6步：攻击与三段连击\n走到绿色木桩旁，连续按 空格 攻击！\n第1段轻→第2段强→第3段最重（屏幕震动+爆炸）\n连续攻击3次触发完整连击！";
    case TutorialStage::SKILL:
        return "第7步：使用技能\n你已习得【斩击】技能\n走到木桩旁按 数字 1 释放！";
    case TutorialStage::COOLDOWN:
        return "第8步：技能冷却\n技能释放后需要等待冷却\nHUD 技能栏下方的蓝色进度条 = 冷却进度\n再按几次 1 观察蓝条变化！";
    case TutorialStage::WEAPON_INFO:
        return "第9步：武器差异（小知识）\n匕首=快速扇形 · 长剑=均衡三段 · 长矛=远距离突刺\n双节棍=范围乱舞 · 弩=远程弹幕 · 每种手感完全不同\n按下 P 进入正式游戏后试试各种武器！\n\n按 Enter 完成教程";
    case TutorialStage::COMPLETE:
        return "恭喜完成所有训练！\nWASD移动 | 空格攻击 | 1-4技能\nE交互 | B背包 | X装备 | U使用 | R圣物 | M地图\n\n按 Enter 返回标题，开始冒险！";
    default:
        return "";
    }
}

void TutorialGuide::notify_skill_used() { _skill_used = true; }

// ---- stage instructions ----
std::vector<std::string> TutorialGuide::get_instructions() const {
    const char* text = tutorial_stage_text(stage);
    if (!text || !*text) return {};
    return split_lines(text);
}

// ---- check and advance ----
void TutorialGuide::check_and_advance(Player* p, bool inv_open,
                                       std::vector<Monster*>& monsters,
                                       std::vector<DroppedItem>& items) {
    switch (stage) {
    case TutorialStage::ELEMENT:     _check_element(p); break;
    case TutorialStage::MOVE:     _check_move(p); break;
    case TutorialStage::ATTACK_COMBO: _check_attack(monsters); break;
    case TutorialStage::PICKUP:    _check_pickup(items); break;
    case TutorialStage::INVENTORY: _check_inventory(p); break;
    case TutorialStage::EQUIP:     _check_equip(p); break;
    case TutorialStage::SKILL:     _check_skill(); break;
    case TutorialStage::COOLDOWN:   _check_cooldown(p); break;
    default: break;
    }
}

void TutorialGuide::advance_stage() {
    switch (stage) {
    case TutorialStage::WELCOME:   stage = TutorialStage::ELEMENT; break;
    case TutorialStage::ELEMENT:   stage = TutorialStage::MOVE; break;
    // G10.10: 重排 — 捡剑→用瓶→装备→连击, 攻击步不再要求"背包装备但背包是空的"
    case TutorialStage::MOVE:      stage = TutorialStage::PICKUP; break;
    case TutorialStage::PICKUP:    stage = TutorialStage::INVENTORY; break;
    case TutorialStage::INVENTORY: stage = TutorialStage::EQUIP; break;
    case TutorialStage::EQUIP:      stage = TutorialStage::ATTACK_COMBO; attack_hits = 0; break;
    case TutorialStage::ATTACK_COMBO: stage = TutorialStage::SKILL; break;
    case TutorialStage::SKILL:     stage = TutorialStage::COOLDOWN; break;
    case TutorialStage::COOLDOWN:  stage = TutorialStage::WEAPON_INFO; break;
    case TutorialStage::WEAPON_INFO: stage = TutorialStage::COMPLETE; break;
    default: break;
    }
}

void TutorialGuide::_check_move(Player* p) {
    float px = p->entity.position.x, py = p->entity.position.y;
    if (!_last_set) { _last_px = px; _last_py = py; _last_set = true; return; }
    move_distance += std::hypot(px - _last_px, py - _last_py);
    _last_px = px; _last_py = py;
    if (move_distance > 120) advance_stage();
}

void TutorialGuide::_check_attack(std::vector<Monster*>& monsters) {
    for (auto& m : monsters) {
        // 当前HP低于最大HP = 被攻击过
        if (m->combat.current_hp >= 0 && m->combat.current_hp < m->combat.max_hp) {
            attack_hits++;
            // 不重置HP! 这样木桩显示上也能看到红色血条变化, 玩家看到反馈
            break;
        }
    }
    if (attack_hits >= 1) advance_stage();
}

void TutorialGuide::_check_pickup(std::vector<DroppedItem>& items) {
    // G10.10: 初始 2 件 (药水+短剑), 全部捡起才过 — 否则短剑留地上, 装备步卡死
    if (items.empty()) advance_stage();
}

void TutorialGuide::_check_inventory(Player* p) {
    // 教程初始HP=50, 使用药水后HP>50说明用过了
    // 注意: item_used 标记在背包面板U键操作后由engine设置
    if (item_used) advance_stage();
}

void TutorialGuide::_check_equip(Player* p) {
    // G10.10: 必须 WeaponComponent 同步到真武器 (非空手 fist_basic) 才算过
    // 防止只看 equipped["weapon"] 有值但 weapon_def_id 为空的假通过
    if (p->inventory.equipped["weapon"] &&
        p->weapon.current_weapon_id() != "fist_basic") advance_stage();
}

void TutorialGuide::_check_skill() {
    if (_skill_used) advance_stage();
}

void TutorialGuide::_check_element(Player* p) {
    // G10.8-B2: 元素选择步骤 — 玩家任选其一即通过
    if (p && p->element.initialized) { element_picked = true; advance_stage(); }
}

void TutorialGuide::_check_cooldown(Player* p) {
    // G10.8-B2: 冷却步骤 — 简单等待 1.5s 自动推进（玩家观察蓝条）
    (void)p;
    if (cooldown_waited) advance_stage();
}

// ---- factory ----
std::shared_ptr<GameMap> build_tutorial_map() {
    int w = 12, h = 9;
    auto gm = std::make_shared<GameMap>(w, h, TILE_SIZE);
    std::vector<std::string> tmpl;
    for (int y = 0; y < h; y++) {
        std::string line;
        for (int x = 0; x < w; x++) {
            if (y == 0 || y == h - 1 || x == 0 || x == w - 1) line += '#';
            else line += '.';
        }
        tmpl.push_back(line);
    }
    gm->load_from_template(tmpl);
    return gm;
}

class DummyAI : public MonsterAI {
public:
    DummyAI() : MonsterAI(0, 0, 999) {}
    void update(Monster*, Player*, GameMap*, double, double,
                std::vector<Monster*>*, std::vector<Effect>*,
                int, int, const RoomManager*) override {}
};

std::unique_ptr<Monster> create_tutorial_dummy(int tx, int ty) {
    auto m = std::make_unique<Monster>(tx * TILE_SIZE, ty * TILE_SIZE,
        "训练木桩", 9999, 0, 0, 0, Color{60, 180, 60, 255}, new DummyAI());
    return m;
}

std::vector<DroppedItem> create_tutorial_items(int tx, int ty) {
    std::vector<DroppedItem> items;
    items.push_back({std::make_shared<ConsumableItem>("初级生命药水", Rarity::COMMON, "heal", 20), tx, ty});
    // G10.10: 训练短剑带真实 weapon_def_id — 否则装备后 WeaponComponent 不同步 (仍空手)
    auto sword = std::make_shared<EquipmentItem>("训练用短剑", Rarity::COMMON, "weapon", 4, 1, 0);
    sword->weapon_def_id = "dagger_common";
    items.push_back({sword, tx + 1, ty});
    return items;
}

void give_tutorial_skill(Player* p) {
    // G3.2: SkillFactory (fallback to direct SlashSkill if registry not loaded)
    auto sk = skill_factory_create("slash");
    if (!sk) sk = std::make_unique<SlashSkill>();
    p->skills.learn(std::move(sk));
}
