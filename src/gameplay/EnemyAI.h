#pragma once

// ============================================================================
// EnemyAI —— 敌人 AI 系统（流场 + Boids 分离力）
// ----------------------------------------------------------------------------
// 设计目标：
//   支持 500+ 敌人同屏 120 FPS。核心策略：
//   1. 流场寻路：所有敌人共享同一方向场，O(1) 查询移动方向，无需单独寻路。
//   2. Boids 分离力：通过 UniformGrid 邻近查询计算分离力，避免 O(N²) 两两比较。
//   3. 数据驱动：EnemyComponent 为 POD 结构，遍历时缓存友好。
//
// 敌人类型（EnemyType）：
//   - Melee：近战追击，接触玩家造成伤害
//   - Ranged：远程射击，保持距离定期发射子弹（子弹系统 Phase 5 实现，此处占位）
//   - Suicide：自爆，高速冲向玩家，接触后自爆造成大范围伤害
//   - Elite：精英，带词缀光环，属性强化
//   - Boss：Boss，体型 2x，高血量高伤害
//
// Boids 分离力算法：
//   Boids 是模拟群体行为的算法，包含三条规则：
//     1. 分离（Separation）：避免与邻居重叠
//     2. 对齐（Alignment）：与邻居方向一致
//     3. 聚合（Cohesion）：向邻居中心移动
//   割草游戏中，敌人移动方向由流场决定（替代对齐与聚合），仅需分离力
//   避免敌人重叠堆叠。分离力计算：
//     - 查询半径 R 内的所有邻居
//     - 对每个邻居计算反向向量（远离邻居），按距离反比加权
//     - 累加所有分离力，归一化后乘以权重
//
// 性能目标：
//   - 500 敌人 AI 更新 < 2ms（依赖 UniformGrid 邻近查询）
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include <cstdint>
#include <vector>
#include "ecs/Entity.h"

namespace cu {

class Registry;
class FlowField;
class UniformGrid;
class ProjectileSystem;
class ParticleSystem;
class CombatSystem;
class EnemySpawner;
struct Dungeon;

// ---- 地裂区域（Boss 冲撞路径留下的持续伤害区域）----
struct FissureZone {
    sf::Vector2f position{0.f, 0.f}; // 地裂中心位置
    float radius = 40.f;             // 伤害范围半径
    float lifetime = 5.f;            // 剩余存在时间（秒）
    float damagePerSec = 0.f;        // 每秒伤害值
    float damageTickTimer = 0.f;     // 伤害 tick 计时器（每 0.5s 造成一次伤害）
};

// ---- 施法者 AoE 预警区域（Caster 施法地面标记圈）----
struct CastWarningZone {
    sf::Vector2f position{0.f, 0.f}; // 预警中心
    float radius = 80.f;             // 爆炸范围半径
    float lifetime = 1.5f;           // 剩余预警时间（秒）
    float damage = 0.f;              // 爆炸伤害值
    bool exploded = false;           // 是否已爆炸（避免重复爆炸）
};

// ---- 敌人类型枚举 ----
enum class EnemyType : uint8_t {
    Melee   = 0, // 近战追击
    Ranged  = 1, // 远程射击
    Suicide = 2, // 自爆
    Elite   = 3, // 精英（带词缀）
    Boss    = 4, // Boss
    // ---- 新增怪物类型（增加多样性）----
    StealthMelee = 5,      // 间歇性隐身近战（隐身时半透明，现身时攻击）
    CountdownSuicide = 6, // 倒计时自爆（靠近后头上出现倒计时，到 0 爆炸）
    Splitter = 7,         // 分裂怪（死亡时分裂成 2 个小怪）
    Shielded = 8,         // 带盾怪（正面减伤 50%）
    SniperRanged = 9,     // 狙击远程（超远距离高伤害射击，靠近时快速撤退）
    Caster = 10,          // 施法者（中距离引导 AoE 法阵，延迟范围爆炸）
};

// ---- 敌人组件 ----
// 挂载到敌人实体上，存储 AI 所需的属性数据。
// 生命值使用独立的 Health 组件（复用现有 ECS 组件），此处不含 hp 字段。
struct EnemyComponent {
    EnemyType type = EnemyType::Melee; // 敌人类型
    float moveSpeed = 80.f;            // 移动速度（像素/秒）
    float attackCooldown = 0.f;        // 攻击冷却剩余时间（秒）
    float attackRange = 20.f;          // 攻击范围（像素，近战=接触距离，远程=射击距离）
    float damage = 5.f;                // 伤害值
    float detectionRange = 400.f;      // 检测范围（远程敌人保持此距离）
    bool isElite = false;              // 是否为精英（带词缀光环）
    bool isBoss = false;               // 是否为 Boss
    bool active = false;               // 是否活跃（对象池复用标志，false=池中待命）
    // 精英强化版（区别于 EnemyType::Elite 固定类型）
    // 任何普通怪有概率升级为 Champion：HP×3 伤害×1.5 速度×1.1 体型×1.5
    // 头上显示小血条，死亡掉落更好装备
    bool isChampion = false;

