#pragma once

// ============================================================================
// SkillSystem —— 技能系统
// ----------------------------------------------------------------------------
// 职责：
//   1. 定义 5 种技能类型及其数据（名称、冷却、描述）
//   2. 处理技能释放逻辑（伤害、击退、buff、区域效果）
//   3. 管理技能槽位与技能背包
//   4. 更新持续技能效果（引力井拉扯、地刺伤害、狂暴计时）
//
// 技能列表：
//   GroundSlam   震地波：范围伤害+击退+破甲，CD 7s
//   LeechStrike  吸血打击：下次攻击1.5x伤害+30%吸血，CD 11s
//   Berserk      狂暴：+50%攻击+30%移速-20%防御，持续6s，CD 20s
//   GravityWell  引力井：拉扯周围敌人4s，CD 15s
//   SpikeGround  地刺：地面伤害区域5s，减速+持续伤害，CD 12s
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include <array>
#include <cstdint>
#include "ecs/Entity.h"

namespace cu {

class Registry;
class ParticleSystem;
class Camera;
class UniformGrid;
class CombatSystem;
struct Dungeon;

// ---- 技能类型枚举 ----
enum class SkillType : uint8_t {
    GroundSlam   = 0,  // 震地波
    LeechStrike  = 1,  // 吸血打击
    Berserk      = 2,  // 狂暴
    GravityWell  = 3,  // 引力井
    SpikeGround  = 4,  // 地刺
    Count        = 5   // 哨兵/空槽位
};

// ---- 技能静态数据 ----
struct SkillData {
    SkillType  type        = SkillType::Count;
    const char* name       = "";       // 中文名
    const char* desc       = "";       // 描述
    float      cooldown    = 0.f;      // 冷却时间（秒）
    float      duration    = 0.f;      // 持续时间（0=瞬发）
    float      manaCost    = 0.f;      // 法力消耗
};

// ---- 技能实例（挂载在 PlayerComponent 中）----
struct SkillInstance {
    SkillType type           = SkillType::Count; // Count=空槽
    float     cooldownRemain = 0.f;              // 冷却剩余（秒）
    int       level          = 1;                // 技能等级（1-3，重复获取升级）
};

// ---- 技能系统常量 ----
inline constexpr int kSkillSlotCount    = 4;  // 技能槽位数（按键1-4）
inline constexpr int kSkillBackpackSize = 5;  // 技能背包容量
inline constexpr int kSkillMaxLevel     = 3;  // 技能最高等级

// ---- 获取技能静态数据 ----
[[nodiscard]] const SkillData& GetSkillData(SkillType type);

// ---- 获取技能中文名 ----
[[nodiscard]] const char* GetSkillName(SkillType type);

// ---- 释放技能 ----
// 返回 true 表示成功释放（冷却已就绪）
bool ExecuteSkill(Registry& registry, EntityId player,
                  SkillType skill, ParticleSystem& particles,
                  Camera& camera, UniformGrid& grid,
                  CombatSystem& combat, const Dungeon* dungeon);

// ---- 更新持续技能效果（每帧调用）----
void UpdateSkillBuffs(Registry& registry, EntityId player,
                      UniformGrid& grid, CombatSystem& combat,
                      ParticleSystem& particles, float dt);

// ---- 技能槽管理 ----
// 装备技能：从背包索引装备到技能槽
bool EquipSkill(struct PlayerComponent& pc, int backpackIndex, int slotIndex);
// 卸下技能：从技能槽卸到背包
bool UnequipSkill(struct PlayerComponent& pc, int slotIndex);
// 添加技能到背包
bool AddSkillToBackpack(struct PlayerComponent& pc, SkillType type);
// 背包是否已满
[[nodiscard]] bool IsSkillBackpackFull(const struct PlayerComponent& pc);
// 玩家是否已拥有某技能（槽位+背包）
[[nodiscard]] bool PlayerHasSkill(const struct PlayerComponent& pc, SkillType type);
// 获取玩家某技能的当前等级（未拥有返回 0）
[[nodiscard]] int GetSkillLevel(const struct PlayerComponent& pc, SkillType type);
// 升级玩家已拥有的技能（level++，返回 true 表示成功升级）
bool UpgradeSkillLevel(struct PlayerComponent& pc, SkillType type);

} // namespace cu
