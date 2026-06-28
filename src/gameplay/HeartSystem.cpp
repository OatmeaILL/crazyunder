#include "gameplay/HeartSystem.h"
#include "gameplay/Player.h"
#include "gameplay/CombatEffects.h"
#include "core/AudioManager.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "rendering/Renderer.h"
#include "utils/Logger.h"
#include <cstdlib>
#include <cmath>

namespace cu {

// ============================================================================
// HeartSystem 构造与初始化
// ============================================================================
HeartSystem::HeartSystem() {
    Initialize();
}

void HeartSystem::Initialize() {
    hearts_.resize(kPoolCapacity);
    freeList_.reserve(kPoolCapacity);
    for (int i = 0; i < kPoolCapacity; ++i) {
        hearts_[i].active = false;
        freeList_.push_back(kPoolCapacity - 1 - i);
    }
    LOG_INFO("HeartSystem 已初始化: 池容量=%d", kPoolCapacity);
}

// ============================================================================
// 随机数辅助
// ============================================================================
float HeartSystem::randomFloat(float min, float max) const {
    return min + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (max - min);
}

// ============================================================================
// Spawn  生成爱心
// ============================================================================
void HeartSystem::Spawn(sf::Vector2f pos) {
    if (freeList_.empty()) {
        LOG_WARN("爱心池已满，无法生成");
        return;
    }

    int idx = freeList_.back();
    freeList_.pop_back();

    HeartData& heart = hearts_[idx];
    heart.position = pos;
    // 随机散射速度（模拟掉落弹跳效果）
    float angle = randomFloat(0.f, 6.2831853f);
    float speed = randomFloat(30.f, 60.f);
    heart.velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
    heart.active = true;
    heart.lifetime = 15.f;
    heart.magnetTimer = 0.f;
}

// ============================================================================
// Update  更新爱心
// ============================================================================
void HeartSystem::Update(Registry& registry, EntityId player,
                         PlayerComponent* playerComp, float dt) {
    Transform* playerTransform = registry.GetComponent<Transform>(player);
    if (!playerTransform) return;

    sf::Vector2f playerPos = playerTransform->position;

    for (int i = 0; i < kPoolCapacity; ++i) {
        HeartData& heart = hearts_[i];
        if (!heart.active) continue;

        // 生命周期衰减
        heart.lifetime -= dt;
        if (heart.lifetime <= 0.f) {
            heart.active = false;
            freeList_.push_back(i);
            continue;
        }

        // 计算到玩家的距离
        sf::Vector2f toPlayer = playerPos - heart.position;
        float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;
        float dist = std::sqrt(distSq);

        // 磁吸效果：生成后 0.15s 散射，随后立即磁吸
        heart.magnetTimer += dt;
        if (heart.magnetTimer > 0.15f && dist > 0.001f) {
            float magnetSpeed = 250.f;
            if (dist < 100.f) {
                magnetSpeed += (1.f - dist / 100.f) * 400.f;
            }
            sf::Vector2f magnetDir = toPlayer / dist;
            heart.velocity = magnetDir * magnetSpeed;
        } else {
            // 散射阶段：速度衰减
            heart.velocity *= (1.f - 3.f * dt);
        }

        // 应用速度
        heart.position += heart.velocity * dt;

        // 拾取检测：距离 < kPickupRadius
        if (dist < kPickupRadius) {
            if (playerComp) {
                // 回复 2% 最大生命
                Health* playerHealth = registry.GetComponent<Health>(player);
                if (playerHealth) {
                    float healAmount = playerHealth->max * 0.02f;
                    playerHealth->current += healAmount;
                    if (playerHealth->current > playerHealth->max) {
                        playerHealth->current = playerHealth->max;
                    }
                    // 绿色回血飘字
                    SpawnFloatText(registry, heart.position,
                                   "+" + std::to_string(static_cast<int>(healAmount)),
                                   sf::Color(100, 255, 100), 16, 1.0f);
                }
                AudioManager::Instance().PlaySFX(AudioManager::kSFXPickup);
            }
            heart.active = false;
            freeList_.push_back(i);
        }
    }
}

// ============================================================================
// Render  渲染爱心（红色发光心形）
// ============================================================================
void HeartSystem::Render(Renderer& renderer) {
    for (int i = 0; i < kPoolCapacity; ++i) {
        const HeartData& heart = hearts_[i];
        if (!heart.active) continue;

        // 生命周期接近结束时闪烁
        float alpha = 255.f;
        if (heart.lifetime < 3.f) {
            alpha = 128.f + 127.f * std::sin(heart.lifetime * 20.f);
        }

        // 外圈光晕（半透明红色）
        renderer.DrawQuad(heart.position, sf::Vector2f(14.f, 14.f),
                          sf::Color(255, 80, 100, static_cast<uint8_t>(alpha * 0.4f)),
                          Layer::Entity);
        // 核心（亮红色发光）
        renderer.DrawQuad(heart.position, sf::Vector2f(7.f, 7.f),
                          sf::Color(255, 120, 140, static_cast<uint8_t>(alpha)),
                          Layer::Entity);
    }
}

} // namespace cu
