#include "event_system.h"
#include "player.h"
#include "item.h"
#include "combat_system.h"
#include "floor_config.h"
#include "build_score.h"
#include "core/logger.h"

// ============================================================
// D4 Step1: Event Text Pools (每种事件≥8条随机文本)
// ============================================================
static const char* TEXTS[][10] = {
    // MERCHANT (0)
    {"一个褴褛的商人从阴影中走出，背包叮当作响。",
     "商贩推着一辆破旧的推车，上面堆满了奇异的物品。",
     "\"欢迎！\" 一个苍老的声音从角落传来。",
     "你闻到了药水和旧皮革的味道——这里有商人。",
     "\"地牢里的生意很冷清，\" 商人朝你笑了笑。",
     "昏暗的火把下，一个摊位摆满了各式商品。",
     "商人从斗篷下掏出一件闪光的东西。",
     "\"陌生人，要不要看看我的珍藏？\""},
    // AMBUSH (1)
    {"脚步声从四面八方涌来——这是埋伏！",
     "天花板上的碎石突然落下，一群怪物从暗处冲出！",
     "你踩到了什么……地板突然塌陷，敌人从四周涌入。",
     "\"抓住他！\" 黑暗中传来嘶哑的呼喊。",
     "狭窄的走廊被两头怪物堵住了退路。",
     "墙上的火把突然熄灭——黑暗中亮起了无数双眼睛。",
     "你感到地面在震动，这不是普通的房间。",
     "陷阱！怪物从天花板上跳了下来！"},
    // CURSED_ROOM (2)
    {"空气中弥漫着古老的诅咒气息。",
     "墙壁上刻满了扭曲的符文，散发着暗紫色的光芒。",
     "一个嘶哑的声音在你脑中低语：\"拿吧……代价很公平。\"",
     "祭坛上放着一件辉煌的战利品，但你知道这不是免费的。",
     "你的手在颤抖——诅咒的力量在召唤。",
     "地板上画着一个法阵，其中的力量令人不安。",
     "\"所有力量都有代价，\" 墙上的文字闪烁。",
     "黑暗在你身边凝聚，等待着你的决定。"},
    // ALTAR_CHOICE (3)
    {"三束光芒从古老祭坛中升起。",
     "祭坛上摆放着三件贡品，你只能拿走一件。",
     "\"选择吧，\" 一个空灵的声音回荡。",
     "三个符号在空中闪烁——力量、生命、知识。",
     "祭坛的石刻显示着三个不同的祝福。",
     "你能感受到每件贡品中蕴含的力量——但只能选一个。",
     "\"命运女神只垂青果断之人，\" 铭文写道。",
     "三道光柱从地板上射出，每一道都在呼唤你。"},
    // STATUE (4)
    {"一座古老的雕像矗立在房间中央，它似乎在注视着你。",
     "雕像的双眼发出微弱的光芒。",
     "灰尘覆盖了雕像大部分表面，但基座上刻着什么。",
     "这座雕像雕刻得栩栩如生，仿佛下一刻就会动起来。",
     "\"触碰我，\" 一个来自远古的声音低语。",
     "雕像周围散落着古代的祭品——有人来过这里。",
     "石像的手中捧着一个发光的物体。",
     "你感到一股莫名的力量从雕像中涌出。"},
    // PRISONER (5)
    {"铁栏杆后蜷缩着一个身影。",
     "\"求你……救救我……\" 声音虚弱而绝望。",
     "囚笼里关着一个幸存者，看起来被困了很久。",
     "一个探险家的背包散落在囚笼旁边。",
     "\"我可以给你回报，\" 囚犯低声说道。",
     "铁链发出叮当声——囚犯在颤抖。",
     "黑暗中传来微弱的呼救声。",
     "你发现了一个被锁住的牢房，里面关着一个人。"},
    // LOST_CAMP (6)
    {"一个废弃的营地在角落散发着温暖的篝火余烬。",
     "篝火旁有一个破旧的背包和几张撕破的地图。",
     "营地虽小，但篝火带来了安全的感觉。",
     "你发现了一本日记，里面记录着上一个冒险者的最后旅程。",
     "帐篷里有一张铺好的床——这是休息的机会。",
     "\"如果还有后来者……\" 日记的第一页写道。",
     "营火虽然微弱，但足以驱散黑暗。",
     "一个小型的补给站，看来有人曾在这里避难。"},
    // TREASURE_GUARD (7)
    {"巨大的宝箱被一只沉睡的怪物守护着。",
     "\"谁敢碰我的财宝？！\" 一只巨兽从暗处出现。",
     "宝箱上镶满了宝石——但守护者不会轻易让路。",
     "你听到了沉重而缓慢的呼吸声——那个守卫不是普通的怪物。",
     "堆满金币的地板上站着一个庞大的身影。",
     "宝库的守卫者用冰冷的眼神注视着你。",
     "\"证明你的价值，\" 守卫发出低沉的怒吼。",
     "黄金的光芒从宝箱的缝隙中透出——但你必须先战胜它的守护者。"},
    // BLOOD_RITUAL (8)
    {"一个腥红色的祭坛在房间中央滴着血。",
     "墙壁上的血痕拼出了一个古老的契约图案。",
     "\"献血者将获得超凡的力量，\" 铭文刻在祭坛上。",
     "祭坛上的一把匕首似乎已经等待了千年。",
     "暗红色的雾气围绕着你——血祭的力量在召唤。",
     "\"你愿意付出多少？\" 一个低沉的声音问道。",
     "祭坛的血管遍布地板，仿佛在呼吸。",
     "这里曾经有过无数次献祭——空气中弥漫着铁锈的腥味。"},
    // NOTHING (9)
    {"这间房间空无一物，只有风吹过缝隙的声音。",
     "灰尘覆盖了地板——很久没有人来过这里了。",
     "墙壁上有一道长长的裂缝，透进一点微弱的光。",
     "除了寂静，这里什么都没有。",
     "一只老鼠窜过房间，消失在墙角的洞里。",
     "你听到了水滴的声音，但找不到来源。",
     "这里曾经可能有过什么，但现在只剩下遗迹。",
     "此地空无一物，但你感到一丝不安。"},
    // D8: TRAP (10)
    {"地板上的符号突然发出红光——陷阱！",
     "你踩到了一块松动的石板，机关启动了！",
     "暗箭从墙上的孔里射出！",
     "脚下一空，毒雾从地板裂缝中涌出。",
     "房间开始旋转，尖刺从天花板降下。",
     "\"别再往前走了，\" 一个机械的声音警告道。",
     "炽热的火焰从墙上的喷口喷出。",
     "你触发了古老的地牢防御机关。"},
    // D8: MYSTERY (11)
    {"一个发光的盒子放在房间中央，上面没有任何标记。",
     "空气中有一种奇怪的能量——你无法判断它是好是坏。",
     "墙上挂着一面破旧的镜子，里面却照出了不同的东西。",
     "地上散落着六张不同的命运卡片。",
     "一个嗡嗡作响的传送门在你面前展开。",
     "你感到命运在摆弄这个房间——一切都有可能。",
     "这个房间里没有规则——只有无法预测的结果。",
     "七个颜色的光环在地板上旋转，每一个都通向不同的可能。"},
    // D8: BLESSING (12)
    {"一束温暖的光从天花板的裂缝倾泻而下。",
     "房间中央喷涌着圣洁的光芒，照亮了每一个角落。",
     "墙壁上爬满了发光的藤蔓——这是生命的力量。",
     "\"你被祝福了，\" 一个温柔的声音低语。",
     "空气中弥漫着花与蜂蜜的香气，令人心旷神怡。",
     "金色的光尘在你周围旋转，渗入你的皮肤。",
     "一道古老的祝福铭刻在地板上，散发出温暖。",
     "这个房间散发着治愈与成长的能量。"},
    // D8: CURSE (13)
    {"即使最明亮的火把也无法照亮这个房间的角落。",
     "一股令人窒息的重压落在你的肩膀上。",
     "角落里的影子在移动——它们在等你。",
     "\"凡是进入者，必受诅咒，\" 墙上的文字变黑了。",
     "你感觉到有什么在吸走你的力量。",
     "黑暗像一张网，缠绕在你的四肢上。",
     "空气中弥漫着腐烂和绝望的气息。",
     "你踏入了地牢最阴暗的角落——这里没有祝福。"},
    // D8: LORE (14)
    {"墙上的壁画描绘着一场远古的战斗。",
     "一本碎裂的日记躺在桌上，等待着读者。",
     "墙壁上刻着一首古老的诗歌，讲述着地牢的起源。",
     "\"我是第127位进入这里的冒险者，\" 墙上写着。",
     "一张褪色的地图被钉在墙上，标注了多条消失的路线。",
     "这里曾经是地牢中最热闹的广场——现在只剩灰烬。",
     "一块石碑上刻着无数个名字，最下面的那个……是刚添上去的。",
     "这间房间本身就是一个故事——记录着地牢从辉煌到衰败。"},
    // D8: NPC_EVENT (15)
    {"一个孤独的身影坐在角落，听到你的脚步声后抬起了头。",
     "\"嘿……你知道出去的路吗？\" 一个探险家问道。",
     "一位年老的学者正在研究墙上的文字。",
     "一个受伤的士兵靠在墙上，手臂上绑着染血的绷带。",
     "\"帮我一个忙，我会给你回报。\" 陌生人开口道。",
     "一位戴着兜帽的旅行者默默地注视着你。",
     "\"你看起来像能打的人——我这里有个目标，\" 一个低沉的嗓音。",
     "一个商人正对着他的货物发愁……也许你需要什么。"},
    // D8: RELIC_DROP (16)
    {"房间中央漂浮着一块发光的碎片——一件圣物。",
     "玻璃展柜里静静地躺着一件古老的遗物。",
     "强大的能量从地板中心涌出，形成一个发光的球体。",
     "\"这间房间只给予最适合的访客，\" 一个声音宣布道。",
     "祭坛上放着一件闪亮的护符，静静地等待它的主人。",
     "墙壁上的凹槽里镶嵌着半透明的晶体——其中一块闪烁着你的颜色。",
     "一个透明的水晶球中悬浮着小巧的圣物。",
     "古老的文字刻在石碑上：\"只有一次机会。\""},
    // D8: TREASURE_CACHE (17)
    {"一堆金币和宝物从倒塌的墙壁中倾泻而出。",
     "打开的木箱里塞满了金币和装备。",
     "\"发货了！\" 一个小妖精在你面前倒空了它的藏宝箱。",
     "一个被遗弃的探险家背包，装满了旅途的收获。",
     "几只宝箱沿墙排列——但这看起来不像是陷阱。",
     "彩色的光芒从板条箱的缝隙中透出。",
     "\"拿你需要的东西，\" 一张纸条写道。",
     "此地遍地都是宝物——但你必须迅速决定拿什么。"},
};

