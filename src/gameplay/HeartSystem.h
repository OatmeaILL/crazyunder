#pragma once

// ============================================================================
// HeartSystem  爱心掉落系统
// ----------------------------------------------------------------------------
// 职责：
//   1. Spawn：生成爱心（Boss 召唤物死亡时概率掉落）
//   2. Update：磁吸效果、拾取检测（回复玩家 2% 最大生命）
//   3. Render：渲染爱心（红色发光心形）
//
// 磁吸算法与 CoinSystem 一致：
//   1. 生成后 0.15s 散射阶段
//   2. 随后向玩家磁吸，基础速度 250px/s
//   3. 距离 < 30px 时拾取，回复 2% 最大生命
//
// 生命周期：15s 后消失
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

// ---- 爱心数据 ----
struct HeartData {
    sf::Vector2f position{0.f, 0.f};
    sf::Vector2f velocity{0.f, 0.f};
    bool active = false;       // 是否活跃
    float lifetime = 15.f;    // 生命周期（秒）
    float magnetTimer = 0.f;  // 磁吸触发计时
};

// ============================================================================
// HeartSystem  爱心掉落系统
// ============================================================================
class HeartSystem {
public:
    // 预分配爱心池容量
    static constexpr int kPoolCapacity = 100;

    // 拾取距离（像素）
    static constexpr float kPickupRadius = 30.f;

    HeartSystem();
    ~HeartSystem() = default;

    // 初始化
    void Initialize();

    // 生成爱心
    // pos: 生成位置
    void Spawn(sf::Vector2f pos);

    // 每帧更新
    // registry: ECS 注册表
    // player: 玩家实体 ID
    // playerComp: 玩家组件（用于回血）
    // dt: 固定步长时间（秒）
    void Update(Registry& registry, EntityId player,
                PlayerComponent* playerComp, float dt);

    // 渲染爱心
    void Render(Renderer& renderer);

    // 获取活跃爱心数量
    [[nodiscard]] int GetActiveCount() const noexcept { return static_cast<int>(hearts_.size()); }

private:
    // 爱心存储（使用 vector，active 标志区分活跃/空闲）
    std::vector<HeartData> hearts_;
    // 空闲槽位索引栈
    std::vector<int> freeList_;

    // 随机数辅助
    [[nodiscard]] float randomFloat(float min, float max) const;
};

} // namespace cu
