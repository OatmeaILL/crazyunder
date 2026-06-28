#pragma once

// ============================================================================
// ExpOrbSystem  经验球系统（Phase 7）
// ----------------------------------------------------------------------------
// 职责：
//   1. Spawn：生成经验球（对象池，预分配 500）
//   2. Update：移动经验球、磁吸效果、拾取检测
//   3. Render：渲染经验球（蓝色小圆形发光）
//
// 磁吸算法：
//   1. 计算经验球到玩家的距离
//   2. 距离 < pickupRange（默认 150px）时触发磁吸
//   3. 磁吸速度 = 基础速度 + (1 - dist/pickupRange) * 加速
//   4. 距离 < 20px 时拾取，调用 UpgradeSystem.AddExp
//
// 生命周期：
//   经验球 10s 后消失（lifetime 衰减）
//
// 性能：
//   - 对象池预分配 500 个，游戏循环内零堆分配
//   - 遍历活跃经验球，O(N)
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include <vector>
#include <cstdint>
#include <functional>
#include "ecs/Entity.h"
#include "utils/ObjectPool.h"

namespace cu {

class Registry;
class Renderer;
class UpgradeSystem;

// ---- 经验球数据 ----
struct ExpOrbData {
    sf::Vector2f position{0.f, 0.f};
    sf::Vector2f velocity{0.f, 0.f};
    int value = 5;            // 经验值
    bool active = false;      // 是否活跃
    float lifetime = 10.f;    // 生命周期（秒）
    float magnetTimer = 0.f;  // 磁吸触发计时
};

// ============================================================================
// ExpOrbSystem  经验球系统
// ============================================================================
class ExpOrbSystem {
public:
    // 预分配经验球池容量
    static constexpr int kPoolCapacity = 500;

    // 磁吸触发距离（像素）
    static constexpr float kMagnetRange = 150.f;

    // 拾取距离（像素）
    static constexpr float kPickupRadius = 30.f;

    ExpOrbSystem();
    ~ExpOrbSystem() = default;

    // 初始化
    void Initialize();

    // 生成经验球
    // pos: 生成位置
    // value: 经验值
    void Spawn(sf::Vector2f pos, int value);

    // 每帧更新
    // registry: ECS 注册表
    // player: 玩家实体 ID
    // upgrade: 升级系统（拾取时调用 AddExp）
    // pickupRange: 磁吸范围（来自 PlayerStats.pickupRange）
    // expMultiplier: 经验获取倍率（来自 PlayerStats.expMultiplier）
    // dt: 固定步长时间（秒）
    void Update(Registry& registry, EntityId player,
                UpgradeSystem& upgrade, float pickupRange,
                float expMultiplier, float dt);

    // 渲染经验球
    void Render(Renderer& renderer);

    // 获取活跃经验球数量
    [[nodiscard]] int GetActiveCount() const noexcept { return static_cast<int>(orbs_.size()); }

    // 获取对象池容量
    [[nodiscard]] int GetPoolCapacity() const noexcept { return kPoolCapacity; }

    // 经验球生成回调（用于触发粒子特效）
    std::function<void(sf::Vector2f pos)> OnOrbSpawned;

private:
    // 经验球存储（使用 vector，active 标志区分活跃/空闲）
    std::vector<ExpOrbData> orbs_;
    // 空闲槽位索引栈
    std::vector<int> freeList_;

    // 随机数辅助
    [[nodiscard]] float randomFloat(float min, float max) const;
};

} // namespace cu