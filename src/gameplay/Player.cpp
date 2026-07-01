#include "gameplay/Player.h"
#include "core/Input.h"
#include "core/AudioManager.h"
#include "rendering/Camera.h"
#include "ecs/Component.h"
#include "gameplay/DungeonGenerator.h"
#include "gameplay/ClassSystem.h"
#include "utils/Logger.h"
#include <cmath>

namespace cu {

// ============================================================================
// CreatePlayer —— 创建玩家实体
// ============================================================================
EntityId CreatePlayer(Registry& registry, sf::Vector2f position,
                      const PlayerSheetInfo& sheetInfo,
                      PlayerClass playerClass) {
    EntityId id = registry.CreateEntity();

    // Transform：位置与缩放
    auto& transform = registry.AddComponent<Transform>(id);
    transform.position = position;
    transform.scale = sf::Vector2f(1.5f, 1.5f); // 放大 1.5 倍，更清晰可见
    transform.rotation = 0.f;

    // Sprite：初始显示 Down 方向 idle 帧
    auto& sprite = registry.AddComponent<Sprite>(id);
    // 第一帧位置 = (sheetBaseX + 0*32, sheetBaseY + 0*32)
    sprite.sourceRect = sf::IntRect(
        sheetInfo.atlasX, sheetInfo.atlasY,
        sheetInfo.frameSize, sheetInfo.frameSize
    );
    sprite.color = sf::Color::White;
    sprite.origin = sf::Vector2f(
        sheetInfo.frameSize * 0.5f,
        sheetInfo.frameSize * 0.5f
    );

    // Velocity：初始静止
    registry.AddComponent<Velocity>(id);

    // Collider：圆形碰撞，半径 16
    auto& collider = registry.AddComponent<Collider>(id);
    collider.isCircle = true;
    collider.radius = 16.f;

    // Health：根据职业设置初始 HP
    auto& health = registry.AddComponent<Health>(id);
    const ClassData& cd = GetClassData(playerClass);
    health.current = cd.maxHp;
    health.max = cd.maxHp;
    health.invincibleTimer = 0.f;

    // Tag：Player
    registry.AddComponent<Tag>(id).flags = TagFlag::Player;

    // AnimationComponent：初始 Idle + Down
    auto& anim = registry.AddComponent<AnimationComponent>(id);
    anim.animState = PlayerAnimState::Idle;
    anim.facing = FacingDirection::Down;
    anim.sheetBaseX = sheetInfo.atlasX;
    anim.sheetBaseY = sheetInfo.atlasY;
    // 初始填充 Idle 帧序列
    anim.frames.push_back(sf::IntRect(
        sheetInfo.atlasX, sheetInfo.atlasY,
        sheetInfo.frameSize, sheetInfo.frameSize
    ));
    anim.currentFrame = 0;
    anim.frameTime = kIdleFrameTime;
    anim.loop = true;
    anim.playing = true;

    // PlayerComponent：玩家属性
    auto& player = registry.AddComponent<PlayerComponent>(id);
    player.playerClass = playerClass;
    player.facing = FacingDirection::Down;
    player.animState = PlayerAnimState::Idle;
    player.attackTimer = 0.f;
    player.hurtTimer = 0.f;
    player.wasMoving = false;
    // 设置职业基础法力值
    player.stats.maxMp = cd.maxMp;
    player.stats.currentMp = cd.maxMp;

    // 显式初始化技能背包为空（SkillType::Count）
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        player.skillBackpack[i] = SkillType::Count;
    }
    // 技能槽也显式初始化为空
    for (int i = 0; i < kSkillSlotCount; ++i) {
        player.skillSlots[i].type = SkillType::Count;
        player.skillSlots[i].cooldownRemain = 0.f;
    }

    LOG_INFO("玩家实体已创建: id=%u, pos=(%.1f, %.1f), class=%s",
             id, position.x, position.y, GetClassName(playerClass));
    return id;
}

// ============================================================================
// ComputeFacing —— 根据鼠标位置计算 4 方向朝向
// ----------------------------------------------------------------------------
// 角度划分（标准数学角度，0=右，逆时针为正，但屏幕坐标 Y 向下，
// 所以 atan2(dy, dx) 中 dy>0 表示鼠标在玩家下方）：
//   atan2(dy, dx) ∈ [-π, π]
//
// 屏幕坐标系下的 4 方向划分：
//   鼠标在右方 (|angle| < 45°)      → Right
//   鼠标在下方 (angle ∈ [45°, 135°)) → Down
//   鼠标在左方 (|angle| > 135°)      → Left
//   鼠标在上方 (angle < -45°)        → Up
// ============================================================================