static const char* pick_text(int type, std::mt19937& rng) {
    if (type < 0 || type > 17) return "???";
    int idx = rng() % 8;
    return TEXTS[type][idx];
}

std::vector<std::string> event_text_pool(int idx) {
    std::vector<std::string> out;
    if (idx < 0 || idx > 17) return out;
    for (int i = 0; i < 8; i++) out.push_back(TEXTS[idx][i]);
    return out;
}

const char* event_type_name(EventType t) {
    switch (t) {
        case EventType::MERCHANT:        return "旅行商人";
        case EventType::AMBUSH:          return "伏击!";
        case EventType::CURSED_ROOM:     return "诅咒房间";
        case EventType::ALTAR_CHOICE:    return "三选祭坛";
        case EventType::STATUE:          return "神秘雕像";
        case EventType::PRISONER:        return "囚犯";
        case EventType::LOST_CAMP:       return "迷失营地";
        case EventType::TREASURE_GUARD:  return "宝库守卫";
        case EventType::BLOOD_RITUAL:    return "血祭仪式";
        case EventType::NOTHING:         return "寂静房间";
        case EventType::TRAP:            return "陷阱房";
        case EventType::MYSTERY:         return "命运之盒";
        case EventType::BLESSING:        return "祝福圣殿";
        case EventType::CURSE:           return "诅咒之地";
        case EventType::LORE:            return "远古记忆";
        case EventType::NPC_EVENT:       return "相遇";
        case EventType::RELIC_DROP:      return "圣物祭坛";
        case EventType::TREASURE_CACHE:  return "宝箱缓存";
        default:                         return "???";
    }
}

