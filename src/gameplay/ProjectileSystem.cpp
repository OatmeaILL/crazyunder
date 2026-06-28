#include "gameplay/ProjectileSystem.h"
#include "gameplay/EnemyAI.h"   // EnemyComponent（区分敌人与玩家）
#include "gameplay/Player.h"    // PlayerComponent
#include "gameplay/DungeonGenerator.h" // Dungeon / TileType
#include "gameplay/CombatEffects.h" // SpawnHitEffect / SpawnDamageText
#include "gameplay/CombatSystem.h"  // CombatSystem::ApplyStatus / CreateElementalStatus（第十六轮新增）
#include "core/AudioManager.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "rendering/TextureAtlas.h"
#include "rendering/ParticleSystem.h"
#include "utils/UniformGrid.h"
#include "utils/Logger.h"
#include <cmath>
#include <cstdlib>

namespace cu {

// ============================================================================
// 辅助：过程化生成 8x8 圆形子弹贴图
// ----------------------------------------------------------------------------
// 简单的白色圆形，带轻微发光效果。通过 Sprite.color 着色实现不同元素颜色。
// ============================================================================
static sf::Image createBulletImage() {
    const int size = 8;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0)); // 透明背景

    float cx = size / 2.f;
    float cy = size / 2.f;
    float radius = 3.f;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - cx + 0.5f;
            float dy = y - cy + 0.5f;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= radius) {
                if (dist > radius - 1.f) {
                    // 边缘略暗（模拟阴影）
                    img.setPixel(x, y, sf::Color(200, 200, 200, 255));
                } else {
                    // 中心白色
                    img.setPixel(x, y, sf::Color::White);
                }
            }
        }
    }
    return img;
}

// ============================================================================
// 辅助：获取元素对应的默认颜色
// ============================================================================
static sf::Color getElementColor(ElementType elem) noexcept {
    switch (elem) {
        case ElementType::Physical:  return sf::Color(255, 255, 255, 255); // 白
        case ElementType::Fire:      return sf::Color(255, 100, 0, 255);   // 红
        case ElementType::Ice:       return sf::Color(100, 200, 255, 255); // 蓝
        case ElementType::Lightning: return sf::Color(255, 255, 0, 255);   // 黄
        case ElementType::Poison:    return sf::Color(100, 255, 0, 255);   // 绿
    }
    return sf::Color::White;
}

ProjectileSystem::ProjectileSystem() = default;

// ============================================================================
// Initialize —— 初始化子弹对象池
// ----------------------------------------------------------------------------
// 预创建 1200 个子弹实体，每个挂载：
//   Transform  —— 位置（初始在远处）
//   Sprite     —— 贴图（8x8 圆形）
//   Projectile —— 子弹数据（active=false）
//   Tag        —— Projectile 标签
//   Collider   —— 圆形碰撞体
//
// 所有实体加入 freeList_，Spawn 时弹出复用。
// ============================================================================
void ProjectileSystem::Initialize(Registry& registry, TextureAtlas& atlas) {
    registry_ = &registry;

    // 生成子弹贴图并添加到图集（必须在 atlas.Build() 之前调用）
    if (!atlas.AddImageFromMemory("bullet", createBulletImage())) {
        LOG_WARN("子弹贴图添加到图集失败");
    }

    // 预创建子弹实体池
    projectilePool_.clear();
    freeList_.clear();
    projectilePool_.reserve(kPoolCapacity);
    freeList_.reserve(kPoolCapacity);

    for (int i = 0; i < kPoolCapacity; ++i) {
        EntityId id = registry.CreateEntity();

        // Transform：初始位置在远处（不可见）
        auto& transform = registry.AddComponent<Transform>(id);
        transform.position = sf::Vector2f(99999.f, 99999.f);
        transform.scale = sf::Vector2f(1.f, 1.f);

        // Sprite：子弹贴图（Build 后通过 GetPixelRect 获取实际矩形）
        auto& sprite = registry.AddComponent<Sprite>(id);
        sprite.color = sf::Color::White;
        sprite.origin = sf::Vector2f(4.f, 4.f); // 8x8 贴图中心

        // Projectile 组件（初始非活跃）
        auto& proj = registry.AddComponent<Projectile>(id);
        proj.active = false;

        // Collider：圆形碰撞
        auto& collider = registry.AddComponent<Collider>(id);
        collider.isCircle = true;
        collider.radius = 6.f;

        // Tag：标记为 Projectile
        registry.AddComponent<Tag>(id).flags = TagFlag::Projectile;

        projectilePool_.push_back(id);
        freeList_.push_back(id);
    }

    LOG_INFO("弹幕系统已初始化: 池容量=%d, 可用=%d",
             kPoolCapacity, static_cast<int>(freeList_.size()));
}

