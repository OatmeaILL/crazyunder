#include "gameplay/CoinSystem.h"
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
// CoinSystem 构造与初始化
// ============================================================================
CoinSystem::CoinSystem() {
    Initialize();
}

void CoinSystem::Initialize() {
    coins_.resize(kPoolCapacity);
    freeList_.reserve(kPoolCapacity);
    for (int i = 0; i < kPoolCapacity; ++i) {
        coins_[i].active = false;
        freeList_.push_back(kPoolCapacity - 1 - i);
    }
    LOG_INFO("CoinSystem 已初始化: 池容量=%d", kPoolCapacity);
}

// ============================================================================
// 随机数辅助
// ============================================================================
float CoinSystem::randomFloat(float min, float max) const {
    return min + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (max - min);
}

// ============================================================================
// Spawn  生成金币
// ----------------------------------------------------------------------------
// 从对象池获取一个空闲槽位，初始化金币数据
// 生成时给一个小随机速度，模拟爆裂效果
// ============================================================================
void CoinSystem::Spawn(sf::Vector2f pos, int value) {
    if (freeList_.empty()) {
        LOG_WARN("金币池已满，无法生成");
        return;
    }

    int idx = freeList_.back();
    freeList_.pop_back();

    CoinData& coin = coins_[idx];
    coin.position = pos;
    // 随机散射速度（模拟爆裂效果）
    float angle = randomFloat(0.f, 6.2831853f);
    float speed = randomFloat(30.f, 60.f);
    coin.velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
    coin.value = value;
    coin.active = true;
    coin.lifetime = 15.f;
    coin.magnetTimer = 0.f;
}

// ============================================================================
// Update  更新金币
// ----------------------------------------------------------------------------
// 算法：
//   1. 遍历所有活跃金币
//   2. 衰减生命周期，超时则回收
//   3. 生成后 0.15s 散射，随后立即向玩家磁吸
//   4. 距离 < kPickupRadius 时拾取：增加玩家金币，回收
// ============================================================================
void CoinSystem::Update(Registry& registry, EntityId player,
                        PlayerComponent* playerComp, float dt) {
    Transform* playerTransform = registry.GetComponent<Transform>(player);
    if (!playerTransform) return;

    sf::Vector2f playerPos = playerTransform->position;

    for (int i = 0; i < kPoolCapacity; ++i) {
        CoinData& coin = coins_[i];
        if (!coin.active) continue;

        // 生命周期衰减
        coin.lifetime -= dt;
        if (coin.lifetime <= 0.f) {
            coin.active = false;
            freeList_.push_back(i);
            continue;
        }

        // 计算到玩家的距离
        sf::Vector2f toPlayer = playerPos - coin.position;
        float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;
        float dist = std::sqrt(distSq);

        // 磁吸效果：生成后 0.15s 散射，随后立即磁吸
        coin.magnetTimer += dt;
        if (coin.magnetTimer > 0.15f && dist > 0.001f) {
            // 磁吸速度：基础 250px/s，近距加速
            float magnetSpeed = 250.f;
            if (dist < 100.f) {
                magnetSpeed += (1.f - dist / 100.f) * 400.f;
            }
            sf::Vector2f magnetDir = toPlayer / dist;
            coin.velocity = magnetDir * magnetSpeed;
        } else {
            // 散射阶段：速度衰减
            coin.velocity *= (1.f - 3.f * dt);
        }

        // 应用速度
        coin.position += coin.velocity * dt;

        // 拾取检测：距离 < kPickupRadius
        if (dist < kPickupRadius) {
            if (playerComp) {
                playerComp->stats.coins += coin.value;
                AudioManager::Instance().PlaySFX(AudioManager::kSFXCoinPickup);
                // 金币拾取飘字（金色）
                SpawnFloatText(registry, coin.position,
                               "+" + std::to_string(coin.value) + " G",
                               sf::Color(255, 215, 0), 16, 0.8f);
                // 触发拾取回调（任务/成就系统统计累计金币）
                if (OnCoinGained) {
                    OnCoinGained(coin.value);
                }
            }
            coin.active = false;
            freeList_.push_back(i);
        }
    }
}

// ============================================================================
// Render  渲染金币（金色发光圆点）
// ============================================================================
void CoinSystem::Render(Renderer& renderer) {
    for (int i = 0; i < kPoolCapacity; ++i) {
        const CoinData& coin = coins_[i];
        if (!coin.active) continue;

        // 生命周期接近结束时闪烁
        float alpha = 255.f;
        if (coin.lifetime < 3.f) {
            alpha = 128.f + 127.f * std::sin(coin.lifetime * 20.f);
        }

        // 外圈光晕（半透明金色）
        renderer.DrawQuad(coin.position, sf::Vector2f(12.f, 12.f),
                          sf::Color(255, 215, 0, static_cast<uint8_t>(alpha * 0.4f)),
                          Layer::Entity);
        // 核心（亮金色发光圆点）
        renderer.DrawQuad(coin.position, sf::Vector2f(6.f, 6.f),
                          sf::Color(255, 240, 150, static_cast<uint8_t>(alpha)),
                          Layer::Entity);
    }
}

} // namespace cu