FacingDirection ComputeFacing(sf::Vector2f playerPos,
                              sf::Vector2f mouseWorldPos) noexcept {
    float dx = mouseWorldPos.x - playerPos.x;
    float dy = mouseWorldPos.y - playerPos.y;

    // 距离过近时不改变朝向（避免抖动）
    float distSq = dx * dx + dy * dy;
    if (distSq < 4.f) {
        // 默认朝下
        return FacingDirection::Down;
    }

    // 计算角度（弧度），atan2 返回 [-π, π]
    float angle = std::atan2(dy, dx);
    // 转为度数 [0, 360)
    float deg = angle * 180.f / 3.14159265358979f;
    if (deg < 0.f) deg += 360.f;

    // 4 方向划分（每方向 90°）：
    //   Right: [315°, 360°) ∪ [0°, 45°)
    //   Down:  [45°, 135°)
    //   Left:  [135°, 225°)
    //   Up:    [225°, 315°)
    if (deg < 45.f || deg >= 315.f) {
        return FacingDirection::Right;
    } else if (deg < 135.f) {
        return FacingDirection::Down;
    } else if (deg < 225.f) {
        return FacingDirection::Left;
    } else {
        return FacingDirection::Up;
    }
}

float GetPlayerFacingAngle(sf::Vector2f playerPos,
                           sf::Vector2f mouseWorldPos) noexcept {
    float dx = mouseWorldPos.x - playerPos.x;
    float dy = mouseWorldPos.y - playerPos.y;
    return std::atan2(dy, dx) * 180.f / 3.14159265358979f;
}

// ============================================================================
// UpdatePlayer —— 更新玩家逻辑
// ----------------------------------------------------------------------------
// 流程：
//   1. 读取移动输入（WASD/方向键），构建 8 方向移动向量
//   2. 对角线归一化
//   3. 更新 Transform.position 与 Velocity
//   4. 计算鼠标朝向，更新 PlayerComponent.facing
//   5. 根据移动状态切换动画（Idle ↔ Walk）
//   6. 处理攻击/受击动画计时器
// ============================================================================