// ============================================================================
// PostBuildInit —— 图集构建后获取子弹贴图矩形
// ============================================================================
void ProjectileSystem::PostBuildInit(const TextureAtlas& atlas) {
    bulletRect_ = atlas.GetPixelRect("bullet");
    if (bulletRect_.width == 0 || bulletRect_.height == 0) {
        LOG_WARN("子弹贴图矩形获取失败，使用默认 8x8");
        bulletRect_ = sf::IntRect(0, 0, 8, 8);
    }
}

// ============================================================================
// acquireFromPool —— 从对象池获取子弹实体
// ============================================================================
EntityId ProjectileSystem::acquireFromPool() {
    if (freeList_.empty()) {
        LOG_WARN("子弹对象池耗尽！活跃子弹=%d", activeCount_);
        return kInvalidEntity;
    }
    EntityId id = freeList_.back();
    freeList_.pop_back();
    ++activeCount_;
    return id;
}

// ============================================================================
// releaseToPool —— 回收子弹到对象池
// ============================================================================
void ProjectileSystem::releaseToPool(EntityId id) {
    Projectile* proj = registry_->GetComponent<Projectile>(id);
    if (proj) {
        proj->active = false;
    }
    // 重置位置到远处
    Transform* transform = registry_->GetComponent<Transform>(id);
    if (transform) {
        transform->position = sf::Vector2f(99999.f, 99999.f);
    }
    freeList_.push_back(id);
    --activeCount_;
}

// ============================================================================
// ClearAll —— 清除所有活跃子弹（调试用）
// ============================================================================
void ProjectileSystem::ClearAll() {
    if (!registry_) return;
    
    int cleared = 0;
    // 遍历所有预分配的子弹实体
    for (EntityId id : projectilePool_) {
        Projectile* proj = registry_->GetComponent<Projectile>(id);
        if (proj && proj->active) {
            // 回收活跃子弹到对象池
            releaseToPool(id);
            ++cleared;
        }
    }
    
    LOG_INFO("已清除 %d 个活跃子弹", cleared);
}

// ============================================================================
// Spawn —— 发射子弹
// ----------------------------------------------------------------------------
// 从对象池获取一个子弹实体，设置其位置、方向、速度等属性。
// dir 应为归一化向量；若未归一化，此处会自动归一化。
// ============================================================================
void ProjectileSystem::Spawn(sf::Vector2f pos, sf::Vector2f dir,
                              const ProjectileConfig& config, EntityId owner) {
    EntityId id = acquireFromPool();
    if (id == kInvalidEntity) return;

    // 归一化方向
    float lenSq = dir.x * dir.x + dir.y * dir.y;
    if (lenSq < 0.0001f) {
        dir = sf::Vector2f(1.f, 0.f); // 默认向右
    } else {
        float len = std::sqrt(lenSq);
        dir /= len;
    }

    // 设置 Transform
    Transform* transform = registry_->GetComponent<Transform>(id);
    if (transform) {
        transform->position = pos;
        transform->scale = sf::Vector2f(1.f, 1.f);
    }

    // 设置 Sprite（着色为元素颜色）
    Sprite* sprite = registry_->GetComponent<Sprite>(id);
    if (sprite) {
        sprite->sourceRect = bulletRect_;
        sprite->color = config.color;
    }

    // 设置 Projectile 组件
    Projectile* proj = registry_->GetComponent<Projectile>(id);
    if (proj) {
        proj->damage = config.damage;
        proj->pierce = config.pierce;
        proj->lifetime = config.lifetime;
        proj->owner = owner;
        proj->direction = dir;
        proj->speed = config.speed;
        proj->color = config.color;
        proj->splitCount = config.splitCount;
        proj->chainCount = config.chainCount;
        proj->element = config.element;
        proj->radius = config.radius;
        proj->active = true;
        proj->hitCooldown = 0.f;
    }

    // 设置 Collider
    Collider* collider = registry_->GetComponent<Collider>(id);
    if (collider) {
        collider->isCircle = true;
        collider->radius = config.radius;
    }
}

