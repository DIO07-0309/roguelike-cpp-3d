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

struct BgmTrack {
    struct NoteEntry { const char* note; float len; };
    float bpm = 110;
    float beat = 60.0f / 110.0f;
    std::vector<NoteEntry> chords, melody, bass;
    const char* melody_wave = "square";
    const char* chord_wave = "sine";
    const char* bass_wave = "triangle";
};

static BgmTrack bgm_track_for(const std::string& name) {
    BgmTrack track;
    if (name == "select") {
        track.bpm = 85;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"A3",2.0f},{"F3",2.0f},{"G3",2.0f},{"E3",2.0f},{"A3",2.0f},{"F3",1.5f},{"C4",1.0f},{"G3",1.5f}};
        track.melody = {{0,0.8f},{"A4",0.5f},{0,0.3f},{"C5",0.4f},{0,0.6f},{"E5",0.5f},{0,0.4f},{"D5",0.3f},{0,0.5f},{"C5",0.4f},{"B4",0.3f},{"A4",0.6f}};
        track.melody_wave = "triangle";
    } else if (name == "dungeon") {
        track.bpm = 75;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"C3",3.0f},{"Db3",3.0f},{"Eb3",3.0f},{"C3",3.0f}};
        track.melody = {{"C4",0.5f},{0,1.0f},{"Db4",0.4f},{0,0.8f},{"Eb4",0.4f},{0,1.2f},{"C4",0.6f},{0,0.6f}};
    } else if (name == "boss") {
        track.bpm = 150;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"C3",1.0f},{"Ab2",1.0f},{"Bb2",1.0f},{"G2",1.0f}};
        track.melody = {{"C4",0.12f},{"Eb4",0.12f},{"G4",0.12f},{"C5",0.2f},{"B4",0.12f},{"G4",0.12f},{"F4",0.15f},{"Eb4",0.1f},{0,0.05f}};
        track.bass = {{"C2",0.25f},{"C2",0.25f},{"Ab1",0.25f},{"Ab1",0.25f},{"Bb1",0.25f},{"Bb1",0.25f},{"G1",0.25f},{"G1",0.25f}};
        track.melody_wave = "saw";
        track.bass_wave = "square";
    } else if (name == "challenge") {
        track.bpm = 160;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"C3",0.5f},{"Ab2",0.5f},{"Bb2",0.5f},{"G2",0.5f}};
        track.melody = {{"C4",0.08f},{"Eb4",0.08f},{"G4",0.08f},{"C5",0.16f},
                        {"Bb4",0.08f},{"G4",0.08f},{"Bb4",0.08f},{"C5",0.16f},
                        {"F4",0.08f},{"Ab4",0.08f},{"C5",0.08f},{"Eb5",0.16f},
                        {"D5",0.08f},{"Bb4",0.08f},{"G4",0.08f},{"C5",0.24f}};
        track.bass = {{"C2",0.125f},{"C2",0.125f},{"C2",0.125f},{"C2",0.125f},
                      {"Ab1",0.125f},{"Ab1",0.125f},{"Ab1",0.125f},{"Ab1",0.125f},
                      {"Bb1",0.125f},{"Bb1",0.125f},{"Bb1",0.125f},{"Bb1",0.125f},
                      {"G1",0.125f},{"G1",0.125f},{"G1",0.125f},{"G1",0.125f}};
        track.melody_wave = "saw";
        track.bass_wave = "square";
    } else if (name == "boss_f5") {
        track.bpm = 150;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"C3",1.0f},{"Ab2",1.0f},{"Bb2",1.0f},{"G2",1.0f}};
        track.melody = {{"C4",0.12f},{"Eb4",0.12f},{"G4",0.12f},{"C5",0.2f},{"B4",0.12f},{"G4",0.12f},{"F4",0.15f},{"Eb4",0.1f},{0,0.05f}};
        track.bass = {{"C2",0.25f},{"C2",0.25f},{"Ab1",0.25f},{"Ab1",0.25f},{"Bb1",0.25f},{"Bb1",0.25f},{"G1",0.25f},{"G1",0.25f}};
        track.melody_wave = "saw";
        track.bass_wave = "square";
    } else if (name == "boss_f10") {
        track.bpm = 160;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"G3",0.8f},{"Bb2",0.8f},{"D3",0.8f},{"G3",0.8f}};
        track.melody = {{"G4",0.1f},{"Bb4",0.1f},{"D5",0.1f},{"G5",0.2f},{"F5",0.1f},{"D5",0.1f},{"Bb4",0.15f},{"A4",0.1f},{0,0.05f}};
        track.bass = {{"G2",0.2f},{"G2",0.2f},{"Bb1",0.2f},{"Bb1",0.2f},{"D2",0.2f},{"D2",0.2f},{"G2",0.2f},{"G2",0.2f}};
        track.melody_wave = "saw";
        track.bass_wave = "square";
    } else if (name == "boss_f15") {
        track.bpm = 140;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"C3",1.2f},{"Ab2",1.2f},{"Eb3",1.2f},{"C3",1.2f}};
        track.melody = {{"C5",0.15f},{"Eb5",0.15f},{"Ab5",0.15f},{"C6",0.3f},{"B5",0.15f},{"Ab5",0.15f},{"Eb5",0.2f},{0,0.1f}};
        track.bass = {{"C2",0.3f},{"C2",0.3f},{"Ab1",0.3f},{"Ab1",0.3f},{"Eb2",0.3f},{"Eb2",0.3f},{"C2",0.3f},{"C2",0.3f}};
        track.melody_wave = "triangle";
        track.bass_wave = "sine";
    } else if (name == "victory") {
        track.bpm = 132;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"C4",1.0f},{"G3",1.0f},{"A3",1.0f},{"F3",1.0f},
                        {"C4",1.0f},{"G3",1.0f},{"F3",1.0f},{"C4",1.0f}};
        track.melody = {
            {"C5",0.25f},{"E5",0.25f},{"G5",0.25f},{"C6",0.25f},
            {"B5",0.25f},{"G5",0.25f},{"D5",0.25f},{0,0.25f},
            {"A5",0.25f},{"C6",0.25f},{"B5",0.25f},{"G5",0.25f},
            {"F5",0.25f},{"A5",0.25f},{"G5",0.25f},{"E5",0.25f},
            {"C5",0.25f},{"E5",0.25f},{"G5",0.25f},{"C6",0.25f},
            {"B5",0.25f},{"C6",0.25f},{"D5",0.25f},{"G5",0.25f},
            {"A5",0.25f},{"F5",0.25f},{"G5",0.25f},{"E5",0.25f},
            {"C6",0.5f},{"G5",0.25f},{"E5",0.25f},
        };
        track.bass = {{"C3",0.5f},{"C3",0.5f},{"G2",0.5f},{"G2",0.5f},
                      {"A2",0.5f},{"A2",0.5f},{"F2",0.5f},{"F2",0.5f},
                      {"C3",0.5f},{"C3",0.5f},{"G2",0.5f},{"G2",0.5f},
                      {"F2",0.5f},{"F2",0.5f},{"C3",0.5f},{"G2",0.5f}};
        track.melody_wave = "square";
        track.bass_wave = "triangle";
    } else if (name == "title") {
        track.chords = {{"C4",1.6f},{"G3",1.6f},{"A3",1.6f},{"F3",1.6f},{"C4",1.6f},{"G3",1.6f},{"F3",1.6f},{"C4",2.4f}};
        track.melody = {{"C5",0.3f},{"E5",0.2f},{"G5",0.3f},{"C6",0.4f},{0,0.2f},{"G5",0.2f},{"E5",0.3f},{"C5",0.3f},{"D5",0.2f},{0,0.1f},{"C6",0.3f},{"B5",0.2f},{"A5",0.3f},{"G5",0.2f},{"E5",0.3f},{"F5",0.3f},{"G5",0.3f},{"C6",0.4f},{0,0.3f},{"C6",0.3f},{"B5",0.2f},{"C6",0.3f},{"G5",0.4f},{"E5",0.3f},{"C5",0.3f},{"D5",0.3f},{"C5",0.6f}};
        track.bass = {{"C3",0.8f},{"G2",0.8f},{"A2",0.8f},{"F2",0.8f},{"C3",0.8f},{"G2",0.8f},{"F2",0.8f},{"C3",1.2f}};
    } else if (name == "prison") {
        track.bpm = 72;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"C3",3.0f},{"Db3",3.0f},{"Eb3",3.0f},{"C3",3.0f}};
        track.melody = {{"C4",0.5f},{0,1.0f},{"Db4",0.4f},{0,0.8f},{"Eb4",0.4f},{0,1.2f},{"C4",0.6f},{0,0.6f}};
        track.bass = {{"C2",1.5f},{"Db2",1.5f},{"Eb2",1.5f},{"C2",1.5f}};
        track.melody_wave = "square";
        track.bass_wave = "square";
    } else if (name == "volcano") {
        track.bpm = 90;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"C3",3.0f},{"Db3",3.0f},{"Eb3",3.0f},{"C3",3.0f}};
        track.melody = {{"C4",0.4f},{"Eb4",0.4f},{"G4",0.4f},{"Bb4",0.4f},{"C5",0.4f},{"Bb4",0.4f},{"G4",0.4f},{"Eb4",0.4f}};
        track.bass = {{"C2",0.75f},{"Db2",0.75f},{"Eb2",0.75f},{"C2",0.75f}};
        track.melody_wave = "saw";
        track.bass_wave = "triangle";
    } else if (name == "abyss") {
        track.bpm = 62;
        track.beat = 60.0f / track.bpm;
        track.chords = {{"C3",4.0f},{"Ab2",4.0f},{"Eb3",4.0f},{"C3",4.0f}};
        track.melody = {{"C5",1.0f},{0,0.5f},{"Eb5",0.5f},{0,1.0f},{"Ab5",1.0f},{0,1.5f}};
        track.bass = {{"C2",2.0f},{"Ab1",2.0f},{"Eb2",2.0f},{"C2",2.0f}};
        track.melody_wave = "triangle";
        track.bass_wave = "sine";
    }
    return track;
}