// ============================================================
// Event Generation — 按章节权重随机
// ============================================================
DungeonEvent generate_event(int floor, const ChapterConfig& ch, std::mt19937& rng) {
    DungeonEvent ev;
    int chap = ch.chapter;

    // G5.5: 扩展权重 — 减少NOTHING, 增加剧情/商人/圣物/Blessing
    int weights[18] = {0};
    if (chap == 0) {
        int w[] = {25,10,5,10,10,12,15,0,0,4,4,6,16,2,14,12,12,16};
        for (int i = 0; i < 18; i++) weights[i] = w[i];
    } else if (chap == 1) {
        int w[] = {20,12,10,10,10,10,8,5,5,3,6,10,12,4,14,14,12,12};
        for (int i = 0; i < 18; i++) weights[i] = w[i];
    } else {
        int w[] = {14,14,10,10,8,6,6,10,8,2,10,12,10,10,14,12,14,10};
        for (int i = 0; i < 18; i++) weights[i] = w[i];
    }

    int total = 0;
    for (int i = 0; i < 18; i++) total += weights[i];
    int roll = (int)(rng() % (uint32_t)total);
    int sum = 0;
    for (int i = 0; i < 18; i++) {
        sum += weights[i];
        if (roll < sum) {
            ev.type = (EventType)(i + 1);
            break;
        }
    }
    if (ev.type == EventType::NONE) ev.type = EventType::NOTHING;

    ev.title  = event_type_name(ev.type);
    ev.text   = pick_text((int)ev.type - 1, rng);
    ev.desc   = ev.text;
    return ev;
}