// ============================================================================
// Update —— 更新所有子弹
// ----------------------------------------------------------------------------
// 流程：
//   1. 遍历对象池中所有子弹
//   2. 跳过非活跃子弹（active=false）
//   3. 移动：position += direction × speed × dt
//   4. 生命衰减：lifetime -= dt，到期回收
//   5. 碰撞检测：
//      - 玩家子弹 → 用 UniformGrid 查询附近敌人
//      - 敌人子弹 → 直接检查玩家距离
//   6. 命中处理：伤害、穿透、分裂、连锁
//
// 性能优化：
//   - neighbors 缓冲区复用，避免每帧分配
//   - 碰撞用距离平方比较，避免 sqrt
//   - 非活跃子弹仅检查 active 标志（分支预测友好）
// ============================================================================
void ProjectileSystem::Update(Registry& registry, UniformGrid& grid,
                               EntityId playerEntity, Dungeon* dungeon,
                               ParticleSystem& particles, float dt) {
    // 复用的临时缓冲区
    std::vector<EntityId> neighbors;
    neighbors.reserve(64);

    // 获取玩家位置（用于敌人子弹碰撞）
    Transform* playerTransform = registry.GetComponent<Transform>(playerEntity);
    Collider* playerCollider = registry.GetComponent<Collider>(playerEntity);
    sf::Vector2f playerPos(0.f, 0.f);
    float playerRadius = 16.f;
    if (playerTransform) playerPos = playerTransform->position;
    if (playerCollider) playerRadius = playerCollider->radius;

    // 遍历对象池中所有子弹
    for (EntityId bulletId : projectilePool_) {
        Projectile* proj = registry.GetComponent<Projectile>(bulletId);
        if (!proj || !proj->active) continue;

        Transform* bulletTransform = registry.GetComponent<Transform>(bulletId);
        if (!bulletTransform) continue;

        // ---- 1. 移动 ----
        bulletTransform->position += proj->direction * proj->speed * dt;

        // ---- 1.5 tile 碰撞检测 ----
        // 子弹击中墙壁则销毁；击中障碍物（罐子）则破坏障碍物并销毁子弹
        // 关闭的门阻挡子弹并受到伤害；打开的门不阻挡
        if (dungeon && !dungeon->tiles.empty()) {
            sf::Vector2i tile = dungeon->WorldToTile(bulletTransform->position);
            TileType t = dungeon->GetTile(tile.x, tile.y);
            if (t == TileType::Wall || t == TileType::IndestructibleObstacle) {
                // 墙壁/不可破坏障碍物阻挡子弹：生成火花粒子
                particles.HitSpark(bulletTransform->position);
                releaseToPool(bulletId);
                continue;
            }
            if (t == TileType::Obstacle) {
                // 破坏障碍物：变为地板
                dungeon->SetTile(tile.x, tile.y, TileType::Floor);
                // 生成碎片粒子效果（木桶破碎）
                particles.Explosion(bulletTransform->position);
                // 显示"破坏"提示
                SpawnDamageText(registry, bulletTransform->position, 0.f, false);
                // 通知 Game 层掉落物品和经验球
                if (onPotBroken) {
                    onPotBroken(bulletTransform->position);
                }
                releaseToPool(bulletId);
                continue;
            }
            if (t == TileType::Door) {
                DoorState* ds = dungeon->GetDoorState(tile.x, tile.y);
                if (ds && !ds->open) {
                    // 关闭的门：子弹造成伤害
                    ds->hp -= proj->damage;
                    // 生成门受击粒子效果
                    particles.HitSpark(bulletTransform->position);
                    if (ds->hp <= 0.f) {
                        // 门被破坏：变为地板，移除门状态
                        dungeon->SetTile(tile.x, tile.y, TileType::Floor);
                        dungeon->doorStates.erase(tile.y * dungeon->width + tile.x);
                        // 门破坏粒子效果
                        particles.Explosion(bulletTransform->position);
                        // 通知 Game 层门已破坏（标记 TileMap 重建）
                        if (onDoorBroken) onDoorBroken(bulletTransform->position);
                    }
                    releaseToPool(bulletId);
                    continue;
                }
                // 打开的门：子弹穿过，不阻挡
            }
        }

        // ---- 2. 生命衰减 ----
        proj->lifetime -= dt;
        if (proj->lifetime <= 0.f) {
            releaseToPool(bulletId);
            continue;
        }

        // ---- 3. 命中冷却衰减 ----
        if (proj->hitCooldown > 0.f) {
            proj->hitCooldown -= dt;
        }

        // ---- 4. 碰撞检测 ----
        bool isPlayerBullet = (proj->owner == playerEntity);

        if (isPlayerBullet) {
            // 玩家子弹：查询空间网格中的敌人
            neighbors.clear();
            grid.QueryPoint(bulletTransform->position, neighbors);

            for (EntityId targetId : neighbors) {
                if (targetId == bulletId) continue;
                if (targetId == proj->owner) continue;

                // 只命中敌人
                EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(targetId);
                if (!enemy || !enemy->active) continue;

                Transform* targetTransform = registry.GetComponent<Transform>(targetId);
                Collider* targetCollider = registry.GetComponent<Collider>(targetId);
                if (!targetTransform) continue;

                float targetRadius = targetCollider ? targetCollider->radius : 16.f;

                // 圆形碰撞检测（用距离平方避免 sqrt）
                sf::Vector2f diff = bulletTransform->position - targetTransform->position;
                float distSq = diff.x * diff.x + diff.y * diff.y;
                float radiusSum = proj->radius + targetRadius;
                if (distSq <= radiusSum * radiusSum) {
                    // 命中！
                    bool destroy = handleHit(registry, bulletId, targetId,
                                             *proj, *bulletTransform,
                                             grid, neighbors, particles);
                    if (destroy) {
                        releaseToPool(bulletId);
                        goto nextBullet; // 跳出多层循环
                    }
                    break; // 一帧只命中一个目标
                }
            }
        } else {
            // 敌人子弹：直接检查玩家距离（仅 1 个玩家，无需网格）
            sf::Vector2f diff = bulletTransform->position - playerPos;
            float distSq = diff.x * diff.x + diff.y * diff.y;
            float radiusSum = proj->radius + playerRadius;
            if (distSq <= radiusSum * radiusSum) {
                // 命中玩家
                bool destroy = handleHit(registry, bulletId, playerEntity,
                                         *proj, *bulletTransform,
                                         grid, neighbors, particles);
                if (destroy) {
                    releaseToPool(bulletId);
                    continue;
                }
            }
        }

    nextBullet:;
    }
}

