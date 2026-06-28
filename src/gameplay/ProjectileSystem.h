#pragma once

// ============================================================================
// ProjectileSystem —— 高性能弹幕系统（对象池 + 空间网格碰撞）
// ----------------------------------------------------------------------------
// 设计目标：支持 1000+ 弹幕同屏，1000 子弹更新 < 2ms。
//
// 核心优化策略：
//   1. 对象池管理子弹实体：
//      - Initialize 时预创建 1200 个子弹实体，挂载 Projectile + Transform + Sprite + Tag
//      - freeList_ 栈存储可用 EntityId，Spawn 时 O(1) 弹出，销毁时 O(1) 压入
//      - 整个游戏循环内零实体创建/销毁，无堆分配
//
//   2. 空间网格碰撞检测（UniformGrid）：
//      - 玩家子弹 → 查询网格中的敌人（O(1) 查询附近单元格）
//      - 敌人子弹 → 直接检查玩家位置（仅 1 个玩家，无需网格）
//      - 避免暴力 O(N×M) 两两检测
//
//   3. 紧凑数组遍历：
//      - 遍历对象池中所有子弹，检查 active 标志
//      - 虽然非活跃子弹也在遍历中，但 active 检查是 O(1) 分支预测友好
//      - 替代方案是维护活跃列表，但 swap-remove 会破坏顺序，影响缓存
//
//   4. 子弹贴图：
//      - 过程化生成 8x8 小圆形贴图，添加到图集
//      - 不同元素通过 Sprite.color 着色区分（白=物理，红=火，蓝=冰，黄=闪电，绿=毒）
//      - 所有子弹共用同一贴图，Renderer 可合并为 1 次 Draw Call
//
// 子弹行为：
//   - 移动：position += direction × speed × dt
//   - 生命周期：lifetime -= dt，到期销毁
//   - 碰撞：圆形距离检测（bullet.radius + target.collider.radius）
//   - 穿透：pierce > 0 时命中后不销毁，pierce 次数减 1
//   - 分裂：命中时分裂出 splitCount 个新子弹（扇形扩散）
//   - 连锁：命中后跳到附近 chainCount 个敌人（闪电链）
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdint>
#include <functional>
#include "ecs/Entity.h"
#include "ecs/Component.h"

namespace cu {

class Registry;
class TextureAtlas;
class UniformGrid;
class ParticleSystem;
class CombatSystem;
struct Dungeon;

// ---- 子弹配置（数据驱动）----
// Spawn 时传入，定义子弹的所有属性。
struct ProjectileConfig {
    float speed = 400.f;               // 飞行速度（像素/秒）
    float damage = 10.f;               // 伤害值
    int pierce = 0;                    // 穿透次数（0=命中即销毁）
    float lifetime = 3.f;              // 生命周期（秒）
    float radius = 6.f;                // 碰撞半径
    sf::Color color = sf::Color::White; // 着色（元素标识）
    int splitCount = 0;                // 命中时分裂子弹数
    int chainCount = 0;                // 连锁闪电次数
    ElementType element = ElementType::Physical; // 元素类型
    float knockback = 100.f;           // 击退力度
    float lifesteal = 0.f;             // 吸血比例
};

class ProjectileSystem {
public:
    // 预分配子弹池容量
    static constexpr int kPoolCapacity = 1200;

    ProjectileSystem();
    ~ProjectileSystem() = default;

    // 初始化：预创建子弹实体池，生成子弹贴图并添加到图集
    // 必须在图集 Build 之前调用（因为要添加贴图）
    void Initialize(Registry& registry, TextureAtlas& atlas);

    // 图集构建后调用：从图集获取子弹贴图的像素矩形
    // 必须在 atlas.Build() 之后、Spawn 之前调用
    void PostBuildInit(const TextureAtlas& atlas);

