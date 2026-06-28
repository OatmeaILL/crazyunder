#include "rendering/ParticleSystem.h"
#include <cstdlib>
#include <cmath>

namespace cu {

ParticleSystem::ParticleSystem()
    : pool_(5000) { // 预分配 5000 粒子
    deadBuffer_.reserve(512);
}

float ParticleSystem::randomRange(float min, float max) {
    return min + static_cast<float>(std::rand()) / RAND_MAX * (max - min);
}

sf::Color ParticleSystem::randomColor(const sf::Color& min, const sf::Color& max) {
    return sf::Color(
        static_cast<sf::Uint8>(randomRange(min.r, max.r)),
        static_cast<sf::Uint8>(randomRange(min.g, max.g)),
        static_cast<sf::Uint8>(randomRange(min.b, max.b)),
        static_cast<sf::Uint8>(randomRange(min.a, max.a))
    );
}

void ParticleSystem::Emit(sf::Vector2f position, int count, const EmitConfig& config) {
    for (int i = 0; i < count; ++i) {
        ParticleData* p = pool_.acquire();
        if (!p) break; // 池已满，丢弃多余粒子

        p->position = position;
        p->size = randomRange(config.sizeMin, config.sizeMax);
        p->life = randomRange(config.lifeMin, config.lifeMax);
        p->maxLife = p->life;
        p->color = randomColor(config.colorMin, config.colorMax);

        if (config.radial) {
            // 径向发射：随机角度 + 随机速度
            float angle = static_cast<float>(std::rand()) / RAND_MAX * 6.2831853f;
            float speed = randomRange(config.speedMin, config.speedMax);
            p->velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
        } else {
            // 矩形范围随机速度
            p->velocity = sf::Vector2f(
                randomRange(config.velocityMin.x, config.velocityMax.x),
                randomRange(config.velocityMin.y, config.velocityMax.y)
            );
        }
    }
}

void ParticleSystem::Update(float dt) {
    deadBuffer_.clear();

    for (auto& p : pool_) {
        // 移动粒子
        p.position += p.velocity * dt;

        // 简单阻力（速度衰减，模拟空气阻力）
        p.velocity *= 0.96f;

        // 衰减生命
        p.life -= dt;
        if (p.life <= 0.f) {
            deadBuffer_.push_back(&p);
        }
    }

    // 批量释放死亡粒子（不在迭代中 release，避免 swap-remove 破坏迭代）
    for (auto* p : deadBuffer_) {
        pool_.release(p);
    }
}

void ParticleSystem::Render(Renderer& renderer) {
    for (auto& p : pool_) {
        float ratio = p.life / p.maxLife; // 1.0=满生命, 0.0=死亡
        if (ratio < 0.f) ratio = 0.f;

        // 颜色随生命衰减（淡出）
        sf::Color c = p.color;
        c.a = static_cast<sf::Uint8>(c.a * ratio);

        // 尺寸随生命缩小
        float sz = p.size * ratio;
        if (sz < 1.f) sz = 1.f;

        renderer.DrawQuad(p.position, sf::Vector2f(sz, sz), c, Layer::Particle);
    }
}

// ============================================================================
// 预设特效
// ============================================================================

void ParticleSystem::Explosion(sf::Vector2f pos) {
    EmitConfig cfg;
    cfg.radial = true;
    cfg.speedMin = 80.f;
    cfg.speedMax = 250.f;
    cfg.colorMin = sf::Color(255, 200, 50, 255);
    cfg.colorMax = sf::Color(255, 80, 0, 255);
    cfg.sizeMin = 4.f;
    cfg.sizeMax = 10.f;
    cfg.lifeMin = 0.4f;
    cfg.lifeMax = 0.9f;
    Emit(pos, 40, cfg);
}

void ParticleSystem::HitSpark(sf::Vector2f pos) {
    EmitConfig cfg;
    cfg.radial = true;
    cfg.speedMin = 100.f;
    cfg.speedMax = 200.f;
    cfg.colorMin = sf::Color(255, 255, 200, 255);
    cfg.colorMax = sf::Color(255, 200, 50, 255);
    cfg.sizeMin = 2.f;
    cfg.sizeMax = 5.f;
    cfg.lifeMin = 0.15f;
    cfg.lifeMax = 0.35f;
    Emit(pos, 15, cfg);
}

void ParticleSystem::LevelUpBeam(sf::Vector2f pos) {
    EmitConfig cfg;
    cfg.radial = false;
    cfg.velocityMin = sf::Vector2f(-30.f, -200.f);
    cfg.velocityMax = sf::Vector2f( 30.f, -100.f);
    cfg.colorMin = sf::Color(100, 200, 255, 255);
    cfg.colorMax = sf::Color(200, 230, 255, 255);
    cfg.sizeMin = 3.f;
    cfg.sizeMax = 7.f;
    cfg.lifeMin = 0.6f;
    cfg.lifeMax = 1.2f;
    Emit(pos, 30, cfg);
}

void ParticleSystem::LootGlow(sf::Vector2f pos) {
    EmitConfig cfg;
    cfg.radial = true;
    cfg.speedMin = 10.f;
    cfg.speedMax = 50.f;
    cfg.colorMin = sf::Color(255, 220, 100, 200);
    cfg.colorMax = sf::Color(255, 180, 50, 255);
    cfg.sizeMin = 2.f;
    cfg.sizeMax = 5.f;
    cfg.lifeMin = 0.5f;
    cfg.lifeMax = 1.0f;
    Emit(pos, 12, cfg);
}

} // namespace cu
