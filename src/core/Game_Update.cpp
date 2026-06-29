#include "core/Game.h"
#include "utils/Logger.h"
#include "utils/TextureGenerator.h"
#include "ecs/Component.h"
#include "gameplay/PlayerCombat.h"
#include "gameplay/CombatEffects.h"
#include "gameplay/SkillSystem.h"
#include <string>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <chrono>
#include <algorithm>

namespace cu {

// ============================================================================
// Playing 状态：更新逻辑
// ============================================================================
void Game::updatePlaying(float dt) {
    // ---- 第三十一轮新增：顿帧系统 ----
    // hitStopTimer_ > 0 时跳过逻辑更新（游戏时间冻结），但仍递减计时器
    // 视觉效果：画面静止一瞬间，增强暴击/击杀精英的打击感
    if (hitStopTimer_ > 0.f) {
        hitStopTimer_ -= dt;
        if (hitStopTimer_ < 0.f) hitStopTimer_ = 0.f;
        // 顿帧期间仍更新摄像机（保持震动效果）和渲染，但跳过所有逻辑
        return;
    }

    // Phase 8: 升级选择激活时暂停玩法更新
    if (upgradeChoiceActive_) {
        upgradeMenu_.Update(dt);
        return;
    }

    // Phase 8: 背包菜单打开时暂停玩法更新
    if (inventoryMenuVisible_) {
        inventoryMenu_.Update(dt);
        return;
    }

    // 圣物查看面板打开时暂停玩法更新（避免被怪物攻击）
    if (relicPanelVisible_) {
        return;
    }

    // 按键教程显示时暂停玩法更新
    if (tutorialVisible_) {
        return;
    }

    // 累计存活时间
    survivalTime_ += dt;

    // 1. 更新玩家
    UpdatePlayer(registry_, playerId_, input_, camera_, dt, playerSheetInfo_,
                 dungeonInitialized_ ? &dungeon_ : nullptr);

    // 2. 更新动画系统
    animationSystem_.Update(registry_, dt);

    // 3. 空格触发标志清理
    if (triggerExplosion_) {
        triggerExplosion_ = false;
    }

    // 4. 摄像机跟随玩家
    Transform* playerTransform = registry_.GetComponent<Transform>(playerId_);
    if (playerTransform) {
        camera_.SetTarget(playerTransform->position);
    }

    // 5. 更新摄像机
    camera_.Update(dt);

    // 6. 更新粒子系统
    particles_.Update(dt);

    // 7. 更新玩家无敌时间
    Health* playerHealth = registry_.GetComponent<Health>(playerId_);
    if (playerHealth && playerHealth->invincibleTimer > 0.f) {
        playerHealth->invincibleTimer -= dt;
    }

    // 7.5 第十八轮新增：连击计时器衰减
    // 每次击杀重置 comboTimer = 3.0s（在 OnKill 中处理），超时则清空 combo。
    // 设计意图：combo 不是永久的，3s 窗口迫使玩家持续寻找新目标维持连击。
    // 死亡/换层时不在此处处理（restartGame/nextLevel 中显式重置）。
    PlayerComponent* playerPc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (playerPc && playerPc->comboCount > 0) {
        playerPc->comboTimer -= dt;
        if (playerPc->comboTimer <= 0.f) {
            playerPc->comboCount = 0;
            playerPc->comboTimer = 0.f;
        }
    }

    // 7.6 第二十轮新增：极限闪避成就检测
    // PlayerCombat 触发极限闪避时增加 pc->perfectDodgeCount，
    // Game 每帧比对 lastPerfectDodgeCount_ 检测变化并上报成就。
    // 设计：不修改 PlayerCombat 接口，通过比对实现解耦。
    if (playerPc && playerPc->perfectDodgeCount > lastPerfectDodgeCount_) {
        int delta = playerPc->perfectDodgeCount - lastPerfectDodgeCount_;
        lastPerfectDodgeCount_ = playerPc->perfectDodgeCount;
        // 上报成就系统：首次极限闪避（特殊成就，UnlockSpecial）+ 累计次数
        achievementSystem_.OnPerfectDodge(delta);
        // 首次触发时解锁特殊成就
        if (playerPc->perfectDodgeCount == 1) {
            achievementSystem_.UnlockSpecial(40); // 成就40：极限闪避（首次触发）
        }
    }

    // 8. 流场重算
    if (playerTransform) {
        flowFieldRecomputeTimer_ += dt;
        if (flowFieldRecomputeTimer_ >= kFlowFieldRecomputeInterval) {
            flowFieldRecomputeTimer_ = 0.f;
            flowField_.SetTarget(playerTransform->position);
            lastFlowFieldTimeMs_ = flowField_.GetLastRecomputeTime();
        }
    }

    // 9. 更新空间网格
    uniformGrid_.Clear();
    if (playerTransform) {
        registry_.ForEach<EnemyComponent, Transform>([&](EntityId id) {
            EnemyComponent* enemy = registry_.GetComponent<EnemyComponent>(id);
            Transform* t = registry_.GetComponent<Transform>(id);
            if (enemy && t && enemy->active) {
                uniformGrid_.Insert(id, t->position);
            }
        });
    }

    // 10. 更新敌人 AI
    float aiShakeRequest = 0.f;
    lastAIUpdateTimeMs_ = UpdateEnemyAI(registry_, flowField_, uniformGrid_, playerId_, dt,
                                        dungeonInitialized_ ? &dungeon_ : nullptr,
                                        &aiShakeRequest);
    // 第三十一轮新增：应用接触伤害震动请求
    if (aiShakeRequest > 0.f) {
        camera_.Shake(aiShakeRequest, 0.2f);
    }

    // 11. 更新敌人生成器
    if (playerTransform) {
        enemySpawner_.Update(dt, playerTransform->position);
    }

    // 12. 更新玩家战斗
    UpdatePlayerCombat(registry_, input_, playerId_, projectileSystem_,
                       particles_, camera_, uniformGrid_, combatSystem_,
                       dungeonInitialized_ ? &dungeon_ : nullptr, dt);

    // 12.5 更新技能持续效果（引力井、地刺、狂暴等）
    UpdateSkillBuffs(registry_, playerId_, uniformGrid_, combatSystem_, particles_, dt);

    // 13. 更新敌人战斗
    lastCombatTimeMs_ = UpdateEnemyCombat(registry_, uniformGrid_, playerId_,
                                          projectileSystem_, particles_,
                                          combatSystem_, dt, &enemySpawner_,
                                          &fissureZones_);

    // 13.5 更新地裂区域（Boss 冲撞留下的持续伤害区域）
    updateFissureZones(dt);

    // 14. 更新弹幕系统
    auto projStart = std::chrono::steady_clock::now();
    projectileSystem_.Update(registry_, uniformGrid_, playerId_,
                              dungeonInitialized_ ? &dungeon_ : nullptr,
                              particles_, dt);
    auto projEnd = std::chrono::steady_clock::now();
    lastProjectileTimeMs_ = static_cast<float>(
        std::chrono::duration_cast<std::chrono::microseconds>(projEnd - projStart).count()) / 1000.f;

    // 15. 更新状态效果
    combatSystem_.UpdateStatusEffects(registry_, dt);

    // 15.5 敌人死亡检测
    // ProjectileSystem.handleHit 直接扣 HP，未触发 CombatSystem.OnKill 回调。
    // 此处统一检测 HP 归零的敌人，触发 OnKill 回调（经验球、战利品、击杀计数）。
    {
        std::vector<EntityId> deadEnemies;
        registry_.ForEach<EnemyComponent, Health>([&](EntityId id) {
            EnemyComponent* enemy = registry_.GetComponent<EnemyComponent>(id);
            Health* health = registry_.GetComponent<Health>(id);
            if (enemy && health && enemy->active && health->current <= 0.f) {
                deadEnemies.push_back(id);
            }
        });
        for (EntityId id : deadEnemies) {
            EnemyComponent* enemy = registry_.GetComponent<EnemyComponent>(id);
            Transform* t = registry_.GetComponent<Transform>(id);
            if (enemy) enemy->active = false;

            // 触发 OnKill 回调（经验球、战利品、击杀计数）
            if (combatSystem_.OnKill) {
                combatSystem_.OnKill(id, playerId_);
            }

            // 生成死亡粒子效果
            if (t) {
                particles_.Explosion(t->position);
            }

            // BOSS 死亡处理
            if (enemy && enemy->type == EnemyType::Boss) {
                bossActive_ = false;
                bossEntityId_ = kInvalidEntity;
                bossDefeatedHintTimer_ = 10.f; // 显示10秒提示
                ++bossKillCountThisRun_; // 第二十四轮新增：用于死亡碎片计算
                LOG_INFO("BOSS 已被击败！(本局累计 %d)", bossKillCountThisRun_);
            }

            // 不销毁实体，仅标记 active=false
            // 对象池的 recycleDeadEnemies 会检查 active 标志回收
            // 销毁会导致 View 查询找不到该实体，进而引发房间立即清理 bug
        }
    }

    // 15.6 BOSS 检测：查找当前存活的 BOSS
    if (!bossActive_) {
        registry_.ForEach<EnemyComponent, Health>([&](EntityId id) {
            if (bossActive_) return;  // 已找到，跳过后续
            EnemyComponent* enemy = registry_.GetComponent<EnemyComponent>(id);
            Health* health = registry_.GetComponent<Health>(id);
            if (enemy && health && enemy->active && enemy->type == EnemyType::Boss
                && health->current > 0.f) {
                bossActive_ = true;
                bossEntityId_ = id;
                LOG_INFO("BOSS 出现！");
            }
        });
    }

    // 15.7 BOSS 回血：玩家离开 BOSS 房间 5 秒后，每秒回 2% 最大生命
    if (bossActive_ && bossEntityId_ != kInvalidEntity && playerTransform) {
        Health* bossHealth = registry_.GetComponent<Health>(bossEntityId_);
        if (bossHealth && bossHealth->current > 0.f) {
            // 检测玩家是否在 BOSS 附近（距离 < 400 像素视为在 BOSS 房间）
            Transform* bossTransform = registry_.GetComponent<Transform>(bossEntityId_);
            if (bossTransform) {
                sf::Vector2f toBoss = bossTransform->position - playerTransform->position;
                float distToBoss = std::sqrt(toBoss.x * toBoss.x + toBoss.y * toBoss.y);
                if (distToBoss < 400.f) {
                    // 玩家在 BOSS 房间，重置计时器
                    bossNoDamageTimer_ = 0.f;
                    bossRoomEntered_ = true;
                } else if (bossRoomEntered_) {
                    // 玩家离开 BOSS 房间，开始计时
                    bossNoDamageTimer_ += dt;
                    if (bossNoDamageTimer_ >= kBossRegenDelay) {
                        // 开始回血：每秒 2% 最大生命
                        float regen = bossHealth->max * kBossRegenRate * dt;
                        bossHealth->current += regen;
                        if (bossHealth->current > bossHealth->max) {
                            bossHealth->current = bossHealth->max;
                        }
                    }
                }
            }
        }
    }

    // 15.8 BOSS 击败提示计时器递减
    if (bossDefeatedHintTimer_ > 0.f) {
        bossDefeatedHintTimer_ -= dt;
    }

    // 16. 更新伤害飘字
    UpdateDamageTexts(registry_, dt);

    // 17. 同步玩家 Health 到 PlayerComponent
    PlayerComponent* playerComp = registry_.GetComponent<PlayerComponent>(playerId_);
    if (playerHealth && playerComp) {
        // 无敌模式：每帧恢复生命值到最大值
        if (godMode_ && playerHealth->current < playerHealth->max) {
            playerHealth->current = playerHealth->max;
        }
        
        playerComp->stats.currentHp = playerHealth->current;
        // 玩家死亡判定
        if (playerHealth->current <= 0.f) {
            // ---- 第三十一轮修复：在 ChangeState 前保存死亡回顾数据 ----
            // 因为 ChangeState 会先调用 Playing.onExit（registry_.Clear()），
            // 再调用 Dead.onEnter，届时所有实体数据已被清空
            lastKillerName_ = "";
            comboAtDeath_ = playerComp->comboCount;
            totalDamageDealt_ = playerComp->totalDamageDealt;
            if (survivalTime_ > 0.f) {
                // DPS 在 Dead onEnter 中计算
            }
            if (playerComp->lastAttackerEntity != kInvalidEntity &&
                registry_.IsAlive(playerComp->lastAttackerEntity)) {
                EnemyComponent* ec = registry_.GetComponent<EnemyComponent>(playerComp->lastAttackerEntity);
                if (ec) {
                    lastKillerName_ = EnemyTypeChineseName(ec->type);
                    if (ec->isChampion) {
                        lastKillerName_ = "Champion " + lastKillerName_;
                    }
                }
            }
            LOG_INFO("死亡回顾预保存: 击杀者=%s combo=%d dmg=%.0f",
                     lastKillerName_.empty() ? "无" : lastKillerName_.c_str(),
                     comboAtDeath_, totalDamageDealt_);

            // 第二十二轮新增：重置 SurviveTime 任务进度（单次生命语义）
            questSystem_.OnPlayerDeath();
            ChangeState(GameState::Dead);
            return; // registry 已被 onExit 清空，必须立即返回
        }
    }

    // 18. 房间系统更新
    if (dungeonInitialized_ && playerTransform) {
        roomSystem_.Update(registry_, dungeon_, playerTransform->position,
                           enemySpawner_, particles_, dt);
    }

    // ---- Phase 7: 经验球与战利品更新 ----
    if (playerTransform && playerComp) {
        // 经验球更新（ExpOrbSystem 内部调用 UpgradeSystem.AddExp）
        expOrbSystem_.Update(registry_, playerId_, upgradeSystem_,
                             playerComp->stats.pickupRange,
                             playerComp->stats.expMultiplier, dt);

        // 金币更新（CoinSystem 内部增加 playerComp->stats.coins）
        coinSystem_.Update(registry_, playerId_, playerComp, dt);

        // 爱心更新（HeartSystem 内部回复玩家生命）
        heartSystem_.Update(registry_, playerId_, playerComp, dt);

        // 战利品更新
        lootSystem_.Update(registry_, playerId_, input_, inventorySystem_, dt);

        // 技能点系统：升级时累积技能点，玩家按 J 主动开启选择界面（不再自动弹窗）
        // HUD 会显示"有未使用的技能点n点 按J开启"提示

        // 同步等级与经验到 PlayerStats
        playerComp->stats.level = upgradeSystem_.GetLevel();
        playerComp->stats.exp = static_cast<float>(upgradeSystem_.GetExp());
        playerComp->stats.expToNext = static_cast<float>(upgradeSystem_.GetExpToNext());
    }

    // ---- Phase 8: 更新 HUD 数据 ----
    updateHUDData();

    // ---- 第十七轮新增：地牢变异系统——玩家每秒回血（"生命之涌" modifier）----
    // regenPerSec 为占最大生命百分比（如 0.01 = 1%/秒），需累加到整数才生效
    // 累加器跨帧保留，确保小数回血不被丢弃
    {
        float regenPct = floorModifiers_.GetPlayerRegenPerSec();
        if (regenPct > 0.f) {
            Health* h = registry_.GetComponent<Health>(playerId_);
            if (h && h->max > 0.f && h->current < h->max) {
                regenAccumulator_ += regenPct * h->max * dt;
                if (regenAccumulator_ >= 1.f) {
                    int regenInt = static_cast<int>(regenAccumulator_);
                    h->current = std::min(h->max, h->current + static_cast<float>(regenInt));
                    regenAccumulator_ -= static_cast<float>(regenInt);
                }
            } else {
                // 满血或异常时不累积，避免回归战斗瞬间一次性回大量血
                regenAccumulator_ = 0.f;
            }
        }
    }

    // ---- 法力回复（玩家 ManaRegen 升级 + 装备词缀）----
    {
        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
        if (pc && pc->stats.manaRegen > 0.f) {
            float regen = pc->stats.manaRegen * dt;
            manaRegenAccumulator_ += regen;
            if (manaRegenAccumulator_ >= 1.f) {
                int regenInt = static_cast<int>(manaRegenAccumulator_);
                pc->stats.currentMp = std::min(pc->stats.maxMp, pc->stats.currentMp + static_cast<float>(regenInt));
                manaRegenAccumulator_ -= static_cast<float>(regenInt);
            }
        }
    }

    // ---- 第十七轮新增：地牢变异 Banner 计时器递减 ----
    if (modifierBannerTimer_ > 0.f) {
        modifierBannerTimer_ -= dt;
        if (modifierBannerTimer_ < 0.f) modifierBannerTimer_ = 0.f;
    }

    // ---- 更新成就 Toast 通知计时器（淡入/淡出/过期回收）----
    hud_.UpdateToasts(dt);

    // ---- 更新任务系统（解锁检测/时间累计/完成检测）----
    // 任务5需要玩家当前金币，任务4需要玩家当前技能总数
    PlayerComponent* pcForQuest = registry_.GetComponent<PlayerComponent>(playerId_);
    int currentCoins = pcForQuest ? pcForQuest->stats.coins : 0;
    int currentSkillCount = 0;
    if (pcForQuest) {
        for (const auto& s : pcForQuest->skillSlots) {
            if (s.type != SkillType::Count) ++currentSkillCount;
        }
        for (const auto& s : pcForQuest->skillBackpack) {
            if (s != SkillType::Count) ++currentSkillCount;
        }
    }
    questSystem_.Update(dt, currentLevel_, currentCoins, currentSkillCount);
    // 成就系统无每帧逻辑（仅事件驱动），保留接口供后续限时成就扩展
    achievementSystem_.Update(dt);

    // ---- 更新调试面板实时状态数据 ----
    updateDebugStats();
}

// ============================================================================
// Phase 8: 更新 HUD 数据
// ============================================================================
void Game::updateHUDData() {
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (pc && dungeonInitialized_) {
        hud_.Update(pc->stats, currentWaveNumber_, enemySpawner_.GetAliveCount(),
                    time_.GetFPS(), dungeon_, roomSystem_.GetCurrentRoomIndex());

        // 更新技能冷却（估算冷却比例）
        // 普攻冷却：attackCooldown / (1/attackSpeed)
        float atkCdMax = (pc->stats.attackSpeed > 0.f) ? (1.f / pc->stats.attackSpeed) : 0.5f;
        hud_.SetSkillCooldown(0, pc->attackCooldown > 0.f ? pc->attackCooldown / atkCdMax : 0.f);
        // 闪避冷却：dodgeCooldown / 2.0（基础冷却 2s）
        hud_.SetSkillCooldown(1, pc->dodgeCooldown > 0.f ? pc->dodgeCooldown / 2.f : 0.f);
        // AOE 冷却：aoeCooldown / 5.0（基础冷却 5s）
        hud_.SetSkillCooldown(2, pc->aoeCooldown > 0.f ? pc->aoeCooldown / 5.f : 0.f);

        // 更新技能槽数据到 HUD
        hud_.SetSkillSlotData(pc->skillSlots);

        // 更新技能点数到 HUD（用于显示"按J开启"提示）
        hud_.SetSkillPoints(upgradeSystem_.GetSkillPoints());

        // 更新玩家世界位置到 HUD（用于小地图玩家标记）
        Transform* tr = registry_.GetComponent<Transform>(playerId_);
        if (tr) hud_.SetPlayerPosition(tr->position);

        // ---- 第十八轮新增：推送连击系统数据到 HUD ----
        // GetComboDamageMultiplier 是静态查表，O(1) 开销，每帧调用安全
        hud_.SetComboData(pc->comboCount, pc->comboTimer,
                          CombatSystem::GetComboDamageMultiplier(pc->comboCount));

        // ---- 第二十轮新增：推送极限闪避系统数据到 HUD ----
        // HUD 根据 perfectDodgeBuffTimer > 0 显示金色光环边框
        hud_.SetPerfectDodgeData(pc->perfectDodgeBuffTimer, pc->perfectDodgeCooldown);

        // ---- 第三十轮新增：推送商人位置到 HUD（用于小地图 $ 标记）----
        hud_.SetMerchantPosition(merchantSystem_.GetPosition(), merchantSystem_.IsActive());
    }
}

// ============================================================================
// Phase 8: 更新 UI
// ============================================================================
void Game::updateUI(float dt) {
    uiManager_.Update(dt);

    // 存档菜单可见时优先更新（覆盖所有状态）
    if (saveLoadMenuVisible_) {
        saveLoadMenu_.Update(dt);
    }

    // 第二十四轮新增：灵魂之井面板可见时更新（悬停状态等）
    if (soulWellMenuVisible_) {
        soulWellMenu_.Update(dt);
    }

    // 根据当前状态更新对应菜单
    switch (state_) {
        case GameState::Menu:
            mainMenu_.Update(dt);
            break;
        case GameState::Playing:
            if (upgradeChoiceActive_) {
                handleUpgradeChoice();
            }
            if (relicChoiceActive_) {
                handleRelicChoice();
            }
            if (inventoryMenuVisible_) {
                inventoryMenu_.Update(dt);
            }
            if (questMenuVisible_) {
                questMenu_.Update(dt); // 累加闪烁计时器
            }
            break;
        case GameState::Paused:
            pauseMenu_.Update(dt);
            break;
        case GameState::Dead:
            deathScreen_.Update(dt);
            break;
        case GameState::Victory:
            victoryScreen_.Update(dt);
            break;
    }
}

// ============================================================================
// updateDebugStats —— 填充 DebugPanel 实时状态数据
// ============================================================================
void Game::updateDebugStats() {
    DebugPanel::DebugStats s;

    // 基本信息
    s.fps = time_.GetFPS();
    s.dungeonLevel = currentLevel_;
    s.currentRoomIndex = roomSystem_.GetCurrentRoomIndex();
    s.totalRooms = roomSystem_.GetTotalRoomCount();
    s.clearedRooms = roomSystem_.GetClearedRoomCount();
    s.currentSlot = currentSlot_;

    // 当前房间类型
    if (s.currentRoomIndex >= 0 && s.currentRoomIndex < static_cast<int>(dungeon_.rooms.size())) {
        s.currentRoomType = RoomTypeName(dungeon_.rooms[s.currentRoomIndex].type);
    } else {
        s.currentRoomType = "None";
    }

    // 实体统计：遍历 registry 计数活跃敌人
    s.enemyCount = 0;
    registry_.ForEach<EnemyComponent>([&](EntityId id) {
        EnemyComponent* ec = registry_.GetComponent<EnemyComponent>(id);
        if (ec && ec->active) ++s.enemyCount;
    });
    s.projectileCount = projectileSystem_.GetActiveCount();
    s.particleCount = static_cast<int>(particles_.GetActiveCount());
    s.fissureCount = static_cast<int>(fissureZones_.size());

    // 玩家状态
    Transform* pT = registry_.GetComponent<Transform>(playerId_);
    Health* pH = registry_.GetComponent<Health>(playerId_);
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (pT) {
        s.playerX = pT->position.x;
        s.playerY = pT->position.y;
    }
    if (pH) {
        s.playerHp = pH->current;
        s.playerMaxHp = pH->max;
    }
    s.playerLevel = upgradeSystem_.GetLevel();
    s.playerExp = upgradeSystem_.GetExp();
    s.playerExpToNext = upgradeSystem_.GetExpToNext();
    s.playerSkillPoints = upgradeSystem_.GetSkillPoints();
    if (pc) {
        s.playerCoins = pc->stats.coins;
        s.playerCursed = pc->cursed;
    }

    // Boss 状态
    s.bossActive = bossActive_ && bossEntityId_ != kInvalidEntity;
    if (s.bossActive) {
        Health* bH = registry_.GetComponent<Health>(bossEntityId_);
        if (bH && bH->max > 0.f) {
            s.bossHpPercent = bH->current / bH->max;
        }
    }

    // 性能
    s.aiTimeMs = lastAIUpdateTimeMs_;
    s.combatTimeMs = lastCombatTimeMs_;
    s.projectileTimeMs = lastProjectileTimeMs_;

    debugPanel_.SetStats(s);
    debugPanel_.SetGodMode(godMode_);
}

// ============================================================================
// updateFissureZones —— 更新地裂区域（递减寿命、对玩家造成持续伤害）
// ============================================================================
void Game::updateFissureZones(float dt) {
    if (fissureZones_.empty()) return;

    Transform* pT = registry_.GetComponent<Transform>(playerId_);
    Health* pHealth = registry_.GetComponent<Health>(playerId_);
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);

