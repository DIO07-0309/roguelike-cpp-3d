#include "bgm_engine.h"
#include "wave_synth.h"
#include <cmath>
#include <cstring>

// 音符→频率 (MIDI=60=C4=261.63Hz, MIDI=69=A4=440Hz)
static float midi_freq(int midi) { return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f); }
static int note_midi(const char* n) {
    static const char* names[] = {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    char name[3] = {(char)n[0], n[1] ? (char)n[1] : (char)0, (char)0};
    int oct = n[1] ? (n[2] ? n[2] : n[1]) : n[1];
    if (oct == 0 || oct < '0' || oct > '9') oct = 4;
    else oct = oct - '0';
    for (int s = 0; s < 12; s++) {
        if (name[0] == names[s][0] && (!names[s][1] || name[1] == names[s][1]))
            return (oct + 1) * 12 + s;
    }
    return 60;
}
static float note_freq(const char* n) { return midi_freq(note_midi(n)); }

static std::vector<short> make_note(float freq, float dur, float amp, const char* wave = "square") {
    int n = (int)(SR * dur);
    if (wave[0] == 't') { // triangle
        return sine_wave(n, [=](float){return freq;}, decay(amp * 0.7f, dur));
    } else {
        return square_wave(n, [=](float){return freq;}, decay(amp, dur));
    }
}

// 和弦渲染
static std::vector<short> render_chord(int root, const std::vector<int>& intervals,
                                        float dur, float amp) {
    std::vector<short> out((int)(SR * dur), 0);
    for (int iv : intervals) {
        float f = midi_freq(root + iv);
        auto ch = sine_wave((int)(SR * dur), [=](float){return f;}, decay(amp / (float)intervals.size() * 2.0f, dur));
        auto mx = mix({&out, &ch});
        out = mx;
    }
    return out;
}

// 鼓（简化）
static std::vector<short> kick() {
    auto ns = noise_wave((int)(SR * 0.12f), spike(0.45f, 0.01f, 0.12f));
    auto lp = sine_wave((int)(SR * 0.12f), [=](float){return 80.0f;}, spike(0.8f, 0.01f, 0.15f));
    return mix({&ns, &lp});
}
static std::vector<short> snare() {
    return noise_wave((int)(SR * 0.08f), spike(0.3f, 0.01f, 0.1f));
}
static std::vector<short> hihat() {
    return noise_wave((int)(SR * 0.04f), spike(0.15f, 0.005f, 0.05f));
}

// ============================================================
// BGM 序列数据 (Python bgm_engine.py 直译)
// ============================================================

Sound BGMEngine::_compile_bgm(const std::string& name) {
    float bpm = 110;
    if (name == "select") bpm = 85;
    else if (name == "dungeon") bpm = 75;
    else if (name == "boss") bpm = 150;
    float beat = 60.0f / bpm;
    float total = 0;
    int n;

    // 选择和弦进行
    struct NoteEntry { const char* note; float len; };
    std::vector<NoteEntry> chords, melody, bass;
    const char* mw = "square", *cw = "sine", *bw = "triangle";

    if (name == "title") {
        chords = {{"C4",1.6f},{"G3",1.6f},{"A3",1.6f},{"F3",1.6f},{"C4",1.6f},{"G3",1.6f},{"F3",1.6f},{"C4",2.4f}};
        melody = {{"C5",0.3f},{"E5",0.2f},{"G5",0.3f},{"C6",0.4f},{0,0.2f},{"G5",0.2f},{"E5",0.3f},{"C5",0.3f},{"D5",0.2f},{0,0.1f},{"C6",0.3f},{"B5",0.2f},{"A5",0.3f},{"G5",0.2f},{"E5",0.3f},{"F5",0.3f},{"G5",0.3f},{"C6",0.4f},{0,0.3f},{"C6",0.3f},{"B5",0.2f},{"C6",0.3f},{"G5",0.4f},{"E5",0.3f},{"C5",0.3f},{"D5",0.3f},{"C5",0.6f}};
        bass  = {{"C3",0.8f},{"G2",0.8f},{"A2",0.8f},{"F2",0.8f},{"C3",0.8f},{"G2",0.8f},{"F2",0.8f},{"C3",1.2f}};
    } else if (name == "challenge") {
        // G10.7-B: 竞技场专属 — 比 boss 更急促的波次战斗曲
        // C 小调 i-VI-VII-v 紧张循环 + 短促顿音旋律 (三连冲锋感)
        bpm = 160; beat = 60.0f / bpm;
        chords = {{"C3",0.5f},{"Ab2",0.5f},{"Bb2",0.5f},{"G2",0.5f}};
        melody = {{"C4",0.08f},{"Eb4",0.08f},{"G4",0.08f},{"C5",0.16f},
                  {"Bb4",0.08f},{"G4",0.08f},{"Bb4",0.08f},{"C5",0.16f},
                  {"F4",0.08f},{"Ab4",0.08f},{"C5",0.08f},{"Eb5",0.16f},
                  {"D5",0.08f},{"Bb4",0.08f},{"G4",0.08f},{"C5",0.24f}};
        bass  = {{"C2",0.125f},{"C2",0.125f},{"C2",0.125f},{"C2",0.125f},
                 {"Ab1",0.125f},{"Ab1",0.125f},{"Ab1",0.125f},{"Ab1",0.125f},
                 {"Bb1",0.125f},{"Bb1",0.125f},{"Bb1",0.125f},{"Bb1",0.125f},
                 {"G1",0.125f},{"G1",0.125f},{"G1",0.125f},{"G1",0.125f}};
        mw = "saw"; bw = "square";
    } else if (name == "select") {
        chords = {{"A3",2.0f},{"F3",2.0f},{"G3",2.0f},{"E3",2.0f},{"A3",2.0f},{"F3",1.5f},{"C4",1.0f},{"G3",1.5f}};
        melody = {{0,0.8f},{"A4",0.5f},{0,0.3f},{"C5",0.4f},{0,0.6f},{"E5",0.5f},{0,0.4f},{"D5",0.3f},{0,0.5f},{"C5",0.4f},{"B4",0.3f},{"A4",0.6f}};
        mw = "triangle";
    } else if (name == "dungeon") {
        chords = {{"C3",3.0f},{"Db3",3.0f},{"Eb3",3.0f},{"C3",3.0f}};
        melody = {{"C4",0.5f},{0,1.0f},{"Db4",0.4f},{0,0.8f},{"Eb4",0.4f},{0,1.2f},{"C4",0.6f},{0,0.6f}};
    } else if (name == "boss") {
        bpm = 150; beat = 60.0f / bpm;
        chords = {{"C3",1.0f},{"Ab2",1.0f},{"Bb2",1.0f},{"G2",1.0f}};
        melody = {{"C4",0.12f},{"Eb4",0.12f},{"G4",0.12f},{"C5",0.2f},{"B4",0.12f},{"G4",0.12f},{"F4",0.15f},{"Eb4",0.1f},{0,0.05f}};
        bass  = {{"C2",0.25f},{"C2",0.25f},{"Ab1",0.25f},{"Ab1",0.25f},{"Bb1",0.25f},{"Bb1",0.25f},{"G1",0.25f},{"G1",0.25f}};
        mw = "saw"; bw = "square";
    } else if (name == "victory") {
        // Q3.16: 通关动画专属 BGM — C 大调 I-V-vi-IV 经典欢快进行 (明快上行旋律,
        // 替代原通关后沿用的紧张 Boss 曲), 播放至回到主界面
        bpm = 132; beat = 60.0f / bpm;
        chords = {{"C4",1.0f},{"G3",1.0f},{"A3",1.0f},{"F3",1.0f},
                  {"C4",1.0f},{"G3",1.0f},{"F3",1.0f},{"C4",1.0f}};
        melody = {
            {"C5",0.25f},{"E5",0.25f},{"G5",0.25f},{"C6",0.25f},   // I: 上行琶音
            {"B5",0.25f},{"G5",0.25f},{"D5",0.25f},{0,0.25f},      // V
            {"A5",0.25f},{"C6",0.25f},{"B5",0.25f},{"G5",0.25f},   // vi
            {"F5",0.25f},{"A5",0.25f},{"G5",0.25f},{"E5",0.25f},   // IV
            {"C5",0.25f},{"E5",0.25f},{"G5",0.25f},{"C6",0.25f},   // I'
            {"B5",0.25f},{"C6",0.25f},{"D5",0.25f},{"G5",0.25f},   // V'
            {"A5",0.25f},{"F5",0.25f},{"G5",0.25f},{"E5",0.25f},   // IV'
            {"C6",0.5f},{"G5",0.25f},{"E5",0.25f},                 // 终止高音解决
        };
        bass  = {{"C3",0.5f},{"C3",0.5f},{"G2",0.5f},{"G2",0.5f},
                 {"A2",0.5f},{"A2",0.5f},{"F2",0.5f},{"F2",0.5f},
                 {"C3",0.5f},{"C3",0.5f},{"G2",0.5f},{"G2",0.5f},
                 {"F2",0.5f},{"F2",0.5f},{"C3",0.5f},{"G2",0.5f}};
        mw = "square"; bw = "triangle";
    } else {
        // G6.1/A7-T1: Biome BGM variants (distinct style per biome)
        if (name == "prison") {
            // 监牢: 72 BPM, square wave, 压抑感
            bpm = 72; beat = 60.0f / bpm;
            mw = "square";
            chords = {{"C3",3.0f},{"Db3",3.0f},{"Eb3",3.0f},{"C3",3.0f}};
            melody = {{"C4",0.5f},{0,1.0f},{"Db4",0.4f},{0,0.8f},{"Eb4",0.4f},{0,1.2f},{"C4",0.6f},{0,0.6f}};
            bw = "square";
            bass = {{"C2",1.5f},{"Db2",1.5f},{"Eb2",1.5f},{"C2",1.5f}};
        } else if (name == "volcano") {
            // 火山: 90 BPM, saw wave, 灼热感
            bpm = 90; beat = 60.0f / bpm;
            mw = "saw"; bw = "triangle";
            chords = {{"C3",3.0f},{"Db3",3.0f},{"Eb3",3.0f},{"C3",3.0f}};
            melody = {{"C4",0.4f},{"Eb4",0.4f},{"G4",0.4f},{"Bb4",0.4f},{"C5",0.4f},{"Bb4",0.4f},{"G4",0.4f},{"Eb4",0.4f}};
            bass = {{"C2",0.75f},{"Db2",0.75f},{"Eb2",0.75f},{"C2",0.75f}};
        } else if (name == "abyss") {
            // 深渊: 62 BPM, triangle wave, 虚空感
            bpm = 62; beat = 60.0f / bpm;
            mw = "triangle"; bw = "sine";
            chords = {{"C3",4.0f},{"Ab2",4.0f},{"Eb3",4.0f},{"C3",4.0f}};
            melody = {{"C5",1.0f},{0,0.5f},{"Eb5",0.5f},{0,1.0f},{"Ab5",1.0f},{0,1.5f}};
            bass = {{"C2",2.0f},{"Ab1",2.0f},{"Eb2",2.0f},{"C2",2.0f}};
        } else {
            // 默认: dungeon 变体
            chords = {{"C3",3.0f},{"Db3",3.0f},{"Eb3",3.0f},{"C3",3.0f}};
            melody = {{"C4",0.5f},{0,1.0f},{"Db4",0.4f},{0,0.8f},{"Eb4",0.4f},{0,1.2f},{"C4",0.6f},{0,0.6f}};
            bpm = 75; beat = 60.0f / bpm;
        }
    }

    // 计算总时长 (重复3遍确保~30秒)
    float one_pass = 0;
    for (auto& c : chords) one_pass += c.len * beat * 4;
    total = one_pass * 3;
    n = (int)(SR * total);
    const int NP = 3;

    // 和弦轨 (3遍)
    std::vector<short> chord_track(n, 0);
    for (int pass = 0; pass < NP; pass++) {
        float pos = pass * one_pass;
        for (auto& c : chords) {
            float dur = c.len * beat * 4;
            int root = note_midi(c.note);
            std::vector<int> iv = (name == "dungeon" || name == "boss"
                               || name == "challenge") ?
                std::vector<int>{0,3,7} : std::vector<int>{0,4,7};
            auto ch = render_chord(root, iv, dur, 0.18f);
            for (int i = 0; i < (int)ch.size() && (int)(pos*SR) + i < n; i++)
                chord_track[(int)(pos*SR) + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)chord_track[(int)(pos*SR)+i] + ch[i]));
            pos += dur;
        }
    }

    // 旋律轨 (3遍)
    std::vector<short> melody_track(n, 0);
    for (int pass = 0; pass < NP; pass++) {
        float pos = pass * one_pass;
        for (auto& m : melody) {
            float dur = m.len * beat * 4;
            if (m.note) {
                auto nt = make_note(note_freq(m.note), dur, 0.35f, mw);
                int start = (int)(pos * SR);
                for (int i = 0; i < (int)nt.size() && start + i < n; i++)
                    melody_track[start + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)melody_track[start+i] + nt[i]));
            }
            pos += dur;
        }
    }

    // 低音轨 (3遍)
    std::vector<short> bass_track(n, 0);
    if (!bass.empty()) {
        for (int pass = 0; pass < NP; pass++) {
            float pos = pass * one_pass;
            for (auto& b : bass) {
                float dur = b.len * beat * 4;
                auto nt = make_note(note_freq(b.note), dur, 0.5f, bw);
                int start = (int)(pos * SR);
                for (int i = 0; i < (int)nt.size() && start + i < n; i++)
                    bass_track[start + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)bass_track[start+i] + nt[i]));
                pos += dur;
            }
        }
    } else {
        for (int pass = 0; pass < NP; pass++) {
            float pos = pass * one_pass;
            for (auto& c : chords) {
                float dur = c.len * beat * 4;
                auto nt = make_note(note_freq(c.note) / 2, dur, 0.4f, "triangle");
                int start = (int)(pos * SR);
                for (int i = 0; i < (int)nt.size() && start + i < n; i++)
                    bass_track[start + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)bass_track[start+i] + nt[i]));
                pos += dur;
            }
        }
    }

    // 鼓轨
    std::vector<short> drum_track(n, 0);
    float bar = beat * 4;
    int nbars = (int)(total / bar) + 1;
    auto kick_snd = kick(), snare_snd = snare(), hihat_snd = hihat();
    for (int bar_i = 0; bar_i < nbars; bar_i++) {
        for (int beat_i = 0; beat_i < 4; beat_i++) {
            float t = bar_i * bar + beat_i * beat;
            int ip = (int)(t * SR);
            auto* snd = (beat_i % 2 == 0) ? &kick_snd : &snare_snd;
            for (int i = 0; i < (int)snd->size() && ip + i < n; i++)
                drum_track[ip + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)drum_track[ip+i] + (*snd)[i]));
            int ip2 = (int)((t + beat/2) * SR);
            for (int i = 0; i < (int)hihat_snd.size() && ip2 + i < n; i++)
                drum_track[ip2 + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)drum_track[ip2+i] + hihat_snd[i]));
        }
    }

    auto mx = mix({&chord_track, &melody_track, &bass_track, &drum_track});

    // 转 Sound
    short* buf = (short*)malloc(mx.size() * sizeof(short));
    memcpy(buf, mx.data(), mx.size() * sizeof(short));
    Wave w; w.frameCount = (int)mx.size(); w.sampleRate = SR; w.sampleSize = 16; w.channels = 1; w.data = buf;
    Sound snd = LoadSoundFromWave(w);
    UnloadWave(w);
    return snd;
}