    // ---- 新增：怪物特殊机制字段 ----
    // 隐身机制（StealthMelee）
    float stealthTimer = 0.f;        // 隐身周期计时（累计时间）
    bool isStealth = false;         // 当前是否隐身
    // 自爆倒计时机制（CountdownSuicide）
    float selfDestructCountdown = 0.f; // 自爆倒计时剩余秒数（>0 表示已激活）
    bool countdownActive = false;   // 倒计时是否已激活（靠近玩家后激活）
    // 分裂机制（Splitter）
    int splitCount = 0;             // 死亡时分裂出的小怪数量（0=不分裂）
    // 带盾机制（Shielded）
    bool hasShield = false;         // 是否有盾
    float shieldAngle = 0.f;        // 盾牌朝向角度（度，朝向玩家时减伤）
    // 通用特殊行为计时器（冲锋等）
    float specialTimer = 0.f;       // 特殊行为计时器
    float walkSoundTimer = 0.f;     // 脚步声计时器

    // ---- 减速效果字段（由 SkillSystem 地刺设置，EnemyAI 应用后重置）----
    // 设计为单帧有效：SkillSystem 每帧在地刺范围内设置 slowFactor，
    // EnemyAI 在合成 desiredVelocity 时乘以 (1 - slowFactor) 后立即重置为 0。
    // 这样退出范围后减速立即消失，且不依赖 SkillSystem/EnemyAI 的执行先后顺序
    // （最差情况延迟 1 帧生效）。
    float slowFactor = 0.f;         // 当前帧减速比例（0=正常速度，0.5=移速减半）

    // ---- 第二十轮新增：攻击前摇字段（极限闪避系统用）----
    // 设计意图：让敌人攻击有可读的"前摇"信号，玩家在前摇期间闪避可触发极限闪避。
    //   - 近战敌人：attackCooldown < 0.3s 时 attackTelegraph = 0.3s（即将挥击）
    //   - 远程敌人：attackCooldown < 0.5s 时 attackTelegraph = 0.5s（蓄力瞄准）
    //   - Boss 不参与（Boss 攻击节奏复杂，且过于强大不适合极限闪避）
    // EnemyAI 每帧根据 attackCooldown 推进 attackTelegraph；PlayerCombat 闪避时读取
    // 附近敌人的 attackTelegraph > 0 判定是否触发极限闪避。
    // 圣物"月光护符"通过 PlayerStats.dodgeWindowMul 延长判定窗口（0.3s → 0.45s）。
    float attackTelegraph = 0.f;    // 攻击前摇剩余时间（>0=即将攻击，玩家可触发极限闪避）

    // ---- 施法者专属机制（Caster）----
    float castTimer = 0.f;          // 施法冷却计时器
    float castActive = 0.f;         // 正在施法剩余时间（>0=引导中）
    sf::Vector2f castTargetPos{0.f, 0.f}; // 施法目标位置（地面 AoE 中心）
    float castWarningRadius = 80.f; // AoE 范围半径
    float castWarningLifetime = 0.f;// 地面预警圈剩余时间（>0=预警中，队友可避开）

    // ---- Boss 机制字段 ----
    float rangedAttackTimer = 0.f;  // Boss 远程攻击计时器
    float summonTimer = 0.f;       // Boss 召唤小兵计时器
    bool isBossMinion = false;     // 是否为 Boss 召唤的小兵（死亡时掉落爱心）