static std::vector<short> render_bgm_chords(const BgmTrack& track, const std::string& name, int n) {
    std::vector<short> out(n, 0);
    float one_pass = 0;
    for (const auto& c : track.chords) one_pass += c.len * track.beat * 4;
    for (int pass = 0; pass < 3; pass++) {
        float pos = pass * one_pass;
        for (const auto& c : track.chords) {
            float dur = c.len * track.beat * 4;
            std::vector<int> iv = (name == "dungeon" || name == "boss" || name == "challenge")
                ? std::vector<int>{0,3,7} : std::vector<int>{0,4,7};
            auto ch = render_chord(note_midi(c.note), iv, dur, 0.18f);
            int start = (int)(pos * SR);
            for (int i = 0; i < (int)ch.size() && start + i < n; i++)
                out[start + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)out[start + i] + ch[i]));
            pos += dur;
        }
    }
    return out;
}

static std::vector<short> render_bgm_melody(const BgmTrack& track, int n) {
    std::vector<short> out(n, 0);
    float one_pass = 0;
    for (const auto& c : track.chords) one_pass += c.len * track.beat * 4;
    for (int pass = 0; pass < 3; pass++) {
        float pos = pass * one_pass;
        for (const auto& m : track.melody) {
            float dur = m.len * track.beat * 4;
            if (m.note) {
                auto nt = make_note(note_freq(m.note), dur, 0.35f, track.melody_wave);
                int start = (int)(pos * SR);
                for (int i = 0; i < (int)nt.size() && start + i < n; i++)
                    out[start + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)out[start + i] + nt[i]));
            }
            pos += dur;
        }
    }
    return out;
}