// ============================================================
// Event Execution — 返回结果消息
// ============================================================
// ═══ execute_event 分支处理 (G13 拆函数) ═══
// 原 254 行 switch 拆成每类型一个 handler。
// 契约: rng() 调用顺序被 tests/combat/action_test.cpp 的"同种子可复现"锁定,
// 拆分与 grant_random_relic 收口都不得改变 rng 消耗次序。

// 发一个玩家未持有的随机 FLOOR 圣物; 无可给返回空串。
// 收口原 6 处重复的 ids/avail 收集循环 (rng 消耗: 无可给 0 次, 可给 1 次)。
static std::string grant_random_relic(Player* player, const char* suffix) {
    std::vector<std::string> avail;
    for (const auto& id : get_all_relic_ids())
        if (!player_has_relic(player, id)) avail.push_back(id);
    if (avail.empty()) return "";
    std::string ch = avail[rng() % avail.size()];
    player->add_relic(ch, PersistenceScope::FLOOR);
    std::string s = std::string("RELIC:") + get_relic_def(ch)->name;
    if (suffix) s += suffix;
    return s;
}

static std::string do_merchant(Player* p) {
    int r = rng() % 3;
    if (r == 0) {
        p->inventory.add(std::make_shared<ConsumableItem>(
            "商人之药", Rarity::RARE, "heal", 40), p);
        return "RELIC:商人给了你一瓶上等药水。";
    }
    if (r == 1) {
        apply_buff(p, "attack_up", 1);
        return "MSG:商人给予了你攻击祝福。";
    }
    std::string relic = grant_random_relic(p, nullptr);
    return relic.empty() ? "MSG:商人已无货可卖。" : relic;
}

// 经验+50% 通过 attack_up buff 模拟
static std::string do_ambush(Player* p) {
    apply_buff(p, "attack_up", 1);
    return "MSG:你从伏击中杀出重围！经验加成+50%。";
}

static std::string do_cursed_room(Player* p) {
    apply_buff(p, "poison", 2);
    std::string relic = grant_random_relic(p, " (受诅咒:中毒)");
    return relic.empty() ? "MSG:你拒绝了诅咒的诱惑。" : relic;
}

static std::string do_altar_choice(Player* p) {
    int r = rng() % 3;
    if (r == 0) {
        heal_player(p, (int)(p->combat.max_hp * 0.30f));
        return "MSG:祭坛恢复了你30%的生命。";
    }
    if (r == 1) {
        apply_buff(p, "attack_up", 2);
        return "MSG:祭坛赐予了你强大的攻击力。";
    }
    for (auto& sk : p->skills.active_skills) {
        if (sk->can_evolve()) {
            std::string evo = sk->evolve();
            if (!evo.empty())
                return "MSG:祭坛进化了你的技能: " + evo;
        }
    }
    return "MSG:祭坛的光芒消散了。";
}

static std::string do_statue(Player* p) {
    int r = rng() % 5;
    if (r == 0) { apply_buff(p, "attack_up", 1); return "MSG:雕像赐予你攻击提升。"; }
    if (r == 1) { apply_buff(p, "slow", 1); return "MSG:雕像施放了减速诅咒。"; }
    if (r == 2) { heal_player(p, 15); return "MSG:雕像治愈了你。"; }
    if (r == 3) {
        std::string relic = grant_random_relic(p, nullptr);
        if (!relic.empty()) return relic;
    }
    return "MSG:雕像似乎什么都没有发生。";
}

