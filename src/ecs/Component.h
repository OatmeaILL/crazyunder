#pragma once

// ============================================================================
// Component —— 组件定义（POD 结构体）
// ----------------------------------------------------------------------------
// 设计原则：
//   1. 组件是纯数据（POD/聚合体），不含逻辑方法，便于 SoA 布局与缓存友好遍历。
//   2. 逻辑由 System（系统）持有，System 遍历 Registry 中拥有特定组件集合的实体。
//   3. 组件字段尽量使用基本类型，避免 std::string 等堆分配成员（Sprite 例外，
//      保留 texturePath 用于非图集模式，但演示场景优先使用 atlasIndex）。
//
// SoA（Structure of Arrays）vs AoS（Array of Structures）：
//   - AoS：每个实体一个 struct，连续存放。遍历时加载整个 struct 到缓存行。
//   - SoA：每种字段一个数组。只遍历所需字段时缓存利用率更高。
//   - 本框架的 ComponentPool<T> 内部用 AoS（std::vector<T>），但因为是按组件类型
//     分池存储，遍历 Transform 时不会加载 Sprite 数据，已具备 SoA 的核心优势：
//     "按需加载，不污染缓存行"。
// ============================================================================

#include <SFML/Graphics.hpp>
#include <cstdint>
#include <string>
#include "ecs/Entity.h"

namespace cu {

// ============================================================================
// Transform —— 变换组件（位置/旋转/缩放）
// ============================================================================
struct Transform {
    sf::Vector2f position{0.f, 0.f}; // 世界坐标
    float rotation = 0.f;            // 旋转角度（度）
    sf::Vector2f scale{1.f, 1.f};    // 缩放
};

// ============================================================================
// Sprite —— 精灵渲染组件
// ----------------------------------------------------------------------------
// atlasIndex 优先：>= 0 时使用 TextureAtlas 中的对应贴图，渲染系统通过图集
// 查找 sf::Texture*，同图集精灵可合并为单次 Draw Call。
// atlasIndex < 0 时回退到 texturePath（从 ResourceManager 加载）。
// ============================================================================
struct Sprite {
    int32_t atlasIndex = -1;          // 纹理图集索引（-1 = 未使用图集）
    std::string texturePath;          // 回退：直接纹理路径
    sf::IntRect sourceRect;           // 源矩形（像素坐标，图集或纹理内子区域）
    sf::Color color = sf::Color::White; // 着色/透明度
    sf::Vector2f origin{16.f, 16.f};  // 中心点（32x32 默认中心）
};

// ============================================================================
// Velocity —— 速度组件（线速度 + 角速度）
// ============================================================================
struct Velocity {
    sf::Vector2f linear{0.f, 0.f}; // 线速度（像素/秒）
    float angular = 0.f;           // 角速度（度/秒）
};

// ============================================================================
// Collider —— 碰撞体组件
// ----------------------------------------------------------------------------
// 割草游戏实体数量大（数千），圆形碰撞计算最简单（距离比较），性能最优。
// boxSize 用于需要矩形碰撞的特殊实体（如障碍物）。
// isCircle=true 用 radius，false 用 boxSize。
// ============================================================================
struct Collider {
    bool isCircle = true;             // true=圆形，false=矩形
    float radius = 16.f;              // 圆形半径
    sf::Vector2f boxSize{32.f, 32.f}; // 矩形尺寸
};

// ============================================================================
// Health —— 生命值组件
// ============================================================================
struct Health {
    float current = 100.f;     // 当前生命值
    float max = 100.f;         // 最大生命值
    float invincibleTimer = 0.f; // 无敌剩余时间（秒）
};

// ============================================================================
// EnemyState —— 敌人 AI 状态枚举
// ============================================================================
enum class EnemyState : uint8_t {
    Idle,    // 待机
    Patrol,  // 巡逻
    Chase,   // 追击
    Attack,  // 攻击
    Flee,    // 逃跑
    Dead     // 死亡
};

// ============================================================================
// AI —— 敌人 AI 组件
// ============================================================================
struct AI {
    EnemyState state = EnemyState::Idle;
    EntityId target = kInvalidEntity; // 追击目标实体 ID
    float moveSpeed = 80.f;           // 移动速度（像素/秒）
    float attackCooldown = 0.f;       // 攻击冷却剩余时间
};

// ============================================================================
// ElementType —— 元素类型枚举
// ----------------------------------------------------------------------------
// 不同元素对应不同状态效果：
//   Physical：无状态效果（纯物理伤害）
//   Fire：燃烧（持续伤害）
//   Ice：冰冻（减速）
//   Lightning：闪电（连锁）
//   Poison：中毒（持续伤害 + 减速）
// ============================================================================
enum class ElementType : uint8_t {
    Physical  = 0, // 物理
    Fire      = 1, // 火
    Ice       = 2, // 冰
    Lightning = 3, // 闪电
    Poison    = 4  // 毒
};

// ============================================================================
// Projectile —— 投射物组件
// ----------------------------------------------------------------------------
// owner 区分玩家子弹与敌人子弹，避免误伤。pierce 为穿透次数。
// Phase 5 扩展：增加方向、速度、元素、分裂、连锁等字段，支持割草爽游的
// 复杂弹幕效果。对象池管理：active=false 表示子弹在池中待命。
// ============================================================================
struct Projectile {
    float damage = 10.f;       // 伤害值
    int pierce = 0;            // 剩余穿透次数（0=命中即销毁）
    float lifetime = 3.f;      // 生命周期（秒，超时自动销毁）
    EntityId owner = kInvalidEntity; // 所有者（玩家/敌人）