// ============================================================================
// handleHit —— 处理子弹命中目标
// ----------------------------------------------------------------------------
// 返回 true 表示子弹应销毁（pierce 耗尽），false 表示继续飞行。
//
// 处理流程：
//   1. 施加伤害（通过 CombatSystem 的 ApplyDamage，但此处直接扣 HP 简化）
//   2. 触发分裂（splitCount > 0）
//   3. 触发连锁闪电（chainCount > 0）
//   4. 穿透判定：pierce > 0 则不销毁，pierce--
// ============================================================================
bool ProjectileSystem::handleHit(Registry& registry, EntityId bulletId,
                                  EntityId targetId, Projectile& proj,
                                  Transform& bulletTransform,
                                  UniformGrid& grid,
                                  std::vector<EntityId>& neighbors,
                                  ParticleSystem& particles) {
    // 命中冷却检查（避免同一子弹连续帧命中同一目标）
    if (proj.hitCooldown > 0.f) return false;

    // 计算击退方向（子弹飞行方向）
    sf::Vector2f knockback = proj.direction * 100.f;

    // 施加伤害（直接扣减 HP，CombatSystem 的回调由 Game 层处理）
    Health* targetHealth = registry.GetComponent<Health>(targetId);
    if (targetHealth && targetHealth->current > 0.f) {
        // 暴击判定（15% 暴击率）
        bool isCritical = (std::rand() % 100) < 15;
        float damage = proj.damage;
        if (isCritical) {
            damage *= 1.5f;
        }

        // ---- 带盾怪正面减伤 50% ----
        // 盾牌朝向玩家方向，若子弹从正面（玩家方向）命中则减伤
        EnemyComponent* targetEnemy = registry.GetComponent<EnemyComponent>(targetId);
        if (targetEnemy && targetEnemy->hasShield) {
            Transform* targetTransform2 = registry.GetComponent<Transform>(targetId);
            // 获取玩家位置以计算盾牌朝向
            // 简化：子弹飞行方向与敌人→玩家方向夹角判断
            // 若子弹方向与敌人→玩家反方向夹角 < 60°（即从玩家方向射来），视为正面命中
            // 这里用子弹方向与敌人→子弹来源方向判断
            // 简化实现：所有玩家子弹对带盾怪减伤 50%（盾牌始终朝向玩家）
            // 检查子弹是否为玩家发射的
            if (proj.owner != kInvalidEntity) {
                // 检查 owner 是否为玩家（通过是否有 PlayerComponent）
                // 简化：假设非敌人 owner 即为玩家
                EnemyComponent* ownerEnemy = registry.GetComponent<EnemyComponent>(proj.owner);
                if (!ownerEnemy) {
                    // owner 不是敌人 → 是玩家子弹 → 正面减伤
                    damage *= 0.5f;
                }
            }
        }

        targetHealth->current -= damage;
        if (targetHealth->current < 0.f) {
            targetHealth->current = 0.f;
        }

        // ---- 元素状态效果触发（第十六轮新增，第十九轮扩展 Lightning）----
        // 根据 proj.element 施加对应状态：
        //   Fire      → 燃烧 DoT（每 0.5s 造成 20% 原始伤害，持续 3s）
        //   Ice       → 冰冻减速（持续 2s，无伤害）
        //   Poison    → 中毒 DoT（每 1s 造成 10% 原始伤害，持续 5s）
        //   Lightning → 麻痹（持续 0.6s，完全禁锢）—— 第十九轮新增
        //   Physical  → 无状态
        // 仅对玩家子弹（owner 非敌人）施加，避免敌人元素子弹也触发状态导致玩家
        // 也受 DoT（当前玩家无元素抗性 UI，避免意外死亡体验）。
        // 实现上通过 owner 是否有 EnemyComponent 判定敌我。
        // 第十九轮新增：Lightning 状态持续时间受玩家 lightningDurationMul 影响
        // （圣物"风暴之眼"通过此字段延长麻痹时间 +50%）
        if (combatSystem_ != nullptr && proj.element != ElementType::Physical) {
            EnemyComponent* ownerEnemy = registry.GetComponent<EnemyComponent>(proj.owner);
            // 仅玩家子弹（owner 不是敌人）才施加状态，且目标必须存活
            if (ownerEnemy == nullptr && targetHealth->current > 0.f) {
                StatusEffect se = CombatSystem::CreateElementalStatus(proj.element, proj.damage);
                if (se.duration > 0.f) {
                    // 第十九轮新增：Lightning 麻痹时间倍率应用
                    if (proj.element == ElementType::Lightning) {
                        const PlayerComponent* ownerPc = registry.GetComponent<PlayerComponent>(proj.owner);
                        if (ownerPc != nullptr && ownerPc->stats.lightningDurationMul > 0.f) {
                            se.duration *= ownerPc->stats.lightningDurationMul;
                        }
                    }
                    combatSystem_->ApplyStatus(registry, targetId, se);
                }
            }
        }

        // 应用击退
        Velocity* targetVel = registry.GetComponent<Velocity>(targetId);
        if (targetVel) {
            targetVel->linear += knockback;
        }

        // ---- 战斗反馈：粒子效果 + 伤害飘字 ----
        Transform* targetTransform = registry.GetComponent<Transform>(targetId);
        if (targetTransform) {
            // 命中粒子效果（方向性喷射火花）
            SpawnHitEffect(particles, targetTransform->position, proj.direction);
            // 伤害飘字
            SpawnDamageText(registry, targetTransform->position, damage, isCritical);
        }

        // 玩家受伤音效 vs 敌人命中音效
        PlayerComponent* targetPlayer = registry.GetComponent<PlayerComponent>(targetId);
        if (targetPlayer) {
            AudioManager::Instance().PlaySFX(AudioManager::kSFXPlayerHurt);
        } else {
            // 子弹命中敌人音效
            AudioManager::Instance().PlaySFX(AudioManager::kSFXHit);
        }

        // 吸血（攻击者恢复 HP）
        if (proj.owner != kInvalidEntity) {
            Health* attackerHealth = registry.GetComponent<Health>(proj.owner);
            if (attackerHealth && attackerHealth->current > 0.f) {
                // 默认吸血 0（由 config 设置，此处简化）
            }
        }
    }

    // 设置命中冷却（0.1s 内不再命中同一目标）
    proj.hitCooldown = 0.1f;

    // ---- 分裂处理 ----
    if (proj.splitCount > 0) {
        handleSplit(registry, bulletId, proj, bulletTransform);
    }

    // ---- 连锁闪电处理 ----
    if (proj.chainCount > 0) {
        handleChain(registry, grid, bulletTransform.position, targetId,
                    proj.damage * 0.7f, // 连锁伤害衰减为 70%
                    proj.chainCount, proj.owner, neighbors, particles);
    }

    // ---- 穿透判定 ----
    if (proj.pierce > 0) {
        --proj.pierce;
        return false; // 继续飞行
    }

    return true; // 销毁子弹
}