void BGMEngine::init() {
    _cache["title"]   = _compile_bgm("title");
    _cache["select"]  = _compile_bgm("select");
    _cache["dungeon"] = _compile_bgm("dungeon");
    _cache["boss"]    = _compile_bgm("boss");
    _cache["victory"] = _compile_bgm("victory");   // Q3.16: 通关动画专属
    _cache["challenge"] = _compile_bgm("challenge");  // G10.7-B: 竞技场专属
}

void BGMEngine::close() { for (auto& [_, s] : _cache) UnloadSound(s); _cache.clear(); }
void BGMEngine::play(const std::string& name, float vol) {
    stop();
    _current = name;
    _volume = vol;
    // G6.1/G8.1: lazy-compile biome BGM variants on first use
    if (_cache.find(name) == _cache.end()) {
        // Reuse dungeon chords/melody, vary by BPM+waveform+drums
        if (name == "prison" || name == "volcano" || name == "abyss")
            _cache[name] = _compile_bgm(name);
    }
    auto it = _cache.find(name);
    if (it != _cache.end()) { SetSoundVolume(it->second, vol); PlaySound(it->second); _playing = true; }
}
void BGMEngine::stop() {
    if (!_playing) return;
    auto it = _cache.find(_current);
    if (it != _cache.end()) StopSound(it->second);
    _playing = false;
    _current.clear();
}

// Q4.2: BGM 循环 — 曲目播放结束后自动重播 (Raylib Sound 无自带 loop)
void BGMEngine::update(float dt) {
    (void)dt;
    if (!_playing) return;
    auto it = _cache.find(_current);
    if (it == _cache.end()) return;
    if (!IsSoundPlaying(it->second)) {
        SetSoundVolume(it->second, _volume);
        PlaySound(it->second);
    }
}