    for (auto& fz : fissureZones_) {
        fz.lifetime -= dt;
        fz.damageTickTimer -= dt;

        // 每 0.5s 检测一次玩家伤害
        if (fz.damageTickTimer <= 0.f && pT && pHealth && pc) {
            sf::Vector2f toPlayer = pT->position - fz.position;
            float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;
            if (distSq <= fz.radius * fz.radius) {
                // 玩家在地裂范围内，造成伤害（每 tick = damagePerSec * 0.5）
                float dmg = fz.damagePerSec * 0.5f;
                if (pHealth->invincibleTimer <= 0.f) {
                    pHealth->current -= dmg;
                    SpawnDamageText(registry_, pT->position, dmg, false);

                    // ---- 第十八轮新增：地裂伤害重置玩家连击 ----
                    // 地裂伤害直接修改 HP 不经过 ApplyDamage，需手动重置 combo。
                    if (pc->comboCount > 0) {
                        pc->comboCount = 0;
                        pc->comboTimer = 0.f;
                    }
                }
            }
            fz.damageTickTimer = 0.5f;
        }
    }

    // 移除过期地裂区域
    fissureZones_.erase(
        std::remove_if(fissureZones_.begin(), fissureZones_.end(),
                       [](const FissureZone& fz) { return fz.lifetime <= 0.f; }),
        fissureZones_.end());
}