static std::string do_prisoner(Player* p) {
    if (rng() % 2 == 0) {
        heal_player(p, (int)(p->combat.max_hp * 0.20f));
        return "MSG:囚犯感激地拍了拍你，分享了他最后的食物。";
    }
    p->inventory.add(std::make_shared<ConsumableItem>(
        "囚犯的谢礼", Rarity::RARE, "heal", 30), p);
    return "MSG:囚犯给了你一件物品作为答谢。";
}

static std::string do_lost_camp(Player* p) {
    heal_player(p, (int)(p->combat.max_hp * 0.25f));
    return "MSG:你在篝火旁休息，恢复了体力。";
}

static std::string do_treasure_guard(Player* p) {
    p->inventory.add(std::make_shared<EquipmentItem>(
        "宝库之剑", Rarity::LEGENDARY, "weapon", 15, 2, 1), p);
    std::string relic = grant_random_relic(p, nullptr);
    return relic.empty() ? "RELIC:宝库之剑" : relic;
}

static std::string do_blood_ritual(Player* p) {
    int loss = (int)(p->combat.current_hp * 0.20f);
    if (loss < 1) loss = 1;
    p->combat.take_damage(loss);
    apply_buff(p, "attack_up", 3);
    return "MSG:鲜血浸染祭坛，你获得了强大的力量。";
}

static std::string do_nothing() {
    return "MSG:这里什么也没有发生。";
}

static std::string do_trap(Player* p) {
    int r = rng() % 6;
    if (r == 0) {
        int dmg = std::max(5, p->combat.current_hp / 6);
        p->combat.take_damage(dmg);
        return "MSG:暗箭击中了你的肩膀！受到" + std::to_string(dmg) + "伤害。";
    }
    if (r == 1) { apply_buff(p, "burn", 2); return "MSG:火焰陷阱喷出烈焰——你被灼伤了！"; }
    if (r == 2) { apply_buff(p, "poison", 3); return "MSG:毒雾从地板裂缝中涌出——你中毒了。"; }
    if (r == 3) { apply_buff(p, "fear", 1); return "MSG:黑暗中的低语让你毛骨悚然——你中了恐惧。"; }
    if (r == 4) { apply_buff(p, "freeze", 1); return "MSG:冰霜陷阱瞬间将你的双脚冻住！"; }
    auto item = generate_random_item();
    if (item) { p->inventory.add(item, p); return "MSG:陷阱被触发——但里面竟藏着一件东西！"; }
    return "MSG:陷阱只发出了一声闷响——你很幸运。";
}

static std::string do_mystery(DungeonEvent& ev, Player* p, int floor) {
    int r = rng() % 17 + 1;               // skip NONE
    EventType fake = (EventType)r;
    if (fake == EventType::MYSTERY) fake = EventType::STATUE;  // 防无限递归
    ev.type = fake;
    ev.triggered = false;                 // 允许重触发
    return execute_event(ev, p, floor);
}

static std::string do_blessing(Player* p) {
    int r = rng() % 5;
    if (r == 0) { apply_buff(p, "blessing", 2); return "MSG:祝福的光芒环绕着你。"; }
    if (r == 1) { apply_buff(p, "growth", 2); return "MSG:你感到体能在永久增强——成长2层。"; }
    if (r == 2) { apply_buff(p, "attack_up", 2); return "MSG:圣光注入了你的武器——攻击力大幅提升。"; }
    if (r == 3) { apply_buff(p, "regen", 3); return "MSG:生命之水渗入你的血液——再生3层。"; }
    heal_player(p, (int)(get_effective_max_hp(p) * 0.25f));
    return "MSG:神圣的力量治愈了你25%的生命。";
}

static std::string do_curse(Player* p) {
    int r = rng() % 4;
    if (r == 0) { apply_buff(p, "curse", 2); return "MSG:你被诅咒了——诅咒2层。"; }
    if (r == 1) { apply_buff(p, "slow", 2); return "MSG:诅咒压制着你的步伐——减速2层。"; }
    if (r == 2) { apply_buff(p, "blind", 2); return "MSG:诅咒遮障了你的视觉——致盲2层。"; }
    int loss = std::max(5, p->combat.current_hp / 4);
    p->combat.take_damage(loss);
    return "MSG:黑暗之触吸取了" + std::to_string(loss) + "点生命。";
}

