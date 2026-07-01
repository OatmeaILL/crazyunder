#pragma once

// ============================================================================
// ClassSystem —— 职业系统
// ----------------------------------------------------------------------------
// 职责：
//   1. 定义职业枚举和职业数据
//   2. 提供职业名称、描述、颜色、基础数值查询
//   3. 根据职业初始化 PlayerStats 的基础属性
//
// 职业设计：
//   法师 (Mage) —— 远程弹幕输出，高蓝量低生命，均衡攻速
//   剑士 (Warrior) —— 近战范围攻击，高生命低蓝量，高伤害低攻速
//
// 数值分配对比：
//   | 属性     | 法师        | 剑士        |
//   |---------|------------|------------|
//   | maxHp   | 80         | 120        |
//   | maxMp   | 60         | 25         |
//   | damage  | 10         | 14         |
//   | attackSpeed | 2.0    | 1.5        |
//   | moveSpeed | 195      | 205        |
//   | critChance | 0.12   | 0.08       |
//   | critDamage | 1.4    | 1.6        |
//   | defense  | 0          | 3          |
// ============================================================================

#include <string>
#include <SFML/Graphics.hpp>
#include "gameplay/PlayerClassTypes.h"
#include "gameplay/Player.h"

namespace cu {

// ---- 职业数据 ----
struct ClassData {
    const char* name;           // 职业中文名
    const char* description;    // 职业描述（用于选择界面）
    const char* weaponName;     // 武器名称
    sf::Color themeColor;       // 主题色（UI/粒子/攻击特效）
    sf::Color projectileColor;  // 攻击特效颜色
    float maxHp;                // 基础最大生命
    float maxMp;                // 基础最大法力
    float damage;               // 基础伤害
    float attackSpeed;          // 基础攻速（次/秒）
    float moveSpeed;            // 基础移速（像素/秒）
    float critChance;           // 基础暴击率
    float critDamage;           // 基础暴击伤害
    float defense;              // 基础防御
};

// ============================================================================
// GetClassData —— 获取职业数据
// ============================================================================
[[nodiscard]] const ClassData& GetClassData(PlayerClass cls) noexcept;

// ============================================================================
// GetClassName —— 获取职业中文名
// ============================================================================
[[nodiscard]] const char* GetClassName(PlayerClass cls) noexcept;

// ============================================================================
// ApplyClassBaseStats —— 根据职业设置 PlayerStats 基础属性
// ----------------------------------------------------------------------------
// 在 recomputePlayerStats 的第一步调用，替代硬编码的基础值。
// 注意：仅设置基础属性，不覆盖升级/装备/圣物等叠加层。
// ============================================================================
void ApplyClassBaseStats(PlayerStats& stats, PlayerClass cls);

// ============================================================================
// IsMeleeClass —— 判断是否为近战职业
// ============================================================================
[[nodiscard]] inline bool IsMeleeClass(PlayerClass cls) noexcept {
    return cls == PlayerClass::Warrior;
}

// ============================================================================
// IsRangedClass —— 判断是否为远程职业
// ============================================================================
[[nodiscard]] inline bool IsRangedClass(PlayerClass cls) noexcept {
    return cls == PlayerClass::Mage;
}

} // namespace cu