// ============================================================================
// recomputePlayerStats  重新计算玩家属性（升级 + 装备词缀）
// ============================================================================
void Game::recomputePlayerStats() {
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (!pc) return;

    // 1. 重置为基础属性
    PlayerStats& s = pc->stats;
    s.moveSpeed = 200.f;
    s.attackSpeed = 2.0f;
    s.damage = 10.f;
    s.maxHp = 100.f;
    s.maxMp = 50.f;
    s.critChance = 0.15f;
    s.critDamage = 1.5f;
    s.lifesteal = 0.f;
    s.pickupRange = 80.f;
    s.expMultiplier = 1.f;
    s.coinMultiplier = 1.f;
    s.projectileBonusSplit = 0;
    s.projectileBonusPierce = 0;
    s.chainLightning = 0;
    s.aoeCooldownReduce = 0.f;
    s.dodgeCooldownReduce = 0.f;
    s.defense = 0.f;
    s.manaRegen = 0.f;
    s.lightningDurationMul = 1.f; // 第十九轮新增：重置 Lightning 麻痹倍率
    s.floorLightningActive = false; // 第十九轮新增：重置雷暴领域标志（由变异系统重新设置）
    s.dodgeWindowMul = 1.f; // 第二十轮新增：重置极限闪避窗口倍率（圣物月光护符加成）
    s.perfectDodgeGuaranteedCrit = false; // 第二十轮新增：重置复仇之刃标志

    // 1.5 每级基础数值微调（很微小的成长，独立于升级选择）
    // 每升一级：伤害+1, 最大生命+2, 防御+1, 暴击率+0.5%, 暴击伤害+1%
    // 等级从 1 开始，所以 (level-1) 表示已升级次数
    {
        int lvl = upgradeSystem_.GetLevel();
        int ups = lvl - 1;
        if (ups > 0) {
            s.damage += 1.f * ups;
            s.maxHp += 2.f * ups;
            s.defense += 1.f * ups;
            s.critChance += 0.005f * ups;
            s.critDamage += 0.01f * ups;
        }
    }

    // 1.6 第二十四轮新增：应用灵魂之忆永久强化（Meta Progression）
    // 设计：加法式叠加，作为"基础属性的一部分"
    // 位置在每级微调之后、升级加成之前——所有后续 multiplier（升级/装备/圣物/变异）
    // 均乘法叠加在 meta 加成之上，符合"meta 局外 → 局内"的层级关系
    // 注：expMultiplier/coinMultiplier 此处为 += 加成，最终仍按乘法作用于经验/金币获取
    soulMemory_.ApplyToPlayerStats(s.maxHp, s.damage, s.moveSpeed,
                                   s.expMultiplier, s.coinMultiplier, s.defense);

    // 2. 应用升级加成
    int dmgLv = upgradeSystem_.GetUpgradeLevel(UpgradeType::DamageUp);
    s.damage += 5.f * dmgLv;

    int atkSpdLv = upgradeSystem_.GetUpgradeLevel(UpgradeType::AttackSpeedUp);
    s.attackSpeed *= (1.f + 0.15f * atkSpdLv);

    int movSpdLv = upgradeSystem_.GetUpgradeLevel(UpgradeType::MoveSpeedUp);
    s.moveSpeed *= (1.f + 0.10f * movSpdLv);

    int maxHpLv = upgradeSystem_.GetUpgradeLevel(UpgradeType::MaxHpUp);
    s.maxHp += 20.f * maxHpLv;

    int critLv = upgradeSystem_.GetUpgradeLevel(UpgradeType::CritRateUp);
    s.critChance += 0.05f * critLv;

    int critDmgLv = upgradeSystem_.GetUpgradeLevel(UpgradeType::CritDamageUp);
    s.critDamage += 0.30f * critDmgLv;

    int lsLv = upgradeSystem_.GetUpgradeLevel(UpgradeType::LifestealUp);
    s.lifesteal += 0.03f * lsLv;

    s.projectileBonusSplit = upgradeSystem_.GetUpgradeLevel(UpgradeType::ProjectileSplit);
    s.projectileBonusPierce = upgradeSystem_.GetUpgradeLevel(UpgradeType::ProjectilePierce);
    s.chainLightning = upgradeSystem_.GetUpgradeLevel(UpgradeType::ChainLightning);
    s.aoeCooldownReduce = static_cast<float>(upgradeSystem_.GetUpgradeLevel(UpgradeType::AoeCooldownReduce));
    s.dodgeCooldownReduce = 0.5f * upgradeSystem_.GetUpgradeLevel(UpgradeType::DashCooldownReduce);
    // s.manaRegen 和 s.defense 由装备词缀/圣物/灵魂之忆系统提供，无需升级系统赋值

    // 3. 应用装备词缀加成
    inventorySystem_.ApplyToPlayerStats(s);

    // 3.4 第二十三轮新增：应用装备套装加成
    // 在装备词缀之后、圣物之前——确保套装 multiplier 与圣物乘法叠加
    // 套装 build 维度与升级/装备/圣物/元素/变异/连击六维正交，玩家在
    // "更高 ilvl 单件"与"较低 ilvl 但凑齐套装"间产生策略选择
    inventorySystem_.ApplySetBonuses(s);

    // 3.5 应用圣物加成（第十五轮新增，与升级/装备三重叠加形成 Build）
    relicSystem_.ApplyToPlayerStats(s);

    // 3.6 第十七轮新增：应用地牢变异系统 multiplier
    // 在圣物之后、Health 同步之前，确保 maxHp 缩放正确反映到 currentHp
    // 仅修改 damage/maxHp/moveSpeed/pickupRange 四个字段（其他字段不受变异影响）
    floorModifiers_.ApplyToPlayerStats(s.damage, s.maxHp, s.moveSpeed, s.pickupRange);

    // 3.7 第十九轮新增：应用雷暴领域标志
    // 在 multiplier 应用之后设置，确保进入/离开雷暴领域层时元素切换正确
    // 设计：让"闪电流 build"拓展到层修饰符维度，无需 chainLightning 升级即可触发
    s.floorLightningActive = floorModifiers_.IsPlayerAttackLightning();

    // 4. 同步 Health 组件
    Health* health = registry_.GetComponent<Health>(playerId_);
    if (health) {
        float oldMaxHp = health->max;
        health->max = s.maxHp;
        if (s.maxHp > oldMaxHp) {
            health->current += (s.maxHp - oldMaxHp);
        }
        if (health->current > health->max) health->current = health->max;
    }
    s.currentHp = health ? health->current : s.maxHp;

    // 5. 应用诅咒效果（诅咒房：移速 -30%、攻速 -20%）
    if (pc->cursed) {
        s.moveSpeed *= 0.7f;
        s.attackSpeed *= 0.8f;
    }

    LOG_INFO("玩家属性重算: DMG=%.1f, AtkSpd=%.2f, Spd=%.1f, HP=%.0f, Crit=%.0f%%, LS=%.0f%%, Curse=%d, Relics=%d, Modifiers=%d",
             s.damage, s.attackSpeed, s.moveSpeed, s.maxHp,
             s.critChance * 100.f, s.lifesteal * 100.f, pc->cursed ? 1 : 0,
             relicSystem_.GetOwnedCount(), floorModifiers_.GetActiveCount());
}

} // namespace cu