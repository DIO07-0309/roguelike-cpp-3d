#pragma once

// NPC 快照 (A6-S2 批次6): 坐标 tile + 是否完成对话 + id
// G12-4: 从 GameScene 嵌套类型抽出成独立头文件 —— 3D 表现层要持有它,
// 不能再为了一个 4 字段结构去 include game_scene.h
struct NpcView {
    int  tile_x = 0, tile_y = 0;
    bool finished = false;
    int  npc_id = 0;                    // floor*10+slot
};
