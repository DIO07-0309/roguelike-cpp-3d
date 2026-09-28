#include "world_reaction.h"

WorldReactionSystem g_reactions;

// 初始化向量 (C++ aggregate先生成完整列表再赋值)
static std::vector<WorldReaction> _build_reactions() {
    std::vector<WorldReaction> out;
    // Blood Ritual
    out.push_back({WorldFlag::Blood_Ritual, StoryStage::INTRO,
     "空气中弥漫着铁锈的腥味——祭坛在你离开后仍在滴血。",
     140, 30, 25, "heartbeat",
     "地面上有暗红色的脚印——不知道是不是你自己的。", true});
    // Accepted Curse
    out.push_back({WorldFlag::Accepted_Curse, StoryStage::INTRO,
     "你的脚步越来越沉——诅咒正在渗入骨骼。",
     60, 40, 120, "chant",
     "墙上的符文在你经过时短暂地亮了起来。", true});
    // Saved Prisoner
    out.push_back({WorldFlag::Saved_Prisoner, StoryStage::INTRO,
     "囚犯的祝福似乎让空气暖和了一点。",
     80, 70, 60, "wind",
     "你隐约听到了埃德加的祈祷——尽管他在三层之上。", false});
    // Saved Priest
    out.push_back({WorldFlag::Saved_Priest, StoryStage::CHAPTER_2,
     "神殿的最后一盏灯火——它在为你亮着。",
     90, 85, 120, "chant",
     "你仍能听见神官的祈祷——微弱，但从未停止。", false});
    // Boss1 Defeated
    out.push_back({WorldFlag::Boss1_Defeated, StoryStage::CHAPTER_1,
     "狱卒的盔甲化为了灰烬——第一道门已经倒塌。",
     70, 60, 90, "silence",
     "怪物们开始彼此低吼——它们在害怕。", true});
    // Boss2 Defeated
    out.push_back({WorldFlag::Boss2_Defeated, StoryStage::CHAPTER_2,
     "火焰熄灭了——但熔岩留下的裂痕仍在发光。",
     120, 50, 30, "rumble",
     "深渊在颤抖——它在愤怒。", true});
    // Boss3 Defeated
    out.push_back({WorldFlag::Boss3_Defeated, StoryStage::FINAL,
     "深渊终于安静了。",
     100, 100, 100, "silence",
     "阳光穿过裂缝——这座地牢第一次见到了光明。", true});
    // Merchant Killed
    out.push_back({WorldFlag::Merchant_Killed, StoryStage::INTRO,
     "商人的货物散落在地上——再也没有人会来买了。",
     60, 40, 40, "silence",
     "你经过商人的摊位时——风把他的空袋子吹走了。", true});
    // All_Boss_Defeated
    out.push_back({WorldFlag::All_Boss_Defeated, StoryStage::FINAL,
     "黑暗开始褪去——这是从未有过的事情。",
     200, 180, 100, "silence",
     "你感到地牢在呼吸——但这一次，它是在喘息。", false});
    // True_Ending_Ready
    out.push_back({WorldFlag::True_Ending_Ready, StoryStage::FINAL,
     "你没有被深渊吞噬——你战胜了它，也战胜了自己。",
     220, 200, 140, "silence",
     "守望者的声音在所有楼层中回荡：'三千年——终于等到了。'", false});
    return out;
}

WorldReactionSystem::WorldReactionSystem() : _reactions(_build_reactions()) {}

// ============================================================
// API
// ============================================================
const WorldReaction* WorldReactionSystem::find(const WorldState& ws,
                                                StoryStage stage) const {
    for (auto& r : _reactions) {
        if (!ws.has(r.required_flag)) continue;
        if (r.stage != StoryStage::INTRO && r.stage != stage) continue;
        return &r;
    }
    return nullptr;
}

const char* WorldReactionSystem::current_narration(const WorldState& ws, StoryStage st) const {
    const WorldReaction* r = find(ws, st);
    return r ? r->narration : nullptr;
}

void WorldReactionSystem::current_tint(const WorldState& ws, StoryStage st,
                                        unsigned char& r, unsigned char& g,
                                        unsigned char& b) const {
    const WorldReaction* re = find(ws, st);
    if (re && re->tint_r >= 0) {
        r = (unsigned char)re->tint_r;
        g = (unsigned char)re->tint_g;
        b = (unsigned char)re->tint_b;
    }
}

const char* WorldReactionSystem::current_world_event(const WorldState& ws,
                                                      StoryStage st) const {
    const WorldReaction* re = find(ws, st);
    return re ? re->world_event : nullptr;
}

const char* WorldReactionSystem::current_ambience(const WorldState& ws,
                                                   StoryStage st) const {
    const WorldReaction* re = find(ws, st);
    return re ? re->ambience : nullptr;
}
