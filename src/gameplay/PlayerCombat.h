#pragma once

// ============================================================================
// PlayerCombat —— 玩家攻击与技能系统
// ----------------------------------------------------------------------------
// 职责：
//   1. 普攻（左键）：朝鼠标方向发射子弹
//   2. 闪避（右键）：短距离冲刺，无敌帧 0.3s
//   3. AOE 技能（空格）：范围伤害 + 粒子特效 + 屏幕震动
//
// 攻击间隔：
//   interval = 1 / attackSpeed
//   attackSpeed=2 → 每 0.5s 攻击一次
//   attackSpeed 受 buff/装备影响可动态变化
//
// 闪避机制：
//   - 冷却 2s
//   - 冲刺方向：当前移动方向（WASD），若静止则朝鼠标方向
//   - 冲刺速度 = moveSpeed × 3（持续 0.15s）
//   - 无敌帧 0.3s（冲刺期间 + 结束后短暂无敌）
//
// AOE 技能：
//   - 冷却 8s
//   - 以玩家为中心半径 200px 范围伤害
//   - 伤害 = playerStats.damage × 3
//   - 触发 Explosion 粒子 + 屏幕震动（magnitude=8, duration=0.3s）
//   - 对范围内所有敌人施加击退
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include "ecs/Entity.h"

namespace cu {

class Registry;
class Input;
class Camera;
class ProjectileSystem;
class ParticleSystem;
class UniformGrid;
class CombatSystem;
struct Dungeon;

// ============================================================================
// UpdatePlayerCombat —— 更新玩家战斗逻辑（每固定步长调用）
// ----------------------------------------------------------------------------
// 处理流程：
//   1. 衰减所有冷却计时器
//   2. 处理闪避冲刺（若在冲刺中，应用冲刺速度）
//   3. 普攻：左键按下且冷却结束 → 发射子弹
//   4. 闪避：右键按下且冷却结束 → 触发冲刺
//   5. AOE：空格按下且冷却结束 → 范围伤害 + 特效
//
// 参数：
//   registry: ECS 注册表
//   input: 输入管理器
//   player: 玩家实体 ID
//   projectiles: 弹幕系统（用于发射子弹）
//   particles: 粒子系统（用于特效）
//   camera: 摄像机（用于鼠标坐标转换 + 屏幕震动）
//   grid: 空间网格（用于 AOE 范围查询）
//   combat: 战斗系统（用于施加伤害）
//   dt: 固定步长时间（秒）
// ============================================================================
void UpdatePlayerCombat(Registry& registry, const Input& input,
                        EntityId player, ProjectileSystem& projectiles,
                        ParticleSystem& particles, Camera& camera,
                        UniformGrid& grid, CombatSystem& combat,
                        Dungeon* dungeon, float dt);

// ---- 战斗参数常量 ----
inline constexpr float kDodgeCooldown = 1.5f;        // 闪避冷却（秒）
inline constexpr float kDodgeInvincibility = 0.35f;   // 闪避无敌帧（秒）
inline constexpr float kDodgeDashDuration = 0.22f;   // 冲刺持续时间（秒）
inline constexpr float kDodgeSpeedMultiplier = 4.0f; // 冲刺速度倍率

inline constexpr float kAOECooldown = 8.0f;          // AOE 冷却（秒）
inline constexpr float kAOERadius = 200.f;           // AOE 半径（像素）
inline constexpr float kAOEDamageMultiplier = 3.0f;  // AOE 伤害倍率
inline constexpr float kAOEShakeMagnitude = 8.f;     // AOE 屏幕震动幅度
inline constexpr float kAOEShakeDuration = 0.3f;     // AOE 屏幕震动持续时间

inline constexpr float kDefaultCritRate = 0.15f;     // 默认暴击率 15%
inline constexpr float kDefaultCritMultiplier = 1.5f; // 默认暴击伤害倍率

} // namespace cu
