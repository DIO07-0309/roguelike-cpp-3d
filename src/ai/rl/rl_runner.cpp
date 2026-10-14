// G8.4/F15.4: RL standalone runner — called from main.cpp before engine init.
// Runs CombatEnvironment + RandomAgent/QAgent without GameScene.
// F15.4: run_rl_mirror_mode() trains QAgent against 4 player styles.
// v0.9.28: Q 表持久化 (saves/rl_qtable.json + rl_mirror_q_<STYLE>.json), 续训
// v0.9.29: epsilon 退火 0.12 → 0.005, 末段 10% 低探索胜率统计
#include "ai/rl/environment.h"
#include "ai/rl/random_agent.h"
#include "ai/rl/q_agent.h"
#include "ai/mcts/simulation_state.h"
#include "ai/mcts/action.h"
#include "ai/mirror/mirror_agent.h"
#include <cstdio>
#include <string>

// epsilon 退火: 训练进度 i/n 从 EPS_START 线性降到 EPS_END
static const double EPS_START = 0.12;
static const double EPS_END = 0.005;

// ── G52: run_rl_mode 子步骤 (原 98 行拆 test/train 两段) ──

// 典型战斗场景 (怪物取"可交战距离" — 见下方坐标注释)
static mcts::SimulationState rl_make_scenario() {
    mcts::SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.x = 5; s.player.y = 5;
    s.player.attack = 10; s.player.pdef = 3; s.player.mdef = 1;
    s.player.alive = true;
    s.player.skill_cooldowns[0] = 0; s.player.skill_cooldowns[1] = 0;
    // v0.9.36 G32-E: 怪物改为可交战距离 (G32-E-B4)
    // 旧坐标 (3,0)/(5,0) 距玩家 (5,5) 是 7/5 格, 而 ATTACK 要求 d<2
    // (action.h)、技能伤害要求 d<3 (environment.cpp) → 近战与技能伤害
    // 永远触发不了, 训出的策略只会绕圈和发呆(技能放出去 0 伤害还锁 11 步冷却)。
    // 现取 slime d=1(可近战) / orc d=2(仅技能)。
    s.monsters.push_back(mcts::MonsterSnapshot{"slime", 20, 20, 5, 6, 3, 1, 0, {}, true});
    s.monsters.push_back(mcts::MonsterSnapshot{"orc", 30, 30, 7, 5, 7, 3, 1, {}, true});
    return s;
}

// Test 模式: RandomAgent 基线
static void rl_run_test_mode(int test_episodes) {
    rl::RandomAgent random_agent;
    rl::CombatEnvironment env;
    const uint32_t seed = 42;
    printf("═══ RL TEST: %d episodes (RandomAgent) ═══\n", test_episodes);
    int wins = 0; double total_reward = 0; int total_steps = 0;
    for (int i = 0; i < test_episodes; i++) {
        auto initial = rl_make_scenario();
        initial.rng.seed = seed + i * 7919u;
        env.reset(initial);
        int steps = 0;
        while (!env.is_done() && steps < 50) {
            uint32_t local_seed = seed + i * 7331u + steps;
            auto a = random_agent.select(env.state(), local_seed);
            auto sr = env.step(a);
            total_reward += sr.reward;
            steps++;
        }
        if (env.state().victory) wins++;
        total_steps += steps;
        if (test_episodes >= 10 && (i+1) % (test_episodes/10) == 0)
            printf("  [%d/%d] wins=%d avg_r=%.1f\n", i+1, test_episodes, wins, total_reward/(i+1));
    }
    printf("RandomAgent: %d/%d wins (%.1f%%), avg reward %.1f, avg steps %.1f\n",
           wins, test_episodes, wins*100.0/test_episodes,
           total_reward/test_episodes, (double)total_steps/test_episodes);
}

// Train 模式: Q-learning (Q 表持久化, 支持续训, epsilon 退火)
// ── G53: 训练结果打印 + Q 表落盘 (train/train_style 共用, 原各写一遍) ──

// 前置声明: 定义在文件后部 (line ~218), episode helper 需要调用
static mcts::SimulationState rl_mirror_scenario(int style_id);

// 加载 Q 表并打印续训提示; 返回加载条目数
static size_t rl_load_qtable(rl::QAgent& agent, const std::string& path) {
    const size_t loaded = agent.load(path) ? agent.table_size() : 0;
    if (loaded > 0)
        printf("  [load] %s: %zu entries — 继续训练\n", path.c_str(), loaded);
    return loaded;
}

