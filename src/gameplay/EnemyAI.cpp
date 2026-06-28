#include "gameplay/EnemyAI.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "gameplay/FlowField.h"
#include "gameplay/ProjectileSystem.h"
#include "gameplay/CombatSystem.h"
#include "gameplay/CombatEffects.h"
#include "gameplay/DungeonGenerator.h"
#include "gameplay/EnemySpawner.h"
#include "gameplay/Player.h"
#include "core/AudioManager.h"
#include "rendering/ParticleSystem.h"
#include "utils/UniformGrid.h"
#include "utils/Logger.h"
#include <cmath>
#include <chrono>
#include <vector>
#include <string>

namespace cu {

// ---- 新怪物机制常量 ----
// 隐身怪：远距离完全隐身，进入显形范围后才可见并攻击
inline constexpr float kStealthRevealRange = 120.f;   // 进入此范围显形
// 倒计时自爆：靠近玩家 150px 内激活，倒计时 1.8s
inline constexpr float kCountdownActivateRange = 150.f;
inline constexpr float kCountdownDuration = 1.8f;
inline constexpr float kCountdownExplodeRange = 120.f; // 倒计时到 0 时若玩家在此范围内则爆炸
// 自爆怪：死亡爆炸范围
inline constexpr float kSuicideExplodeRange = 160.f;

// ============================================================================
// 计算单个敌人的 Boids 分离力
// ----------------------------------------------------------------------------
// 通过 UniformGrid 查询半径内的邻居，对每个邻居计算反向向量（远离），
// 按距离反比加权累加，最后归一化。
//
// 参数：
//   selfPos: 当前敌人位置
//   selfId: 当前敌人实体 ID（排除自身）
//   grid: 已插入所有敌人位置的空间网格
//   neighbors: 复用的临时缓冲（避免每次调用都分配）
//
// 返回：分离力向量（未归一化的累加值）
// ============================================================================
static sf::Vector2f computeSeparation(sf::Vector2f selfPos, EntityId selfId,
                                       const UniformGrid& grid,
                                       std::vector<EntityId>& neighbors) {
    neighbors.clear();
    grid.QueryRange(selfPos, kSeparationRadius, neighbors);

    sf::Vector2f separation(0.f, 0.f);
    int count = 0;

    for (EntityId otherId : neighbors) {
        if (otherId == selfId) continue; // 排除自身

        // 注意：这里无法直接获取邻居位置（函数签名未传 Registry）
        // 分离力计算需要邻居位置，因此在 UpdateEnemyAI 中内联实现
        // 此函数仅作为参考，实际逻辑在 UpdateEnemyAI 中
    }
    (void)separation;
    (void)count;
    return separation;
}

// ============================================================================
// UpdateEnemyAI —— 敌人 AI 主更新
// ============================================================================
float UpdateEnemyAI(Registry& registry, const FlowField& flowField,
                    const UniformGrid& grid, EntityId playerEntity, float dt,
                    Dungeon* dungeon) {
    auto startTime = std::chrono::steady_clock::now();

    // 获取玩家位置（用于攻击判定与远程敌人距离保持）
    Transform* playerTransform = registry.GetComponent<Transform>(playerEntity);
    Health* playerHealth = registry.GetComponent<Health>(playerEntity);
    sf::Vector2f playerPos(0.f, 0.f);
    if (playerTransform) {
        playerPos = playerTransform->position;
    }

    // 复用的临时缓冲，避免每个敌人都分配 vector
    std::vector<EntityId> neighbors;
    neighbors.reserve(64);

    // 遍历所有拥有 EnemyComponent + Transform 的实体
    // 使用 ForEach 遍历，避免每帧分配临时 vector
    registry.ForEach<EnemyComponent, Transform>([&](EntityId id) {
        EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(id);
        Transform* transform = registry.GetComponent<Transform>(id);
        Velocity* velocity = registry.GetComponent<Velocity>(id);
        if (!enemy || !transform) return;
        if (!enemy->active) return; // 跳过池中待命的非活跃敌人

        sf::Vector2f myPos = transform->position;

        // ---- 1. 从流场获取移动方向 ----
        sf::Vector2f flowDir = flowField.GetDirection(myPos);

        // ---- 2. Boids 分离力 ----
        // 查询半径内的邻居，计算分离力避免重叠
        neighbors.clear();
        grid.QueryRange(myPos, kSeparationRadius, neighbors);

        sf::Vector2f separation(0.f, 0.f);
        int separationCount = 0;

        for (EntityId otherId : neighbors) {
            if (otherId == id) continue; // 排除自身

            // 只对其他敌人计算分离力（忽略玩家、子弹等）
            EnemyComponent* otherEnemy = registry.GetComponent<EnemyComponent>(otherId);
            if (!otherEnemy) continue;

            Transform* otherTransform = registry.GetComponent<Transform>(otherId);
            if (!otherTransform) continue;

            sf::Vector2f toOther = myPos - otherTransform->position;
            float distSq = toOther.x * toOther.x + toOther.y * toOther.y;

            // 距离过近时产生分离力（距离越近力越大）
            if (distSq > 0.0001f && distSq < kSeparationRadius * kSeparationRadius) {
                float dist = std::sqrt(distSq);
                // 归一化方向 × (1/dist) 加权：距离越近，分离力越大
                sf::Vector2f away = toOther / dist;
                float weight = (kSeparationRadius - dist) / kSeparationRadius;
                separation += away * weight;
                ++separationCount;
            }
        }

        // 归一化分离力并乘以权重
        if (separationCount > 0) {
            float sepLen = std::sqrt(separation.x * separation.x + separation.y * separation.y);
            if (sepLen > 0.0001f) {
                separation /= sepLen;
                separation *= kSeparationWeight;
            }
        }

        // ---- 3. 合成最终速度 ----
        sf::Vector2f desiredVelocity(0.f, 0.f);

        // 计算到玩家的距离（用于攻击与远程行为判定）
        sf::Vector2f toPlayer = playerPos - myPos;
        float distToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

        switch (enemy->type) {
            case EnemyType::Melee:
            case EnemyType::Elite:
            case EnemyType::Splitter:
            case EnemyType::Shielded: {
                // 近战类：沿流场方向追击玩家
                // Splitter：与普通近战相同，死亡时分裂（在 UpdateEnemyCombat 中处理）
                // Shielded：与普通近战相同，但正面减伤（在 UpdateEnemyCombat 中处理）
                desiredVelocity = flowDir * enemy->moveSpeed;

                // ---- 第三十一轮新增：近战敌人"挤开"行为 ----
                // 当多个近战敌人在流场同一格时，给予随机横向偏移力，避免排队送死
                // 分离力邻居中已有 otherId，此处仅对近战类型追加横向抖动
                if (separationCount > 1) {
                    // 分离力方向的垂直向量（顺时针 90°）
                    sf::Vector2f perp(-separation.y, separation.x);
                    float perpLen = std::sqrt(perp.x * perp.x + perp.y * perp.y);
                    if (perpLen > 0.001f) {
                        perp /= perpLen;
                    }
                    // 用实体 ID 作为伪随机种子，确保每个敌人行为一致但不同
                    float offset = (static_cast<float>(id % 100) / 100.f - 0.5f) * 2.f;
                    desiredVelocity += perp * enemy->moveSpeed * 0.4f * offset;
                }
                break;
            }
            case EnemyType::Boss: {
                // Boss：默认沿流场追击，冲撞激活时按预设方向高速直线移动
                if (enemy->chargeActive > 0.f) {
                    // 冲撞期间：使用预设方向 × 速度 × 倍率，不受流场影响
                    desiredVelocity = enemy->chargeDir * enemy->moveSpeed * enemy->chargeSpeedMul;
                    // 冲撞计时递减
                    enemy->chargeActive -= dt;
                } else {
                    desiredVelocity = flowDir * enemy->moveSpeed;
                }
                break;
            }
            case EnemyType::Suicide: {
                // 自爆：高速冲撞玩家
                // 进入冲锋范围（250px）后速度提升 2.2 倍，体现"冲刺"行为
                float speedMul = (distToPlayer <= 250.f) ? 2.2f : 1.0f;

                // ---- 第三十一轮新增：自爆怪预判 ----
                // 冲锋时预测玩家移动方向，略微偏移瞄准点，不再直线冲向当前位置
                // 预测量 = 玩家速度 × 预测时间（距离越近预测越准）
                if (distToPlayer <= 250.f && distToPlayer > 0.001f) {
                    // 获取玩家速度用于预判
                    Velocity* playerVel = registry.GetComponent<Velocity>(playerEntity);
                    sf::Vector2f predictOffset(0.f, 0.f);
                    if (playerVel) {
                        // 预测时间 = 距离 / 自爆怪速度，但限制在 0.1-0.4s 避免过度预判
                        float chargeSpeed = enemy->moveSpeed * speedMul;
                        float predictTime = std::min(0.4f, std::max(0.1f, distToPlayer / chargeSpeed));
                        predictOffset = playerVel->linear * predictTime * 0.6f; // 60% 预判权重，避免过准
                    }
                    sf::Vector2f targetPos = playerPos + predictOffset;
                    sf::Vector2f toTarget = targetPos - myPos;
                    float targetDist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
                    if (targetDist > 0.001f) {
                        desiredVelocity = (toTarget / targetDist) * enemy->moveSpeed * speedMul;
                    } else {
                        desiredVelocity = flowDir * enemy->moveSpeed * speedMul;
                    }
                } else {
                    desiredVelocity = flowDir * enemy->moveSpeed * speedMul;
                }

                // 脚步声
                enemy->walkSoundTimer -= dt;
                if (enemy->walkSoundTimer <= 0.f) {
                    AudioManager::Instance().PlaySFX(AudioManager::kSFXBomberWalk);
                    enemy->walkSoundTimer = 0.5f + 0.2f * (static_cast<float>(std::rand()) / RAND_MAX);
                }
                break;
            }
            case EnemyType::Ranged: {
                // 远程类：保持距离，太近则后退，太远则靠近
                if (distToPlayer < enemy->detectionRange * 0.7f) {
                    // 太近，后退（远离玩家）
                    if (distToPlayer > 0.001f) {
                        desiredVelocity = (myPos - playerPos) / distToPlayer * enemy->moveSpeed;
                    }
                } else if (distToPlayer > enemy->detectionRange) {
                    // 太远，靠近（沿流场）
                    desiredVelocity = flowDir * enemy->moveSpeed;
                } else {
                    // ---- 第三十一轮新增：远程敌人侧移 ----
                    // 合适距离时做垂直于玩家方向的横向移动，不再原地站桩
                    if (distToPlayer > 0.001f) {
                        sf::Vector2f toPlayerDir = toPlayer / distToPlayer;
                        // 垂直方向（顺时针 90°），用 ID 决定左/右
                        sf::Vector2f perp(-toPlayerDir.y, toPlayerDir.x);
                        float side = (id % 2 == 0) ? 1.f : -1.f;
                        desiredVelocity = perp * enemy->moveSpeed * 0.5f * side;
                    }
                }
                break;
            }
            case EnemyType::SniperRanged: {
                // 狙击远程：保持超远距离，靠近时快速撤退
                if (distToPlayer < enemy->detectionRange * 0.5f) {
                    // 玩家太近，快速撤退（1.5 倍速远离）
                    if (distToPlayer > 0.001f) {
                        desiredVelocity = (myPos - playerPos) / distToPlayer * enemy->moveSpeed * 1.5f;
                    }
                } else if (distToPlayer > enemy->detectionRange) {
                    // 太远，缓慢靠近
                    desiredVelocity = flowDir * enemy->moveSpeed * 0.5f;
                } else {
                    // ---- 第三十一轮新增：狙击手侧移 ----
                    // 合适距离时做横向移动，增加被瞄准难度
                    if (distToPlayer > 0.001f) {
                        sf::Vector2f toPlayerDir = toPlayer / distToPlayer;
                        sf::Vector2f perp(-toPlayerDir.y, toPlayerDir.x);
                        float side = (id % 2 == 0) ? 1.f : -1.f;
                        desiredVelocity = perp * enemy->moveSpeed * 0.6f * side;
                    }
                }
                break;
            }
            case EnemyType::StealthMelee: {
                // 隐身怪：远距离完全隐身，近距离显形并攻击
                // 以玩家距离判定显形/隐身，不再使用周期性切换
                bool wasStealth = enemy->isStealth;
                enemy->isStealth = (distToPlayer > kStealthRevealRange);

                // 切换状态时更新 Sprite 透明度
                if (wasStealth != enemy->isStealth) {
                    Sprite* sp = registry.GetComponent<Sprite>(id);
                    if (sp) {
                        if (enemy->isStealth) {
                            sp->color = sf::Color(100, 200, 200, 0);   // 完全隐身
                        } else {
                            sp->color = sf::Color(100, 200, 200, 255); // 显形
                        }
                    }
                }

                // ---- 第三十一轮新增：隐身伏击行为 ----
                // 隐身时：不走流场，直接朝玩家直线移动（无视地形绕路，更智能的追踪）
                // 显形瞬间：短暂加速冲刺（1.3x 持续 0.5s），给玩家突然遭遇的紧张感
                if (enemy->isStealth) {
                    // 隐身时直接朝玩家直线移动
                    if (distToPlayer > 0.001f) {
                        desiredVelocity = (playerPos - myPos) / distToPlayer * enemy->moveSpeed;
                    }
                } else {
                    // 显形后：检查是否刚显形（冲刺窗口）
                    if (wasStealth && !enemy->isStealth) {
                        // 刚显形，启动 0.5s 冲刺
                        enemy->specialTimer = 0.5f; // 复用 specialTimer 作为冲刺计时
                    }
                    if (enemy->specialTimer > 0.f) {
                        // 冲刺中：1.3x 加速追击
                        enemy->specialTimer -= dt;
                        desiredVelocity = flowDir * enemy->moveSpeed * 1.3f;
                    } else {
                        // 冲刺结束：正常追击
                        desiredVelocity = flowDir * enemy->moveSpeed;
                    }
                }
                break;
            }
            case EnemyType::CountdownSuicide: {
                // 倒计时自爆：靠近玩家后激活倒计时
                // 激活后以 60% 速度持续追击玩家，倒计时到 0 爆炸
                if (!enemy->countdownActive) {
                    // 检测是否进入激活范围
                    if (distToPlayer <= kCountdownActivateRange) {
                        enemy->countdownActive = true;
                        enemy->selfDestructCountdown = kCountdownDuration;
                        AudioManager::Instance().PlaySFX(AudioManager::kSFXBomberCharge);
                        LOG_INFO("倒计时自爆怪已激活，1.8 秒后爆炸");
                    }
                    // 未激活：正常追击
                    desiredVelocity = flowDir * enemy->moveSpeed;
                } else {
                    // 已激活：倒计时递减，持续追击（60% 移速，不死不休）
                    enemy->selfDestructCountdown -= dt;
                    desiredVelocity = flowDir * enemy->moveSpeed * 0.6f;

                    // 头上显示倒计时数字（每秒更新一次，避免每帧生成飘字）
                    int remainingSec = static_cast<int>(std::ceil(enemy->selfDestructCountdown));
                    int lastShown = static_cast<int>(enemy->specialTimer);
                    if (lastShown != remainingSec && remainingSec > 0) {
                        enemy->specialTimer = static_cast<float>(remainingSec);
                        sf::Vector2f textPos = myPos + sf::Vector2f(0.f, -24.f);
                        SpawnFloatText(registry, textPos, std::to_string(remainingSec),
                                       sf::Color(255, 80, 80), 20, 0.9f);
                    }

                    // 倒计时到 0：标记死亡，触发爆炸（在 UpdateEnemyCombat 中处理范围伤害）
                    if (enemy->selfDestructCountdown <= 0.f) {
                        Health* myHealth = registry.GetComponent<Health>(id);
                        if (myHealth) {
                            myHealth->current = 0.f;
                        }
                        // 死亡瞬间加速冲向玩家（最后一搏）
                        if (distToPlayer > 0.001f) {
                            desiredVelocity = (playerPos - myPos) / distToPlayer * enemy->moveSpeed * 1.5f;
                        }
                    }
                }
                // 脚步声
                enemy->walkSoundTimer -= dt;
                if (enemy->walkSoundTimer <= 0.f) {
                    AudioManager::Instance().PlaySFX(AudioManager::kSFXBomberWalk);
                    enemy->walkSoundTimer = 0.5f + 0.2f * (static_cast<float>(std::rand()) / RAND_MAX);
                }
                break;
            }
            case EnemyType::Caster: {
                // 施法者：保持中距离（200-300px），不靠太近也不跑太远
                if (distToPlayer < 180.f) {
                    // 太近，后退（远离玩家）
                    if (distToPlayer > 0.001f) {
                        desiredVelocity = (myPos - playerPos) / distToPlayer * enemy->moveSpeed;
                    }
                } else if (distToPlayer > 350.f) {
                    // 太远，缓慢靠近
                    desiredVelocity = flowDir * enemy->moveSpeed * 0.5f;
                } else {
                    // 合适距离，施法期间停止移动，其他时间小幅游走
                    if (enemy->castActive > 0.f) {
                        desiredVelocity = sf::Vector2f(0.f, 0.f);
                    } else {
                        desiredVelocity = flowDir * enemy->moveSpeed * 0.2f;
                    }
                }
                break;
            }
        }

        // 保存外部速度（击退/引力井拉扯，由 CombatSystem/SkillSystem 设置）
        sf::Vector2f externalVel = velocity->linear;

        // ---- 应用减速/麻痹效果 ----
        // 减速来源（取最大值，不叠加，避免 100% 减速卡死）：
        //   1. SkillSystem 地刺技能设置的 slowFactor（单帧有效，0.5=减速 50%）
        //   2. StatusEffectComponent 中的 Ice（50% 减速）/ Poison（30% 减速）状态
        //      —— 第十六轮新增，激活 CombatSystem 已有的元素状态系统
        //   3. StatusEffectComponent 中的 Lightning 麻痹（完全禁锢 0.6s）
        //      —— 第十九轮新增，激活 Lightning 元素状态，闪电流 build 的核心控制
        //
        // 注意：减速必须作用于 desiredVelocity（敌人主动移动速度），
        // 而非 vel->linear（仅作为 externalVel 用于击退/拉扯）。
        // 此前 SkillSystem 直接乘 vel->linear 完全无效，因为 desiredVelocity
        // = flowDir * moveSpeed 根本不读取 vel->linear。
        float slowAmount = enemy->slowFactor;

        // 读取元素状态效果，叠加 Ice/Poison 减速
        // Ice 由引力井施加（2s，50% 减速），Poison 由地刺施加（5s，30% 减速）
        // 二者不叠加，取最大值；上限 80% 避免完全卡死
        const StatusEffectComponent* statusComp = registry.GetComponent<StatusEffectComponent>(id);
        bool paralyzed = false; // 第十九轮新增：Lightning 麻痹标志
        if (statusComp) {
            float statusSlow = 0.f;
            for (const auto& eff : statusComp->effects) {
                if (eff.type == ElementType::Ice) {
                    statusSlow = std::max(statusSlow, 0.5f); // 冰冻：50% 减速
                } else if (eff.type == ElementType::Poison) {
                    statusSlow = std::max(statusSlow, 0.3f); // 中毒：30% 减速
                } else if (eff.type == ElementType::Lightning) {
                    // 第十九轮新增：Lightning 麻痹——完全禁锢
                    // 设计：比 Ice 减速更强（100% 停止），但持续时间更短（0.6s）
                    // 形成控制层次：Ice=软控（减速50% 2s）< Lightning=硬控（禁锢 0.6s）
                    paralyzed = true;
                    break;
                }
            }
            if (statusSlow > slowAmount) {
                slowAmount = statusSlow;
            }
        }

        if (paralyzed) {
            // 第十九轮新增：麻痹时完全清零主动移动速度，并阻止本帧攻击
            // 注意：仅清零 desiredVelocity，保留 externalVel（击退/拉扯仍生效），
            // 避免麻痹时敌人被卡在原地无法被任何力推动
            desiredVelocity = sf::Vector2f(0.f, 0.f);
            // 重置攻击冷却，麻痹期间无法攻击
            if (enemy->attackCooldown < 0.3f) {
                enemy->attackCooldown = 0.3f;
            }
            enemy->slowFactor = 0.f; // 麻痹期间不叠加减速
        } else if (slowAmount > 0.f) {
            // 上限 80%，避免完全停止导致 AI 卡死
            if (slowAmount > 0.8f) slowAmount = 0.8f;
            desiredVelocity *= (1.f - slowAmount);
            enemy->slowFactor = 0.f; // 重置，下一帧由 SkillSystem 重新设置
        }

        // 叠加分离力 + 外部速度
        sf::Vector2f finalVelocity = desiredVelocity + separation + externalVel;

        // 限制最大速度（分离力可能使总速度超过 moveSpeed）
        // 但允许外部速度（击退）超过限制
        float maxSpeed = enemy->moveSpeed * 1.5f;
        float aiSpeedSq = (desiredVelocity + separation).x * (desiredVelocity + separation).x
                       + (desiredVelocity + separation).y * (desiredVelocity + separation).y;
        if (aiSpeedSq > maxSpeed * maxSpeed) {
            float aiSpeed = std::sqrt(aiSpeedSq);
            sf::Vector2f aiVel = (desiredVelocity + separation) / aiSpeed * maxSpeed;
            finalVelocity = aiVel + externalVel;
        }

        // ---- 4. 更新位置与速度（Phase 6: 加入墙壁碰撞检测，轴分离）----
        if (dungeon && !dungeon->tiles.empty()) {
            // 敌人碰撞半径（比实际碰撞体略小，便于穿过门）
            float radius = (enemy->isBoss) ? 20.f : 8.f;

            // 辅助：检查某点是否阻挡（含关闭的门）
            auto isEnemyBlocked = [dungeon, radius](sf::Vector2f p) -> bool {
                sf::Vector2f samples[4] = {
                    {p.x - radius, p.y},
                    {p.x + radius, p.y},
                    {p.x, p.y - radius},
                    {p.x, p.y + radius}
                };
                for (const auto& s : samples) {
                    sf::Vector2i tile = dungeon->WorldToTile(s);
                    if (dungeon->IsBlocked(tile.x, tile.y)) {
                        return true;
                    }
                }
                return false;
            };

            sf::Vector2f curPos = transform->position;
            sf::Vector2f moveDelta = finalVelocity * dt;

            // X 轴独立移动
            sf::Vector2f tryX(curPos.x + moveDelta.x, curPos.y);
            if (!isEnemyBlocked(tryX)) {
                curPos.x = tryX.x;
            } else {
                finalVelocity.x = 0.f;
            }
            // Y 轴独立移动
            sf::Vector2f tryY(curPos.x, curPos.y + moveDelta.y);
            if (!isEnemyBlocked(tryY)) {
                curPos.y = tryY.y;
            } else {
                finalVelocity.y = 0.f;
            }
            transform->position = curPos;

            // ---- 4.5 门交互：遇到关闭的门时自动开门通过 ----
            // 怪物不攻击门，而是直接开门（避免怪物困在房间内）
            {
                sf::Vector2i myTile = dungeon->WorldToTile(curPos);
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int cx = myTile.x + dx;
                        int cy = myTile.y + dy;
                        if (dungeon->GetTile(cx, cy) != TileType::Door) continue;
                        DoorState* ds = dungeon->GetDoorState(cx, cy);
                        if (!ds || ds->open) continue;
                        // 上锁的门（陷阱房）敌人无法开启
                        if (ds->locked) continue;
                        // 自动开门
                        ds->open = true;
                    }
                }
            }
        } else {
            // 无地牢数据：保持原行为
            transform->position += finalVelocity * dt;
        }
        if (velocity) {
            // 衰减外部速度（击退/拉扯效果逐帧减弱）
            velocity->linear = externalVel * 0.82f;
        }

        // ---- 5. 攻击逻辑 ----
        enemy->attackCooldown -= dt;

        // ---- 第二十轮新增：攻击前摇信号（极限闪避系统用）----
        // 当 attackCooldown 即将归零时（< 阈值），标记 attackTelegraph 为剩余时间。
        // 玩家在此期间闪避可触发极限闪避反击。
        // 设计：Boss/自爆怪不参与（Boss 太强不适合极限闪避，自爆怪接触即爆无冷却）
        //       仅近战/远程敌人在"即将能攻击玩家"时才设置前摇，避免远处无意义触发
        if (!enemy->isBoss) {
            float telegraphThreshold = 0.f;
            switch (enemy->type) {
                case EnemyType::Melee:
                case EnemyType::Elite:
                case EnemyType::Splitter:
                case EnemyType::Shielded:
                case EnemyType::StealthMelee:
                    telegraphThreshold = 0.3f; // 近战前摇 0.3s
                    break;
                case EnemyType::Ranged:
                case EnemyType::Caster:
                case EnemyType::SniperRanged:
                    telegraphThreshold = 0.5f; // 远程蓄力 0.5s
                    break;
                default:
                    break;
            }
            if (telegraphThreshold > 0.f &&
                enemy->attackCooldown > 0.f &&
                enemy->attackCooldown < telegraphThreshold) {
                // 仅当敌人在玩家附近（即将能攻击）时才设置前摇
                // 近战：attackRange + 40px 缓冲；远程：detectionRange 内
                bool inAttackImminent = false;
                if (enemy->type == EnemyType::Ranged || enemy->type == EnemyType::SniperRanged) {
                    inAttackImminent = (distToPlayer <= enemy->detectionRange);
                } else {
                    inAttackImminent = (distToPlayer <= enemy->attackRange + 40.f);
                }
                if (inAttackImminent) {
                    enemy->attackTelegraph = enemy->attackCooldown; // 距离攻击的剩余时间
                } else {
                    enemy->attackTelegraph = 0.f;
                }
            } else {
                enemy->attackTelegraph = 0.f;
            }
        }

        if (enemy->attackCooldown <= 0.f && playerHealth && playerTransform) {
            bool canAttack = false;

            switch (enemy->type) {
                case EnemyType::Melee:
                case EnemyType::Elite:
                case EnemyType::Boss:
                case EnemyType::Splitter:
                case EnemyType::Shielded: {
                    // 近战：接触玩家时造成伤害
                    // Splitter/Shielded：与普通近战相同的接触攻击
                    if (distToPlayer <= enemy->attackRange) {
                        canAttack = true;
                    }
                    break;
                }
                case EnemyType::StealthMelee: {
                    // 隐身怪：仅在现身状态下攻击
                    if (!enemy->isStealth && distToPlayer <= enemy->attackRange) {
                        canAttack = true;
                    }
                    break;
                }
                case EnemyType::Suicide: {
                    // 自爆：接触玩家时自爆
                    if (distToPlayer <= enemy->attackRange) {
                        canAttack = true;
                        // 自爆敌人攻击后死亡（标记 Health 为 0）
                        Health* myHealth = registry.GetComponent<Health>(id);
                        if (myHealth) {
                            myHealth->current = 0.f;
                        }
                    }
                    break;
                }
                case EnemyType::CountdownSuicide: {
                    // 倒计时自爆：不进行接触攻击，由倒计时触发爆炸
                    // 倒计时到 0 时已在上面标记 Health=0，爆炸伤害在 UpdateEnemyCombat 中处理
                    break;
                }
                case EnemyType::Ranged: {
                    // 远程：不进行接触攻击，由 UpdateEnemyCombat 处理射击
                    // 此处仅保持移动行为（已在上面处理）
                    break;
                }
                case EnemyType::Caster: {
                    // 施法者：不进行接触攻击，由 UpdateEnemyCombat 处理 AoE 施法
                    break;
                }
                case EnemyType::SniperRanged: {
                    // 狙击远程：不进行接触攻击，由 UpdateEnemyCombat 处理射击
                    break;
                }
            }

            if (canAttack) {
                // 对玩家造成伤害（检查无敌时间）
                if (playerHealth->invincibleTimer <= 0.f) {
                    playerHealth->current -= enemy->damage;
                    playerHealth->invincibleTimer = 0.5f; // 0.5s 无敌
                    // 玩家受伤红色飘字
                    SpawnDamageText(registry, playerPos, enemy->damage, false,
                                    sf::Color::Red);
                    // 播放玩家受伤音效
                    AudioManager::Instance().PlaySFX(AudioManager::kSFXPlayerHurt);

                    // ---- 第三十轮新增：死亡回顾 - 记录最后攻击者 ----
                    // ---- 第十八轮新增：玩家受伤重置连击 ----
                    // 近战接触伤害直接修改 HP 不经过 CombatSystem::ApplyDamage，
                    // 因此 OnHit 回调不会触发，需在此处手动重置 combo。
                    // 设计意图：避免玩家无脑肉搏堆 combo，受伤意味着走位失败。
                    PlayerComponent* playerPc = registry.GetComponent<PlayerComponent>(playerEntity);
                    if (playerPc) {
                        playerPc->lastAttackerEntity = id;
                        if (playerPc->comboCount > 0) {
                            playerPc->comboCount = 0;
                            playerPc->comboTimer = 0.f;
                        }
                    }
                }
                // 重置攻击冷却（远程 2s，狙击 2.5s，其他 1s）
                if (enemy->type == EnemyType::Ranged) {
                    enemy->attackCooldown = 2.0f;
                } else if (enemy->type == EnemyType::Caster) {
                    enemy->attackCooldown = 3.0f;
                } else if (enemy->type == EnemyType::SniperRanged) {
                    enemy->attackCooldown = 2.5f;
                } else {
                    enemy->attackCooldown = 1.0f;
                }
            }
        }
    });

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    return static_cast<float>(duration.count()) / 1000.f;
}

