#pragma once

// ============================================================================
// Animation —— 动画组件与动画系统
// ----------------------------------------------------------------------------
// Sprite Sheet 切帧原理：
//   Sprite Sheet 是将角色所有动画帧打包到一张大图的技术。
//   例如 128x128 的图，每帧 32x32，排列为 4 行 × 4 列：
//     行 0：Down  方向（idle, walk1, walk2, walk3）
//     行 1：Left  方向
//     行 2：Right 方向
//     行 3：Up    方向
//
//   动画播放 = 按时间推进，在帧数组中循环切换。每帧的 sf::IntRect 指定
//   在纹理中的像素位置（left, top, width, height）。更新 Sprite 组件的
//   sourceRect 即可显示不同帧。
//
// 动画状态机：
//   玩家有 4 种动画状态：Idle / Walk / Attack / Hurt。
//   状态切换由玩家行为驱动（移动→Walk，停止→Idle，攻击→Attack，受击→Hurt）。
//   每个状态对应不同的帧序列与播放参数（帧率、是否循环）。
//
// 与 ECS 的集成：
//   AnimationComponent 是普通 ECS 组件，AnimationSystem 遍历所有拥有
//   AnimationComponent + Sprite 组件的实体，推进动画时间并更新 Sprite.sourceRect。
// ============================================================================

#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdint>
#include "ecs/Entity.h"

namespace cu {

class Registry;

// ---- 朝向枚举 ----
enum class FacingDirection : uint8_t {
    Down  = 0, // 朝下（面向屏幕）
    Left  = 1, // 朝左
    Right = 2, // 朝右
    Up    = 3  // 朝上（背对屏幕）
};

// ---- 玩家动画状态 ----
enum class PlayerAnimState : uint8_t {
    Idle,   // 静止
    Walk,   // 行走
    Attack, // 攻击
    Hurt    // 受击
};

// ============================================================================
// AnimationComponent —— 动画组件（通用，不限于玩家）
// ----------------------------------------------------------------------------
// frames: 帧矩形数组，每项是纹理中的像素区域 (left, top, w, h)
// currentFrame: 当前播放到的帧索引
// frameTime: 每帧持续时间（秒），越小切换越快
// accumulator: 时间累加器，达到 frameTime 时切换下一帧
// loop: 是否循环播放（false = 播放到最后一帧停止）
// playing: 是否正在播放（false = 暂停，不推进时间）
// ============================================================================

struct AnimationComponent {
    std::vector<sf::IntRect> frames; // 帧矩形数组（纹理像素坐标）
    int currentFrame = 0;            // 当前帧索引
    float frameTime = 0.15f;         // 每帧持续时间（秒）
    float accumulator = 0.f;         // 时间累加器
    bool loop = true;                // 是否循环
    bool playing = true;             // 是否播放中

    // 玩家专用字段（非玩家实体可忽略）
    PlayerAnimState animState = PlayerAnimState::Idle;
    FacingDirection facing = FacingDirection::Down;
    // Sprite Sheet 在图集中的基点（左上角像素坐标）
    // 帧 (row, col) 的纹理位置 = (sheetBaseX + col*frameW, sheetBaseY + row*frameH)
    int sheetBaseX = 0;
    int sheetBaseY = 0;
};

// ============================================================================
// AnimationSystem —— 动画系统
// ----------------------------------------------------------------------------
// 遍历所有拥有 AnimationComponent + Sprite 的实体：
//   1. 累加 dt 到 accumulator
//   2. 当 accumulator >= frameTime 时，切换到下一帧（循环或停止）
//   3. 更新 Sprite.sourceRect 为当前帧的矩形
// ============================================================================

class AnimationSystem {
public:
    // 每帧更新所有动画（dt = 固定步长）
    void Update(Registry& registry, float dt);
};

// ============================================================================
// 玩家动画辅助函数
// ============================================================================

// Sprite Sheet 布局常量
inline constexpr int kPlayerFrameSize = 32;   // 每帧 32x32 像素
inline constexpr int kPlayerFramesPerRow = 4; // 每行 4 帧
inline constexpr int kPlayerSheetRows = 4;    // 4 行（4 个方向）

// 帧时间常量（秒）
inline constexpr float kIdleFrameTime = 0.4f;  // Idle 呼吸动画较慢
inline constexpr float kWalkFrameTime = 0.12f; // 行走动画较快
inline constexpr float kAttackFrameTime = 0.08f; // 攻击动画快速
inline constexpr float kHurtFrameTime = 0.2f;  // 受击动画

// 设置玩家动画：根据状态与朝向填充帧数组
// registry: ECS 注册表
// player: 玩家实体 ID
// state: 目标动画状态
// facing: 朝向（决定使用哪一行帧）
// sheetBaseX, sheetBaseY: Sprite Sheet 在图集中的左上角像素坐标
void SetPlayerAnimation(Registry& registry, EntityId player,
                        PlayerAnimState state, FacingDirection facing,
                        int sheetBaseX, int sheetBaseY);

// 获取朝向名称（调试用）
[[nodiscard]] inline const char* FacingDirectionName(FacingDirection d) noexcept {
    switch (d) {
        case FacingDirection::Down:  return "Down";
        case FacingDirection::Left:  return "Left";
        case FacingDirection::Right: return "Right";
        case FacingDirection::Up:    return "Up";
    }
    return "?";
}

// 获取动画状态名称（调试用）
[[nodiscard]] inline const char* PlayerAnimStateName(PlayerAnimState s) noexcept {
    switch (s) {
        case PlayerAnimState::Idle:   return "Idle";
        case PlayerAnimState::Walk:   return "Walk";
        case PlayerAnimState::Attack: return "Attack";
        case PlayerAnimState::Hurt:   return "Hurt";
    }
    return "?";
}

} // namespace cu
