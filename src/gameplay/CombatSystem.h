#pragma once

// ============================================================================
// CombatSystem —— 战斗系统：伤害计算、状态效果、事件回调
// ----------------------------------------------------------------------------
// 职责：
//   1. ApplyDamage：施加伤害，处理暴击、击退、吸血
//   2. ApplyStatus：添加状态效果（燃烧/冰冻/中毒等）
//   3. UpdateStatusEffects：每帧推进状态计时器，施加周期伤害
//   4. 事件回调：OnHit（命中）、OnKill（击杀）、OnDamage（造成伤害）
//
// 伤害计算公式：
//   最终伤害 = baseDamage × (isCritical ? critMultiplier : 1.0)
//   暴击判定：随机数 < critRate 则暴击
//   默认暴击率 15%，暴击伤害倍率 1.5x
//
// 击退处理：
//   将 DamageInfo.knockback 叠加到目标的 Velocity.linear，
//   使目标被推开。击退力度由调用者决定（如子弹方向 × 力度）。
//
// 吸血处理：
//   攻击者恢复 HP = damage × lifesteal。
//   lifesteal=0.1 表示吸取 10% 伤害值为生命。
//
// 死亡处理：
//   HP <= 0 时触发 OnKill 回调，并将 Health.current 设为 0。
//   实体的实际销毁由调用者（Game/EnemySpawner）负责，CombatSystem 仅标记。
// ============================================================================
#include <SFML/System/Vector2.hpp>
#include <functional>
#include "ecs/Entity.h"
#include "ecs/Component.h"

namespace cu {

class Registry;

// ---- 伤害信息结构体 ----
// 封装一次伤害的所有参数，传递给 ApplyDamage。
struct DamageInfo {
    EntityId attacker = kInvalidEntity;   // 攻击者实体
    EntityId target = kInvalidEntity;     // 目标实体
    float amount = 0.f;                   // 基础伤害值（暴击前）
    bool isCritical = false;              // 是否暴击（由 ApplyDamage 内部判定或外部指定）
    ElementType element = ElementType::Physical; // 元素类型
    sf::Vector2f knockback{0.f, 0.f};     // 击退向量（像素/秒，叠加到目标速度）
    float lifesteal = 0.f;                // 吸血比例（0~1）
};

// ---- 状态效果结构体 ----
// 用于 ApplyStatus 添加状态效果到实体。
struct StatusEffect {
    ElementType type = ElementType::Physical; // 状态类型
    float duration = 0.f;     // 总持续时间（秒）
    float tickDamage = 0.f;   // 每次周期伤害值
    float tickInterval = 0.5f; // 周期间隔（秒，燃烧 0.5s，中毒 1s）
};

class CombatSystem {
public:
    CombatSystem() = default;
    ~CombatSystem() = default;

    // ---- 事件回调（由 Game 层注册）----
    // OnHit：每次命中目标时触发（无论是否击杀）
    std::function<void(const DamageInfo&)> OnHit;
    // OnKill：目标死亡时触发（参数：受害者、击杀者）
    std::function<void(EntityId victim, EntityId killer)> OnKill;
    // OnDamage：每次造成实际伤害时触发（含状态效果周期伤害）
    std::function<void(const DamageInfo&)> OnDamage;

    // ---- 核心接口 ----

    // 施加伤害到目标
    // 处理流程：
    //   1. 检查目标是否存活、是否无敌
    //   2. 应用暴击（若 isCritical=false 且需判定，可由调用者预先判定）
    //   3. 计算最终伤害 = amount × (isCritical ? critMultiplier : 1)
    //   4. 扣减目标 HP
    //   5. 应用击退（修改 Velocity）
    //   6. 应用吸血（恢复攻击者 HP）
    //   7. 触发 OnHit / OnDamage 回调
    //   8. 若 HP <= 0，触发 OnKill 回调
    void ApplyDamage(Registry& registry, const DamageInfo& info);

    // 添加状态效果到目标
    // 若目标无 StatusEffectComponent，自动添加。
    // 同类型状态刷新持续时间与伤害（不叠加，取较强者）。
    void ApplyStatus(Registry& registry, EntityId target, const StatusEffect& effect);

    // 更新所有状态效果（每固定步长调用）
    // 推进计时器，到周期时施加 tickDamage，持续时间结束则移除。
    void UpdateStatusEffects(Registry& registry, float dt);

    // ---- 暴击计算 ----

    // 掷骰判定暴击：随机数 < critRate 则暴击
    [[nodiscard]] static bool RollCritical(float critRate) noexcept;

    // 计算暴击伤害 = baseDamage × critMultiplier
    [[nodiscard]] static float CalculateCriticalDamage(float baseDamage,
                                                        float critMultiplier) noexcept;

    // ---- 第十八轮新增：连击系统伤害乘数查表 ----
    // 输入当前连击数，返回伤害乘数（1.0 = 无加成）。
    // 阶梯设计（保证前中期有感知、后期有上限避免破坏平衡）：
    //   combo < 10       → 1.00x（无加成，鼓励玩家上 10 击）
    //   combo 10 ~ 24    → 1.20x（+20%，前期甜头）
    //   combo 25 ~ 49    → 1.35x（+35%，中期核心）
    //   combo 50 ~ 99    → 1.50x（+50%，激进 build 收益）
    //   combo >= 100     → 1.75x（+75%，硬上限，避免一击秒杀后期 Boss）
    [[nodiscard]] static float GetComboDamageMultiplier(int comboCount) noexcept;

    // ---- 第二十轮新增：极限闪避伤害乘数 ----
    // 返回极限闪避 buff 激活时的伤害乘数（1.5x = +50%）。
    // 设计意图：与连击系统乘法累乘。例：combo 50 (1.5x) + 极限闪避 (1.5x) = 2.25x
    // 上限控制：固定 1.5x 不叠加，避免极端情况下伤害爆炸。
    // 应用位置：ApplyDamage 内部，检测 attacker 的 perfectDodgeBuffTimer > 0
    // 注意：DoT 周期伤害（燃烧/中毒）不经过 ApplyDamage，因此不受此加成（与 combo 一致）。
    [[nodiscard]] static float GetPerfectDodgeDamageMultiplier() noexcept { return 1.50f; }

    // ---- 默认参数 ----
    static constexpr float kDefaultCritRate = 0.15f;       // 默认暴击率 15%
    static constexpr float kDefaultCritMultiplier = 1.5f;  // 默认暴击伤害倍率 1.5x

    // ---- 统计 ----
    [[nodiscard]] int GetKillCount() const noexcept { return killCount_; }
    void ResetKillCount() noexcept { killCount_ = 0; }

    // 获取元素对应的默认状态效果参数
    [[nodiscard]] static StatusEffect CreateElementalStatus(ElementType elem,
                                                             float damage) noexcept;

private:
    int killCount_ = 0; // 累计击杀数
};

} // namespace cu
