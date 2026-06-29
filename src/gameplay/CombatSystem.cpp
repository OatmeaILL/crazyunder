#include "gameplay/CombatSystem.h"
#include "gameplay/CombatEffects.h"
#include "gameplay/Player.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "utils/Logger.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace cu {

// ============================================================================
// ApplyDamage —— 施加伤害到目标
// ----------------------------------------------------------------------------
// 伤害计算流程：
//   1. 验证目标存活且有 Health 组件
//   2. 检查无敌时间（invincibleTimer > 0 则免伤）
//   3. 计算最终伤害（含暴击倍率）
//   4. 扣减 HP
//   5. 应用击退（叠加到 Velocity）
//   6. 应用吸血（恢复攻击者 HP）
//   7. 触发回调
//   8. 死亡判定
// ============================================================================
void CombatSystem::ApplyDamage(Registry& registry, const DamageInfo& info) {
    // 验证目标
    if (info.target == kInvalidEntity) return;
    Health* health = registry.GetComponent<Health>(info.target);
    if (!health) return;

    // 死亡实体不再受伤害
    if (health->current <= 0.f) return;

    // 无敌时间检查（闪避、受击后的短暂无敌）
    if (health->invincibleTimer > 0.f) {
        return; // 无敌期间免伤
    }

    // ---- 计算最终伤害 ----
    // 暴击时伤害 × critMultiplier（默认 1.5x）
    float finalDamage = info.amount;
    if (info.isCritical) {
        finalDamage *= kDefaultCritMultiplier;
    }

    // ---- 第十八轮新增：连击系统伤害加成 ----
    // 当攻击者是玩家时，根据其 PlayerComponent.comboCount 应用阶梯式伤害乘数。
    // 设计意图：奖励激进玩法，让连续击杀的玩家获得正反馈加速。
    // 此处集中处理而非在每个伤害源（子弹/AOE/技能）单独应用，原因：
    //   1. 普攻子弹伤害在 Spawn 时确定，飞行期间 combo 变化无法动态应用；
    //   2. 保证所有玩家伤害源（普攻/AOE/技能/元素状态衍生）统一应用乘数；
    //   3. 避免遗漏导致某些伤害源不享受 combo 加成。
    // 注意：DoT 周期伤害（燃烧/中毒）由 UpdateStatusEffects 直接扣 HP，不经过
    //       ApplyDamage，因此不受 combo 加成。这是有意设计——DoT 作为"持续伤害"
    //       不应被瞬时 combo 倍率放大，否则会导致 combo 高时 DoT 失控。
    if (info.attacker != kInvalidEntity) {
        const PlayerComponent* attackerPc = registry.GetComponent<PlayerComponent>(info.attacker);
        if (attackerPc != nullptr && attackerPc->comboCount > 0) {
            finalDamage *= GetComboDamageMultiplier(attackerPc->comboCount);
        }
    }

    // ---- 第二十轮新增：极限闪避伤害加成 ----
    // 当攻击者玩家的 perfectDodgeBuffTimer > 0 时，应用 1.5x 伤害乘数。
    // 与 combo 乘数乘法累乘：combo 50 (1.5x) + 极限闪避 (1.5x) = 2.25x
    // 设计意图：与连击系统形成"攻防双反馈"，极限闪避作为防御端的伤害奖励。
    // 圣物"复仇之刃"额外效果：buff 期间所有非暴击攻击强制暴击（perfectDodgeGuaranteedCrit）
    //   - 普攻子弹 Spawn 时已判定暴击的（isCritical=true）不重复应用
    //   - 反击伤害（isCritical=false）也会被复仇之刃暴击，作为圣物的强力效果
    if (info.attacker != kInvalidEntity) {
        const PlayerComponent* attackerPc = registry.GetComponent<PlayerComponent>(info.attacker);
        if (attackerPc != nullptr && attackerPc->perfectDodgeBuffTimer > 0.f) {
            finalDamage *= GetPerfectDodgeDamageMultiplier();
            // 复仇之刃：buff 期间所有非暴击攻击强制暴击
            if (attackerPc->stats.perfectDodgeGuaranteedCrit && !info.isCritical) {
                finalDamage *= kDefaultCritMultiplier;
            }
        }
    }

    // ---- 扣减 HP ----
    health->current -= finalDamage;
    if (health->current < 0.f) {
        health->current = 0.f;
    }

    // ---- 第三十轮新增：死亡回顾 - 伤害统计 ----
    // 当攻击者是玩家时，累计总伤害（用于 DPS 计算）
    if (info.attacker != kInvalidEntity) {
        PlayerComponent* attackerPc = registry.GetComponent<PlayerComponent>(info.attacker);
        if (attackerPc) {
            attackerPc->totalDamageDealt += finalDamage;
            // 调试日志（仅首次和每 1000 伤害打印一次，避免刷屏）
            if (attackerPc->totalDamageDealt <= finalDamage * 1.1f ||
                static_cast<int>(attackerPc->totalDamageDealt) % 1000 < static_cast<int>(finalDamage)) {
                LOG_INFO("[DPS追踪] 玩家造成 %.1f 伤害, 累计 %.1f", finalDamage, attackerPc->totalDamageDealt);
            }
        }
    }
    // 当目标是玩家时，记录最后攻击者（用于死亡回顾击杀者显示）
    if (info.target != kInvalidEntity) {
        PlayerComponent* targetPc = registry.GetComponent<PlayerComponent>(info.target);
        if (targetPc && info.attacker != kInvalidEntity) {
            targetPc->lastAttackerEntity = info.attacker;
        }
    }

    // ---- 应用击退 ----
    // 将击退向量叠加到目标速度，使目标被推开
    // 击退力度由 DamageInfo.knockback 的模决定
    if (info.knockback.x != 0.f || info.knockback.y != 0.f) {
        Velocity* vel = registry.GetComponent<Velocity>(info.target);
        if (vel) {
            vel->linear += info.knockback;
        }
    }

    // ---- 应用吸血 ----
    // 攻击者恢复 HP = damage × lifesteal
    if (info.lifesteal > 0.f && info.attacker != kInvalidEntity) {
        Health* attackerHealth = registry.GetComponent<Health>(info.attacker);
        Transform* attackerTransform = registry.GetComponent<Transform>(info.attacker);
        if (attackerHealth && attackerHealth->current > 0.f) {
            float heal = finalDamage * info.lifesteal;
            attackerHealth->current += heal;
            // 吸血不超过最大生命值
            if (attackerHealth->current > attackerHealth->max) {
                attackerHealth->current = attackerHealth->max;
            }
            // 回复血量绿色飘字 "+X"
            if (attackerTransform) {
                SpawnDamageText(registry, attackerTransform->position,
                                heal, false, sf::Color::Green, "+");
            }
        }
    }

    // ---- 触发回调 ----
    // OnHit：命中事件（无论是否击杀）
    if (OnHit) {
        OnHit(info);
    }

    // OnDamage：造成伤害事件
    if (OnDamage) {
        OnDamage(info);
    }

    // ---- 死亡判定 ----
    if (health->current <= 0.f) {
        ++killCount_;
        if (OnKill) {
            OnKill(info.target, info.attacker);
        }
    }
}

