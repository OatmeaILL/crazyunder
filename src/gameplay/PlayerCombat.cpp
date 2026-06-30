#include "gameplay/PlayerCombat.h"
#include "gameplay/ProjectileSystem.h"
#include "gameplay/CombatSystem.h"
#include "gameplay/Player.h"
#include "gameplay/EnemyAI.h"   // EnemyComponent（AOE 只伤害敌人）
#include "gameplay/DungeonGenerator.h" // Dungeon, TileType
#include "gameplay/SkillSystem.h"
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
                        const Dungeon* dungeon, float dt) {
    Transform* transform = registry.GetComponent<Transform>(player);
    PlayerComponent* playerComp = registry.GetComponent<PlayerComponent>(player);
    Velocity* velocity = registry.GetComponent<Velocity>(player);
    Health* health = registry.GetComponent<Health>(player);

    if (!transform || !playerComp) return;

    // ---- 1. 衰减冷却计时器 ----
    if (playerComp->attackCooldown > 0.f) {
        playerComp->attackCooldown -= dt;
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
    // 左键按下且冷却结束 → 发射子弹
    if (input.IsMouseDown(sf::Mouse::Left) && playerComp->attackCooldown <= 0.f) {
        // 计算攻击间隔
        float attackInterval = 1.f / playerComp->stats.attackSpeed;
        playerComp->attackCooldown = attackInterval;

        // 计算射击方向（玩家 → 鼠标世界坐标）
        sf::Vector2f mouseWorld = input.GetMouseWorldPosition(camera);
        sf::Vector2f shootDir = mouseWorld - transform->position;
        // 归一化
        float lenSq = shootDir.x * shootDir.x + shootDir.y * shootDir.y;
        if (lenSq > 0.0001f) {
            float len = std::sqrt(lenSq);
            shootDir /= len;
        } else {
            shootDir = sf::Vector2f(1.f, 0.f);
        }

        // 配置子弹
        ProjectileConfig config;
        config.speed = 500.f;
        config.damage = playerComp->stats.damage;
        // 狂暴加成：+50%伤害，并赋予 Fire 元素（第十六轮新增）
        // 设计意图：让狂暴从"纯数值增益"升级为"玩法流派切换"——
        // 激活狂暴期间，普攻不仅伤害更高，还会附加燃烧 DoT（每 0.5s
        // 造成 20% 子弹伤害，持续 3s），形成"狂暴火攻流"build。
        // 子弹颜色从黄白变为橙红，视觉上明确告知玩家元素切换。
        bool berserkActive = (playerComp->berserkTimer > 0.f);
        // ---- 第十九轮新增：闪电流元素切换 ----
        // 拥有 chainLightning 升级 OR 当前层为雷暴领域时，普攻子弹自动附加 Lightning 元素
        // （狂暴优先级更高，激活时仍保持 Fire 元素以维持"火攻流"build）
        // 设计意图：让 chainLightning 升级从"纯数值连锁"升级为"闪电流 build"——
        // 子弹不仅会连锁到附近敌人，还会施加 0.6s 麻痹（完全禁锢），
        // 形成"控制流"玩法。子弹颜色从黄白变为亮黄，视觉上明确元素切换。
        // 雷暴领域层修饰符扩展：即使未升级 chainLightning，进入雷暴领域层也可触发闪电元素，
        // 让"闪电流 build"从"圣物+升级"二维拓展到"层修饰符"第三维度
        bool hasChainLightning = (playerComp->stats.chainLightning > 0);
        bool floorLightning = playerComp->stats.floorLightningActive; // 第十九轮新增：雷暴领域
        if (berserkActive) {
            config.damage *= 1.5f;
            config.element = ElementType::Fire;
            config.color = sf::Color(255, 110, 50, 255); // 橙红色（火元素标识）
        } else if (hasChainLightning || floorLightning) {
            // 第十九轮新增：闪电流——普攻附加 Lightning 元素
            config.element = ElementType::Lightning;
            config.color = sf::Color(255, 230, 80, 255); // 亮黄色（闪电元素标识）
        } else {
            config.element = ElementType::Physical;
            config.color = sf::Color(255, 255, 100, 255); // 黄白色子弹
        }
        // 吸血打击加成：持续时间内所有攻击吸血（比例随等级 30%+10%*(lv-1)）
        float lifestealBonus = 0.f;
        if (playerComp->leechStrikeActive > 0.f) {
            lifestealBonus = 0.3f + 0.1f * (playerComp->leechStrikeLevel - 1);
            // 不再重置 leechStrikeActive，持续到时间结束
        }
        config.pierce = 0 + playerComp->stats.projectileBonusPierce; // 普通子弹命中即销毁（pierce=0），仅升级加成提供穿透
        config.lifetime = 2.f;
        config.radius = 6.f;
        config.splitCount = playerComp->stats.projectileBonusSplit;
        config.chainCount = playerComp->stats.chainLightning;
        config.knockback = 150.f;
        config.lifesteal = playerComp->stats.lifesteal + lifestealBonus;

        // 从玩家位置发射
        projectiles.Spawn(transform->position, shootDir, config, player);

        // 射击音效
        AudioManager::Instance().PlaySFX(AudioManager::kSFXShoot);

        // 触发攻击动画
        playerComp->attackTimer = 0.2f;
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
