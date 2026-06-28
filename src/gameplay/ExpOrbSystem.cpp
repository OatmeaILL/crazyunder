#include "gameplay/ExpOrbSystem.h"
#include "gameplay/UpgradeSystem.h"
#include "gameplay/CombatEffects.h"
#include "core/AudioManager.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "rendering/Renderer.h"
#include "utils/Logger.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace cu {

// ============================================================================
// ExpOrbSystem 构造与初始化
// ============================================================================
ExpOrbSystem::ExpOrbSystem() {
    Initialize();
}

void ExpOrbSystem::Initialize() {
    orbs_.resize(kPoolCapacity);
    freeList_.reserve(kPoolCapacity);
    // 所有槽位初始为空闲，按 LIFO 入栈
    for (int i = 0; i < kPoolCapacity; ++i) {
        orbs_[i].active = false;
        freeList_.push_back(kPoolCapacity - 1 - i);
    }
    LOG_INFO("ExpOrbSystem 已初始化: 池容量=%d", kPoolCapacity);
}

// ============================================================================
// 随机数辅助
// ============================================================================
float ExpOrbSystem::randomFloat(float min, float max) const {
    return min + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (max - min);
}

// ============================================================================
// Spawn  生成经验球
// ----------------------------------------------------------------------------
// 从对象池获取一个空闲槽位，初始化经验球数据
// 生成时给一个小随机速度，模拟爆裂效果
// ============================================================================
void ExpOrbSystem::Spawn(sf::Vector2f pos, int value) {
    if (freeList_.empty()) {
        LOG_WARN("经验球池已满，无法生成");
        return;
    }

    int idx = freeList_.back();
    freeList_.pop_back();

    ExpOrbData& orb = orbs_[idx];
    orb.position = pos;
    // 随机散射速度（模拟敌人死亡爆裂，但速度较小）
    float angle = randomFloat(0.f, 6.2831853f);
    float speed = randomFloat(20.f, 50.f);
    orb.velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
    orb.value = value;
    orb.active = true;
    orb.lifetime = 10.f;
    orb.magnetTimer = 0.f;

    // 触发生成回调（粒子特效）
    if (OnOrbSpawned) OnOrbSpawned(pos);
}

// ============================================================================
// Update  更新经验球
// ----------------------------------------------------------------------------
// 算法：
//   1. 遍历所有活跃经验球
//   2. 衰减生命周期，超时则回收
//   3. 计算到玩家的距离
//   4. 距离 < pickupRange 时触发磁吸：速度朝向玩家加速
//   5. 距离 < kPickupRadius 时拾取：调用 AddExp，回收
//   6. 应用速度衰减（散射后逐渐减速）
// ============================================================================
void ExpOrbSystem::Update(Registry& registry, EntityId player,
                          UpgradeSystem& upgrade, float pickupRange,
                          float expMultiplier, float dt) {
    Transform* playerTransform = registry.GetComponent<Transform>(player);
    if (!playerTransform) return;

    sf::Vector2f playerPos = playerTransform->position;

    for (int i = 0; i < kPoolCapacity; ++i) {
        ExpOrbData& orb = orbs_[i];
        if (!orb.active) continue;

        // 生命周期衰减
        orb.lifetime -= dt;
        if (orb.lifetime <= 0.f) {
            orb.active = false;
            freeList_.push_back(i);
            continue;
        }

        // 计算到玩家的距离
        sf::Vector2f toPlayer = playerPos - orb.position;
        float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;
        float dist = std::sqrt(distSq);

        // 磁吸效果：始终激活（不再依赖 pickupRange）
        // 生成后短暂散射 0.15s，随后立即开始向玩家磁吸，确保击杀后迅速获得经验
        orb.magnetTimer += dt;
        if (orb.magnetTimer > 0.15f && dist > 0.001f) {
            // 磁吸速度：距离越近速度越快，远处也有基础速度保证回收
            float ratio = (dist < pickupRange) ? (1.f - dist / pickupRange) : 0.f;
            float magnetSpeed = 250.f + ratio * 450.f;
            sf::Vector2f magnetDir = toPlayer / dist;
            orb.velocity = magnetDir * magnetSpeed;
        } else {
            // 散射阶段：速度衰减（爆裂后逐渐减速）
            orb.velocity *= (1.f - 3.f * dt);
        }

        // 应用速度
        orb.position += orb.velocity * dt;

        // 拾取检测：距离 < kPickupRadius
        if (dist < kPickupRadius) {
            int expGained = static_cast<int>(orb.value * expMultiplier);
            upgrade.AddExp(expGained);
            AudioManager::Instance().PlaySFX(AudioManager::kSFXExpPickup);
            // 经验获取亮绿色飘字 "+EXP 10"
            // 使用亮绿色 (50,255,50) 与回复血量纯绿 (0,255,0) 区分
            SpawnDamageText(registry, orb.position,
                            static_cast<float>(expGained), false,
                            sf::Color(50, 255, 50, 255), "+EXP");
            orb.active = false;
            freeList_.push_back(i);
        }
    }
}

// ============================================================================
// Render  渲染经验球（绿色发光圆点）
// ============================================================================
void ExpOrbSystem::Render(Renderer& renderer) {
    for (int i = 0; i < kPoolCapacity; ++i) {
        const ExpOrbData& orb = orbs_[i];
        if (!orb.active) continue;

        // 生命周期接近结束时闪烁
        float alpha = 255.f;
        if (orb.lifetime < 2.f) {
            alpha = 128.f + 127.f * std::sin(orb.lifetime * 20.f);
        }

        // 外圈光晕（半透明绿色）
        renderer.DrawQuad(orb.position, sf::Vector2f(12.f, 12.f),
                          sf::Color(50, 255, 50, static_cast<uint8_t>(alpha * 0.4f)),
                          Layer::Entity);
        // 核心（亮绿色发光圆点）
        renderer.DrawQuad(orb.position, sf::Vector2f(6.f, 6.f),
                          sf::Color(180, 255, 180, static_cast<uint8_t>(alpha)),
                          Layer::Entity);
    }
}

} // namespace cu