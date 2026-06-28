#include "gameplay/Animation.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "utils/Logger.h"

namespace cu {

// ============================================================================
// AnimationSystem::Update
// ----------------------------------------------------------------------------
// 遍历所有拥有 AnimationComponent 的实体，推进动画时间并更新 Sprite。
// 使用 Registry::View 查询同时拥有 AnimationComponent + Sprite 的实体。
// ============================================================================

void AnimationSystem::Update(Registry& registry, float dt) {
    // 查询同时拥有 AnimationComponent 和 Sprite 的实体
    auto entities = registry.View<AnimationComponent, Sprite>();

    for (EntityId id : entities) {
        AnimationComponent* anim = registry.GetComponent<AnimationComponent>(id);
        Sprite* sprite = registry.GetComponent<Sprite>(id);
        if (!anim || !sprite) continue;
        if (!anim->playing || anim->frames.empty()) continue;

        // 累加时间
        anim->accumulator += dt;

        // 累加器超过帧时间时，推进帧
        // 用 while 而非 if：若 dt 很大（如断点恢复后），可能需跳过多帧
        while (anim->accumulator >= anim->frameTime) {
            anim->accumulator -= anim->frameTime;
            ++anim->currentFrame;

            if (anim->currentFrame >= static_cast<int>(anim->frames.size())) {
                if (anim->loop) {
                    anim->currentFrame = 0; // 循环：回到第一帧
                } else {
                    anim->currentFrame = static_cast<int>(anim->frames.size()) - 1; // 不循环：停在最后一帧
                    anim->playing = false; // 播放结束
                }
            }
        }

        // 更新 Sprite 的源矩形为当前帧
        sprite->sourceRect = anim->frames[anim->currentFrame];
    }
}

// ============================================================================
// SetPlayerAnimation
// ----------------------------------------------------------------------------
// 根据动画状态与朝向，计算帧数组并填充到 AnimationComponent。
//
// Sprite Sheet 布局（128x128，每帧 32x32）：
//   行 0 (Down) : [idle] [walk1] [walk2] [walk3]
//   行 1 (Left) : [idle] [walk1] [walk2] [walk3]
//   行 2 (Right): [idle] [walk1] [walk2] [walk3]
//   行 3 (Up)   : [idle] [walk1] [walk2] [walk3]
//
// 帧的纹理坐标计算：
//   frameX = sheetBaseX + col * 32
//   frameY = sheetBaseY + row * 32
//   其中 row = FacingDirection 枚举值，col = 帧序号
// ============================================================================

void SetPlayerAnimation(Registry& registry, EntityId player,
                        PlayerAnimState state, FacingDirection facing,
                        int sheetBaseX, int sheetBaseY) {
    AnimationComponent* anim = registry.GetComponent<AnimationComponent>(player);
    if (!anim) {
        LOG_WARN("SetPlayerAnimation: 实体 %u 无 AnimationComponent", player);
        return;
    }

    // 行号 = 朝向枚举值（Down=0, Left=1, Right=2, Up=3）
    int row = static_cast<int>(facing);

    // 清空旧帧
    anim->frames.clear();
    anim->currentFrame = 0;
    anim->accumulator = 0.f;
    anim->animState = state;
    anim->facing = facing;
    anim->sheetBaseX = sheetBaseX;
    anim->sheetBaseY = sheetBaseY;

    // 辅助：计算 (row, col) 帧的纹理矩形
    auto makeRect = [sheetBaseX, sheetBaseY, row](int col) {
        return sf::IntRect(
            sheetBaseX + col * kPlayerFrameSize,
            sheetBaseY + row * kPlayerFrameSize,
            kPlayerFrameSize,
            kPlayerFrameSize
        );
    };

    switch (state) {
        case PlayerAnimState::Idle:
            // Idle：使用第 0 帧（静止站姿），单帧不循环
            // 也可用第 0 帧与第 2 帧交替做呼吸动画，这里用单帧保持简洁
            anim->frames.push_back(makeRect(0));
            anim->frameTime = kIdleFrameTime;
            anim->loop = true; // 单帧循环无副作用，保持 playing=true
            anim->playing = true;
            break;

        case PlayerAnimState::Walk:
            // Walk：使用全部 4 帧循环播放
            // [idle, walk1, walk2, walk3] 循环，模拟行走步态
            anim->frames.push_back(makeRect(0));
            anim->frames.push_back(makeRect(1));
            anim->frames.push_back(makeRect(2));
            anim->frames.push_back(makeRect(3));
            anim->frameTime = kWalkFrameTime;
            anim->loop = true;
            anim->playing = true;
            break;

        case PlayerAnimState::Attack:
            // Attack：使用前 2 帧快速播放，不循环（播完回到 Idle/Walk）
            anim->frames.push_back(makeRect(0));
            anim->frames.push_back(makeRect(1));
            anim->frameTime = kAttackFrameTime;
            anim->loop = false;
            anim->playing = true;
            break;

        case PlayerAnimState::Hurt:
            // Hurt：使用第 0 帧，配合 Sprite.color 红色闪烁
            anim->frames.push_back(makeRect(0));
            anim->frameTime = kHurtFrameTime;
            anim->loop = false;
            anim->playing = true;
            break;
    }

    // 立即更新 Sprite 的源矩形为第一帧
    Sprite* sprite = registry.GetComponent<Sprite>(player);
    if (sprite && !anim->frames.empty()) {
        sprite->sourceRect = anim->frames[0];
    }
}

} // namespace cu