static std::string do_lore() {
    static const char* lore_msgs[] = {
        "这些废墟曾经是地牢中最繁荣的区域——三千年前。",
        "你读到: '吾王永恒，吾等永守。' 签名已经模糊。",
        "一幅壁画描绘着一场巨大的战争——光与暗在空中碰撞。",
        "日记最后一页写着: '今天是我第47天。我发现了新的东西。'",
        "石碑记载着一位名为阿斯特拉的古代英雄——与你的旅程惊人地相似。",
        "你找到了一封信，日期是1327年前——地址是你现在站的地方。",
        "\"深渊不是地牢——它是监狱，\" 墙上潦草地写着。",
        "一本古籍打开了，上面画着一棵树——深埋在地下但仍在生长。",
    };
    return "MSG:" + std::string(lore_msgs[rng() % 8]);
}

static std::string do_npc_event(Player* p) {
    int r = rng() % 3;
    if (r == 0) {
        p->inventory.add(std::make_shared<ConsumableItem>(
            "冒险家的礼物", Rarity::RARE, "heal", 35), p);
        return "MSG:陌生人微笑着递给你一份礼物。";
    }
    if (r == 1) {
        apply_buff(p, "blessing", 1);
        return "MSG:学者为你念了一段远古的祝福咒语。";
    }
    std::string relic = grant_random_relic(p, " (来自NPC)");
    return relic.empty() ? "MSG:陌生人与你分享了故事——但没什么实质的东西。" : relic;
}

static std::string do_relic_drop(Player* p) {
    std::string relic = grant_random_relic(p, nullptr);
    return relic.empty() ? "MSG:圣物祭坛已经空了——你已经集齐了所有圣物。" : relic;
}

static std::string do_treasure_cache(Player* p) {
    int r = rng() % 4;
    if (r == 0) {
        p->inventory.add(std::make_shared<EquipmentItem>(
            "遗迹之宝", Rarity::RARE, "weapon", 10, 3, 2), p);
        return "MSG:你找到了一件精心保护的武器。";
    }
    if (r == 1) {
        p->inventory.add(std::make_shared<ConsumableItem>(
            "珍藏秘药", Rarity::EPIC, "heal", 60), p);
        return "MSG:宝箱里藏着一瓶闪耀的药水。";
    }
    if (r == 2) {
        apply_buff(p, "attack_up", 2);
        apply_buff(p, "defense_up", 2);
        return "MSG:宝箱中喷出魔法——你获得了攻击和防御双重增强。";
    }
    std::string relic = grant_random_relic(p, " (宝箱中)");
    return relic.empty() ? "MSG:宝箱虽空，但金币仍能让你会心一笑。" : relic;
}

std::string execute_event(DungeonEvent& ev, Player* player, int floor) {
    if (ev.triggered || !player) return "";
    ev.triggered = true;

    switch (ev.type) {
    case EventType::MERCHANT:        return do_merchant(player);
    case EventType::AMBUSH:          return do_ambush(player);
    case EventType::CURSED_ROOM:     return do_cursed_room(player);
    case EventType::ALTAR_CHOICE:    return do_altar_choice(player);
    case EventType::STATUE:          return do_statue(player);
    case EventType::PRISONER:        return do_prisoner(player);
    case EventType::LOST_CAMP:       return do_lost_camp(player);
    case EventType::TREASURE_GUARD:  return do_treasure_guard(player);
    case EventType::BLOOD_RITUAL:    return do_blood_ritual(player);
    case EventType::NOTHING:         return do_nothing();
    // D8 Step6: New event type executions
    case EventType::TRAP:            return do_trap(player);
    case EventType::MYSTERY:         return do_mystery(ev, player, floor);
    case EventType::BLESSING:        return do_blessing(player);
    case EventType::CURSE:           return do_curse(player);
    case EventType::LORE:            return do_lore();
    case EventType::NPC_EVENT:       return do_npc_event(player);
    case EventType::RELIC_DROP:      return do_relic_drop(player);
    case EventType::TREASURE_CACHE:  return do_treasure_cache(player);
    default: break;
    }
    return "";
}