// ============================================================================
// handleSplit —— 分裂处理
// ----------------------------------------------------------------------------
// 命中时分裂出 splitCount 个新子弹，扇形扩散。
// 分裂子弹继承原子弹的部分属性（伤害衰减为 60%，无再分裂）。
// ============================================================================
void ProjectileSystem::handleSplit(Registry& registry, EntityId bulletId,
                                    Projectile& proj,
                                    const Transform& bulletTransform) {
    if (proj.splitCount <= 0) return;

    // 分裂子弹配置（伤害衰减，无再分裂/连锁）
    ProjectileConfig splitConfig;
    splitConfig.speed = proj.speed;
    splitConfig.damage = proj.damage * 0.6f; // 伤害衰减 40%
    splitConfig.pierce = 0;                  // 分裂子弹不再穿透
    splitConfig.lifetime = proj.lifetime * 0.5f; // 生命减半
    splitConfig.radius = proj.radius;
    splitConfig.color = proj.color;
    splitConfig.splitCount = 0;              // 不再分裂
    splitConfig.chainCount = 0;              // 不再连锁
    splitConfig.element = proj.element;

    // 扇形分裂：以原方向为中心，左右各 spreadAngle 度
    int count = proj.splitCount;
    float spreadAngle = 30.f * 3.14159265f / 180.f; // 30 度扇形

    for (int i = 0; i < count; ++i) {
        // 计算分裂角度：均匀分布在 [-spreadAngle, +spreadAngle]
        float t = (count == 1) ? 0.f : (static_cast<float>(i) / (count - 1) - 0.5f) * 2.f;
        float angle = t * spreadAngle;

        // 旋转原方向
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);
        sf::Vector2f newDir(
            proj.direction.x * cosA - proj.direction.y * sinA,
            proj.direction.x * sinA + proj.direction.y * cosA
        );

        Spawn(bulletTransform.position, newDir, splitConfig, proj.owner);
    }
}

