#include "gameplay/CombatEffects.h"
#include "rendering/ParticleSystem.h"
#include "rendering/Camera.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "utils/Logger.h"
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <iomanip>

namespace cu {

// ============================================================================
// SpawnHitEffect —— 命中特效（方向性喷射火花）
// ----------------------------------------------------------------------------
// 增强原 HitSpark：粒子主要朝子弹飞行反方向喷射，模拟撞击飞溅。
// 同时保留少量径向粒子，增加视觉丰富度。
// ============================================================================
void SpawnHitEffect(ParticleSystem& particles,
                    sf::Vector2f pos, sf::Vector2f dir) {
    // 1. 方向性喷射：朝子弹反方向喷射 10 个粒子
    EmitConfig dirCfg;
    dirCfg.radial = false;

    // 计算反方向角度
    float angle = std::atan2(dir.y, dir.x);
    // 反方向 ± 60 度的扇形
    float spread = 60.f * 3.14159265f / 180.f;
    float baseVx = -dir.x * 200.f; // 反方向速度
    float baseVy = -dir.y * 200.f;

    dirCfg.velocityMin = sf::Vector2f(
        baseVx - std::cos(spread) * 100.f,
        baseVy - std::sin(spread) * 100.f
    );
    dirCfg.velocityMax = sf::Vector2f(
        baseVx + std::cos(spread) * 100.f,
        baseVy + std::sin(spread) * 100.f
    );
    dirCfg.colorMin = sf::Color(255, 255, 200, 255);
    dirCfg.colorMax = sf::Color(255, 200, 50, 255);
    dirCfg.sizeMin = 2.f;
    dirCfg.sizeMax = 5.f;
    dirCfg.lifeMin = 0.15f;
    dirCfg.lifeMax = 0.35f;
    particles.Emit(pos, 10, dirCfg);

    // 2. 径向火花：保留少量径向粒子（原 HitSpark 效果）
    particles.HitSpark(pos);
}

// ============================================================================
// SpawnDeathEffect —— 死亡特效（多层环形爆炸）
// ----------------------------------------------------------------------------
// 增强原 Explosion：
//   第一层：小范围快速扩散（模拟核心爆炸）
//   第二层：大范围慢速扩散（模拟冲击波）
//   颜色基于敌人颜色，混合橙红色
// ============================================================================
void SpawnDeathEffect(ParticleSystem& particles,
                      sf::Vector2f pos, sf::Color color) {
    // 第一层：核心快速爆炸（20 个粒子，高速短命）
    EmitConfig coreCfg;
    coreCfg.radial = true;
    coreCfg.speedMin = 150.f;
    coreCfg.speedMax = 300.f;
    // 混合敌人颜色与橙红色
    coreCfg.colorMin = sf::Color(
        static_cast<sf::Uint8>((color.r + 255) / 2),
        static_cast<sf::Uint8>((color.g + 150) / 2),
        static_cast<sf::Uint8>((color.b + 50) / 2),
        255
    );
    coreCfg.colorMax = sf::Color(255, 100, 0, 255);
    coreCfg.sizeMin = 3.f;
    coreCfg.sizeMax = 7.f;
    coreCfg.lifeMin = 0.3f;
    coreCfg.lifeMax = 0.6f;
    particles.Emit(pos, 20, coreCfg);

    // 第二层：冲击波慢速扩散（25 个粒子，低速长命）
    EmitConfig waveCfg;
    waveCfg.radial = true;
    waveCfg.speedMin = 50.f;
    waveCfg.speedMax = 120.f;
    waveCfg.colorMin = sf::Color(
        static_cast<sf::Uint8>(color.r * 0.7f),
        static_cast<sf::Uint8>(color.g * 0.7f),
        static_cast<sf::Uint8>(color.b * 0.7f),
        200
    );
    waveCfg.colorMax = sf::Color(
        static_cast<sf::Uint8>((color.r + 200) / 2),
        static_cast<sf::Uint8>((color.g + 200) / 2),
        static_cast<sf::Uint8>((color.b + 200) / 2),
        255
    );
    waveCfg.sizeMin = 4.f;
    waveCfg.sizeMax = 9.f;
    waveCfg.lifeMin = 0.5f;
    waveCfg.lifeMax = 1.0f;
    particles.Emit(pos, 25, waveCfg);

    // 额外触发标准 Explosion（增加视觉冲击）
    particles.Explosion(pos);
}

// ============================================================================
// SpawnDamageText —— 生成伤害飘字
// ----------------------------------------------------------------------------
// 创建一个 ECS 实体，挂载 DamageTextComponent + Transform。
// 暴击时颜色变黄、字号放大；普通伤害为白色。
// 支持自定义颜色（红色=受伤，绿色=回复，金色=经验）和前缀。
// ============================================================================
void SpawnDamageText(Registry& registry,
                     sf::Vector2f pos, float amount, bool isCritical,
                     sf::Color color,
                     const std::string& prefix) {
    EntityId id = registry.CreateEntity();

    // Transform：在目标头顶偏上 20px 位置显示
    auto& transform = registry.AddComponent<Transform>(id);
    transform.position = sf::Vector2f(pos.x, pos.y - 20.f);

    // DamageTextComponent
    auto& dmgText = registry.AddComponent<DamageTextComponent>(id);
    dmgText.amount = amount;
    dmgText.isCritical = isCritical;
    dmgText.lifetime = kDamageTextLifetime;
    dmgText.maxLifetime = kDamageTextLifetime;
    dmgText.velocity = sf::Vector2f(0.f, -kDamageTextSpeed);
    dmgText.prefix = prefix;

    // 颜色优先级：自定义颜色 > 暴击黄色 > 普通白色
    if (color != sf::Color::White) {
        dmgText.color = color;
    } else if (isCritical) {
        dmgText.color = sf::Color(255, 220, 0, 255); // 暴击黄色
    } else {
        dmgText.color = sf::Color::White;
    }

    // Tag 标记（可选，便于查询）
    registry.AddComponent<Tag>(id).flags = TagFlag::Particle;
}

// ============================================================================
// SpawnFloatText —— 通用飘字（自定义文本和颜色）
// ----------------------------------------------------------------------------
// 支持自定义生命周期（物品拾取建议 2.5s）和淡入效果
// ============================================================================
void SpawnFloatText(Registry& registry,
                    sf::Vector2f pos, const std::string& text,
                    sf::Color color, int fontSize, float lifetime) {
    EntityId id = registry.CreateEntity();

    auto& transform = registry.AddComponent<Transform>(id);
    transform.position = sf::Vector2f(pos.x, pos.y - 20.f);

    auto& dmgText = registry.AddComponent<DamageTextComponent>(id);
    dmgText.amount = 0.f;
    dmgText.isCritical = false;
    dmgText.lifetime = lifetime;
    dmgText.maxLifetime = lifetime;
    dmgText.velocity = sf::Vector2f(0.f, -kDamageTextSpeed);
    dmgText.color = color;
    dmgText.prefix = text;
    dmgText.fontSize = fontSize;
    dmgText.fadeIn = true;  // 启用淡入效果

    registry.AddComponent<Tag>(id).flags = TagFlag::Particle;
}

// ============================================================================
// UpdateDamageTexts —— 更新伤害飘字
// ----------------------------------------------------------------------------
// 遍历所有 DamageTextComponent 实体：
//   1. 位置 += velocity × dt（向上漂浮）
//   2. 生命 -= dt
//   3. 到期销毁实体
// ============================================================================
void UpdateDamageTexts(Registry& registry, float dt) {
    std::vector<EntityId> toDestroy;

    registry.ForEach<DamageTextComponent, Transform>([&](EntityId id) {
        DamageTextComponent* dmgText = registry.GetComponent<DamageTextComponent>(id);
        Transform* transform = registry.GetComponent<Transform>(id);
        if (!dmgText || !transform) return;

        // 上浮
        transform->position += dmgText->velocity * dt;

        // 生命衰减
        dmgText->lifetime -= dt;
        if (dmgText->lifetime <= 0.f) {
            toDestroy.push_back(id);
        }
    });

    // 销毁过期飘字
    for (EntityId id : toDestroy) {
        registry.DestroyEntity(id);
    }
}

// ============================================================================
// RenderDamageTexts —— 渲染伤害飘字
// ----------------------------------------------------------------------------
// 遍历所有 DamageTextComponent 实体，将世界坐标转为屏幕坐标后绘制。
// 使用 sf::Text 绘制，每个飘字单独 draw（数量少，<100，性能可接受）。
//
// 淡出效果：alpha = (lifetime / maxLifetime) × 255
// 暴击放大：字号 × 1.5
// ============================================================================
void RenderDamageTexts(Registry& registry, sf::RenderWindow& window,
                       const Camera& camera, const sf::Font& font) {

    registry.ForEach<DamageTextComponent, Transform>([&](EntityId id) {
        DamageTextComponent* dmgText = registry.GetComponent<DamageTextComponent>(id);
        Transform* transform = registry.GetComponent<Transform>(id);
        if (!dmgText || !transform) return;

        // 世界坐标 → 屏幕坐标
        sf::Vector2f screenPos = camera.WorldToScreen(transform->position);

        // 计算淡入淡出比例
        // ratio = lifetime / maxLifetime（1.0 → 0.0）
        float ratio = dmgText->lifetime / dmgText->maxLifetime;
        if (ratio < 0.f) ratio = 0.f;
        if (ratio > 1.f) ratio = 1.f;

        // alpha 计算：
        // - fadeIn=true：前 20% 时间淡入(0→255)，中间 50% 全显示，后 30% 淡出(255→0)
        // - fadeIn=false：仅淡出（原行为），alpha = ratio × 255
        float alphaRatio;
        if (dmgText->fadeIn) {
            float elapsed = 1.f - ratio;  // 已过时间比例 (0.0 → 1.0)
            if (elapsed < 0.2f) {
                // 淡入阶段：0 → 1
                alphaRatio = elapsed / 0.2f;
            } else if (elapsed < 0.7f) {
                // 全显示阶段
                alphaRatio = 1.f;
            } else {
                // 淡出阶段：1 → 0
                alphaRatio = (1.f - elapsed) / 0.3f;
            }
        } else {
            // 仅淡出
            alphaRatio = ratio;
        }

        // 字号：自定义 > 暴击放大 > 普通
        int fontSize = kDamageTextFontSize;
        if (dmgText->fontSize > 0) {
            fontSize = dmgText->fontSize;
        } else if (dmgText->isCritical) {
            fontSize = kDamageTextCritFontSize;
        }

        // 颜色：应用淡入淡出
        sf::Color color = dmgText->color;
        color.a = static_cast<sf::Uint8>(255.f * alphaRatio);

        // 构造文本
        sf::Text text;
        text.setFont(font);
        text.setCharacterSize(fontSize);
        text.setFillColor(color);

        // 格式化文本：prefix 优先，否则显示伤害数字
        std::ostringstream oss;
        if (!dmgText->prefix.empty()) {
            // 有前缀时：prefix + 数值（如 "+EXP 10"）或纯文本（如 "+5 HP"）
            if (dmgText->amount > 0.f) {
                oss << dmgText->prefix << " " << static_cast<int>(dmgText->amount + 0.5f);
            } else {
                oss << dmgText->prefix;
            }
        } else {
            // 无前缀：显示伤害数字
            if (dmgText->isCritical) {
                oss << "!";
            }
            int intDamage = static_cast<int>(dmgText->amount + 0.5f);
            oss << intDamage;
        }
        // 使用 UTF-8 转换，避免中文乱码（SFML 默认 ANSI 解码无法处理多字节 UTF-8）
        text.setString(utf8ToSfString(oss.str()));

        // 居中对齐
        sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin(bounds.left + bounds.width * 0.5f,
                       bounds.top + bounds.height * 0.5f);
        text.setPosition(screenPos);

        // 暴击加粗
        if (dmgText->isCritical) {
            text.setStyle(sf::Text::Bold);
        }

        window.draw(text);
    });
}

} // namespace cu