    // ---- Phase 5 扩展字段 ----
    sf::Vector2f direction{1.f, 0.f};  // 归一化飞行方向
    float speed = 400.f;               // 飞行速度（像素/秒）
    sf::Color color = sf::Color::White; // 着色（元素标识）
    int splitCount = 0;                // 命中时分裂出的子弹数
    int chainCount = 0;                // 连锁闪电次数
    ElementType element = ElementType::Physical; // 元素类型
    float radius = 6.f;                // 碰撞半径
    bool active = false;               // 是否活跃（对象池复用标志）
    float hitCooldown = 0.f;           // 命中冷却（避免同一帧多次命中）
};

// ============================================================================
// Lifetime —— 生命周期组件（自动销毁）
// ============================================================================
struct Lifetime {
    float remaining = 0.f;   // 剩余时间（秒）
    bool autoDestroy = true; // 到期是否自动销毁实体
};

// ============================================================================
// Particle —— 粒子数据组件
// ============================================================================
struct Particle {
    sf::Vector2f velocity{0.f, 0.f}; // 速度
    sf::Color color = sf::Color::White;
    float size = 4.f;       // 像素大小
    float life = 1.f;       // 剩余生命
    float maxLife = 1.f;    // 最大生命（用于计算衰减比例）
};

// ============================================================================
// TagFlag —— 实体标签位掩码
// ----------------------------------------------------------------------------
// 用于快速筛选实体类别，避免为每个类别创建独立组件。
// 位掩码支持多标签组合：TagFlag::Enemy | TagFlag::Loot
// ============================================================================
enum class TagFlag : uint32_t {
    None       = 0,
    Player     = 1u << 0,
    Enemy      = 1u << 1,
    Projectile = 1u << 2,
    Particle   = 1u << 3,
    Loot       = 1u << 4,
    Prop       = 1u << 5,
};

// 位运算符重载
inline constexpr TagFlag operator|(TagFlag a, TagFlag b) noexcept {
    return static_cast<TagFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline constexpr TagFlag operator&(TagFlag a, TagFlag b) noexcept {
    return static_cast<TagFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline constexpr TagFlag operator~(TagFlag a) noexcept {
    return static_cast<TagFlag>(~static_cast<uint32_t>(a));
}
inline constexpr bool HasTag(TagFlag flags, TagFlag test) noexcept {
    return (flags & test) != TagFlag::None;
}

// Tag 组件：持有标签位掩码
struct Tag {
    TagFlag flags = TagFlag::None;
};

// ============================================================================
// StatusEffectData —— 单个状态效果数据（POD）
// ----------------------------------------------------------------------------
// 状态效果类型：
//   Fire（燃烧）：每 0.5s 造成 tickDamage 伤害，持续 duration 秒
//   Ice（冰冻）：减速 50%，持续 duration 秒
//   Lightning（闪电）：瞬时伤害，无持续效果（连锁由 ProjectileSystem 处理）
//   Poison（中毒）：每 1s 造成 tickDamage 伤害 + 减速 30%
// ============================================================================
struct StatusEffectData {
    ElementType type = ElementType::Physical; // 状态类型
    float duration = 0.f;     // 总持续时间（秒）
    float tickDamage = 0.f;   // 每次周期伤害值
    float timer = 0.f;        // 周期计时器（到 0 时触发一次伤害并重置）
    float remaining = 0.f;    // 剩余持续时间（秒）
};

// ============================================================================
// StatusEffectComponent —— 状态效果组件
// ----------------------------------------------------------------------------
// 挂载到可受状态效果的实体（玩家、敌人）。一个实体可同时拥有多种状态。
// CombatSystem.UpdateStatusEffects 遍历此组件，推进计时器并施加周期伤害。
// ============================================================================
struct StatusEffectComponent {
    std::vector<StatusEffectData> effects; // 当前活跃状态列表
};

// ============================================================================
// DamageTextComponent —— 伤害飘字组件
// ----------------------------------------------------------------------------
// 在世界坐标显示浮动伤害数字，向上漂浮 + 淡出。
// 暴击时字体放大并变黄色，普通伤害为白色。
// 生命周期结束后由系统自动销毁实体。
// ============================================================================
struct DamageTextComponent {
    float amount = 0.f;              // 伤害数值
    bool isCritical = false;         // 是否暴击
    float lifetime = 0.8f;           // 总生命（秒）
    float maxLifetime = 0.8f;        // 最大生命（用于计算淡出比例）
    sf::Vector2f velocity{0.f, -80.f}; // 漂浮速度（向上为负）
    sf::Color color = sf::Color::White; // 文字颜色
    std::string prefix;              // 文本前缀（如 "+EXP"、""）
    int fontSize = 0;                // 自定义字号（0=使用默认）
    bool fadeIn = false;             // 是否启用淡入效果（物品拾取飘字用）
};

// ============================================================================
// Phase 7: 战利品系统组件
// ============================================================================

// ExpOrb —— 经验球组件
// 敌人死亡时生成，玩家拾取后增加 EXP。
// 使用 ECS 实体 + 对象池预分配，整局游戏内零分配。
struct ExpOrb {
    int value = 5;            // 经验值
    bool active = false;      // 对象池复用标志
    float lifetime = 60.f;    // 生存时间（秒，超时自动消失）
    float magnetTimer = 0.f;  // 磁吸触发计时（避免每帧重算）
};

// LootDrop —— 装备掉落物组件
// 精英/Boss/宝箱掉落，玩家拾取后加入背包/自动装备。
// 注意：完整 Item 数据由 PickupSystem 通过外部映射维护，
// 此处仅记录索引，避免在组件中存储 std::string/vector（堆分配）。
struct LootDrop {
    int itemIndex = -1;       // 在 LootDropRegistry 中的索引
    bool active = false;      // 对象池复用标志
    float lifetime = 30.f;    // 生存时间（秒，30s 后消失）
};

// EnemyAffix —— 精英怪词缀组件
// 标记敌人为精英并附加词缀强化效果。
// 词缀类型由位掩码组合：HpBoost | DamageBoost | SpeedBoost | Regenerating
struct EnemyAffix {
    uint32_t affixMask = 0;    // 词缀位掩码（多个词缀可叠加）
    float regenTimer = 0.f;    // 回血计时器（Regenerating 词缀用）
    float auraTimer = 0.f;     // 光环粒子计时器
};

// 精英词缀位掩码定义
enum class EliteAffix : uint32_t {
    None         = 0,
    HpBoost      = 1u << 0,  // HP x3
    DamageBoost  = 1u << 1,  // 伤害 x2
    SpeedBoost   = 1u << 2,  // 速度 x1.5
    Regenerating = 1u << 3   // 每秒回 1% HP
};

[[nodiscard]] inline bool HasEliteAffix(uint32_t mask, EliteAffix a) noexcept {
    return (mask & static_cast<uint32_t>(a)) != 0u;
}

[[nodiscard]] inline const char* EliteAffixName(EliteAffix a) noexcept {
    switch (a) {
        case EliteAffix::HpBoost:      return "HpBoost";
        case EliteAffix::DamageBoost:  return "DamageBoost";
        case EliteAffix::SpeedBoost:   return "SpeedBoost";
        case EliteAffix::Regenerating: return "Regenerating";
        default: return "?";
    }
}

// ============================================================================
// NPCComponent —— 可对话 NPC 组件（第三十三轮新增）
// ----------------------------------------------------------------------------
// 标记实体为可对话 NPC，包含对话树引用 ID。
// 对话树 ID 指向 DialogueSystem 中注册的对话树。
// ============================================================================
struct NPCComponent {
    int dialogueTreeId = -1;  // 对话树 ID（-1 = 无对话）
    bool dialogueTriggered = false; // 对话是否已触发过（防止重复触发）
    const char* npcName = nullptr;  // NPC 名称（中文，用于交互提示）
    const char* interactHint = nullptr; // 交互提示文本（如"按 E 对话"）
};

} // namespace cu
