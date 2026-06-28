#pragma once

// ============================================================================
// ParticleSystem —— 粒子系统
// ----------------------------------------------------------------------------
// 核心原理：对象池管理粒子
//   割草游戏中粒子数量巨大（爆炸、命中、拾取等），若每帧 new/delete 粒子对象，
//   堆碎片化与分配开销会严重影响性能。
//
//   使用 ObjectPool<ParticleData> 预分配 5000 个粒子槽位：
//     - Emit 时从池中 acquire（O(1)，无堆分配）
//     - 粒子死亡时 release 回池（O(1) swap-remove）
//     - 遍历活跃粒子时访问紧凑数组，缓存友好
//   整个游戏循环内零堆分配，性能稳定。
//
// 渲染：
//   粒子作为纯色四边形通过 Renderer::DrawQuad 入队，使用加法混合实现发光效果。
//   同层粒子使用同一白色纹理，可被 Renderer 批量合并为单次 Draw Call。
//
// 预设特效：
//   Explosion（爆炸）、HitSpark（命中火花）、LevelUpBeam（升级光柱）、
//   LootGlow（拾取发光）封装了常用参数组合，一键触发。
// ============================================================================

#include <SFML/Graphics.hpp>
#include "utils/ObjectPool.h"
#include "rendering/Renderer.h"

namespace cu {

// 粒子数据（包含位置，独立于 ECS 的 Particle 组件）
struct ParticleData {
    sf::Vector2f position;
    sf::Vector2f velocity;
    sf::Color color;
    float size = 4.f;
    float life = 1.f;
    float maxLife = 1.f;
};

// 发射器配置：定义粒子的随机范围
struct EmitConfig {
    sf::Vector2f velocityMin{-100.f, -100.f};
    sf::Vector2f velocityMax{ 100.f,  100.f};
    sf::Color colorMin{255, 200, 50, 255};
    sf::Color colorMax{255, 100, 0, 255};
    float sizeMin = 3.f;
    float sizeMax = 8.f;
    float lifeMin = 0.3f;
    float lifeMax = 0.8f;
    // 是否径向发射（velocity 方向 = 从中心向外随机角度）
    bool radial = true;
    float speedMin = 50.f;
    float speedMax = 200.f;
};

class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem() = default;

    // 发射粒子：在 position 处生成 count 个，参数由 config 随机化
    void Emit(sf::Vector2f position, int count, const EmitConfig& config);

    // 每帧更新：移动粒子、衰减生命、释放死亡粒子
    void Update(float dt);

    // 渲染所有活跃粒子（加法混合发光效果）
    void Render(Renderer& renderer);

    // ---- 预设特效 ----
    void Explosion(sf::Vector2f pos);      // 爆炸：橙红径向扩散
    void HitSpark(sf::Vector2f pos);       // 命中火花：黄白短促
    void LevelUpBeam(sf::Vector2f pos);    // 升级光柱：蓝白上升
    void LootGlow(sf::Vector2f pos);       // 拾取发光：金色柔和

    // 统计
    [[nodiscard]] std::size_t GetActiveCount() const noexcept { return pool_.size(); }
    [[nodiscard]] std::size_t GetCapacity() const noexcept { return pool_.capacity(); }

private:
    ObjectPool<ParticleData> pool_;

    // 死亡粒子缓冲（复用，避免每帧堆分配）
    std::vector<ParticleData*> deadBuffer_;

    // 随机数辅助
    static float randomRange(float min, float max);
    static sf::Color randomColor(const sf::Color& min, const sf::Color& max);
};

} // namespace cu
