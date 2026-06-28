#pragma once

// ============================================================================
// CoinSystem  金币系统
// ----------------------------------------------------------------------------
// 职责：
//   1. Spawn：生成金币实体（对象池，预分配 300）
//   2. Update：移动金币、磁吸效果、拾取检测
//   3. Render：渲染金币（金色发光圆点）
//
// 磁吸算法：
//   1. 生成后 0.15s 散射阶段
//   2. 随后立即向玩家磁吸，基础速度 250px/s
//   3. 距离 < 30px 时拾取，增加玩家金币
//
// 生命周期：
//   金币 15s 后消失（lifetime 衰减）
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include <vector>
#include <cstdint>
#include <functional>
#include "ecs/Entity.h"

namespace cu {

class Registry;
class Renderer;
struct PlayerComponent;

// ---- 金币数据 ----
struct CoinData {
    sf::Vector2f position{0.f, 0.f};
    sf::Vector2f velocity{0.f, 0.f};
    int value = 1;            // 金币数量
    bool active = false;      // 是否活跃
    float lifetime = 15.f;    // 生命周期（秒）
    float magnetTimer = 0.f;  // 磁吸触发计时
};

// ============================================================================
// CoinSystem  金币系统
// ============================================================================
class CoinSystem {
public:
    // 预分配金币池容量
    static constexpr int kPoolCapacity = 300;

    // 拾取距离（像素）
    static constexpr float kPickupRadius = 30.f;

    CoinSystem();
    ~CoinSystem() = default;

    // 初始化
    void Initialize();

    // 生成金币
    // pos: 生成位置
    // value: 金币数量
    void Spawn(sf::Vector2f pos, int value);

    // 每帧更新
    // registry: ECS 注册表
    // player: 玩家实体 ID
    // playerComp: 玩家组件（用于增加 coins）
    // dt: 固定步长时间（秒）
    void Update(Registry& registry, EntityId player,
                PlayerComponent* playerComp, float dt);

    // 渲染金币
    void Render(Renderer& renderer);

    // 获取活跃金币数量
    [[nodiscard]] int GetActiveCount() const noexcept { return static_cast<int>(coins_.size()); }

    // 获取对象池容量
    [[nodiscard]] int GetPoolCapacity() const noexcept { return kPoolCapacity; }

    // 金币拾取回调（用于任务/成就系统统计累计金币）
    // 参数：本次拾取的金币数量
    std::function<void(int amount)> OnCoinGained;

private:
    // 金币存储（使用 vector，active 标志区分活跃/空闲）
    std::vector<CoinData> coins_;
    // 空闲槽位索引栈
    std::vector<int> freeList_;

    // 随机数辅助
    [[nodiscard]] float randomFloat(float min, float max) const;
};

} // namespace cu
