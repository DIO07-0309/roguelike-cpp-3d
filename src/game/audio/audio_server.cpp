#include "audio_server.h"
#include "wave_synth.h"
#include "resource_manager.h"   // G10.2-B3A: 音频路径经 Asset ID 查询
#include "core/logger.h"
#include <cmath>
#include <random>
#include <cstring>
#include <unordered_set>

// ---- 将样本数组打包为 Raylib Sound ----
static Sound _vec_to_sound(const std::vector<short>& data) {
    int n = (int)data.size();
    short* buf = (short*)malloc(n * sizeof(short));
    memcpy(buf, data.data(), n * sizeof(short));

    Wave wave;
    wave.frameCount = n;
    wave.sampleRate = SR;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = buf;

    Sound snd = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return snd;
}

// ---- 各音效合成器 (Python sfx_engine.py 直译) ----

static Sound _compile_melee() {
    float dur = 0.18f; int n = (int)(SR * dur);
    auto sw = square_wave(n, [=](float t){return 500 - 420 * std::pow(t/dur, 0.6f);}, spike(0.7f, 0.01f, dur));
    auto ns = noise_wave(n, spike(0.25f, 0.02f, dur*0.5f));
    auto mx = mix({&sw, &ns});
    return _vec_to_sound(mx);
}

static Sound _compile_hit() {
    float dur = 0.12f; int n = (int)(SR * dur);
    auto th = sine_wave(n, [=](float){return 80.0f;}, spike(0.9f, 0.005f, dur));
    auto ns = noise_wave(n, spike(0.3f, 0.01f, dur*0.6f));
    auto mx = mix({&th, &ns});
    return _vec_to_sound(mx);
}

static Sound _compile_slash() {
    float dur = 0.35f; int n = (int)(SR * dur);
    auto sw = square_wave(n, [=](float t){return 800 - 650 * std::sqrt(t/dur);}, spike(0.8f, 0.01f, dur));
    auto ring = sine_wave(n, [=](float t){return 2400.0f - 600 * (t/dur);}, decay(0.25f, dur));
    auto ns = noise_wave(n, spike(0.15f, 0.005f, 0.1f));
    auto mx = mix({&sw, &ring, &ns});
    return _vec_to_sound(mx);
}

static Sound _compile_bolt() {
    float dur = 0.45f; int n = (int)(SR * dur);
    std::vector<short> result(n, 0);
    std::mt19937 rng2(42);
    std::uniform_int_distribution<int> pos_dist(0, n-1);
    for (int k = 0; k < 32; k++) {
        int start = pos_dist(rng2);
        int length = 80 + rng2() % 321;
        float burst = (float)(rng2() % 900) / 1000.0f;
        for (int i = 0; i < length; i++) {
            int pos = start + i;
            if (pos < n) {
                float d = std::pow(std::max(0.0f, 1.0f - (float)i / length), 3.0f);
                result[pos] = (short)(MAX_AMP * burst * d * ((float)(rng2()%2000)/1000.0f - 1.0f));
            }
        }
    }
    auto thunder = sine_wave(n, [=](float t){return 45.0f + 20.0f * sinf(t*15);}, decay(0.4f, dur));
    auto mx = mix({&result, &thunder});
    return _vec_to_sound(mx);
}

static Sound _compile_heal() {
    float dur = 0.7f; int n = (int)(SR * dur);
    std::vector<short> result(n, 0);
    float notes[] = {1046.5f, 1318.5f, 1568.0f, 2093.0f};
    for (int j = 0; j < 4; j++) {
        int start = (int)(SR * j * 0.12f);
        auto chunk = sine_wave((int)(SR * 0.25f), [=](float){return notes[j];}, decay(0.45f, 0.25f));
        for (int i = 0; i < (int)chunk.size(); i++) {
            if (start + i < n) {
                int v = (int)result[start+i] + (int)chunk[i];
                result[start+i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, v));
            }
        }
    }
    auto wind = sine_wave(n, [=](float){return 800.0f;}, decay(0.15f, dur));
    auto pad = sine_wave(n, [=](float){return 523.25f;}, decay(0.12f, dur));
    auto mx = mix({&result, &wind, &pad});
    return _vec_to_sound(mx);
}

static Sound _compile_pickup() {
    float dur = 0.25f; int n = (int)(SR * dur);
    auto c5 = sine_wave(n, [=](float){return 523.25f;}, decay(0.35f, dur));
    auto e5 = sine_wave(n, [=](float){return 659.25f;}, decay(0.35f, dur));
    auto mx = mix({&c5, &e5});
    return _vec_to_sound(mx);
}

