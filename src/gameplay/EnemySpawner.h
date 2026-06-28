#pragma once

// ============================================================================
// EnemySpawner —— 波次敌人生成器
// ----------------------------------------------------------------------------
// 职责：
//   1. 对象池管理：预分配 600 个敌人实体，复用避免运行期创建/销毁开销。
//   2. 波次管理：按 WaveConfig 配置生成不同类型、数量的敌人。
//   3. 生成位置：玩家屏幕外环形随机，距离 600-800px。
//   4. 死亡回收：检测 Health <= 0 的敌人，回收到对象池。
//
// 对象池设计：
//   - Initialize 时预创建 600 个敌人实体，挂载所有组件（EnemyComponent、
//     Transform、Sprite、Velocity、Collider、Health、Tag），全部标记 active=false。
//   - freeList_ 存储可用的 EntityId 栈。
//   - Spawn 时从 freeList 弹出一个 EntityId，重置组件数据，设 active=true。
//   - Update 时检测死亡敌人，设 active=false，压回 freeList。
//   - 整个游戏循环内零实体创建/销毁，性能稳定。
//
// 波次配置（数据驱动）：
//   每波定义敌人数量、类型分布、生成间隔。波次号越大，敌人越多越强。
//   5 种敌人原型配置：
//     Melee:   hp=20,  speed=80,  damage=5,  color=红
//     Ranged:  hp=15,  speed=50,  damage=3,  color=紫
//     Suicide: hp=10,  speed=120, damage=20, color=橙
//     Elite:   hp=100, speed=70,  damage=15, color=金（带词缀光环）
//     Boss:    hp=1000,speed=40,  damage=30, color=暗红（体型 2x）
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdint>
#include "ecs/Entity.h"
#include "gameplay/EnemyAI.h"

namespace cu {

class Registry;
class TextureAtlas;

// ---- 敌人原型配置（数据驱动）----
struct EnemyPrototype {
    EnemyType type;
    float hp;
    float moveSpeed;
    float damage;
    float attackRange;
    float attackCooldown;
    float detectionRange;
    sf::Color color;
    float scale;       // 体型缩放（Boss 为 2.0）
    const char* spriteName; // 图集中的精灵名称
};

// ---- 波次配置 ----
struct WaveConfig {
    int totalEnemies;        // 本波总敌人数
    int meleeCount;          // 近战数量
    int rangedCount;         // 远程数量
    int suicideCount;        // 自爆数量
    int eliteCount;          // 精英数量
    int bossCount;           // Boss 数量
    // ---- 新增怪物数量 ----
    int stealthCount = 0;             // 隐身近战数量
    int countdownSuicideCount = 0;    // 倒计时自爆数量
    int splitterCount = 0;            // 分裂怪数量
    int shieldedCount = 0;            // 带盾怪数量
    int sniperCount = 0;              // 狙击远程数量
    int casterCount = 0;              // 施法者数量
    float spawnInterval;     // 生成间隔（秒）
};

class EnemySpawner {
public:
    // 预分配的敌人池容量（600 + 余量）
    static constexpr int kPoolCapacity = 650;

    EnemySpawner();
    ~EnemySpawner() = default;

    // 初始化：预创建敌人实体池
    // registry: ECS 注册表
    // atlas: 已构建的纹理图集（需包含敌人精灵）
    // playerPos: 玩家初始位置（用于生成位置计算）
    void Initialize(Registry& registry, const TextureAtlas& atlas,
                    const sf::Vector2f& playerPos);

    // 开始新一波
    // waveNumber: 波次号（1 开始）
    void StartWave(int waveNumber);

    // 在指定位置生成一个敌人（用于房间内生成，Phase 6）
    // type: 敌人类型
    // position: 生成位置（世界坐标）
    // champion: 是否为精英强化版（HP×3 伤害×1.5 速度×1.1 体型×1.5，头上显示血条）
    // 返回: 生成的敌人实体 ID（失败返回 kInvalidEntity）
    EntityId SpawnEnemyAt(EnemyType type, sf::Vector2f position, bool champion = false);

    // 在指定位置范围内生成多个敌人（用于房间内生成，Phase 6）
    // type: 敌人类型
    // center: 生成中心（世界坐标）
    // count: 生成数量
    // radius: 生成半径（像素）
    // championChance: 每个敌人升级为精英强化版的概率（0.0-1.0，默认 0=不升级）
    void SpawnEnemiesInArea(EnemyType type, sf::Vector2f center, int count, float radius,
                            float championChance = 0.f);

    // 每帧更新：处理生成队列、回收死亡敌人
    // dt: 固定步长时间（秒）
    // playerPos: 玩家当前位置（生成位置基于玩家）
    void Update(float dt, const sf::Vector2f& playerPos);

