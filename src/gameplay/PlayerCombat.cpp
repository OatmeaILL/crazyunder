#include "gameplay/PlayerCombat.h"
#include "gameplay/ProjectileSystem.h"
#include "gameplay/CombatSystem.h"
#include "gameplay/Player.h"
#include "gameplay/EnemyAI.h"   // EnemyComponent（AOE 只伤害敌人）
#include "gameplay/DungeonGenerator.h" // Dungeon, TileType
#include "gameplay/SkillSystem.h"
#include "gameplay/ClassSystem.h"
#include "core/Input.h"
#include "core/AudioManager.h"
#include "rendering/Camera.h"
#include "rendering/ParticleSystem.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "utils/UniformGrid.h"
#include "utils/Logger.h"
#include <cmath>
#include <cstdlib>

namespace cu {

// ============================================================================
// UpdatePlayerCombat —— 玩家战斗逻辑更新
// ============================================================================
void UpdatePlayerCombat(Registry& registry, const Input& input,
                        EntityId player, ProjectileSystem& projectiles,
                        ParticleSystem& particles, Camera& camera,
                        UniformGrid& grid, CombatSystem& combat,
                        Dungeon* dungeon, float dt) {
    Transform* transform = registry.GetComponent<Transform>(player);
    PlayerComponent* playerComp = registry.GetComponent<PlayerComponent>(player);
    Velocity* velocity = registry.GetComponent<Velocity>(player);
    Health* health = registry.GetComponent<Health>(player);

    if (!transform || !playerComp) return;

    // ---- 1. 衰减冷却计时器 ----
    if (playerComp->attackCooldown > 0.f) {
        playerComp->attackCooldown -= dt;
    }
    // 剑士剑体贴图显示计时器衰减（独立于 attackTimer，显示时间更长）
    if (playerComp->swordDisplayTimer > 0.f) {
        playerComp->swordDisplayTimer -= dt;
        if (playerComp->swordDisplayTimer < 0.f) playerComp->swordDisplayTimer = 0.f;
    }
    if (playerComp->dodgeCooldown > 0.f) {
        playerComp->dodgeCooldown -= dt;
    }
    if (playerComp->dodgeInvincibility > 0.f) {
        playerComp->dodgeInvincibility -= dt;
    }
    if (playerComp->aoeCooldown > 0.f) {
        playerComp->aoeCooldown -= dt;
    }
    // 第二十轮新增：极限闪避计时器衰减
    // buff 持续 2s（伤害 +50%），cooldown 3s 避免连续触发
    if (playerComp->perfectDodgeBuffTimer > 0.f) {
        playerComp->perfectDodgeBuffTimer -= dt;
        if (playerComp->perfectDodgeBuffTimer < 0.f) playerComp->perfectDodgeBuffTimer = 0.f;
    }
    if (playerComp->perfectDodgeCooldown > 0.f) {
        playerComp->perfectDodgeCooldown -= dt;
        if (playerComp->perfectDodgeCooldown < 0.f) playerComp->perfectDodgeCooldown = 0.f;
    }
    // 技能槽冷却衰减
    for (int i = 0; i < kSkillSlotCount; ++i) {
        if (playerComp->skillSlots[i].cooldownRemain > 0.f) {
            playerComp->skillSlots[i].cooldownRemain -= dt;
        }
    }

    // ---- 2. 处理闪避冲刺 ----
    // 冲刺期间应用额外速度，使用轴分离碰撞检测避免穿墙
    if (playerComp->dodgeDashTimer > 0.f) {
        playerComp->dodgeDashTimer -= dt;

        // 闪避音效：剩余2次急促播放
        if (playerComp->dodgeSoundCount > 0) {
            playerComp->dodgeSoundTimer -= dt;
            if (playerComp->dodgeSoundTimer <= 0.f) {
                AudioManager::Instance().PlaySFX(AudioManager::kSFXFootstep);
                --playerComp->dodgeSoundCount;
                playerComp->dodgeSoundTimer = 0.05f; // 间隔50ms
            }
        }

        float dashSpeed = playerComp->stats.moveSpeed * kDodgeSpeedMultiplier;
        sf::Vector2f delta = playerComp->dodgeDirection * dashSpeed * dt;

        if (dungeon && !dungeon->tiles.empty()) {
            // 轴分离碰撞（与 UpdatePlayer 正常移动一致）
            Collider* col = registry.GetComponent<Collider>(player);
            float radius = col ? col->radius * 0.6f : 10.f;

            auto isBlockedAt = [dungeon, radius](sf::Vector2f p) -> bool {
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
            // X 轴单独移动
            sf::Vector2f tryX(curPos.x + delta.x, curPos.y);
            if (!isBlockedAt(tryX)) {
                curPos.x = tryX.x;
            }
            // Y 轴单独移动
            sf::Vector2f tryY(curPos.x, curPos.y + delta.y);
            if (!isBlockedAt(tryY)) {
                curPos.y = tryY.y;
            }
            transform->position = curPos;
        } else {
            transform->position += delta;
        }
    }

    // 无敌期间同步到 Health 组件
    if (health) {
        if (playerComp->dodgeInvincibility > 0.f) {
            health->invincibleTimer = playerComp->dodgeInvincibility;
        }
    }

   // ---- 3. 普攻（左键）----
    // 攻击间隔 = 1 / attackSpeed
    // 左键按下且冷却结束 → 根据职业执行不同攻击
    if (input.IsMouseDown(sf::Mouse::Left) && playerComp->attackCooldown <= 0.f) {
        // 计算攻击间隔
        float attackInterval = 1.f / playerComp->stats.attackSpeed;
        playerComp->attackCooldown = attackInterval;

        // 计算攻击方向（玩家 → 鼠标世界坐标）
        sf::Vector2f mouseWorld = input.GetMouseWorldPosition(camera);
        sf::Vector2f attackDir = mouseWorld - transform->position;
        float lenSq = attackDir.x * attackDir.x + attackDir.y * attackDir.y;
        if (lenSq > 0.0001f) {
            float len = std::sqrt(lenSq);
            attackDir /= len;
        } else {
            attackDir = sf::Vector2f(1.f, 0.f);
        }

        // 触发攻击动画
        playerComp->attackTimer = 0.2f;

        if (IsMeleeClass(playerComp->playerClass)) {
            // 剑士：剑体贴图显示时间更长（0.8s），让玩家看清剑头方向和挥砍轨迹
            playerComp->swordDisplayTimer = 0.8f;
            // ---- 剑士：近战扇形斩击 ----
            // 攻击范围：前方 120° 扇形，半径 80px
            const float kMeleeRange = 80.f;
            const float kMeleeAngle = 60.f; // 半角 60°，全角 120°
            float cosHalfAngle = std::cos(kMeleeAngle * 3.14159265f / 180.f); // cos(60°) = 0.5

            // 记录攻击方向供渲染剑体贴图使用
            playerComp->lastAttackDir = attackDir;

            // 屏幕震动（轻微）
            camera.Shake(3.f, 0.1f);

            // 斩击粒子特效（扇形方向上的弧形粒子爆发）
            {
                // 在攻击方向上生成扇形粒子
                float baseAngle = std::atan2(attackDir.y, attackDir.x);
                EmitConfig cfg;
                cfg.radial = false;
                cfg.speedMin = 100.f;
                cfg.speedMax = 250.f;
                cfg.colorMin = sf::Color(255, 200, 80, 255);
                cfg.colorMax = sf::Color(255, 240, 150, 255);
                cfg.sizeMin = 3.f;
                cfg.sizeMax = 7.f;
                cfg.lifeMin = 0.15f;
                cfg.lifeMax = 0.3f;
                // 在扇形范围内随机方向生成粒子
                for (int i = 0; i < 12; ++i) {
                    float spread = (std::rand() % 120 - 60) * 3.14159265f / 180.f; // -60° ~ +60°
                    float angle = baseAngle + spread;
                    sf::Vector2f dir(std::cos(angle), std::sin(angle));
                    sf::Vector2f pos = transform->position + dir * 40.f;
                    particles.Emit(pos, 1, cfg);
                }
            }

            // 音效（剑士专属挥砍音效）
            AudioManager::Instance().PlaySFX(AudioManager::kSFXSwordSweep);

            // 查询范围内敌人
            std::vector<EntityId> targets;
            targets.reserve(32);
            grid.QueryRange(transform->position, kMeleeRange, targets);

            float meleeDamage = playerComp->stats.damage;
            // 狂暴加成
            bool berserkActive = (playerComp->berserkTimer > 0.f);
            if (berserkActive) {
                meleeDamage *= 1.5f;
            }

            for (EntityId tid : targets) {
                if (tid == player) continue;
                EnemyComponent* te = registry.GetComponent<EnemyComponent>(tid);
                if (!te || !te->active) continue;
                Transform* tt = registry.GetComponent<Transform>(tid);
                if (!tt) continue;

                sf::Vector2f toTarget = tt->position - transform->position;
                float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
                if (dist > kMeleeRange) continue;

                // 扇形角度判定
                if (dist > 0.1f) {
                    sf::Vector2f normTarget = toTarget / dist;
                    float dot = normTarget.x * attackDir.x + normTarget.y * attackDir.y;
                    if (dot < cosHalfAngle) continue; // 超出扇形范围
                }

                // 施加伤害
                DamageInfo dmg;
                dmg.attacker = player;
                dmg.target = tid;
                dmg.amount = meleeDamage;
                dmg.isCritical = false;
                dmg.element = berserkActive ? ElementType::Fire : ElementType::Physical;
                dmg.knockback = (dist > 0.1f) ? (toTarget / dist) * 200.f : sf::Vector2f(0.f, 0.f);
                dmg.lifesteal = playerComp->stats.lifesteal;
                combat.ApplyDamage(registry, dmg);
            }

            // ---- 破坏扇形范围内的罐子和门 ----
            // 近战攻击可以破坏前方扇形区域内的可破坏物
            if (dungeon && !dungeon->tiles.empty()) {
                // 遍历玩家周围 kMeleeRange+32 范围内的 tile
                sf::Vector2i playerTile = dungeon->WorldToTile(transform->position);
                int tileRange = static_cast<int>(kMeleeRange / 32.f) + 1;
                for (int ty = playerTile.y - tileRange; ty <= playerTile.y + tileRange; ++ty) {
                    for (int tx = playerTile.x - tileRange; tx <= playerTile.x + tileRange; ++tx) {
                        if (tx < 0 || tx >= dungeon->width || ty < 0 || ty >= dungeon->height) continue;

                        TileType t = dungeon->GetTile(tx, ty);
                        if (t != TileType::Obstacle && t != TileType::Door) continue;

                        // 计算 tile 中心到玩家的距离和角度
                        sf::Vector2f tileCenter = dungeon->TileCenterToWorld(sf::Vector2i(tx, ty));
                        sf::Vector2f toTile = tileCenter - transform->position;
                        float tileDist = std::sqrt(toTile.x * toTile.x + toTile.y * toTile.y);
                        if (tileDist > kMeleeRange) continue;

                        // 扇形角度判定
                        if (tileDist > 0.1f) {
                            sf::Vector2f normTile = toTile / tileDist;
                            float dot = normTile.x * attackDir.x + normTile.y * attackDir.y;
                            if (dot < cosHalfAngle) continue;
                        }

                        if (t == TileType::Obstacle) {
                            // 破坏罐子
                            dungeon->SetTile(tx, ty, TileType::Floor);
                            particles.Explosion(tileCenter);
                            if (projectiles.onPotBroken) {
                                projectiles.onPotBroken(tileCenter);
                            }
                            LOG_INFO("剑士近战破坏罐子 (%d,%d)", tx, ty);
                        } else if (t == TileType::Door) {
                            // 破坏门
                            DoorState* ds = dungeon->GetDoorState(tx, ty);
                            if (ds && !ds->open) {
                                ds->hp -= meleeDamage;
                                particles.HitSpark(tileCenter);
                                if (ds->hp <= 0.f) {
                                    dungeon->SetTile(tx, ty, TileType::Floor);
                                    dungeon->doorStates.erase(ty * dungeon->width + tx);
                                    particles.Explosion(tileCenter);
                                    if (projectiles.onDoorBroken) {
                                        projectiles.onDoorBroken(tileCenter);
                                    }
                                    LOG_INFO("剑士近战破坏门 (%d,%d)", tx, ty);
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // ---- 法师：远程弹幕攻击（原有逻辑）----
            ProjectileConfig config;
            config.speed = 500.f;
            config.damage = playerComp->stats.damage;
            bool berserkActive = (playerComp->berserkTimer > 0.f);
            bool hasChainLightning = (playerComp->stats.chainLightning > 0);
            bool floorLightning = playerComp->stats.floorLightningActive;
            if (berserkActive) {
                config.damage *= 1.5f;
                config.element = ElementType::Fire;
                config.color = sf::Color(255, 110, 50, 255);
            } else if (hasChainLightning || floorLightning) {
                config.element = ElementType::Lightning;
                config.color = sf::Color(255, 230, 80, 255);
            } else {
                config.element = ElementType::Physical;
                config.color = sf::Color(255, 255, 100, 255);
            }
            float lifestealBonus = 0.f;
            if (playerComp->leechStrikeActive > 0.f) {
                lifestealBonus = 0.3f + 0.1f * (playerComp->leechStrikeLevel - 1);
            }
            config.pierce = 0 + playerComp->stats.projectileBonusPierce;
            config.lifetime = 2.f;
            config.radius = 6.f;
            config.splitCount = playerComp->stats.projectileBonusSplit;
            config.chainCount = playerComp->stats.chainLightning;
            config.knockback = 150.f;
            config.lifesteal = playerComp->stats.lifesteal + lifestealBonus;

            projectiles.Spawn(transform->position, attackDir, config, player);
            AudioManager::Instance().PlaySFX(AudioManager::kSFXShoot);
        }
    }

    // ---- 4. 闪避（右键）----
    // 使用 IsMouseDown + dodgeButtonHeld 标志，避免边沿检测漏帧
    if (input.IsMouseDown(sf::Mouse::Right)) {
        if (!playerComp->dodgeButtonHeld && playerComp->dodgeCooldown <= 0.f) {
            playerComp->dodgeButtonHeld = true;
            playerComp->dodgeCooldown = std::max(0.f, kDodgeCooldown - playerComp->stats.dodgeCooldownReduce);
            playerComp->dodgeInvincibility = kDodgeInvincibility;
            playerComp->dodgeDashTimer = kDodgeDashDuration;

            // 确定冲刺方向：
            // 优先使用当前移动方向（WASD），若静止则朝鼠标方向
            sf::Vector2f dashDir(0.f, 0.f);
            if (input.IsActionDown("MoveUp"))    dashDir.y -= 1.f;
            if (input.IsActionDown("MoveDown"))  dashDir.y += 1.f;
            if (input.IsActionDown("MoveLeft"))  dashDir.x -= 1.f;
            if (input.IsActionDown("MoveRight")) dashDir.x += 1.f;

            if (dashDir.x == 0.f && dashDir.y == 0.f) {
                // 静止时朝鼠标方向冲刺
                sf::Vector2f mouseWorld = input.GetMouseWorldPosition(camera);
                dashDir = mouseWorld - transform->position;
            }

            // 归一化
            float lenSq = dashDir.x * dashDir.x + dashDir.y * dashDir.y;
            if (lenSq > 0.0001f) {
                float len = std::sqrt(lenSq);
                dashDir /= len;
            } else {
                dashDir = sf::Vector2f(0.f, 1.f); // 默认向下
            }

            playerComp->dodgeDirection = dashDir;

            // 闪避音效：急促3次脚步声
            playerComp->dodgeSoundCount = 3;
            playerComp->dodgeSoundTimer = 0.f;
            AudioManager::Instance().PlaySFX(AudioManager::kSFXFootstep);
            --playerComp->dodgeSoundCount;

            // ---- 第二十轮新增：极限闪避反击检测 ----
            // 触发条件：
            //   1. 极限闪避冷却已就绪（perfectDodgeCooldown <= 0）
            //   2. 玩家附近 80px 内有敌人且敌人 attackTelegraph > 0（攻击前摇中）
            // 触发效果：
            //   - perfectDodgeBuffTimer = 2s（伤害 +50%）
            //   - perfectDodgeCooldown = 3s（避免连续触发）
            //   - 反击：闪避方向扇形 120° 半径 100px 范围内敌人受到 200% 玩家伤害
            //   - 屏幕震动 + 金色粒子爆发 + "极限闪避!" 飘字
            //   - 圣物"月光护符"通过 dodgeWindowMul 放宽检测窗口（80px → 120px）
            if (playerComp->perfectDodgeCooldown <= 0.f) {
                // 检测窗口：基础 80px，月光护符加成后 120px
                const float kPerfectDodgeRange = 80.f * playerComp->stats.dodgeWindowMul;
                std::vector<EntityId> nearbyEnemies;
                nearbyEnemies.reserve(16);
                grid.QueryRange(transform->position, kPerfectDodgeRange, nearbyEnemies);

                bool triggered = false;
                for (EntityId eid : nearbyEnemies) {
                    EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(eid);
                    if (!enemy || !enemy->active) continue;
                    if (enemy->attackTelegraph > 0.f) {
                        triggered = true;
                        break;
                    }
                }

                if (triggered) {
                    // 触发极限闪避
                    playerComp->perfectDodgeBuffTimer = 2.0f;  // 2s 伤害 +50%
                    playerComp->perfectDodgeCooldown = 3.0f;   // 3s 冷却
                    ++playerComp->perfectDodgeCount;
                    if (playerComp->perfectDodgeCount > playerComp->perfectDodgeMaxThisLife) {
                        playerComp->perfectDodgeMaxThisLife = playerComp->perfectDodgeCount;
                    }

                    // 圣物"复仇之刃"效果：buff 期间所有攻击必暴击
                    // 实现位置：CombatSystem::ApplyDamage 中检测
                    //   attacker->perfectDodgeBuffTimer > 0 && stats.perfectDodgeGuaranteedCrit
                    //   时强制 isCritical = true（无需在此处设置标志）

                    // 屏幕震动（与 AOE 同等级别）
                    camera.Shake(8.f, 0.3f);

                    // 金色粒子爆发（极限闪避特效）
                    particles.Explosion(transform->position);

                    // 反击伤害：闪避方向扇形 120° 半径 100px 范围内敌人
                    const float kCounterRange = 100.f;
                    const float kCounterDamageMul = 2.0f; // 200% 玩家伤害
                    std::vector<EntityId> counterTargets;
                    counterTargets.reserve(32);
                    grid.QueryRange(transform->position, kCounterRange, counterTargets);

                    float counterDamage = playerComp->stats.damage * kCounterDamageMul;
                    for (EntityId tid : counterTargets) {
                        if (tid == player) continue;
                        EnemyComponent* te = registry.GetComponent<EnemyComponent>(tid);
                        if (!te || !te->active) continue;
                        Transform* tt = registry.GetComponent<Transform>(tid);
                        if (!tt) continue;

                        sf::Vector2f toTarget = tt->position - transform->position;
                        float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
                        if (dist > kCounterRange) continue;

                        // 扇形角度判定（基于闪避方向）
                        if (dist > 0.1f) {
                            sf::Vector2f normTarget = toTarget / dist;
                            float dot = normTarget.x * dashDir.x + normTarget.y * dashDir.y;
                            // dot = cos(角度)，半角 60° → cos(60°) = 0.5
                            if (dot < 0.5f) continue; // 超出扇形范围
                        }

                        // 施加反击伤害（不触发暴击/吸血/击退，避免循环）
                        DamageInfo counterDmg;
                        counterDmg.attacker = player;
                        counterDmg.target = tid;
                        counterDmg.amount = counterDamage;
                        counterDmg.isCritical = false;
                        counterDmg.element = ElementType::Physical;
                        counterDmg.lifesteal = 0.f;
                        combat.ApplyDamage(registry, counterDmg);
                    }

                    LOG_INFO("极限闪避触发！累计 %d 次，buff 2s (+50%% 伤害)",
                             playerComp->perfectDodgeCount);
                }
            }
        }
    } else {
        // 右键释放后重置标志，允许下次闪避
        playerComp->dodgeButtonHeld = false;
    }

    // ---- 5. AOE 技能（空格）----
    // 冷却 8s，范围伤害 + 粒子 + 屏幕震动
    if (input.IsKeyDown(sf::Keyboard::Space) && playerComp->aoeCooldown <= 0.f) {
        playerComp->aoeCooldown = kAOECooldown - playerComp->stats.aoeCooldownReduce;

        // 触发爆炸粒子与 AOE 音效
        particles.Explosion(transform->position);
        AudioManager::Instance().PlaySFX(AudioManager::kSFXAOE);

        // 触发屏幕震动
        camera.Shake(kAOEShakeMagnitude, kAOEShakeDuration);

        // 查询范围内的敌人并造成伤害
        std::vector<EntityId> targets;
        targets.reserve(64);
        grid.QueryRange(transform->position, kAOERadius, targets);

        float aoeDamage = playerComp->stats.damage * kAOEDamageMultiplier;

        for (EntityId targetId : targets) {
            if (targetId == player) continue;

            EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(targetId);
            if (!enemy || !enemy->active) continue;

            Transform* enemyTransform = registry.GetComponent<Transform>(targetId);
            if (!enemyTransform) continue;

            // 计算击退方向（从玩家指向敌人）
            sf::Vector2f knockbackDir = enemyTransform->position - transform->position;
            float knockbackLen = std::sqrt(knockbackDir.x * knockbackDir.x +
                                            knockbackDir.y * knockbackDir.y);
            if (knockbackLen > 0.001f) {
                knockbackDir /= knockbackLen;
            }

            // 构造伤害信息
            DamageInfo dmg;
            dmg.attacker = player;
            dmg.target = targetId;
            dmg.amount = aoeDamage;
            dmg.isCritical = false; // AOE 不暴击
            dmg.element = ElementType::Physical;
            dmg.knockback = knockbackDir * 300.f; // 强力击退
            dmg.lifesteal = 0.f;

            combat.ApplyDamage(registry, dmg);
        }

        LOG_INFO("AOE 技能释放: 伤害=%.1f, 命中=%zu 个敌人",
                 aoeDamage, targets.size());
    }

    // ---- 6. 技能按键1-4 ----
    static const sf::Keyboard::Key skillKeys[kSkillSlotCount] = {
        sf::Keyboard::Num1, sf::Keyboard::Num2,
        sf::Keyboard::Num3, sf::Keyboard::Num4
    };
    for (int i = 0; i < kSkillSlotCount; ++i) {
        if (input.IsKeyDown(skillKeys[i])) {
            SkillInstance& slot = playerComp->skillSlots[i];
            if (slot.type != SkillType::Count && slot.cooldownRemain <= 0.f) {
                const SkillData& sd = GetSkillData(slot.type);
                if (ExecuteSkill(registry, player, slot.type,
                                 particles, camera, grid, combat, dungeon)) {
                    slot.cooldownRemain = sd.cooldown;
                }
            }
        }
    }
}

} // namespace cu