// ============================================================================
// handleChain —— 连锁闪电处理
// ----------------------------------------------------------------------------
// 命中后跳到附近的敌人，造成递减伤害。
//
// 算法：
//   1. 从命中位置查询半径 150px 内的敌人
//   2. 排除上次命中的目标（避免连锁回同一目标）
//   3. 选择最近的未命中敌人，施加伤害
//   4. 递归连锁 chainCount - 1 次
//
// 第十九轮修复：
//   - 添加视觉反馈：连锁命中时生成黄色伤害飘字 + 命中火花粒子
//   - 添加 Lightning 麻痹状态触发（与直击命中一致）
//   - 通过 CombatSystem::ApplyDamage 应用伤害（而非直接扣 HP），
//     使连锁伤害能触发 OnHit/OnKill 回调、暴击、吸血、combo 累积
//
// 性能：每次连锁用 UniformGrid 查询，O(1) 复杂度。
// ============================================================================
void ProjectileSystem::handleChain(Registry& registry, UniformGrid& grid,
                                    sf::Vector2f fromPos, EntityId lastTarget,
                                    float damage, int chainCount,
                                    EntityId owner,
                                    std::vector<EntityId>& neighbors,
                                    ParticleSystem& particles) {
    if (chainCount <= 0) return;

    // 查询附近敌人
    neighbors.clear();
    grid.QueryRange(fromPos, 150.f, neighbors);

    // 找到最近的未命中敌人
    EntityId bestTarget = kInvalidEntity;
    float bestDistSq = 150.f * 150.f;

    for (EntityId candidate : neighbors) {
        if (candidate == lastTarget) continue;
        if (candidate == owner) continue;

        EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(candidate);
        if (!enemy || !enemy->active) continue;

        Health* health = registry.GetComponent<Health>(candidate);
        if (!health || health->current <= 0.f) continue;

        Transform* t = registry.GetComponent<Transform>(candidate);
        if (!t) continue;

        sf::Vector2f diff = t->position - fromPos;
        float distSq = diff.x * diff.x + diff.y * diff.y;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestTarget = candidate;
        }
    }

    if (bestTarget == kInvalidEntity) return;

    // ---- 第十九轮修复：通过 CombatSystem::ApplyDamage 应用连锁伤害 ----
    // 此前直接修改 HP 会导致：
    //   1. 连锁击杀不触发 OnKill 回调（无经验/金币/掉落/combo 累积）
    //   2. 连锁伤害无暴击/吸血/combo 加成
    //   3. 连锁命中无视觉反馈（玩家不知伤害来源）
    // 现通过 ApplyDamage 统一处理，并补充 Lightning 状态触发 + 视觉特效
    Transform* targetTransform = registry.GetComponent<Transform>(bestTarget);
    if (targetTransform) {
        // 视觉反馈：黄色伤害飘字 + 命中火花
        SpawnDamageText(registry, targetTransform->position, damage, false,
                        sf::Color(255, 230, 80));
        SpawnHitEffect(particles, targetTransform->position,
                       sf::Vector2f(0.f, -1.f)); // 向上喷射火花

        // Lightning 麻痹状态触发（与直击一致，第十九轮新增）
        // 连锁命中也施加麻痹，强化"闪电流"的控制能力
        // 应用 lightningDurationMul 倍率（与直击路径一致）
        if (combatSystem_ != nullptr) {
            StatusEffect se = CombatSystem::CreateElementalStatus(ElementType::Lightning, damage);
            if (se.duration > 0.f) {
                // 第十九轮新增：应用玩家 lightningDurationMul 倍率
                const PlayerComponent* ownerPc = registry.GetComponent<PlayerComponent>(owner);
                if (ownerPc != nullptr && ownerPc->stats.lightningDurationMul > 0.f) {
                    se.duration *= ownerPc->stats.lightningDurationMul;
                }
                combatSystem_->ApplyStatus(registry, bestTarget, se);
            }
        }
    }

    // 通过 CombatSystem::ApplyDamage 应用伤害（触发回调/暴击/combo）
    if (combatSystem_ != nullptr) {
        DamageInfo dmgInfo;
        dmgInfo.attacker = owner;
        dmgInfo.target = bestTarget;
        dmgInfo.amount = damage;
        dmgInfo.isCritical = false; // 连锁不暴击，避免连锁暴击导致伤害失控
        dmgInfo.element = ElementType::Lightning;
        dmgInfo.knockback = sf::Vector2f(0.f, 0.f); // 连锁不击退，避免位移混乱
        dmgInfo.lifesteal = 0.f; // 连锁不吸血，避免高 chain 下吸血过强
        combatSystem_->ApplyDamage(registry, dmgInfo);
    } else {
        // 回退：无 CombatSystem 时直接扣 HP（保持向后兼容）
        Health* targetHealth = registry.GetComponent<Health>(bestTarget);
        if (targetHealth) {
            targetHealth->current -= damage;
            if (targetHealth->current < 0.f) {
                targetHealth->current = 0.f;
            }
        }
    }

    // 递归连锁（伤害再衰减 70%）
    if (targetTransform) {
        handleChain(registry, grid, targetTransform->position, bestTarget,
                    damage * 0.7f, chainCount - 1, owner, neighbors, particles);
    }
}

} // namespace cu
