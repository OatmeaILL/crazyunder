#include "gameplay/RoomSystem.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "gameplay/EnemySpawner.h"
#include "gameplay/EnemyAI.h"
#include "rendering/ParticleSystem.h"
#include "utils/Logger.h"
#include <cmath>
#include <cstdlib>

namespace cu {

// ============================================================================
// 构造函数
// ============================================================================
RoomSystem::RoomSystem() = default;

// ============================================================================
// Initialize —— 初始化房间状态
// ============================================================================
void RoomSystem::Initialize(const Dungeon& dungeon) {
    roomStates_.clear();
    roomStates_.resize(dungeon.rooms.size());

    // 起点房间默认已清理（无需战斗）
    startRoomIndex_ = dungeon.startRoom;
    if (startRoomIndex_ >= 0 && startRoomIndex_ < static_cast<int>(roomStates_.size())) {
        roomStates_[startRoomIndex_].cleared = true;
        roomStates_[startRoomIndex_].doorsOpen = true;
        roomStates_[startRoomIndex_].enemiesSpawned = true;
    }

    // Boss 房间也标记为已生成（进入时由 Update 触发）
    currentRoomIndex_ = -1;
    previousRoomIndex_ = -1;
    clearedRoomCount_ = 1; // 起点房间已清理
    totalRoomCount_ = static_cast<int>(dungeon.rooms.size()) - 1; // 不含起点

    LOG_INFO("RoomSystem 已初始化: %zu 个房间, 起点房=%d",
             dungeon.rooms.size(), startRoomIndex_);
}

// ============================================================================
// Update —— 每帧更新
// ============================================================================
void RoomSystem::Update(Registry& registry, Dungeon& dungeon,
                        sf::Vector2f playerPos, EnemySpawner& spawner,
                        ParticleSystem& particles, float dt) {
    (void)dt; // 当前不需要时间参数

    // 1. 检测玩家所在房间
    int newRoomIndex = findRoomContaining(dungeon, playerPos);

    // 2. 房间切换检测
    if (newRoomIndex != currentRoomIndex_) {
        previousRoomIndex_ = currentRoomIndex_;
        currentRoomIndex_ = newRoomIndex;

        if (newRoomIndex >= 0) {
            // 进入新房间
            onRoomEntered(registry, dungeon, spawner, newRoomIndex);
        }
    }

    // 3. 房间清理检测：当前房间未清理且已生成敌人，检查是否全部死亡
    if (currentRoomIndex_ >= 0 &&
        currentRoomIndex_ < static_cast<int>(roomStates_.size())) {
        RoomState& state = roomStates_[currentRoomIndex_];
        if (!state.cleared && state.enemiesSpawned) {
            const Room& room = dungeon.rooms[currentRoomIndex_];
            if (!hasAliveEnemiesInRoom(registry, dungeon, room)) {
                // 房间清理完成
                onRoomCleared(dungeon, particles, currentRoomIndex_);
            }
        }
    }

    // 门状态由玩家手动控制（E 键开关），陷阱房门由 RoomSystem 上锁/解锁
}

// ============================================================================
// GetCurrentRoom —— 获取当前房间
// ============================================================================
const Room* RoomSystem::GetCurrentRoom() const noexcept {
    if (currentRoomIndex_ < 0) return nullptr;
    // 注意：这里返回 nullptr，因为 RoomSystem 不持有 Dungeon
    // 调用者应通过 Dungeon::rooms[currentRoomIndex_] 获取
    return nullptr;
}

// ============================================================================
// IsDoorOpen —— 检查房间的门是否已打开
// ============================================================================
bool RoomSystem::IsDoorOpen(const Room& room, int doorIndex) const noexcept {
    if (doorIndex < 0 || doorIndex >= static_cast<int>(room.doors.size())) return true;
    if (room.index < 0 || room.index >= static_cast<int>(roomStates_.size())) return true;
    return roomStates_[room.index].doorsOpen;
}

// ============================================================================
// AreAllDoorsOpen —— 检查房间的所有门是否已打开
// ============================================================================
bool RoomSystem::AreAllDoorsOpen(const Room& room) const noexcept {
    if (room.index < 0 || room.index >= static_cast<int>(roomStates_.size())) return true;
    return roomStates_[room.index].doorsOpen;
}

// ============================================================================
// IsTileWalkable —— 检查 tile 是否可通行（考虑门开关状态）
// ----------------------------------------------------------------------------
// Door tile 的可通行性由 Dungeon::doorStates 决定：
//   - 门打开 → 可通行
//   - 门关闭 → 不可通行
//   - 门上锁 → 不可通行（陷阱房逻辑）
// ============================================================================
bool RoomSystem::IsTileWalkable(const Dungeon& dungeon, int tileX, int tileY) const noexcept {
    TileType tile = dungeon.GetTile(tileX, tileY);

    switch (tile) {
        case TileType::Floor:
        case TileType::Stairs:
            return true;
        case TileType::Door: {
            // 使用 Dungeon 的门状态（由玩家 E 键控制）
            const DoorState* ds = dungeon.GetDoorState(tileX, tileY);
            if (ds && ds->locked) return false; // 上锁门不可通行
            return dungeon.IsDoorOpen(tileX, tileY);
        }
        case TileType::Obstacle:
        case TileType::IndestructibleObstacle:
        case TileType::Chest:
            return false; // 障碍物和宝箱不可通行
        case TileType::Wall:
        case TileType::Empty:
        default:
            return false;
    }
}

// ============================================================================
// findRoomContaining —— 检查玩家所在房间
// ============================================================================
int RoomSystem::findRoomContaining(const Dungeon& dungeon, sf::Vector2f playerPos) const noexcept {
    for (int i = 0; i < static_cast<int>(dungeon.rooms.size()); ++i) {
        const Room& room = dungeon.rooms[i];
        sf::FloatRect worldBounds = room.getWorldBounds(dungeon.worldOffset);
        if (worldBounds.contains(playerPos)) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// hasAliveEnemiesInRoom —— 检查房间内是否有活跃敌人
// ============================================================================
bool RoomSystem::hasAliveEnemiesInRoom(Registry& registry, const Dungeon& dungeon,
                                        const Room& room) const {
    // 遍历所有敌人，检查是否在房间世界边界内
    auto enemies = registry.View<EnemyComponent, Transform>();
    for (EntityId id : enemies) {
        EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(id);
        Transform* t = registry.GetComponent<Transform>(id);
        if (!enemy || !t || !enemy->active) continue;

        // 获取房间的世界边界，扩展 64px（允许敌人在门口附近）
        sf::FloatRect worldBounds = room.getWorldBounds(dungeon.worldOffset);
        worldBounds.left -= 64.f;
        worldBounds.top -= 64.f;
        worldBounds.width += 128.f;
        worldBounds.height += 128.f;

        if (worldBounds.contains(t->position)) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// onRoomEntered —— 触发房间进入逻辑
// ============================================================================
void RoomSystem::onRoomEntered(Registry& registry, Dungeon& dungeon,
                                EnemySpawner& spawner, int roomIndex) {
    if (roomIndex < 0 || roomIndex >= static_cast<int>(roomStates_.size())) return;
    if (roomIndex >= static_cast<int>(dungeon.rooms.size())) return;

    RoomState& state = roomStates_[roomIndex];
    const Room& room = dungeon.rooms[roomIndex];

    LOG_INFO("进入房间 %d (类型=%s, 已清理=%d)", roomIndex,
             RoomTypeName(room.type), state.cleared ? 1 : 0);

    // 触发进入回调
    if (OnRoomEnter) {
        OnRoomEnter(roomIndex);
    }

    // 已清理的房间无需处理
    if (state.cleared) {
        state.doorsOpen = true;
        return;
    }

    // 起点/出生房不生成敌人
    if (roomIndex == startRoomIndex_) {
        state.cleared = true;
        state.doorsOpen = true;
        state.enemiesSpawned = true;
        return;
    }

    // 生成敌人（仅首次进入）
    if (!state.enemiesSpawned) {
        state.enemiesSpawned = true;

        // 根据房间类型生成不同数量/类型的敌人
        sf::Vector2f roomCenter = dungeon.TileCenterToWorld(room.center);
        float roomRadius = std::min(
            room.bounds.width * kTileSize * 0.4f,
            room.bounds.height * kTileSize * 0.4f
        );

        switch (room.type) {
            case RoomType::Boss:
                // Boss 房：生成 1 个 Boss + 少量小怪
                spawner.SpawnEnemyAt(EnemyType::Boss, roomCenter);
                spawner.SpawnEnemiesInArea(EnemyType::Melee, roomCenter, 3, roomRadius);
                LOG_INFO("Boss 房间已生成 Boss + 3 小怪");
                break;
            case RoomType::Elite:
                // 精英房：生成精英敌人 + 小怪 + 1 个带盾怪
                spawner.SpawnEnemiesInArea(EnemyType::Elite, roomCenter, 1, roomRadius * 0.5f);
                spawner.SpawnEnemiesInArea(EnemyType::Melee, roomCenter, 4, roomRadius);
                spawner.SpawnEnemiesInArea(EnemyType::Shielded, roomCenter, 1, roomRadius * 0.6f);
                LOG_INFO("精英房间已生成 1 精英 + 4 小怪 + 1 带盾怪");
                break;
            case RoomType::Treasure:
                // 宝箱房：少量敌人守护（含 1 个隐身怪）
                spawner.SpawnEnemiesInArea(EnemyType::Melee, roomCenter, 3, roomRadius);
                spawner.SpawnEnemiesInArea(EnemyType::StealthMelee, roomCenter, 1, roomRadius * 0.7f);
                LOG_INFO("宝箱房间已生成 3 小怪 + 1 隐身怪");
                break;
            case RoomType::Trap:
                // 陷阱房：进入后锁门，生成大量怪物（35-50）
                state.locked = true;
                for (const auto& doorTile : room.doors) {
                    DoorState* ds = dungeon.GetDoorState(doorTile.x, doorTile.y);
                    if (ds) {
                        ds->locked = true;
                        ds->open = false;
                    }
                }
                LOG_INFO("陷阱房 %d 门已上锁，生成大量怪物", roomIndex);

                {
                    int trapEnemyCount = 35 + (std::rand() % 16); // 35-50
                    // 混合多种敌人，确保挑战性
                    spawner.SpawnEnemiesInArea(EnemyType::Melee, roomCenter, trapEnemyCount / 3, roomRadius);
                    spawner.SpawnEnemiesInArea(EnemyType::Ranged, roomCenter, trapEnemyCount / 5, roomRadius * 0.8f);
                    spawner.SpawnEnemiesInArea(EnemyType::Suicide, roomCenter, trapEnemyCount / 6, roomRadius * 0.7f);
                    spawner.SpawnEnemiesInArea(EnemyType::StealthMelee, roomCenter, trapEnemyCount / 8, roomRadius);
                    spawner.SpawnEnemiesInArea(EnemyType::CountdownSuicide, roomCenter, trapEnemyCount / 10, roomRadius * 0.6f);
                    spawner.SpawnEnemiesInArea(EnemyType::Shielded, roomCenter, trapEnemyCount / 12, roomRadius * 0.6f);
                    spawner.SpawnEnemiesInArea(EnemyType::SniperRanged, roomCenter, trapEnemyCount / 12, roomRadius * 0.9f);
                    LOG_INFO("陷阱房已生成约 %d 个怪物", trapEnemyCount);
                }
                break;

            case RoomType::Event:
                // 事件房：不锁门，不生成敌人，触发事件交互提示
                // 实际事件逻辑由 Game 层在玩家按 E 键时处理
                LOG_INFO("事件房 %d 进入（事件类型=%s）",
                         roomIndex, EventTypeName(room.eventType));
                if (OnEventRoomEnter) {
                    OnEventRoomEnter(roomIndex, room.eventType);
                }
                // 事件房立即标记为已清理（无需战斗）
                state.cleared = true;
                state.doorsOpen = true;
                state.enemiesSpawned = true;
                break;

            case RoomType::Cursed:
                // 诅咒房：进入后锁门 + 施加诅咒 + 生成精英敌人
                state.locked = true;
                for (const auto& doorTile : room.doors) {
                    DoorState* ds = dungeon.GetDoorState(doorTile.x, doorTile.y);
                    if (ds) {
                        ds->locked = true;
                        ds->open = false;
                    }
                }
                LOG_INFO("诅咒房 %d 门已上锁，施加诅咒", roomIndex);

                // 生成 1 精英 + 4 小怪 + 1 带盾怪 + 1 自爆
                spawner.SpawnEnemiesInArea(EnemyType::Elite, roomCenter, 1, roomRadius * 0.5f);
                spawner.SpawnEnemiesInArea(EnemyType::Melee, roomCenter, 4, roomRadius);
                spawner.SpawnEnemiesInArea(EnemyType::Shielded, roomCenter, 1, roomRadius * 0.6f);
                spawner.SpawnEnemiesInArea(EnemyType::Suicide, roomCenter, 1, roomRadius * 0.7f);
                LOG_INFO("诅咒房已生成 1 精英 + 4 小怪 + 1 带盾 + 1 自爆");

                // 触发诅咒回调（Game 层应用诅咒效果到玩家）
                if (OnCursedRoomEnter) {
                    OnCursedRoomEnter(roomIndex);
                }
                break;
            default:
                // 普通房间：数量随层数递增，最多 25 个
                // 普通怪有 8% 概率升级为精英强化版（HP×3 伤害×1.5 速度×1.1 体型×1.5 头上血条）
                {
                    int normalCount = std::min(25, 4 + dungeonLevel_ * 2);
                    constexpr float kChampionChance = 0.08f;
                    spawner.SpawnEnemiesInArea(EnemyType::Melee, roomCenter, normalCount / 2, roomRadius, kChampionChance);
                    spawner.SpawnEnemiesInArea(EnemyType::Ranged, roomCenter, normalCount / 4, roomRadius * 0.8f, kChampionChance);
                    // 剩余位置用 Suicide 和特殊怪填充
                    int remaining = normalCount - normalCount / 2 - normalCount / 4;
                    if (remaining > 0) {
                        int extraType = std::rand() % 5;
                        switch (extraType) {
                            case 0:
                                spawner.SpawnEnemiesInArea(EnemyType::StealthMelee, roomCenter, remaining, roomRadius * 0.7f, kChampionChance);
                                break;
                            case 1:
                                spawner.SpawnEnemiesInArea(EnemyType::CountdownSuicide, roomCenter, remaining, roomRadius * 0.7f, kChampionChance);
                                break;
                            case 2:
                                spawner.SpawnEnemiesInArea(EnemyType::Splitter, roomCenter, remaining, roomRadius * 0.7f, kChampionChance);
                                break;
                            case 3:
                                spawner.SpawnEnemiesInArea(EnemyType::Shielded, roomCenter, remaining, roomRadius * 0.7f, kChampionChance);
                                break;
                            case 4:
                                spawner.SpawnEnemiesInArea(EnemyType::SniperRanged, roomCenter, remaining, roomRadius * 0.7f, kChampionChance);
                                break;
                        }
                    }
                    LOG_INFO("普通房间已生成 %d 个敌人（层数=%d, 精英概率=%.0f%%）", normalCount, dungeonLevel_, kChampionChance * 100.f);
                }
                break;
        }
    }
}

// ============================================================================
// onRoomCleared —— 触发房间清理逻辑
// ============================================================================
void RoomSystem::onRoomCleared(Dungeon& dungeon, ParticleSystem& particles, int roomIndex) {
    if (roomIndex < 0 || roomIndex >= static_cast<int>(roomStates_.size())) return;
    if (roomIndex >= static_cast<int>(dungeon.rooms.size())) return;

    RoomState& state = roomStates_[roomIndex];
    state.cleared = true;
    state.doorsOpen = true;
    state.locked = false;
    ++clearedRoomCount_;

    const Room& room = dungeon.rooms[roomIndex];
    LOG_INFO("房间 %d 已清理 (类型=%s, 已清理=%d/%d)",
             roomIndex, RoomTypeName(room.type), clearedRoomCount_, totalRoomCount_);

    // 陷阱房：解锁所有门
    if (room.type == RoomType::Trap) {
        for (const auto& doorTile : room.doors) {
            DoorState* ds = dungeon.GetDoorState(doorTile.x, doorTile.y);
            if (ds) {
                ds->locked = false;
                ds->open = true;
            }
        }
        LOG_INFO("陷阱房 %d 门已解锁", roomIndex);
    }

    // 诅咒房：解锁所有门，触发诅咒解除回调（由 Game 层解除玩家诅咒）
    if (room.type == RoomType::Cursed) {
        for (const auto& doorTile : room.doors) {
            DoorState* ds = dungeon.GetDoorState(doorTile.x, doorTile.y);
            if (ds) {
                ds->locked = false;
                ds->open = true;
            }
        }
        LOG_INFO("诅咒房 %d 门已解锁，诅咒解除", roomIndex);
        // 复用 OnRoomClear 回调，Game 层检查 room.type==Cursed 时解除诅咒
    }

    // 生成清理完成粒子特效（房间中心）
    sf::Vector2f roomCenter = dungeon.TileCenterToWorld(room.center);
    particles.LevelUpBeam(roomCenter);

    // 触发清理回调
    if (OnRoomClear) {
        OnRoomClear(roomIndex);
    }
}

} // namespace cu