// ============================================================================
// ApplyStatus —— 添加状态效果
// ----------------------------------------------------------------------------
// 同类型状态效果不叠加，而是刷新（取较强者）：
//   - 若新状态持续时间更长，则更新持续时间
//   - 若新状态周期伤害更高，则更新伤害值
// ============================================================================
void CombatSystem::ApplyStatus(Registry& registry, EntityId target,
                                const StatusEffect& effect) {
    if (target == kInvalidEntity) return;

    // 确保目标有 StatusEffectComponent
    StatusEffectComponent* statusComp = registry.GetComponent<StatusEffectComponent>(target);
    if (!statusComp) {
        statusComp = &registry.AddComponent<StatusEffectComponent>(target);
    }

    // 查找是否已有同类型状态
    for (auto& existing : statusComp->effects) {
        if (existing.type == effect.type) {
            // 刷新：取较强者
            if (effect.duration > existing.remaining) {
                existing.remaining = effect.duration;
                existing.duration = effect.duration;
            }
            if (effect.tickDamage > existing.tickDamage) {
                existing.tickDamage = effect.tickDamage;
            }
            // 第十六轮修复：不再重置 existing.timer = effect.tickInterval
            // 原实现每次刷新都重置 tick 计时器，导致高频刷新源（如地刺每 0.5s
            // 刷新 Poison）会让 timer 永远无法减到 0，DoT 永远不触发伤害。
            // 现改为只刷新持续时间和伤害值，让 tick 按自然节奏触发。
            return;
        }
    }

    // 新增状态效果
    StatusEffectData data;
    data.type = effect.type;
    data.duration = effect.duration;
    data.remaining = effect.duration;
    data.tickDamage = effect.tickDamage;
    data.timer = effect.tickInterval;
    statusComp->effects.push_back(data);
}