    // ---- Boss 新增机制字段 ----
    // 冲撞+地裂机制
    float chargeTimer = 0.f;        // 冲撞冷却计时器（每 6s 触发一次）
    float chargeActive = 0.f;       // 冲撞持续剩余时间（>0=正在冲撞）
    sf::Vector2f chargeDir{0.f, 0.f}; // 冲撞方向（归一化）
    float chargeSpeedMul = 3.0f;    // 冲撞期间速度倍率
    // 旋转弹幕机制
    float spiralTimer = 0.f;        // 旋转弹幕冷却计时器（每 10s 触发一次）
    float spiralActive = 0.f;       // 旋转弹幕持续剩余时间（>0=正在发射）
    float spiralAngle = 0.f;        // 旋转弹幕当前角度（用于螺旋效果）
    float spiralFireTimer = 0.f;    // 旋转弹幕发射间隔计时器（独立于 specialTimer，避免与冲撞地裂冲突）
    // 召唤精英怪机制
    float eliteSummonTimer = 0.f;   // 召唤精英计时器（每 12s 检查一次，仅在 HP<50% 触发）
    bool eliteSummoned50 = false;   // 是否已触发过 HP<50% 的精英召唤（避免每次都触发）
};

// ============================================================================
// UpdateEnemyAI —— 敌人 AI 主更新函数
// ----------------------------------------------------------------------------
// 遍历所有拥有 EnemyComponent + Transform 的实体，执行：
//   1. 从 FlowField 获取移动方向
//   2. 通过 UniformGrid 查询邻近敌人，计算 Boids 分离力
//   3. 合成最终速度：流场方向 × moveSpeed + 分离力 × weight
//   4. 更新 Transform.position 与 Velocity
//   5. 攻击逻辑（近战接触玩家造成伤害）
//
// 参数：
//   registry: ECS 注册表
//   flowField: 流场寻路数据
//   grid: 空间网格（已插入所有敌人位置）
//   playerEntity: 玩家实体 ID（用于攻击判定）
//   dt: 固定步长时间（秒）
//
// 返回：本次 AI 更新耗时（毫秒，调试用）
// ============================================================================
float UpdateEnemyAI(Registry& registry, const FlowField& flowField,
                    const UniformGrid& grid, EntityId playerEntity, float dt,
                    Dungeon* dungeon = nullptr);

// ============================================================================
// UpdateEnemyCombat —— 敌人战斗更新（Phase 5）
// ----------------------------------------------------------------------------
// 处理敌人的攻击行为与死亡处理：
//   1. 近战敌人：接触玩家时造成伤害（攻击冷却 1s）
//   2. 远程敌人：定期发射敌人子弹（朝玩家方向，攻击冷却 2s）
//   3. 自爆敌人：接近玩家时自爆，范围伤害 + 粒子
//   4. Boss：定期 AOE 技能
//   5. 死亡处理：触发 OnKill 事件、生成 HitSpark 粒子、释放回对象池
//
// 注意：近战攻击的基础逻辑已在 UpdateEnemyAI 中实现（接触伤害），
//       本函数补充远程射击、自爆爆炸、Boss AOE 等高级战斗行为。
//       死亡处理统一在此函数完成，避免与 EnemySpawner 的回收逻辑冲突。
//
// 参数：
//   registry: ECS 注册表
//   grid: 空间网格（用于 AOE 范围查询）
//   playerEntity: 玩家实体 ID
//   projectiles: 弹幕系统（用于远程敌人发射子弹）
//   particles: 粒子系统（用于自爆/死亡特效）
//   combat: 战斗系统（用于施加伤害）
//   dt: 固定步长时间（秒）
//
// 返回：本次战斗更新耗时（毫秒，调试用）
// ============================================================================
float UpdateEnemyCombat(Registry& registry, UniformGrid& grid,
                        EntityId playerEntity, ProjectileSystem& projectiles,
                        ParticleSystem& particles, CombatSystem& combat, float dt,
                        EnemySpawner* spawner = nullptr,
                        std::vector<FissureZone>* fissures = nullptr,
                        std::vector<CastWarningZone>* castWarnings = nullptr);

// ---- Boids 分离力参数 ----
// 这些参数可调整以改变敌人群体行为
inline constexpr float kSeparationRadius = 32.f;     // 分离力查询半径（像素）
inline constexpr float kSeparationWeight = 80.f;     // 分离力权重（与 moveSpeed 同量级）

// ---- 敌人类型名称（调试用）----
[[nodiscard]] inline const char* EnemyTypeName(EnemyType t) noexcept {
    switch (t) {
        case EnemyType::Melee:            return "Melee";
        case EnemyType::Ranged:           return "Ranged";
        case EnemyType::Suicide:          return "Suicide";
        case EnemyType::Elite:            return "Elite";
        case EnemyType::Boss:             return "Boss";
        case EnemyType::StealthMelee:     return "StealthMelee";
        case EnemyType::CountdownSuicide: return "CountdownSuicide";
        case EnemyType::Splitter:         return "Splitter";
        case EnemyType::Shielded:         return "Shielded";
        case EnemyType::SniperRanged:     return "SniperRanged";
        case EnemyType::Caster:           return "Caster";
    }
    return "?";
}

// ---- 敌人类型中文名（死亡回顾/UI 用）----
[[nodiscard]] inline const char* EnemyTypeChineseName(EnemyType t) noexcept {
    switch (t) {
        case EnemyType::Melee:            return "近战兵";
        case EnemyType::Ranged:           return "远程兵";
        case EnemyType::Suicide:          return "自爆兵";
        case EnemyType::Elite:            return "精英怪";
        case EnemyType::Boss:             return "BOSS";
        case EnemyType::StealthMelee:     return "隐身刺客";
        case EnemyType::CountdownSuicide: return "倒计时炸弹";
        case EnemyType::Splitter:         return "分裂怪";
        case EnemyType::Shielded:         return "盾卫";
        case EnemyType::SniperRanged:     return "狙击手";
        case EnemyType::Caster:           return "施法者";
    }
    return "未知";
}

} // namespace cu
