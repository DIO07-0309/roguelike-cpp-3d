#include "ai/player_behavior/player_behavior_recorder.h"
#include <cstdio>
#include <cmath>
#include <cstring>

// ══════════════════════════════════════════════════════
// Action stream
// ══════════════════════════════════════════════════════

PlayerBehaviorRecorder& PlayerBehaviorRecorder::inst() {
    static PlayerBehaviorRecorder rec;
    return rec;
}

void PlayerBehaviorRecorder::set_context(float hp_pct, float enemy_dist,
    int skill_ready_mask) {
    _ctx_hp = hp_pct;
    _ctx_dist = enemy_dist;
    _ctx_mask = skill_ready_mask;
}

void PlayerBehaviorRecorder::set_battle_context(int facing_dir, int hit_in_1s) {
    _ctx_facing = facing_dir;
    _ctx_hits = hit_in_1s;
}

void PlayerBehaviorRecorder::record(const PlayerAction& action) {
    if (action.type == PlayerActionType::NONE) return;
    PlayerAction a = action;
    if (a.hp < 0) a.hp = _ctx_hp;
    if (a.enemy_dist < 0) a.enemy_dist = _ctx_dist;
    a.skill_ready_mask = _ctx_mask;
    if (a.facing_dir < 0) a.facing_dir = _ctx_facing;
    if (a.hit_in_1s <= 0) a.hit_in_1s  = _ctx_hits;
    _history.push_back(a);

    // ── Update aggregate data in real time ──
    if (action.type == PlayerActionType::ATTACK && action.weapon_name) {
        _data.record_weapon_attack(action.weapon_name);
    } else if (action.type == PlayerActionType::SKILL) {
        // skill_id: 0=slash, 1=fireball, 2=self_heal, 3=the_world
        const char* ids[] = {"slash","fireball","self_heal","the_world"};
        if (action.skill_id >= 0 && action.skill_id < 4)
            _data.record_skill_use(ids[action.skill_id]);
    } else if (action.type == PlayerActionType::TAKE_DAMAGE && action.value > 0) {
        _data.record_damage_taken(action.value, action.floor);
    } else if (action.type == PlayerActionType::DODGE) {
        _data.dodge_count++;
    } else if (action.type == PlayerActionType::MOVE) {
        if (action.value == 0) _data.move_left++;
        else if (action.value == 1) _data.move_right++;
        else if (action.value == 2) _data.move_up++;
        else if (action.value == 3) _data.move_down++;
    }
}

void PlayerBehaviorRecorder::clear() { _history.clear(); _data.reset(); }

// ══════════════════════════════════════════════════════
// Convenience hooks
// ══════════════════════════════════════════════════════

void PlayerBehaviorRecorder::on_weapon_attack(const char* wt_name, float time,
    int floor, float px, float py, int weapon_type, int combo_stage) {
    PlayerAction a;
    a.type = PlayerActionType::ATTACK;
    a.timestamp = time; a.floor = floor; a.pos_x = px; a.pos_y = py;
    a.weapon_name = wt_name;
    a.weapon_type = weapon_type;
    a.combo_stage = combo_stage;
    record(a);
}

void PlayerBehaviorRecorder::on_skill_use(const char* skill_id, float time,
    int floor, float px, float py) {
    int sid = -1;
    if (strcmp(skill_id,"slash")==0) sid=0;
    else if (strcmp(skill_id,"fireball")==0) sid=1;
    else if (strcmp(skill_id,"self_heal")==0) sid=2;
    else if (strcmp(skill_id,"the_world")==0) sid=3;

    PlayerAction a;
    a.type = PlayerActionType::SKILL;
    a.timestamp = time; a.floor = floor; a.pos_x = px; a.pos_y = py;
    a.skill_id = sid; a.value = 0;
    record(a);
}

void PlayerBehaviorRecorder::on_move_state_change(float time, int floor,
    float px, float py, int direction) {
    PlayerAction a;
    a.type = PlayerActionType::MOVE;
    a.timestamp = time; a.floor = floor; a.pos_x = px; a.pos_y = py;
    a.value = direction; // 0=left, 1=right, 2=up, 3=down
    record(a);
}

void PlayerBehaviorRecorder::on_dodge(float time, int floor, float px, float py) {
    PlayerAction a;
    a.type = PlayerActionType::DODGE;
    a.timestamp = time; a.floor = floor; a.pos_x = px; a.pos_y = py;
    record(a);
}

void PlayerBehaviorRecorder::on_heal(float time, int floor, int amount) {
    PlayerAction a;
    a.type = PlayerActionType::HEAL;
    a.timestamp = time; a.floor = floor; a.value = amount;
    record(a);
}

void PlayerBehaviorRecorder::on_floor_enter(float time, int floor) {
    PlayerAction a;
    a.type = PlayerActionType::FLOOR_ENTER;
    a.timestamp = time; a.floor = floor;
    _data.floors_recorded++;
    record(a);
}

// ══════════════════════════════════════════════════════
// Debug output
// ══════════════════════════════════════════════════════

void PlayerBehaviorRecorder::print_debug(char* buf, size_t buf_size) const {
    int atk = 0, sk = 0, mov = 0, dodge = 0, dmg = 0;
    for (auto& a : _history) {
        switch (a.type) {
        case PlayerActionType::ATTACK: atk++; break;
        case PlayerActionType::SKILL:   sk++; break;
        case PlayerActionType::MOVE:   mov++; break;
        case PlayerActionType::DODGE:  dodge++; break;
        case PlayerActionType::TAKE_DAMAGE: dmg++; break;
        default: break;
        }
    }
    float avg_interval = atk > 0 ? (_history.back().timestamp - _history.front().timestamp) / (float)atk : 0;
    snprintf(buf, buf_size,
        "ATTACK %d | SKILL %d | MOVE %d | DODGE %d\n"
        "DMG taken %d | avg atk interval %.2fs | floors %d",
        atk, sk, mov, dodge, _data.total_damage_taken,
        avg_interval, _data.floors_recorded);
}

void PlayerBehaviorRecorder::save_to_file(const char* path) const {
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "[\n");
    int n = (int)_history.size();
    for (int i = 0; i < n; i++) {
        auto& a = _history[i];
        const char* tname = "NONE";
        switch (a.type) {
        case PlayerActionType::ATTACK: tname="ATTACK"; break;
        case PlayerActionType::SKILL:  tname="SKILL"; break;
        case PlayerActionType::MOVE:   tname="MOVE"; break;
        case PlayerActionType::DODGE:  tname="DODGE"; break;
        case PlayerActionType::TAKE_DAMAGE: tname="TAKE_DMG"; break;
        case PlayerActionType::DEAL_DAMAGE: tname="DEAL_DMG"; break;
        case PlayerActionType::HEAL:   tname="HEAL"; break;
        case PlayerActionType::FLOOR_ENTER: tname="FLOOR"; break;
        default: break;
        }
        fprintf(f, "  {\"type\":\"%s\",\"t\":%.2f,\"floor\":%d",
            tname, a.timestamp, a.floor);
        if (a.value) fprintf(f, ",\"val\":%d", a.value);
        if (a.skill_id >= 0) fprintf(f, ",\"sk\":%d", a.skill_id);
        if (a.weapon_name) fprintf(f, ",\"wp\":\"%s\"", a.weapon_name);
        fprintf(f, "}%s\n", (i < n-1) ? "," : "");
    }
    fprintf(f, "]\n");
    fclose(f);
}