// ============================================================================
// UpdateStatusEffects —— 更新所有状态效果
// ----------------------------------------------------------------------------
// 遍历所有拥有 StatusEffectComponent 的实体：
//   1. 推进每个状态的 remaining 计时器
//   2. 推进周期 timer，到 0 时施加 tickDamage 并重置
//   3. remaining <= 0 时移除该状态
//
// 周期伤害通过直接扣减 HP 实现（不触发 OnHit 回调，避免无限连锁）。
// ============================================================================
void CombatSystem::UpdateStatusEffects(Registry& registry, float dt) {
    registry.ForEach<StatusEffectComponent>([&](EntityId id) {
        StatusEffectComponent* statusComp = registry.GetComponent<StatusEffectComponent>(id);
        Health* health = registry.GetComponent<Health>(id);
        if (!statusComp || !health) return;
        // 仅对存活实体推进状态（已死亡的不应再受 DoT）
        if (health->current <= 0.f) return;

        // 逆序遍历以便安全删除过期状态
        for (int i = static_cast<int>(statusComp->effects.size()) - 1; i >= 0; --i) {
            auto& effect = statusComp->effects[i];
            effect.remaining -= dt;
            effect.timer -= dt;

            // 周期伤害（Fire/Poison 才有 tickDamage，Ice 不造成周期伤害）
            if (effect.timer <= 0.f && effect.tickDamage > 0.f) {
                if (health->current > 0.f) {
                    health->current -= effect.tickDamage;
                    if (health->current < 0.f) {
                        health->current = 0.f;
                    }

                    // ---- DoT 飘字反馈（第十六轮新增）----
                    // 此前 UpdateStatusEffects 直接扣 HP 无任何视觉反馈，
                    // 玩家只看到敌人血条下降却不知来源，体感"伤害凭空消失"。
                    // 现按元素类型生成不同颜色的飘字，与 CombatEffects 颜色规范一致：
                    //   Fire  燃烧：橙红 (255, 110, 50)
                    //   Poison 中毒：毒绿 (110, 200, 80)
                    Transform* targetTr = registry.GetComponent<Transform>(id);
                    if (targetTr) {
                        sf::Color dotColor = sf::Color::White;
                        if (effect.type == ElementType::Fire) {
                            dotColor = sf::Color(255, 110, 50);
                        } else if (effect.type == ElementType::Poison) {
                            dotColor = sf::Color(110, 200, 80);
                        }
                        SpawnDamageText(registry, targetTr->position,
                                        effect.tickDamage, false, dotColor);
                    }
                }
                // 重置周期计时器
                // 根据类型设置周期间隔：燃烧 0.5s，中毒 1s，其他 1s
                float interval = (effect.type == ElementType::Fire) ? 0.5f : 1.0f;
                effect.timer = interval;
            }

            // 状态过期，移除
            if (effect.remaining <= 0.f) {
                statusComp->effects.erase(statusComp->effects.begin() + i);
            }
        }
    });
}