// 胜率 + 末段低探索收敛水平 (两个训练模式共用同样的两行格式)
static void rl_print_train_result(const char* tag, int wins, int episodes,
                                  int tail_wins, int tail_start, double avg_r,
                                  size_t table_size) {
    printf("  [%s] %d/%d wins (%.1f%%), avg_r=%.1f, Q=%zu\n",
        tag, wins, episodes, wins * 100.0 / episodes, avg_r, table_size);
    printf("  [tail %d eps] %d/%d wins (%.1f%%) — 低探索收敛水平\n",
        episodes - tail_start, tail_wins, episodes - tail_start,
        tail_wins * 100.0 / (episodes - tail_start));
}

// Q 表落盘 (失败也打印, 不静默)
static void rl_save_qtable(rl::QAgent& agent, const std::string& path) {
    if (agent.save(path))
        printf("  [save] %s: %zu entries\n", path.c_str(), agent.table_size());
    else
        printf("  [save] FAILED: %s\n", path.c_str());
}

// epsilon 退火: 训练进度 i/n 从 EPS_START 线性降到 EPS_END
static float rl_epsilon_at(int i, int episodes) {
    return (float)(EPS_START - (EPS_START - EPS_END) * (double)i / episodes);
}

// 跑一局训练 episode (退火 epsilon + 场景 + Q 更新); 返回是否胜利
static bool rl_train_one_episode(rl::QAgent& q_agent, rl::CombatEnvironment& env,
                                 int i, int train_episodes) {
    // v0.9.29: epsilon 退火 — 后期低探索收敛 (突破 95%)
    q_agent.set_epsilon(rl_epsilon_at(i, train_episodes));
    auto initial = rl_make_scenario();
    initial.rng.seed = 42 + i * 7919u;
    auto obs = env.reset(initial);
    while (!env.is_done()) {
        uint32_t local_seed = 42 + i * 7331u + env.state().depth;
        auto a = q_agent.select(env.state(), local_seed);
        auto sr = env.step(a);
        // v0.9.36 G32-E: bootstrap 只在下一状态合法动作里取 max (G32-E-B1)
        q_agent.update(obs, a, sr.reward, sr.observation,
                       mcts::get_possible_actions(env.state()),
                       env.is_done());
        obs = sr.observation;
    }
    return env.state().victory;
}

// 跑一局镜像训练 episode (同上用 mirror_reward 叠加风格奖励);
// total_r 累加本局总回报
static bool rl_mirror_one_episode(rl::QAgent& q_agent, rl::CombatEnvironment& env,
                                  int style_i, int i, int episodes,
                                  const PlayerHabitProfile& profile,
                                  double& total_r) {
    // v0.9.29: epsilon 退火 — 后期低探索收敛
    q_agent.set_epsilon(rl_epsilon_at(i, episodes));
    auto initial = rl_mirror_scenario(style_i);
    initial.rng.seed = 42 + style_i * 7919u + i * 7331u;
    auto obs = env.reset(initial);
    while (!env.is_done()) {
        uint32_t local_seed = 42 + style_i * 7331u + i * 7919u + env.state().depth;
        auto a = q_agent.select(env.state(), local_seed);
        auto sr = env.step(a);
        // Mirror reward replaces default step reward
        double base = sr.reward;
        bool dodged = (a == mcts::CombatAction::MOVE_LEFT
                    || a == mcts::CombatAction::MOVE_RIGHT
                    || a == mcts::CombatAction::MOVE_UP
                    || a == mcts::CombatAction::MOVE_DOWN);
        double mirror_bonus = MirrorAgent::mirror_reward(
            profile, (int)a, base, 0, dodged, false);
        // v0.9.36 G32-E: bootstrap 只在下一状态合法动作里取 max (G32-E-B1)
        q_agent.update(obs, a, base + mirror_bonus, sr.observation,
                       mcts::get_possible_actions(env.state()),
                       env.is_done());
        obs = sr.observation;
        total_r += base + mirror_bonus;
    }
    return env.state().victory;
}

static void rl_run_train_mode(int train_episodes) {
    rl::QAgent q_agent(0.1, 0.9, 0.1);
    rl::CombatEnvironment env;
    const std::string qpath = "saves/rl_qtable.json";
    rl_load_qtable(q_agent, qpath);
    printf("═══ RL TRAIN: %d episodes (QAgent) ═══\n", train_episodes);

    int wins = 0;
    int tail_wins = 0;
    const int tail_start = train_episodes - train_episodes / 10;  // 末段 10% 低探索统计
    for (int i = 0; i < train_episodes; i++) {
        const bool victory = rl_train_one_episode(q_agent, env, i, train_episodes);
        if (victory) { wins++; if (i >= tail_start) tail_wins++; }
        if (train_episodes >= 10 && (i + 1) % (train_episodes / 10) == 0)
            printf("  [%d/%d] wins=%d Q-table=%zu\n",
                   i + 1, train_episodes, wins, q_agent.table_size());
    }

    printf("QAgent after training: %d/%d wins (%.1f%%), Q-table size=%zu\n",
           wins, train_episodes, wins * 100.0 / train_episodes,
           q_agent.table_size());
    rl_print_train_result("tail", wins, train_episodes, tail_wins, tail_start,
                          0.0, q_agent.table_size());
    rl_save_qtable(q_agent, qpath);

    auto dist = q_agent.action_distribution();
    printf("Action Q-values:\n");
    for (auto& d : dist)
        if (d.count > 0)
            printf("  %10s: %5d entries  avg_q=%.2f\n",
                   d.action_name.c_str(), d.count, d.avg_q);
}

