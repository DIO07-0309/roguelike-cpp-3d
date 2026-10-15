#include "ai/player_behavior/player_behavior_analyzer.h"
#include <cmath>

int PlayerBehaviorAnalyzer::count_type(const std::vector<PlayerAction>& h,
                                         PlayerActionType t) {
    int n = 0;
    for (auto& a : h) if (a.type == t) n++;
    return n;
}

// ── G53: analyze 拆出的统计段 ──────────────────────────────

// 频率类: 攻击/技能/治疗频率 + 闪避率 + 攻击性 + 技能偏好分布
void PlayerBehaviorAnalyzer::_compute_frequencies(
    const std::vector<PlayerAction>& h, PlayerHabitProfile& p, float total_t) {
    int atk  = count_type(h, PlayerActionType::ATTACK);
    int sk   = count_type(h, PlayerActionType::SKILL);
    int mov  = count_type(h, PlayerActionType::MOVE);
    int dodge = count_type(h, PlayerActionType::DODGE);
    int heal = count_type(h, PlayerActionType::HEAL);
    (void)mov;

    p.attack_frequency = (float)atk / total_t;
    p.skill_frequency  = (float)sk / total_t;
    p.heal_frequency   = (float)heal / total_t;

    int combat_actions = atk + sk + dodge;
    if (combat_actions > 0) {
        p.dodge_rate = (float)dodge / (float)combat_actions;
        p.aggression_score = (float)(atk + sk) / (float)(atk + sk + dodge);
    }

    // Skill preference
    int sk_counts[4] = {};
    for (auto& a : h)
        if (a.type == PlayerActionType::SKILL && a.skill_id >= 0 && a.skill_id < 4)
            sk_counts[a.skill_id]++;
    for (int i = 0; i < 4; i++)
        if (sk > 0) p.skill_preference[i] = (float)sk_counts[i] / (float)sk;
}

// 移动类: 左右偏向 + 后撤率 (都基于 MOVE 总数归一)
void PlayerBehaviorAnalyzer::_compute_movement(
    const std::vector<PlayerAction>& h, PlayerHabitProfile& p) {
    int lr = 0, rl = 0, total_dir = 0;
    for (auto& a : h) {
        if (a.type != PlayerActionType::MOVE) continue;
        total_dir++;
        if (a.value == 0) lr++; else if (a.value == 1) rl++;
    }
    if (total_dir > 0) p.left_bias = (float)lr / (float)total_dir;

    // Retreat rate: moves AWAY from target (moves up when player faces down = retreat)
    int retreat = 0;
    for (auto& a : h)
        if (a.type == PlayerActionType::MOVE && a.value == 2) retreat++;
    if (total_dir > 0) p.retreat_rate = (float)retreat / (float)total_dir;
}

// 距离与伤害: 平均交战距离(像素) + 每层平均承伤
void PlayerBehaviorAnalyzer::_compute_distance_damage(
    const std::vector<PlayerAction>& h, PlayerHabitProfile& p) {
    // Average combat distance to nearest enemy (pixels) — 修复: 此前从未计算 (恒0)
    float dist_sum = 0.0f; int dist_n = 0;
    for (auto& a : h)
        if (a.enemy_dist >= 0.0f) { dist_sum += a.enemy_dist; dist_n++; }
    if (dist_n > 0) p.average_distance = dist_sum / (float)dist_n * 32.0f;

    // Average damage taken per floor — 修正: 原为 atk/dmg 计数比率, 语义错误 (应为伤害量)
    int dmg = count_type(h, PlayerActionType::TAKE_DAMAGE);
    if (dmg > 0 && h.back().floor > 0) {
        float dmg_sum = 0.0f;
        for (auto& a : h)
            if (a.type == PlayerActionType::TAKE_DAMAGE) dmg_sum += (float)a.value;
        p.avg_damage_taken = dmg_sum / (float)h.back().floor;
    }
}