void UpdatePlayer(Registry& registry, EntityId playerId,
                  const Input& input, const Camera& camera, float dt,
                  const PlayerSheetInfo& sheetInfo,
                  const Dungeon* dungeon) {
    Transform* transform = registry.GetComponent<Transform>(playerId);
    Velocity* velocity = registry.GetComponent<Velocity>(playerId);
    PlayerComponent* player = registry.GetComponent<PlayerComponent>(playerId);
    Sprite* sprite = registry.GetComponent<Sprite>(playerId);

    if (!transform || !player || !velocity) return;

    // ---- 1. 读取移动输入 ----
    sf::Vector2f move(0.f, 0.f);
    // 使用动作映射查询（支持 WASD + 方向键）
    if (input.IsActionDown("MoveUp"))    move.y -= 1.f;
    if (input.IsActionDown("MoveDown"))  move.y += 1.f;
    if (input.IsActionDown("MoveLeft"))  move.x -= 1.f;
    if (input.IsActionDown("MoveRight")) move.x += 1.f;

    bool isMoving = (move.x != 0.f || move.y != 0.f);

    // ---- 2. 对角线归一化 ----
    // 对角线移动时 |move| = √2 ≈ 1.414，比直线快 41%
    // 归一化后 |move| = 1，各方向速度一致
    if (isMoving) {
        float len = std::sqrt(move.x * move.x + move.y * move.y);
        if (len > 0.f) {
            move /= len;
        }
    }

    // ---- 3. 更新位置与速度（Phase 6: 加入墙壁碰撞，使用轴分离碰撞）----
    // 轴分离碰撞（Axis-Separated Collision）原理：
    //   将移动分解为 X 轴与 Y 轴两次独立位移，分别检测目标位置是否阻挡：
    //   - X 轴位移后检测墙壁：若阻挡，撤销 X 轴位移（沿 Y 轴可继续移动）
    //   - Y 轴位移后检测墙壁：若阻挡，撤销 Y 轴位移（沿 X 轴可继续移动）
    //   优势：玩家沿墙壁滑动而非完全卡住，符合 2D 游戏直觉。
    float speed = player->stats.moveSpeed;
    // 狂暴加成：+30%移速
    if (player->berserkTimer > 0.f) {
        speed *= 1.3f;
    }
    velocity->linear = move * speed;
    sf::Vector2f delta = velocity->linear * dt;

    if (dungeon && !dungeon->tiles.empty()) {
        // 玩家碰撞半径（用于检查身体而非中心点）
        Collider* col = registry.GetComponent<Collider>(playerId);
        float radius = col ? col->radius * 0.6f : 10.f; // 碰撞半径略小以便穿过门
        sf::Vector2f curPos = transform->position;

        // 辅助：检查某点周围 4 个采样点是否阻挡（圆形碰撞简化）
        auto isBlockedAt = [dungeon, radius](sf::Vector2f p) -> bool {
            // 检查 4 个边界采样点（左右上下）
            sf::Vector2f samples[4] = {
                {p.x - radius, p.y},
                {p.x + radius, p.y},
                {p.x, p.y - radius},
                {p.x, p.y + radius}
            };
            for (const auto& s : samples) {
                sf::Vector2i tile = dungeon->WorldToTile(s);
                if (dungeon->IsBlocked(tile.x, tile.y)) {
                    return true;
                }
            }
            return false;
        };

        // X 轴单独移动
        sf::Vector2f tryX(curPos.x + delta.x, curPos.y);
        if (!isBlockedAt(tryX)) {
            curPos.x = tryX.x;
        } else {
            velocity->linear.x = 0.f; // 取消 X 轴速度
        }
        // Y 轴单独移动
        sf::Vector2f tryY(curPos.x, curPos.y + delta.y);
        if (!isBlockedAt(tryY)) {
            curPos.y = tryY.y;
        } else {
            velocity->linear.y = 0.f; // 取消 Y 轴速度
        }
        transform->position = curPos;
    } else {
        // 无地牢数据：保持原行为
        transform->position += delta;
    }

    // ---- 4. 计算鼠标朝向 ----
    sf::Vector2f mouseWorld = input.GetMouseWorldPosition(camera);
    FacingDirection newFacing = ComputeFacing(transform->position, mouseWorld);
    bool facingChanged = (newFacing != player->facing);
    player->facing = newFacing;

    // ---- 脚步声：移动时定时播放 ----
    if (isMoving && player->dodgeDashTimer <= 0.f) {
        player->footstepTimer -= dt;
        if (player->footstepTimer <= 0.f) {
            AudioManager::Instance().PlaySFX(AudioManager::kSFXFootstep);
            // 脚步声间隔随移速变化，越快越密
            float interval = 0.35f / std::max(0.5f, speed / 150.f);
            player->footstepTimer = interval;
        }
    } else {
        player->footstepTimer = 0.f; // 停止移动时重置
    }

    // ---- 5. 动画状态切换 ----
    // 优先级：Hurt > Attack > Walk/Idle
    PlayerAnimState desiredState = player->animState;

    // 受击动画计时
    if (player->hurtTimer > 0.f) {
        player->hurtTimer -= dt;
        desiredState = PlayerAnimState::Hurt;
        if (player->hurtTimer <= 0.f) {
            // 受击结束，回到 Idle/Walk
            desiredState = isMoving ? PlayerAnimState::Walk : PlayerAnimState::Idle;
        }
    } else if (player->attackTimer > 0.f) {
        // 攻击动画计时
        player->attackTimer -= dt;
        desiredState = PlayerAnimState::Attack;
        if (player->attackTimer <= 0.f) {
            desiredState = isMoving ? PlayerAnimState::Walk : PlayerAnimState::Idle;
        }
    } else {
        // 正常状态：根据移动切换 Idle/Walk
        desiredState = isMoving ? PlayerAnimState::Walk : PlayerAnimState::Idle;
    }

    // 状态或朝向变化时重新设置动画帧
    if (desiredState != player->animState || facingChanged) {
        player->animState = desiredState;
        SetPlayerAnimation(registry, playerId, desiredState, player->facing,
                           sheetInfo.atlasX, sheetInfo.atlasY);
    }

    // ---- 6. 受击时的颜色闪烁 ----
    if (sprite) {
        if (player->hurtTimer > 0.f) {
            // 受击时红色着色
            sprite->color = sf::Color(255, 100, 100, 255);
        } else {
            sprite->color = sf::Color::White;
        }
    }

    player->wasMoving = isMoving;
}

} // namespace cu
