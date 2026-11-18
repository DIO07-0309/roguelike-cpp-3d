#pragma once
// P5: 特殊房 9 色基色 —— 2D (game_map) 与 3D (scene_builder) 共用的单一真相。
// 之前两处各自 switch 重复, 颜色会漂移; 这里统一定义。
#include "raylib.h"
#include "special_room.h"

inline Color special_room_base_color(SpecialRoomType type) {
    switch (type) {
        case SpecialRoomType::ALTAR:      return {60, 44, 22, 255};
        case SpecialRoomType::TREASURE:   return {34, 46, 76, 255};
        case SpecialRoomType::FOUNTAIN:   return {28, 56, 34, 255};
        case SpecialRoomType::SHOP:       return {62, 56, 24, 255};
        case SpecialRoomType::BLACKSMITH: return {68, 40, 28, 255};
        case SpecialRoomType::LIBRARY:    return {22, 38, 68, 255};
        case SpecialRoomType::GAMBLER:    return {56, 22, 60, 255};
        case SpecialRoomType::SHRINE:     return {52, 52, 16, 255};
        case SpecialRoomType::SECRET:     return {62, 16, 16, 255};
        default:                          return {25, 25, 35, 255};
    }
}