    // 获取当前活跃敌人数
    [[nodiscard]] int GetAliveCount() const noexcept { return aliveCount_; }

    // 当前波次是否已完成（所有敌人生成且全部死亡）
    [[nodiscard]] bool IsWaveComplete() const noexcept;

    // 获取当前波次号
    [[nodiscard]] int GetCurrentWave() const noexcept { return currentWave_; }

    // 清除所有活跃敌人（回收到池中）
    void ClearAllEnemies();

    // 获取对象池总容量
    [[nodiscard]] int GetPoolCapacity() const noexcept { return kPoolCapacity; }

    // 获取对象池可用数量
    [[nodiscard]] int GetFreeCount() const noexcept { return static_cast<int>(freeList_.size()); }

    // 设置当前地牢层数，用于敌人属性缩放
    void SetDungeonLevel(int level) noexcept { dungeonLevel_ = std::max(1, level); }

    // ---- 第十七轮新增：地牢变异系统 multiplier 注入接口 ----
    // 由 Game 在 setupPlayingScene / nextLevel 时根据 FloorModifierSystem 设置
    // 默认值均为 1.0（无影响），在 SpawnEnemyAt 内与层数缩放累乘应用
    void SetModifierEnemyHpMul(float m)          noexcept { modEnemyHpMul_ = m; }
    void SetModifierEnemyDamageMul(float m)      noexcept { modEnemyDmgMul_ = m; }
    void SetModifierEnemyMoveSpeedMul(float m)   noexcept { modEnemySpdMul_ = m; }
    void SetModifierEnemyAttackSpeedMul(float m) noexcept { modEnemyAtkSpdMul_ = m; }
    void SetModifierSpawnIntervalMul(float m)    noexcept { modSpawnIntervalMul_ = m; }

    // ---- 分维度层数缩放系数（线性增长，避免指数爆炸）----
    // HP 缩放：1.0 + (level-1) × 0.30，决定击杀时间缓慢增长
    [[nodiscard]] float GetHpScaling() const noexcept;
    // 伤害缩放：1.0 + (level-1) × 0.18，避免后期一击秒杀
    [[nodiscard]] float GetDamageScaling() const noexcept;
    // 速度缩放：1.0 + (level-1) × 0.04，封顶 1.6，保证玩家始终可躲避
    [[nodiscard]] float GetSpeedScaling() const noexcept;

private:
    // 从对象池获取一个敌人实体（返回 kInvalidEntity 若池空）
    [[nodiscard]] EntityId acquireFromPool();

    // 将敌人实体回收到对象池
    void releaseToPool(EntityId id);

    // 根据类型获取敌人原型配置
    [[nodiscard]] const EnemyPrototype& getPrototype(EnemyType type) const;

    // 生成一个敌人
    void spawnEnemy(EnemyType type, const sf::Vector2f& playerPos);

    // 生成随机环形位置（玩家屏幕外 600-800px）
    [[nodiscard]] sf::Vector2f randomSpawnPosition(const sf::Vector2f& playerPos) const;

    // 回收死亡敌人（Health <= 0 且 active）
    void recycleDeadEnemies();

    // 根据波次号生成配置
    [[nodiscard]] WaveConfig generateWaveConfig(int waveNumber) const;

private:
    Registry* registry_ = nullptr;
    const TextureAtlas* atlas_ = nullptr;

    // 对象池：预创建的敌人 EntityId 列表
    std::vector<EntityId> enemyPool_;
    // 空闲 EntityId 栈（可复用）
    std::vector<EntityId> freeList_;

    // 生成队列：待生成的敌人类型列表
    std::vector<EnemyType> spawnQueue_;
    float spawnTimer_ = 0.f;       // 生成计时器
    float spawnInterval_ = 0.1f;   // 生成间隔（秒）

    int aliveCount_ = 0;           // 当前活跃敌人数
    int currentWave_ = 0;          // 当前波次号
    int totalToSpawn_ = 0;         // 本波待生成总数
    int spawnedCount_ = 0;         // 本波已生成数
    int dungeonLevel_ = 1;         // 当前地牢层数（影响敌人属性缩放）

    // ---- 第十七轮新增：地牢变异系统 multiplier（默认 1.0=无影响）----
    // 在 SpawnEnemyAt 内与层数缩放、Champion 倍率累乘应用
    float modEnemyHpMul_          = 1.f;
    float modEnemyDmgMul_         = 1.f;
    float modEnemySpdMul_         = 1.f;
    float modEnemyAtkSpdMul_      = 1.f; // >1=更快攻击（冷却更短）
    float modSpawnIntervalMul_    = 1.f; // <1=更频繁刷怪
};

} // namespace cu