void run_rl_mode(int test_episodes, int train_episodes) {
    if (test_episodes > 0) rl_run_test_mode(test_episodes);
    if (train_episodes > 0) rl_run_train_mode(train_episodes);
}

// ── F15.4: Mirror self-play training ──
// ── G52: run_rl_mirror_mode 子步骤 (原 91 行拆 4 段) ──

// 四种对战风格 (镜像 Boss 要学的对象)
static void rl_mirror_build_profiles(PlayerHabitProfile (&profiles)[4]) {
    // AGGRESSIVE: high attack, low dodge
    profiles[0].attack_frequency = 0.90f; profiles[0].dodge_rate = 0.08f;
    profiles[0].aggression_score = 0.95f; profiles[0].style = PlayerStyle::AGGRESSIVE;
    profiles[0].predict_attack_heavy = true;
    // DEFENSIVE: high dodge, low attack
    profiles[1].attack_frequency = 0.25f; profiles[1].dodge_rate = 0.45f;
    profiles[1].aggression_score = 0.35f; profiles[1].style = PlayerStyle::DEFENSIVE;
    profiles[1].predict_low_dodge = false;
    // SNIPER + MAGE 共用下标 2 (retreat + skill-heavy 的组合风格)
    profiles[2].attack_frequency = 0.55f; profiles[2].retreat_rate = 0.40f;
    profiles[2].style = PlayerStyle::SNIPER;
    profiles[2].skill_frequency = 0.25f; profiles[2].predict_skill_spam = true;
    profiles[2].predicted_fav_skill = 1;   // fireball
    // BALANCED (default)
    profiles[3] = PlayerHabitProfile{};
}

// 带风格标记的战斗场景 (Boss 取 d=1 可交战距离 — 同 rl_make_scenario 的修正)
static mcts::SimulationState rl_mirror_scenario(int style_id) {
    mcts::SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.x = 5; s.player.y = 5;
    s.player.attack = 10; s.player.pdef = 3; s.player.mdef = 1;
    s.player.alive = true;
    s.player.skill_cooldowns[0] = 0; s.player.skill_cooldowns[1] = 0;
    // v0.9.36 G32-E: Boss 原坐标 (3,0) 距玩家 (5,5) 为 7 格, 近战与技能
    // 伤害均无法触发, 镜像训练学不到真实交战。改到 (5,6) 即 d=1
    s.monsters.push_back(mcts::MonsterSnapshot{
        "mirror_boss", 200, 200, 5, 6, 15, 5, 5, {}, true, true});
    s.player_style = style_id + 1;   // 1-4
    return s;
}

// 单个风格的自对弈训练: 跑 episodes 局, Q 表落盘, 输出胜率/tail/reward
static void rl_mirror_train_style(int style_i, int episodes,
                                  const PlayerHabitProfile& profile) {
    rl::QAgent q_agent(0.15, 0.9, 0.12);
    rl::CombatEnvironment env;
    const std::string qpath = std::string("saves/rl_mirror_q_") +
                              profile.style_name() + ".json";
    rl_load_qtable(q_agent, qpath);

    int wins = 0;
    double total_r = 0;
    int tail_wins = 0;
    const int tail_start = episodes - episodes / 10;   // 末段 10% 低探索统计
    for (int i = 0; i < episodes; i++) {
        const bool victory = rl_mirror_one_episode(q_agent, env, style_i, i,
                                                   episodes, profile, total_r);
        if (victory) { wins++; if (i >= tail_start) tail_wins++; }
    }

    rl_print_train_result(profile.style_name(), wins, episodes, tail_wins,
                          tail_start, total_r / episodes, q_agent.table_size());
    rl_save_qtable(q_agent, qpath);
}

void run_rl_mirror_mode(int episodes) {
    PlayerHabitProfile profiles[4];
    rl_mirror_build_profiles(profiles);

    printf("═══ RL MIRROR: %d episodes × 4 styles ═══\n", episodes);
    for (int style_i = 0; style_i < 4; style_i++)
        rl_mirror_train_style(style_i, episodes, profiles[style_i]);
    printf("Mirror RL complete.\n");
}
