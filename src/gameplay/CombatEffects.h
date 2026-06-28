#pragma once

// ============================================================================
// CombatEffects —— 战斗特效系统
// ----------------------------------------------------------------------------
// 职责：
//   1. SpawnHitEffect：命中特效（方向性喷射火花）
//   2. SpawnDeathEffect：死亡特效（多层环形爆炸）
//   3. SpawnDamageText：伤害飘字（浮动数字，暴击放大变黄）
//   4. UpdateDamageTexts：更新飘字位置与生命
//   5. RenderDamageTexts：渲染飘字（世界坐标 → 屏幕坐标）
//
// 伤害飘字设计：
//   - 使用 ECS DamageTextComponent + Transform 组件
//   - 在世界坐标显示，向上漂浮 + 淡出
//   - 暴击：字体放大 1.5x，颜色变黄
//   - 普通：白色，正常大小
//   - 生命周期 0.8s，结束后自动销毁实体
//
// 命中特效增强：
//   - 原 HitSpark 为径向均匀喷射
//   - 增强为方向性喷射：粒子主要朝子弹飞行反方向喷射（模拟撞击飞溅）
//
// 死亡特效增强：
//   - 原 Explosion 为单层径向扩散
//   - 增强为多层环形：先小范围快速扩散，再大范围慢速扩散
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics.hpp>
#include "ecs/Entity.h"

namespace cu {

class Registry;
class ParticleSystem;
class Camera;
class Renderer;

// ---- 飘字渲染参数 ----
inline constexpr float kDamageTextLifetime = 0.8f;     // 飘字生命（秒）
inline constexpr float kDamageTextSpeed = 80.f;        // 上浮速度（像素/秒）
inline constexpr int kDamageTextFontSize = 16;         // 普通伤害字号
inline constexpr int kDamageTextCritFontSize = 24;     // 暴击伤害字号

// ============================================================================
// 特效生成函数
// ============================================================================

// 命中特效：方向性喷射火花
// pos: 命中位置
// dir: 子弹飞行方向（粒子朝反方向喷射）
void SpawnHitEffect(ParticleSystem& particles,
                    sf::Vector2f pos, sf::Vector2f dir);

// 死亡特效：多层环形爆炸
// pos: 死亡位置
// color: 敌人颜色（爆炸粒子着色）
void SpawnDeathEffect(ParticleSystem& particles,
                      sf::Vector2f pos, sf::Color color);

// 伤害飘字：浮动数字
// pos: 显示位置（世界坐标，通常在目标头顶）
// amount: 伤害数值
// isCritical: 是否暴击（暴击放大变黄）
// color: 自定义颜色（默认白色）。红色=受伤，绿色=回复，金色=经验
// prefix: 文本前缀（如 "+EXP"、"-"），默认空
void SpawnDamageText(Registry& registry,
                     sf::Vector2f pos, float amount, bool isCritical,
                     sf::Color color = sf::Color::White,
                     const std::string& prefix = "");

// 通用飘字：自定义文本和颜色（用于经验、回复等）
// pos: 显示位置（世界坐标）
// text: 显示文本（如 "+EXP 10"、"+5 HP"）
// color: 颜色
// fontSize: 字号
// lifetime: 生命周期（秒），默认 0.8s，物品拾取建议 2.5s
void SpawnFloatText(Registry& registry,
                    sf::Vector2f pos, const std::string& text,
                    sf::Color color, int fontSize = 16,
                    float lifetime = 0.8f);

// ============================================================================
// 飘字更新与渲染
// ============================================================================

// 更新所有伤害飘字（每固定步长调用）
// 推进位置（上浮）与生命衰减，到期销毁实体
void UpdateDamageTexts(Registry& registry, float dt);

// 渲染所有伤害飘字
// 使用 sf::Text 在屏幕空间绘制，世界坐标 → 屏幕坐标转换由 Camera 完成
void RenderDamageTexts(Registry& registry, sf::RenderWindow& window,
                       const Camera& camera, const sf::Font& font);

} // namespace cu