// ============================================================================
// 暴击计算
// ============================================================================

bool CombatSystem::RollCritical(float critRate) noexcept {
    // rand() 返回 [0, RAND_MAX]，除以 RAND_MAX 得到 [0, 1]
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) < critRate;
}

float CombatSystem::CalculateCriticalDamage(float baseDamage,
                                             float critMultiplier) noexcept {
    return baseDamage * critMultiplier;
}

// ============================================================================
// GetComboDamageMultiplier —— 连击系统伤害乘数查表
// ----------------------------------------------------------------------------
// 阶梯设计原则：
//   - 前 10 击无加成，给玩家"起步"空间，避免一开始就过强
//   - 10/25/50 三个感知点，每个阶梯都有明显提升
//   - 100 击为硬上限，避免极端 combo 下破坏 Boss 战平衡
//   - 与暴击率/暴击伤害/吸血等装备词缀乘法累乘（不冲突）
//   - 与圣物"战士之证 +15% 伤害"乘法累乘（不冲突）
// ============================================================================
float CombatSystem::GetComboDamageMultiplier(int comboCount) noexcept {
    if (comboCount < 10)  return 1.00f;
    if (comboCount < 25)  return 1.20f;
    if (comboCount < 50)  return 1.35f;
    if (comboCount < 100) return 1.50f;
    return 1.75f;
}

// ============================================================================
// CreateElementalStatus —— 根据元素类型创建默认状态效果
// ----------------------------------------------------------------------------
// 不同元素的状态效果参数：
//   Fire:      持续 3s，每 0.5s 造成 20% 原始伤害（燃烧 DoT）
//   Ice:       持续 2s，无周期伤害（50% 减速由 AI 系统读取状态判断）
//   Lightning: 持续 0.6s，无周期伤害（麻痹：敌人完全无法移动/攻击）
//              —— 第十九轮新增：激活原本为 dead code 的 Lightning 元素状态，
//                 与 chainLightning 升级/雷霆圣物形成"闪电流"build 维度
//   Poison:    持续 5s，每 1s 造成 10% 原始伤害（中毒 DoT + 30% 减速）
//   Physical:  无状态效果
// ============================================================================
StatusEffect CombatSystem::CreateElementalStatus(ElementType elem,
                                                  float damage) noexcept {
    StatusEffect effect;
    effect.tickDamage = damage;

    switch (elem) {
        case ElementType::Fire:
            effect.type = ElementType::Fire;
            effect.duration = 3.f;
            effect.tickInterval = 0.5f;
            effect.tickDamage = damage * 0.2f; // 燃烧伤害 = 20% 原始伤害
            break;
        case ElementType::Ice:
            effect.type = ElementType::Ice;
            effect.duration = 2.f;
            effect.tickInterval = 2.f; // 不造成周期伤害
            effect.tickDamage = 0.f;
            break;
        case ElementType::Lightning:
            // 第十九轮新增：麻痹效果
            // 设计：短时间（0.6s）完全禁锢敌人，无法移动和攻击
            // 比 Ice（50% 减速）更强但持续时间更短，形成控制层次
            // 无周期伤害（伤害由 chain/hit 本身承担），tickInterval 设较大值避免误触
            effect.type = ElementType::Lightning;
            effect.duration = 0.6f;
            effect.tickInterval = 1.f; // 不造成周期伤害
            effect.tickDamage = 0.f;
            break;
        case ElementType::Poison:
            effect.type = ElementType::Poison;
            effect.duration = 5.f;
            effect.tickInterval = 1.f;
            effect.tickDamage = damage * 0.1f; // 中毒伤害 = 10% 原始伤害
            break;
        case ElementType::Physical:
        default:
            // 无持续状态
            effect.duration = 0.f;
            break;
    }

    return effect;
}

} // namespace cu
