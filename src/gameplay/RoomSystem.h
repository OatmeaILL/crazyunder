#pragma once

// ============================================================================
// RoomSystem —— 房间逻辑与门连接系统
// ----------------------------------------------------------------------------
// 职责：
//   1. 跟踪玩家所在房间
//   2. 房间清理检测：进入房间后，所有敌人死亡则开门
//   3. 门开关逻辑：清理前关闭（碰撞阻挡），清理后打开
//   4. 房间事件回调：OnRoomEnter、OnRoomClear
//
// 房间清理流程：
//   1. 玩家进入未清理的房间 → 触发 OnRoomEnter
//   2. 关闭所有门（TileType::Door 设为阻挡）
//   3. EnemySpawner 在房间内生成敌人
//   4. 每帧检测：若房间内无活跃敌人 → 触发 OnRoomClear
//   5. 打开所有门（TileType::Door 设为可通行）
//   6. 生成战利品（Phase 7 实现，此处占位）
//   7. 粒子特效（清理完成提示）
//
// 门开关逻辑：
//   - 清理前：门关闭，碰撞阻挡（IsWalkable 返回 false）
//   - 清理后：门打开，可通行（IsWalkable 返回 true）
//   - 实现方式：RoomSystem 维护 doorOpen_ 标志，影响 IsWalkable 判定
// ============================================================================

#include <SFML/Graphics.hpp>
#include <functional>
#include <vector>
#include "gameplay/DungeonGenerator.h"

namespace cu {

class Registry;
class EnemySpawner;
class ParticleSystem;

class RoomSystem {
public:
    RoomSystem();
    ~RoomSystem() = default;

    // 初始化：根据地牢数据设置房间状态
    void Initialize(const Dungeon& dungeon);

    // 设置当前地牢层数（影响普通房怪物数量）
    void SetDungeonLevel(int level) noexcept { dungeonLevel_ = std::max(1, level); }

    // 每帧更新：房间清理检测、门开关逻辑
    // registry: ECS 注册表（用于查询敌人）
    // dungeon: 地牢数据（可修改，用于门开关时修改 Tile 类型）
    // playerPos: 玩家世界坐标
    // spawner: 敌人生成器（用于在房间内生成敌人）
    // particles: 粒子系统（用于清理特效）
    // dt: 固定步长时间（秒）
    void Update(Registry& registry, Dungeon& dungeon,
                sf::Vector2f playerPos, EnemySpawner& spawner,
                ParticleSystem& particles, float dt);

    // 获取当前玩家所在房间（nullptr 若不在任何房间内）
    [[nodiscard]] const Room* GetCurrentRoom() const noexcept;

    // 获取当前房间索引（-1 若不在任何房间内）
    [[nodiscard]] int GetCurrentRoomIndex() const noexcept { return currentRoomIndex_; }

    // 检查房间的门是否已打开
    [[nodiscard]] bool IsDoorOpen(const Room& room, int doorIndex) const noexcept;

    // 检查房间的所有门是否已打开
    [[nodiscard]] bool AreAllDoorsOpen(const Room& room) const noexcept;

    // 检查 tile 是否可通行（考虑门开关状态）
    [[nodiscard]] bool IsTileWalkable(const Dungeon& dungeon, int tileX, int tileY) const noexcept;

    // ---- 事件回调 ----

    // 进入房间回调（参数：房间索引）
    std::function<void(int)> OnRoomEnter;

    // 房间清理完成回调（参数：房间索引）
    std::function<void(int)> OnRoomClear;

    // 事件房进入回调（参数：房间索引、事件类型）
    // Game 层订阅此回调以显示事件交互提示
    std::function<void(int, EventType)> OnEventRoomEnter;

    // 诅咒房进入回调（参数：房间索引）—— Game 层应用诅咒效果
    std::function<void(int)> OnCursedRoomEnter;

    // ---- 调试信息 ----

    // 获取已清理房间数
    [[nodiscard]] int GetClearedRoomCount() const noexcept { return clearedRoomCount_; }

    // 获取总房间数（不含起点）
    [[nodiscard]] int GetTotalRoomCount() const noexcept { return totalRoomCount_; }

private:
    // 检查玩家是否在房间内
    [[nodiscard]] int findRoomContaining(const Dungeon& dungeon, sf::Vector2f playerPos) const noexcept;

    // 检查房间内是否有活跃敌人
    [[nodiscard]] bool hasAliveEnemiesInRoom(Registry& registry, const Dungeon& dungeon,
                                              const Room& room) const;

    // 触发房间进入逻辑（关门、生成敌人）
    void onRoomEntered(Registry& registry, Dungeon& dungeon,
                       EnemySpawner& spawner, int roomIndex);

    // 触发房间清理逻辑（开门、特效）
    void onRoomCleared(Dungeon& dungeon, ParticleSystem& particles, int roomIndex);

    // 房间状态数据
    struct RoomState {
        bool doorsOpen = true;       // 门是否打开
        bool enemiesSpawned = false; // 敌人是否已生成
        bool cleared = false;        // 是否已清理
        bool locked = false;         // 门是否上锁（陷阱房用）
    };

    std::vector<RoomState> roomStates_;
    int currentRoomIndex_ = -1;      // 当前房间索引
    int previousRoomIndex_ = -1;     // 上一帧房间索引
    int clearedRoomCount_ = 0;       // 已清理房间数
    int totalRoomCount_ = 0;         // 总房间数（不含起点）

    // 起点房间索引
    int startRoomIndex_ = -1;

    // 当前地牢层数
    int dungeonLevel_ = 1;
};

} // namespace cu