// Q4.4: 玩家受击 — 低沉闷响 + 短促噪声
static Sound _compile_hurt() {
    float dur = 0.22f; int n = (int)(SR * dur);
    auto th = sine_wave(n, [=](float t){return 70.0f - 25.0f * (t/dur);}, spike(0.85f, 0.005f, dur));
    auto ns = noise_wave(n, spike(0.35f, 0.01f, dur*0.6f));
    auto mx = mix({&th, &ns});
    return _vec_to_sound(mx);
}

// Q4.4: 怪物攻击 — 低频嘶吼 (方波下滑 + 噪声)
static Sound _compile_monster_atk() {
    float dur = 0.28f; int n = (int)(SR * dur);
    auto sw = square_wave(n, [=](float t){return 180.0f - 90.0f * (t/dur);}, spike(0.5f, 0.01f, dur));
    auto ns = noise_wave(n, spike(0.25f, 0.02f, dur*0.7f));
    auto mx = mix({&sw, &ns});
    return _vec_to_sound(mx);
}

// Q4.6: 冰裂 — 高频碎冰 (短促叠加)
static Sound _compile_ice_crack() {
    float dur = 0.25f; int n = (int)(SR * dur);
    std::vector<short> result(n, 0);
    float cents[] = {2400.0f, 3000.0f, 3600.0f, 2100.0f};
    for (int j = 0; j < 4; j++) {
        int start = (int)(SR * j * 0.05f);
        auto chunk = square_wave((int)(SR * 0.12f), [=](float){return cents[j];}, decay(0.35f, 0.12f));
        for (int i = 0; i < (int)chunk.size() && start + i < n; i++)
            result[start + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)result[start+i] + chunk[i]));
    }
    auto ns = noise_wave(n, spike(0.3f, 0.005f, dur));
    auto mx = mix({&result, &ns});
    return _vec_to_sound(mx);
}

// Q4.6: 闪电 — 高频噼啪 (噪声 burst 快速衰减)
static Sound _compile_lightning() {
    float dur = 0.35f; int n = (int)(SR * dur);
    auto ns = noise_wave(n, spike(0.55f, 0.002f, dur*0.5f));
    auto hi = sine_wave(n, [=](float t){return 1800.0f + 400.0f * sinf(t * 40.0f);}, decay(0.3f, dur*0.6f));
    auto mx = mix({&ns, &hi});
    return _vec_to_sound(mx);
}

// Q4.6: 召唤 — 上升共鸣 (正弦滑升)
static Sound _compile_summon() {
    float dur = 0.6f; int n = (int)(SR * dur);
    auto up = sine_wave(n, [=](float t){return 200.0f + 500.0f * (t/dur) + 100.0f * sinf(t * 6.0f);},
                        decay(0.4f, dur));
    auto pad = sine_wave(n, [=](float){return 98.0f;}, decay(0.25f, dur));
    auto mx = mix({&up, &pad});
    return _vec_to_sound(mx);
}

// Q4.5: UI 点击 — 短促清脆 (高频短音)
static Sound _compile_ui_click() {
    float dur = 0.06f; int n = (int)(SR * dur);
    auto tick = square_wave(n, [=](float){return 1400.0f;}, spike(0.4f, 0.002f, dur));
    return _vec_to_sound(tick);
}

// Q4.5: UI 确认 — 双音上行 (确认/进入)
static Sound _compile_ui_confirm() {
    float dur = 0.16f; int n = (int)(SR * dur);
    std::vector<short> result(n, 0);
    float notes[] = {784.0f, 1046.5f};
    for (int j = 0; j < 2; j++) {
        int start = (int)(SR * j * 0.07f);
        auto chunk = sine_wave((int)(SR * 0.1f), [=](float){return notes[j];}, decay(0.4f, 0.1f));
        for (int i = 0; i < (int)chunk.size() && start + i < n; i++)
            result[start + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)result[start+i] + chunk[i]));
    }
    return _vec_to_sound(result);
}

// 门开启 — 门轴摩擦噪声 + 低频沉降
// (此前 player_controller/game_scene_input 共 4 处 play_sfx("door_open") 从未注册, 开门完全无声)
static Sound _compile_door_open() {
    float dur = 0.28f; int n = (int)(SR * dur);
    auto creak = noise_wave(n, spike(0.26f, 0.05f, dur));
    auto low   = sine_wave(n, [=](float t){return 110.0f - 42.0f * (t/dur);}, decay(0.30f, dur));
    return _vec_to_sound(mix({&creak, &low}));
}