    // 发射子弹
    // pos: 发射位置（世界坐标）
    // dir: 飞行方向（归一化向量）
    // config: 子弹配置
    // owner: 所有者实体（玩家或敌人，用于区分敌我）
    void Spawn(sf::Vector2f pos, sf::Vector2f dir,
               const ProjectileConfig& config, EntityId owner);

    // 每帧更新：移动子弹、碰撞检测、穿透/分裂/连锁处理
    // registry: ECS 注册表
    // grid: 已插入敌人的空间网格（用于玩家子弹碰撞查询）
    // playerEntity: 玩家实体（用于敌人子弹碰撞检测）
    // dungeon: 地牢数据（非空时检查 tile 碰撞：Obstacle 可破坏，Wall 挡子弹）
    // particles: 粒子系统（用于命中/碰撞特效）
    // dt: 固定步长时间（秒）
    void Update(Registry& registry, UniformGrid& grid,
                EntityId playerEntity, Dungeon* dungeon,
                ParticleSystem& particles, float dt);

    // 获取当前活跃子弹数
    [[nodiscard]] int GetActiveCount() const noexcept { return activeCount_; }

    // 获取对象池总容量
    [[nodiscard]] int GetPoolCapacity() const noexcept { return kPoolCapacity; }

    // 获取可用子弹数
    [[nodiscard]] int GetFreeCount() const noexcept { return static_cast<int>(freeList_.size()); }

    // 清除所有活跃子弹（调试用）
    void ClearAll();

    // 罐子破坏回调（参数：罐子位置）
    // Game 层设置此回调以触发 LootSystem 掉落和 ExpOrbSystem 经验球
    std::function<void(sf::Vector2f)> onPotBroken;

    // 门破坏回调（参数：门位置）
    // Game 层设置此回调以标记 TileMap 顶点缓存为脏
    std::function<void(sf::Vector2f)> onDoorBroken;

    // ---- CombatSystem 指针（第十六轮新增）----
    // 用于 handleHit 中根据 proj.element 触发元素状态效果（Fire/Ice/Poison）。
    // Game 层在系统初始化后调用 SetCombatSystem 注入；若为 nullptr（未注入），
    // 则子弹命中时不施加状态效果，保持向后兼容。
    void SetCombatSystem(CombatSystem* combat) noexcept { combatSystem_ = combat; }

private:
    // 从对象池获取一个子弹实体（返回 kInvalidEntity 若池空）
    [[nodiscard]] EntityId acquireFromPool();

    // 将子弹实体回收到对象池
    void releaseToPool(EntityId id);

    // 处理子弹命中目标
    // 返回 true 表示子弹应销毁（pierce 耗尽），false 表示继续飞行
    bool handleHit(Registry& registry, EntityId bulletId, EntityId targetId,
                   Projectile& proj, Transform& bulletTransform,
                   UniformGrid& grid, std::vector<EntityId>& neighbors,
                   ParticleSystem& particles);

    // 处理分裂：命中时分裂出 splitCount 个新子弹
    void handleSplit(Registry& registry, EntityId bulletId,
                     Projectile& proj, const Transform& bulletTransform);

    // 处理连锁闪电：命中后跳到附近敌人
    // chainCount: 剩余连锁次数
    // lastTarget: 上一个命中的目标（避免连锁回同一目标）
    // particles: 粒子系统（第十九轮新增，用于连锁命中视觉特效）
    void handleChain(Registry& registry, UniformGrid& grid,
                     sf::Vector2f fromPos, EntityId lastTarget,
                     float damage, int chainCount,
                     EntityId owner, std::vector<EntityId>& neighbors,
                     ParticleSystem& particles);

    // 子弹贴图在图集中的像素矩形
    sf::IntRect bulletRect_;

    // 对象池：预创建的子弹 EntityId 列表
    std::vector<EntityId> projectilePool_;
    // 空闲 EntityId 栈
    std::vector<EntityId> freeList_;

    int activeCount_ = 0;
    Registry* registry_ = nullptr;
    CombatSystem* combatSystem_ = nullptr; // 第十六轮新增：用于元素状态效果触发
};

} // namespace cu