// ============================================================================
// UpdateEnemyCombat —— 敌人战斗更新（Phase 5）
// ----------------------------------------------------------------------------
// 处理远程射击、自爆爆炸、Boss AOE、死亡特效。
// 近战接触伤害已在 UpdateEnemyAI 中处理，本函数不重复。
//
// 死亡处理流程：
//   1. 检测 Health <= 0 且 active 的敌人
//   2. 触发 OnKill 回调（通过 CombatSystem）
//   3. 生成死亡粒子特效（SpawnDeathEffect）
//   4. 生成伤害飘字（可选）
//   5. 注意：实际回收到对象池由 EnemySpawner.Update 中的 recycleDeadEnemies 完成
// ============================================================================
float UpdateEnemyCombat(Registry& registry, UniformGrid& grid,
                        EntityId playerEntity, ProjectileSystem& projectiles,
                        ParticleSystem& particles, CombatSystem& combat, float dt,
                        EnemySpawner* spawner,
                        std::vector<FissureZone>* fissures,
                        std::vector<CastWarningZone>* castWarnings) {
    auto startTime = std::chrono::steady_clock::now();

    // 获取玩家位置
    Transform* playerTransform = registry.GetComponent<Transform>(playerEntity);
    sf::Vector2f playerPos(0.f, 0.f);
    if (playerTransform) {
        playerPos = playerTransform->position;
    }

    // 复用的临时缓冲
    std::vector<EntityId> neighbors;
    neighbors.reserve(64);

    // 遍历所有敌人
    registry.ForEach<EnemyComponent, Transform>([&](EntityId id) {
        EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(id);
        Transform* transform = registry.GetComponent<Transform>(id);
        if (!enemy || !transform) return;
        if (!enemy->active) return;

        Health* health = registry.GetComponent<Health>(id);
        sf::Vector2f myPos = transform->position;

        // ---- 死亡检测 ----
        // HP <= 0 的敌人触发死亡特效
        // 实际回收到对象池由 EnemySpawner 处理
        // 注意：OnKill 回调（经验/金币/掉落/任务进度）由 Game.cpp 的统一死亡检测处理，
        //       此处仅负责视觉/物理特效（死亡粒子、自爆伤害、分裂生成），
        //       避免与 Game.cpp 重复调用 OnKill 导致奖励翻倍。
        if (health && health->current <= 0.f) {
            // 生成死亡特效
            sf::Color deathColor = sf::Color(200, 50, 50);
            Sprite* sprite = registry.GetComponent<Sprite>(id);
            if (sprite) {
                deathColor = sprite->color;
            }
            SpawnDeathEffect(particles, myPos, deathColor);

            // ---- 自爆类敌人死亡时产生范围伤害 ----
            if (enemy->type == EnemyType::Suicide) {
                // 查询附近实体并造成范围伤害（范围扩大）
                neighbors.clear();
                grid.QueryRange(myPos, kSuicideExplodeRange, neighbors);

                for (EntityId targetId : neighbors) {
                    if (targetId == id) continue;

                    // 只对玩家造成伤害（自爆不伤害其他敌人）
                    if (targetId == playerEntity) {
                        DamageInfo dmg;
                        dmg.attacker = id;
                        dmg.target = targetId;
                        dmg.amount = enemy->damage * 2.5f; // 自爆伤害翻2.5倍
                        dmg.isCritical = false;
                        dmg.element = ElementType::Fire;
                        dmg.knockback = sf::Vector2f(350.f, 350.f); // 大击退，给玩家强烈的冲击反馈
                        combat.ApplyDamage(registry, dmg);
                    }
                }

                // 自爆额外爆炸特效：多次爆炸形成更大范围的视觉反馈
                particles.Explosion(myPos);
                particles.Explosion(myPos + sf::Vector2f(25.f, 0.f));
                particles.Explosion(myPos + sf::Vector2f(-25.f, 0.f));
                particles.Explosion(myPos + sf::Vector2f(0.f, 25.f));
                particles.Explosion(myPos + sf::Vector2f(0.f, -25.f));
                particles.Explosion(myPos + sf::Vector2f(18.f, 18.f));
                particles.Explosion(myPos + sf::Vector2f(-18.f, -18.f));
                AudioManager::Instance().PlaySFX(AudioManager::kSFXExplosion);
            }

            // ---- 倒计时自爆怪死亡时产生更大范围伤害 ----
            if (enemy->type == EnemyType::CountdownSuicide) {
                neighbors.clear();
                grid.QueryRange(myPos, 140.f, neighbors); // 更大范围

                for (EntityId targetId : neighbors) {
                    if (targetId == id) continue;

                    if (targetId == playerEntity) {
                        DamageInfo dmg;
                        dmg.attacker = id;
                        dmg.target = targetId;
                        dmg.amount = enemy->damage * 2.5f; // 倒计时自爆伤害更高
                        dmg.isCritical = false;
                        dmg.element = ElementType::Fire;
                        dmg.knockback = sf::Vector2f(400.f, 400.f); // 大击退
                        dmg.lifesteal = 0.f;
                        combat.ApplyDamage(registry, dmg);
                    }
                }

                // 更大的爆炸特效
                particles.Explosion(myPos);
                particles.Explosion(myPos + sf::Vector2f(30.f, 0.f));
                particles.Explosion(myPos + sf::Vector2f(-30.f, 0.f));
                particles.Explosion(myPos + sf::Vector2f(0.f, 30.f));
                particles.Explosion(myPos + sf::Vector2f(0.f, -30.f));
                particles.Explosion(myPos + sf::Vector2f(20.f, 20.f));
                particles.Explosion(myPos + sf::Vector2f(-20.f, -20.f));
                AudioManager::Instance().PlaySFX(AudioManager::kSFXExplosion);
            }

            // ---- 分裂怪死亡时生成 2 个小怪 ----
            if (enemy->type == EnemyType::Splitter && enemy->splitCount > 0 && spawner) {
                for (int i = 0; i < enemy->splitCount; ++i) {
                    // 在死亡位置附近随机生成小怪（Melee 类型，HP 减半）
                    float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
                    float dist = 20.f + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 20.f;
                    sf::Vector2f spawnPos(
                        myPos.x + std::cos(angle) * dist,
                        myPos.y + std::sin(angle) * dist
                    );
                    spawner->SpawnEnemyAt(EnemyType::Melee, spawnPos);
                    // 生成分裂特效
                    particles.Explosion(spawnPos);
                }
                LOG_INFO("分裂怪死亡，生成 %d 个小怪", enemy->splitCount);
            }

            return; // 死亡敌人不再处理攻击
        }

        // ---- 第二十一轮新增：激活 EnemyAffix 词缀系统 ----
        // 处理两类词缀效果：
        //   1. Regenerating：每秒回 1% maxHp（复用 EnemyAffix::regenTimer）
        //   2. 光环粒子：词缀敌人头顶定期发射紫色光环粒子（复用 EnemyAffix::auraTimer），
        //      让玩家能视觉识别"词缀精英"并优先击杀
        // 设计：词缀组件可能不存在（普通敌人未挂载），GetComponent 返回 nullptr 时跳过
        EnemyAffix* affix = registry.GetComponent<EnemyAffix>(id);
        if (affix && affix->affixMask != 0u) {
            // Regenerating 词缀：每秒回 1% maxHp
            if (HasEliteAffix(affix->affixMask, EliteAffix::Regenerating) && health) {
                affix->regenTimer += dt;
                if (affix->regenTimer >= 1.0f) {
                    affix->regenTimer -= 1.0f;
                    float regenAmount = health->max * 0.01f; // 1% maxHp
                    health->current = std::min(health->current + regenAmount, health->max);
                }
            }

            // 词缀光环粒子（紫色，0.3s 一次，让玩家识别词缀精英）
            affix->auraTimer += dt;
            if (affix->auraTimer >= 0.3f) {
                affix->auraTimer = 0.f;
                EmitConfig cfg;
                cfg.colorMin = sf::Color(180, 80, 255, 200);
                cfg.colorMax = sf::Color(220, 120, 255, 230);
                cfg.sizeMin = 2.f;
                cfg.sizeMax = 4.f;
                cfg.lifeMin = 0.4f;
                cfg.lifeMax = 0.7f;
                cfg.radial = true;
                cfg.speedMin = 20.f;
                cfg.speedMax = 50.f;
                // 在敌人头顶发射 2 个粒子（光环效果）
                particles.Emit(myPos + sf::Vector2f(0.f, -16.f), 2, cfg);
            }
        }

        // 计算到玩家的距离
        sf::Vector2f toPlayer = playerPos - myPos;
        float distToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

        // ---- Boss 独立机制（不受 AOE attackCooldown 限制）----
        // 【性能保护】场上 Boss 召唤物（isBossMinion）总数上限 kMaxBossMinions，
        //   避免长时间战斗无限堆积导致对象池耗尽 / FPS 骤降。
        //   上限 12 = 单次召唤 2 小兵 × 6 波次余量，兼顾压力与性能。
        constexpr int kMaxBossMinions = 12;
        if (enemy->type == EnemyType::Boss) {
            // 计时器递减
            enemy->rangedAttackTimer -= dt;
            enemy->summonTimer -= dt;

            // Boss 远程攻击：每 2s 朝玩家方向发射 3 发扇形子弹
            if (enemy->rangedAttackTimer <= 0.f && distToPlayer > 50.f && distToPlayer <= enemy->detectionRange) {
                sf::Vector2f shootDir = toPlayer / distToPlayer;
                for (int i = -1; i <= 1; ++i) {
                    float angle = i * 0.35f; // 约 20°
                    float cosA = std::cos(angle);
                    float sinA = std::sin(angle);
                    sf::Vector2f dir(
                        shootDir.x * cosA - shootDir.y * sinA,
                        shootDir.x * sinA + shootDir.y * cosA
                    );

                    ProjectileConfig config;
                    config.speed = 300.f;
                    config.damage = enemy->damage * 0.6f;
                    config.pierce = 0;
                    config.lifetime = 3.f;
                    config.radius = 6.f;
                    config.color = sf::Color(255, 80, 80, 255); // 红色 Boss 子弹
                    config.splitCount = 0;
                    config.chainCount = 0;
                    config.element = ElementType::Physical;
                    config.knockback = 60.f;

                    projectiles.Spawn(myPos, dir, config, id);
                }
                enemy->rangedAttackTimer = 2.0f;
            }

            // Boss 召唤小兵：每 8s 召唤 2 个近战小兵
            if (enemy->summonTimer <= 0.f && spawner) {
                // 统计当前场上 Boss 召唤物数量
                int activeMinions = 0;
                registry.ForEach<EnemyComponent>([&](EntityId mid) {
                    EnemyComponent* mc = registry.GetComponent<EnemyComponent>(mid);
                    if (mc && mc->active && mc->isBossMinion) {
                        ++activeMinions;
                    }
                });
                if (activeMinions < kMaxBossMinions) {
                    constexpr int kSummonCount = 2;
                    constexpr float kSummonRadius = 60.f;
                    for (int i = 0; i < kSummonCount; ++i) {
                        float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
                        sf::Vector2f spawnPos(
                            myPos.x + std::cos(angle) * kSummonRadius,
                            myPos.y + std::sin(angle) * kSummonRadius
                        );
                        EntityId minionId = spawner->SpawnEnemyAt(EnemyType::Melee, spawnPos);
                        if (minionId != kInvalidEntity) {
                            EnemyComponent* minion = registry.GetComponent<EnemyComponent>(minionId);
                            if (minion) {
                                minion->isBossMinion = true;
                            }
                            particles.Explosion(spawnPos);
                        }
                    }
                    LOG_INFO("Boss 召唤 %d 个小兵（当前场上召唤物 %d/%d）",
                             kSummonCount, activeMinions + kSummonCount, kMaxBossMinions);
                } else {
                    LOG_INFO("Boss 召唤物已达上限 %d，本次跳过召唤", kMaxBossMinions);
                }
                enemy->summonTimer = 8.0f;
            }

            // ---- Boss 新机制 1：冲撞 + 地裂 ----
            // 每 6s 触发一次冲撞，朝玩家方向高速直线移动 0.8s
            // 冲撞路径上每 0.15s 留下一个地裂区域（持续 5s，每秒造成 boss damage × 0.3 伤害）
            enemy->chargeTimer -= dt;
            if (enemy->chargeTimer <= 0.f && enemy->chargeActive <= 0.f &&
                distToPlayer > 80.f && distToPlayer <= enemy->detectionRange) {
                // 触发冲撞：朝玩家方向
                sf::Vector2f dir = toPlayer / distToPlayer;
                enemy->chargeDir = dir;
                enemy->chargeActive = 0.8f;  // 冲撞持续 0.8s
                enemy->chargeTimer = 6.0f;   // 冷却 6s
                // 冲撞起始爆炸特效
                particles.Explosion(myPos);
                LOG_INFO("Boss 发动冲撞，方向 (%.2f, %.2f)", dir.x, dir.y);
            }
            // 冲撞期间每 0.15s 留下地裂区域
            if (enemy->chargeActive > 0.f && fissures) {
                // 使用 specialTimer 作为地裂生成计时器（冲撞期间复用）
                enemy->specialTimer -= dt;
                if (enemy->specialTimer <= 0.f) {
                    FissureZone fz;
                    fz.position = myPos;
                    fz.radius = 40.f;
                    fz.lifetime = 5.f;
                    fz.damagePerSec = enemy->damage * 0.3f;
                    fz.damageTickTimer = 0.f;
                    fissures->push_back(fz);
                    // 地裂视觉特效（暗棕色冲击波）
                    particles.Explosion(myPos);
                    enemy->specialTimer = 0.15f; // 每 0.15s 留一个地裂点
                }
            }

            // ---- Boss 新机制 2：旋转弹幕 ----
            // 每 10s 触发一次，持续 2s 期间每 0.12s 发射 1 发低伤害子弹（约 1 点伤害）
            // 主要为视觉效果，子弹呈螺旋形分布
            enemy->spiralTimer -= dt;
            if (enemy->spiralTimer <= 0.f && enemy->spiralActive <= 0.f) {
                enemy->spiralActive = 2.0f;  // 持续 2s
                enemy->spiralTimer = 10.0f;  // 冷却 10s
                enemy->spiralAngle = 0.f;
                LOG_INFO("Boss 发动旋转弹幕");
            }
            if (enemy->spiralActive > 0.f) {
                enemy->spiralActive -= dt;
                // 使用独立的 spiralFireTimer 计时器，避免与冲撞地裂的 specialTimer 冲突。
                // 旋转弹幕与冲撞可能同时触发（CD 6s vs 10s，持续时间 0.8s vs 2s），
                // 此前复用 specialTimer 会导致冲撞期间旋转弹幕完全不发射。
                enemy->spiralFireTimer -= dt;
                if (enemy->spiralFireTimer <= 0.f) {
                    // 发射 1 发子弹，角度随时间递增形成螺旋
                    enemy->spiralAngle += 0.4f; // 每发偏移 0.4rad
                    sf::Vector2f dir(std::cos(enemy->spiralAngle), std::sin(enemy->spiralAngle));
                    ProjectileConfig config;
                    config.speed = 200.f;        // 慢速子弹
                    config.damage = 1.f;          // 伤害约 1 点（视觉为主）
                    config.pierce = 0;
                    config.lifetime = 4.f;
                    config.radius = 5.f;
                    config.color = sf::Color(220, 180, 255, 255); // 淡紫色
                    config.splitCount = 0;
                    config.chainCount = 0;
                    config.element = ElementType::Physical;
                    config.knockback = 10.f;
                    projectiles.Spawn(myPos, dir, config, id);
                    enemy->spiralFireTimer = 0.12f; // 每 0.12s 发射 1 发
                }
            }

            // ---- Boss 新机制 3：召唤精英怪 ----
            // HP < 50% 时立即召唤 1 个精英（仅触发一次）
            // 之后每 12s 召唤 1 个精英（持续压力）
            // 【性能保护】同样受 kMaxBossMinions 上限约束，与小兵共享配额。
            enemy->eliteSummonTimer -= dt;
            if (health) {
                float hpRatio = health->current / health->max;
                if (!enemy->eliteSummoned50 && hpRatio <= 0.5f && spawner) {
                    // HP 首次跌破 50%，立即召唤 1 精英（受上限约束）
                    int activeMinions = 0;
                    registry.ForEach<EnemyComponent>([&](EntityId mid) {
                        EnemyComponent* mc = registry.GetComponent<EnemyComponent>(mid);
                        if (mc && mc->active && mc->isBossMinion) {
                            ++activeMinions;
                        }
                    });
                    if (activeMinions < kMaxBossMinions) {
                        float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
                        sf::Vector2f spawnPos(
                            myPos.x + std::cos(angle) * 70.f,
                            myPos.y + std::sin(angle) * 70.f
                        );
                        EntityId eliteId = spawner->SpawnEnemyAt(EnemyType::Elite, spawnPos);
                        if (eliteId != kInvalidEntity) {
                            EnemyComponent* elite = registry.GetComponent<EnemyComponent>(eliteId);
                            if (elite) {
                                elite->isBossMinion = true; // 标记为 Boss 召唤物（掉落爱心）
                            }
                            particles.Explosion(spawnPos);
                        }
                        LOG_INFO("Boss HP<50%%，召唤 1 个精英怪（场上召唤物 %d/%d）",
                                 activeMinions + 1, kMaxBossMinions);
                    } else {
                        LOG_INFO("Boss 召唤物已达上限 %d，HP<50%% 精英召唤跳过", kMaxBossMinions);
                    }
                    enemy->eliteSummoned50 = true;
                    enemy->eliteSummonTimer = 12.0f; // 之后每 12s 召唤一次
                }
                if (enemy->eliteSummoned50 && enemy->eliteSummonTimer <= 0.f && spawner) {
                    // 持续召唤精英（受上限约束）
                    int activeMinions = 0;
                    registry.ForEach<EnemyComponent>([&](EntityId mid) {
                        EnemyComponent* mc = registry.GetComponent<EnemyComponent>(mid);
                        if (mc && mc->active && mc->isBossMinion) {
                            ++activeMinions;
                        }
                    });
                    if (activeMinions < kMaxBossMinions) {
                        float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
                        sf::Vector2f spawnPos(
                            myPos.x + std::cos(angle) * 70.f,
                            myPos.y + std::sin(angle) * 70.f
                        );
                        EntityId eliteId = spawner->SpawnEnemyAt(EnemyType::Elite, spawnPos);
                        if (eliteId != kInvalidEntity) {
                            EnemyComponent* elite = registry.GetComponent<EnemyComponent>(eliteId);
                            if (elite) {
                                elite->isBossMinion = true;
                            }
                            particles.Explosion(spawnPos);
                        }
                        LOG_INFO("Boss 持续召唤 1 个精英怪（场上召唤物 %d/%d）",
                                 activeMinions + 1, kMaxBossMinions);
                    } else {
                        LOG_INFO("Boss 召唤物已达上限 %d，持续精英召唤跳过", kMaxBossMinions);
                    }
                    enemy->eliteSummonTimer = 12.0f;
                }
            }
        }

        // ---- 施法者（Caster）AoE 施法逻辑 ----
        // 设计意图：施法者保持中距离，周期性地在玩家当前位置召唤地面 AoE 法阵。
        // 法阵有 1.5s 预警时间（红色范围圈），玩家可在此期间走出范围规避伤害。
        // 产生"站桩 vs 走位"的战术决策，丰富战斗交互层次。
        // 预警圈通过 CastWarningZone 列表由外部渲染系统绘制。
        constexpr float kCasterCastCD = 3.5f;
        constexpr float kCasterCastDuration = 1.5f;
        constexpr float kCasterAoERadius = 80.f;
        if (enemy->type == EnemyType::Caster) {
            enemy->castTimer -= dt;
            // 仅在合适距离、非 Boss 时释放（当前层未持有雷暴领域修饰符也可施法）
            if (enemy->castTimer <= 0.f && !enemy->isBoss &&
                distToPlayer > 50.f && distToPlayer <= 500.f &&
                enemy->castActive <= 0.f) {
                // 开始施法：标记目标位置为玩家当前位置
                enemy->castActive = kCasterCastDuration;
                enemy->castTimer = kCasterCastCD;
                enemy->castTargetPos = playerPos;
                enemy->castWarningRadius = kCasterAoERadius;
                // 创建预警圈
                if (castWarnings) {
                    CastWarningZone wz;
                    wz.position = enemy->castTargetPos;
                    wz.radius = kCasterAoERadius;
                    wz.lifetime = kCasterCastDuration;
                    wz.damage = enemy->damage * 1.2f; // AoE 比普攻略高
                    wz.exploded = false;
                    castWarnings->push_back(wz);
                }
                // 施法开始特效（紫色粒子爆发）
                particles.Explosion(myPos + sf::Vector2f(0.f, -10.f));
                LOG_INFO("施法者开始施法，目标 (%.0f, %.0f)", playerPos.x, playerPos.y);
            }
            // 施法进行中：castActive 递减，由外部渲染绘制预警圈
            if (enemy->castActive > 0.f) {
                enemy->castActive -= dt;
            }
        }

        // ---- 处理 CastWarningZone 爆炸 ----
        if (castWarnings) {
            for (auto& wz : *castWarnings) {
                wz.lifetime -= dt;
                if (wz.lifetime <= 0.f && !wz.exploded) {
                    wz.exploded = true;
                    // 查询范围内的实体造成伤害
                    neighbors.clear();
                    float queryRadius = wz.radius * 0.8f; // 伤害范围略小于预警范围，给予边缘容错
                    grid.QueryRange(wz.position, queryRadius, neighbors);
                    for (EntityId targetId : neighbors) {
                        if (targetId == playerEntity) {
                            DamageInfo dmg;
                            dmg.attacker = id;
                            dmg.target = targetId;
                            dmg.amount = wz.damage;
                            dmg.isCritical = false;
                            dmg.element = ElementType::Fire; // 火焰元素 AoE
                            dmg.knockback = sf::Vector2f(0.f, 0.f);
                            dmg.lifesteal = 0.f;
                            combat.ApplyDamage(registry, dmg);
                        }
                    }
                    // 爆炸特效
                    particles.Explosion(wz.position);
                    AudioManager::Instance().PlaySFX(AudioManager::kSFXExplosion);
                    LOG_INFO("施法者 AoE 爆炸，位置 (%.0f, %.0f), 伤害=%.0f",
                             wz.position.x, wz.position.y, wz.damage);
                }
            }
            // 清理已爆炸的预警圈
            castWarnings->erase(
                std::remove_if(castWarnings->begin(), castWarnings->end(),
                    [](const CastWarningZone& z) { return z.exploded; }),
                castWarnings->end()
            );
        }
        if (enemy->attackCooldown > 0.f) return;

        // ---- 远程敌人射击 ----
        if (enemy->type == EnemyType::Ranged) {
            if (distToPlayer <= enemy->detectionRange && distToPlayer > 10.f) {
                // 朝玩家方向发射子弹
                sf::Vector2f shootDir = toPlayer / distToPlayer;

                ProjectileConfig config;
                config.speed = 250.f;
                config.damage = enemy->damage;
                config.pierce = 0;
                config.lifetime = 3.f;
                config.radius = 5.f;
                config.color = sf::Color(200, 100, 255, 255); // 紫色敌人子弹
                config.splitCount = 0;
                config.chainCount = 0;
                config.element = ElementType::Physical;
                config.knockback = 50.f;

                // owner 设为敌人自身，区分敌我子弹
                projectiles.Spawn(myPos, shootDir, config, id);

                // 重置攻击冷却（2s）
                enemy->attackCooldown = 2.0f;
            }
        }

        // ---- 狙击远程敌人射击（高伤害、快速子弹、长射程）----
        if (enemy->type == EnemyType::SniperRanged) {
            if (distToPlayer <= enemy->detectionRange && distToPlayer > 50.f) {
                sf::Vector2f shootDir = toPlayer / distToPlayer;

                ProjectileConfig config;
                config.speed = 400.f;       // 狙击子弹更快
                config.damage = enemy->damage;
                config.pierce = 0;
                config.lifetime = 4.f;     // 射程更长
                config.radius = 4.f;        // 子弹更小（难躲避）
                config.color = sf::Color(80, 255, 200, 255); // 青色狙击子弹
                config.splitCount = 0;
                config.chainCount = 0;
                config.element = ElementType::Physical;
                config.knockback = 80.f;    // 击退更强

                projectiles.Spawn(myPos, shootDir, config, id);

                // 重置攻击冷却（2.5s）
                enemy->attackCooldown = 2.5f;
            }
        }

        // ---- Boss AOE 技能（受 attackCooldown 限制，每 3s 一次）----
        if (enemy->type == EnemyType::Boss) {
            if (distToPlayer <= enemy->detectionRange) {
                // Boss 定期释放 AOE：以自身为中心范围伤害
                neighbors.clear();
                grid.QueryRange(myPos, 120.f, neighbors);

                for (EntityId targetId : neighbors) {
                    if (targetId == id) continue;
                    if (targetId == playerEntity) {
                        DamageInfo dmg;
                        dmg.attacker = id;
                        dmg.target = targetId;
                        dmg.amount = enemy->damage * 0.5f; // AOE 伤害减半
                        dmg.isCritical = false;
                        dmg.element = ElementType::Physical;
                        dmg.knockback = sf::Vector2f(0.f, 0.f);
                        dmg.lifesteal = 0.f;
                        combat.ApplyDamage(registry, dmg);
                    }
                }

                // Boss AOE 特效
                particles.Explosion(myPos);

                // 重置攻击冷却（3s）
                enemy->attackCooldown = 3.0f;
            }
        }
    });

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    return static_cast<float>(duration.count()) / 1000.f;
}

} // namespace cu