// 领域扩张 — 低频上扬轰鸣 (assets/domain_expand.* 缺失时的合成回退, 对齐 timestop 的做法)
static Sound _compile_domain_expand() {
    float dur = 0.7f; int n = (int)(SR * dur);
    auto swell = sine_wave(n, [=](float t){return 60.0f + 90.0f * (t/dur);}, decay(0.32f, dur));
    auto body  = square_wave(n, [=](float t){return 52.0f + 28.0f * (t/dur);}, decay(0.15f, dur));
    return _vec_to_sound(mix({&swell, &body}));
}

static Sound _compile_levelup() {
    float dur = 0.5f; int n = (int)(SR * dur);
    std::vector<short> result(n, 0);
    float notes[] = {392.0f, 523.25f, 659.25f, 784.0f, 1046.5f};
    for (int j = 0; j < 5; j++) {
        int start = (int)(SR * j * 0.08f);
        auto chunk = square_wave((int)(SR * 0.15f), [=](float){return notes[j];}, decay(0.4f, 0.15f));
        for (int i = 0; i < (int)chunk.size(); i++) {
            if (start + i < n) {
                int v = (int)result[start+i] + (int)chunk[i];
                result[start+i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, v));
            }
        }
    }
    return _vec_to_sound(result);
}

static Sound _compile_victory() {
    float dur = 2.5f; int n = (int)(SR * dur);
    std::vector<short> result(n, 0);

    // 琶音上行
    struct Note { float freq, start, len; };
    Note notes[] = {
        {261.63f,0.0f,0.35f},{329.63f,0.2f,0.3f},{392.0f,0.4f,0.3f},
        {523.25f,0.6f,0.4f},{659.25f,0.9f,0.25f},{784.0f,1.05f,0.3f},{1046.5f,1.25f,0.5f}
    };
    for (auto& nt : notes) {
        int start = (int)(SR * nt.start), chunk_n = (int)(SR * nt.len);
        auto chunk = square_wave(chunk_n, [=](float){return nt.freq;}, decay(0.5f, nt.len));
        for (int i = 0; i < chunk_n; i++)
            if (start + i < n) { int v = (int)result[start+i] + (int)chunk[i]; result[start+i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, v)); }
    }
    // 和弦铺垫
    int pad_start = (int)(SR * 1.2f), pad_n = (int)(SR * 1.1f);
    float chords[] = {261.63f, 329.63f, 392.0f};
    for (auto f : chords) {
        auto pad = sine_wave(pad_n, [=](float){return f;}, decay(0.35f, 1.1f));
        for (int i = 0; i < pad_n; i++)
            if (pad_start + i < n) { int v = (int)result[pad_start+i] + (int)pad[i]; result[pad_start+i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, v)); }
    }
    // 终响
    int fin_start = (int)(SR * 1.9f), fin_n = (int)(SR * 0.55f);
    float fin_notes[] = {523.25f, 659.25f, 784.0f, 1046.5f};
    for (auto f : fin_notes) {
        auto fn = square_wave(fin_n, [=](float){return f;}, decay(0.6f, 0.55f));
        for (int i = 0; i < fin_n; i++)
            if (fin_start + i < n) { int v = (int)result[fin_start+i] + (int)fn[i]; result[fin_start+i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, v)); }
    }
    return _vec_to_sound(result);
}

// ============================================================
// AudioServer
// ============================================================
bool AudioServer::g_muted = false;  // Q3.1: --sim 静音