static std::vector<short> render_bgm_bass(const BgmTrack& track, int n) {
    std::vector<short> out(n, 0);
    float one_pass = 0;
    for (const auto& c : track.chords) one_pass += c.len * track.beat * 4;
    if (!track.bass.empty()) {
        for (int pass = 0; pass < 3; pass++) {
            float pos = pass * one_pass;
            for (const auto& b : track.bass) {
                float dur = b.len * track.beat * 4;
                auto nt = make_note(note_freq(b.note), dur, 0.5f, track.bass_wave);
                int start = (int)(pos * SR);
                for (int i = 0; i < (int)nt.size() && start + i < n; i++)
                    out[start + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)out[start + i] + nt[i]));
                pos += dur;
            }
        }
    } else {
        for (int pass = 0; pass < 3; pass++) {
            float pos = pass * one_pass;
            for (const auto& c : track.chords) {
                float dur = c.len * track.beat * 4;
                auto nt = make_note(note_freq(c.note) / 2, dur, 0.4f, "triangle");
                int start = (int)(pos * SR);
                for (int i = 0; i < (int)nt.size() && start + i < n; i++)
                    out[start + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)out[start + i] + nt[i]));
                pos += dur;
            }
        }
    }
    return out;
}

static std::vector<short> render_bgm_drums(const BgmTrack& track, int n, float total) {
    std::vector<short> out(n, 0);
    float bar = track.beat * 4;
    int nbars = (int)(total / bar) + 1;
    auto kick_snd = kick();
    auto snare_snd = snare();
    auto hihat_snd = hihat();
    for (int bar_i = 0; bar_i < nbars; bar_i++) {
        for (int beat_i = 0; beat_i < 4; beat_i++) {
            float t = bar_i * bar + beat_i * track.beat;
            int ip = (int)(t * SR);
            auto* snd = (beat_i % 2 == 0) ? &kick_snd : &snare_snd;
            for (int i = 0; i < (int)snd->size() && ip + i < n; i++)
                out[ip + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)out[ip + i] + (*snd)[i]));
            int ip2 = (int)((t + track.beat / 2) * SR);
            for (int i = 0; i < (int)hihat_snd.size() && ip2 + i < n; i++)
                out[ip2 + i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, (int)out[ip2 + i] + hihat_snd[i]));
        }
    }
    return out;
}

Sound BGMEngine::_compile_bgm(const std::string& name) {
    BgmTrack track = bgm_track_for(name);
    float one_pass = 0;
    for (const auto& c : track.chords) one_pass += c.len * track.beat * 4;
    int n = (int)(SR * one_pass * 3);

    std::vector<short> chord_track = render_bgm_chords(track, name, n);
    std::vector<short> melody_track = render_bgm_melody(track, n);
    std::vector<short> bass_track = render_bgm_bass(track, n);
    std::vector<short> drum_track = render_bgm_drums(track, n, one_pass * 3);

    auto mx = mix({&chord_track, &melody_track, &bass_track, &drum_track});
    short* buf = (short*)malloc(mx.size() * sizeof(short));
    memcpy(buf, mx.data(), mx.size() * sizeof(short));
    Wave w;
    w.frameCount = (int)mx.size();
    w.sampleRate = SR;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = buf;
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