// 风格分类 (四档阈值原样保留)
void PlayerBehaviorAnalyzer::_classify_style(PlayerHabitProfile& p) {
    if (p.attack_frequency > 0.8f && p.dodge_rate < 0.15f)
        p.style = PlayerStyle::AGGRESSIVE;
    else if (p.dodge_rate > 0.3f && p.attack_frequency < 0.4f)
        p.style = PlayerStyle::DEFENSIVE;
    else if (p.attack_frequency > 0.6f && p.retreat_rate > 0.3f)
        p.style = PlayerStyle::SNIPER;
    else if (p.skill_frequency > 0.15f)
        p.style = PlayerStyle::MAGE;
}

PlayerHabitProfile PlayerBehaviorAnalyzer::analyze(
    const std::vector<PlayerAction>& history) {
    PlayerHabitProfile p;
    if (history.empty()) return p;

    float total_t = history.back().timestamp - history.front().timestamp;
    if (total_t <= 0.1f) total_t = 1.0f;
    p.total_actions = (int)history.size();

    _compute_frequencies(history, p, total_t);
    _compute_movement(history, p);
    _compute_distance_damage(history, p);
    _classify_style(p);

    generate_counter_hints(p);

    // M5: 条件维度统计 — 受压反击率/朝向稳定度/攻击节奏方差
    compute_condition_dimensions(history, p);
    return p;
}

// ── M5: 从方向/受击上下文计算真实习惯 ──
void PlayerBehaviorAnalyzer::compute_condition_dimensions(
    const std::vector<PlayerAction>& history, PlayerHabitProfile& p) {
    int dmg_n = 0, retaliate_n = 0;   // 受压反击率: 被打后1s内立刻反击动作占比
    for (auto& a : history) {
        if (a.type == PlayerActionType::TAKE_DAMAGE) { dmg_n++; continue; }
        if (a.hit_in_1s > 0 &&
            (a.type == PlayerActionType::ATTACK || a.type == PlayerActionType::SKILL))
            retaliate_n++;
    }
    if (dmg_n > 0) p.fight_back_rate = (float)retaliate_n / (float)dmg_n;

    int faces[4] = {}; int fn = 0;   // 朝向稳定度: 主朝向占比, 高=退避轴可预测
    for (auto& a : history)
        if (a.facing_dir >= 0 && a.facing_dir < 4) { faces[a.facing_dir]++; fn++; }
    if (fn > 0) {
        int best = faces[0];
        for (int i = 1; i < 4; i++) if (faces[i] > best) best = faces[i];
        p.face_enemy_rate = (float)best / (float)fn;
    }
    std::vector<float> gaps;          // 攻击节奏方差: 相邻攻击间隔 stddev
    float last_ts = -1.0f;
    for (auto& a : history) {
        if (a.type != PlayerActionType::ATTACK) continue;
        if (last_ts >= 0.0f) gaps.push_back(a.timestamp - last_ts);
        last_ts = a.timestamp;
    }
    if (gaps.size() >= 3) {
        float mean = 0.0f;
        for (float g : gaps) mean += g;
        mean /= (float)gaps.size();
        float var = 0.0f;
        for (float g : gaps) var += (g - mean) * (g - mean);
        p.attack_rhythm_var = std::sqrt(var / (float)gaps.size());
    }
}

void PlayerBehaviorAnalyzer::generate_counter_hints(PlayerHabitProfile& p) {
    // Attack-heavy player → bait and punish during recovery
    p.predict_attack_heavy = (p.attack_frequency > 0.7f);

    // Finds favorite skill (highest usage among 4)
    float best = 0.0f; int best_i = -1;
    for (int i = 0; i < 4; i++)
        if (p.skill_preference[i] > best) { best = p.skill_preference[i]; best_i = i; }
    p.predict_skill_spam = (best > 0.6f);
    p.predicted_fav_skill = best_i;

    // Low dodge → pressure close, don't let them escape
    p.predict_low_dodge = (p.dodge_rate < 0.1f);

    // Panic heal: heals at high HP (>50%) = waste, punishable during healing
    p.predict_panic_heal = (p.heal_frequency > 0.02f && p.aggression_score > 0.5f);

    // Default heal threshold
    p.hp_counter_threshold = p.predict_panic_heal ? 50.0f : 35.0f;
}