void AudioServer::init() {
    if (g_muted) return;  // Q3.1: sim 模式跳过音频合成
    LOG_INFO("音频: 初始化SFX...");
    _sfx["melee"]   = _compile_melee();
    _sfx["hit"]     = _compile_hit();
    _sfx["slash"]   = _compile_slash();
    _sfx["bolt"]    = _compile_bolt();
    _sfx["heal"]    = _compile_heal();
    _sfx["pickup"]  = _compile_pickup();
    _sfx["levelup"] = _compile_levelup();
    _sfx["victory"] = _compile_victory();
    _sfx["hurt"]        = _compile_hurt();         // Q4.4: 玩家受击
    _sfx["monster_atk"] = _compile_monster_atk();  // Q4.4: 怪物攻击
    _sfx["ice_crack"]   = _compile_ice_crack();    // Q4.6: recipe 音效
    _sfx["lightning"]   = _compile_lightning();    // Q4.6: recipe 音效
    _sfx["summon"]      = _compile_summon();       // Q4.6: recipe 音效
    _sfx["ui_click"]    = _compile_ui_click();     // Q4.5: UI 点击
    _sfx["ui_confirm"]  = _compile_ui_confirm();   // Q4.5: UI 确认
    _sfx["door_open"]   = _compile_door_open();    // 门开启: 4 处调用点此前无注册 → 完全无声

    // timestop: 直接从已知路径加载 (ResourceManager 在 AudioServer 之后初始化)
    _sfx["timestop"] = _compile_bolt();  // fallback
    {
        const char* paths[] = {"assets/jojo_timestop.wav", "assets/jojo_timestop.mp3"};
        for (auto p : paths) {
            if (!FileExists(p)) continue;
            int dataSize = 0;
            unsigned char* data = LoadFileData(p, &dataSize);
            if (data && dataSize > 0) {
                Wave wave = LoadWaveFromMemory(".wav", data, dataSize);
                if (wave.frameCount > 0) {
                    _sfx["timestop"] = LoadSoundFromWave(wave);
                    LOG_INFO("音频: timestop loaded frameCount=%d path=%s", _sfx["timestop"].frameCount, p);
                }
                UnloadWave(wave);
                UnloadFileData(data);
                break;
            }
            if (data) UnloadFileData(data);
        }
    }

    // domain_expand: 直接从已知路径加载
    _sfx["domain_expand"] = _compile_domain_expand();  // fallback: 文件缺失不再静默无声
    {
        const char* paths[] = {"assets/domain_expand.wav", "assets/domain_expand.mp3"};
        for (auto p : paths) {
            if (!FileExists(p)) continue;
            int dataSize = 0;
            unsigned char* data = LoadFileData(p, &dataSize);
            if (data && dataSize > 0) {
                Wave wave = LoadWaveFromMemory(".wav", data, dataSize);
                if (wave.frameCount > 0) {
                    _sfx["domain_expand"] = LoadSoundFromWave(wave);
                    LOG_INFO("音频: domain_expand loaded frameCount=%d path=%s", _sfx["domain_expand"].frameCount, p);
                }
                UnloadWave(wave);
                UnloadFileData(data);
                break;
            }
            if (data) UnloadFileData(data);
        }
    }

    // A7-T4: 加载音效资源 (Kenney CC0)
    const char* sfx_list[] = {
        "hit_sword", "hit_spear", "hit_dagger", "hit_crossbow", "hit_staff",
        "death_normal", "death_elite", "death_boss",
        "skill_cast", "ui_click", "ui_select"
    };
    int sfx_loaded = 0;
    for (const char* sfx : sfx_list) {
        std::string path = _sfx_path + sfx + ".wav";
        if (FileExists(path.c_str())) {
            _sfx[sfx] = LoadSound(path.c_str());
            sfx_loaded++;
        }
    }
    if (sfx_loaded == 0)
        LOG_WARN("音频: %s 下 0/%d 个 Kenney SFX 命中 — 该层静默缺失, 全程仅用合成音",
                 _sfx_path.c_str(), (int)(sizeof(sfx_list) / sizeof(sfx_list[0])));

    // BGM
    LOG_INFO("音频: 合成BGM(4支)...");
    _bgm.init();
    LOG_INFO("音频: 就绪 (SFX %d 项, 其中文件 %d 项) + BGM 4 支", (int)_sfx.size(), sfx_loaded);
}

void AudioServer::close() {
    if (g_muted) return;  // Q3.1
    for (auto& [_, snd] : _sfx) UnloadSound(snd);
    _sfx.clear();
    _bgm.close();
}

void AudioServer::play_bgm(const std::string& name, float vol) {
    if (g_muted) return;  // Q3.1
    LOG_DEBUG("BGM -> %s (vol:%.2f)", name.c_str(), vol);
    _bgm.play(name, vol);
}
void AudioServer::stop_bgm(float) {
    if (g_muted) return;  // Q3.1
    _bgm.stop();
}

void AudioServer::play_sfx(const std::string& name, float vol) {
    if (g_muted) return;  // Q3.1
    auto it = _sfx.find(name);
    if (it != _sfx.end()) { SetSoundVolume(it->second, vol); PlaySound(it->second); return; }
    // 未注册名此前静默丢弃: 每个名字首次告警一次, 避免每帧刷屏
    static std::unordered_set<std::string> warned;
    if (warned.insert(name).second)
        LOG_WARN("音频: play_sfx('%s') 未注册 — 静默丢弃 (漏注册或配置空串)", name.c_str());
}

void AudioServer::update(float dt) { _bgm.update(dt); }
