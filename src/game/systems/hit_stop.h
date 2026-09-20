#pragma once
// A6-T2: HitStop 计时器 — 击杀/重击时短暂暂停游戏逻辑 (wall clock)
// 红线: 无头 sim 不实例化; 期间渲染继续但 update 跳过

class HitStop {
public:
    void trigger(float duration);
    void update(float dt);          // wall clock, 非 game time
    bool active() const;
    float remaining() const;
    bool is_stunned() const;        // 游戏暂停信号

private:
    float _remaining = 0.0f;
    bool _active = false;
};
