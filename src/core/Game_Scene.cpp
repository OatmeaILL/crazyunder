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
// Playing 状态：演示场景搭建
// ============================================================================
void Game::setupPlayingScene(bool preserveProgress) {
    registry_.Clear();
    atlas_.Clear();

    // 清空 Boss 冲撞地裂区域
    fissureZones_.clear();
    // 重置事件房状态
    activeEventRoomIdx_ = -1;
    activeEventType_ = EventType::None;
    eventDialogVisible_ = false;
    // 重置顿帧
    hitStopTimer_ = 0.f;

    // ---- Phase 6: 生成地牢 ----
    dungeonSeed_ = static_cast<uint32_t>(std::time(nullptr));
    dungeon_ = dungeonGenerator_.Generate(dungeonSeed_);
    dungeonInitialized_ = true;

    LOG_INFO("地牢已生成: seed=%u, %dx%d, %zu 个房间, worldOffset=(%.1f, %.1f)",
             dungeonSeed_, dungeon_.width, dungeon_.height, dungeon_.rooms.size(),
             dungeon_.worldOffset.x, dungeon_.worldOffset.y);

    // 1. 加载玩家 Sprite Sheet 到图集
    playerSheetInfo_ = loadPlayerSpriteSheet();

    // 2. 添加环境装饰精灵
    bool ok = true;

    ok &= atlas_.AddImageFromMemory("tile_floor",
        TextureGenerator::CreateFloorTile());
    ok &= atlas_.AddImageFromMemory("tile_wall",
        TextureGenerator::CreateWallTile());
    ok &= atlas_.AddImageFromMemory("tile_door",
        TextureGenerator::CreateDoorTile());
    ok &= atlas_.AddImageFromMemory("tile_door_open",
        TextureGenerator::CreateDoorOpenTile());
    ok &= atlas_.AddImageFromMemory("tile_obstacle",
        TextureGenerator::CreateObstacleTile());
    ok &= atlas_.AddImageFromMemory("tile_indestructible_obstacle",
        TextureGenerator::CreateIndestructibleObstacleTile());
    ok &= atlas_.AddImageFromMemory("tile_stairs",
        TextureGenerator::CreateStairsTile());
    ok &= atlas_.AddImageFromMemory("tile_chest",
        TextureGenerator::CreateChestTile());

    ok &= atlas_.AddImageFromMemory("enemy_red",
        TextureGenerator::CreateCirclePlaceholder(sf::Color(200, 50, 50)));
    ok &= atlas_.AddImageFromMemory("enemy_blue",
        TextureGenerator::CreateCirclePlaceholder(sf::Color(50, 100, 200)));
    ok &= atlas_.AddImageFromMemory("prop",
        TextureGenerator::CreateColorBlock(sf::Color(200, 180, 50), sf::Color::Black));

    // ---- Phase 4: 添加 5 种敌人贴图到图集 ----
    ok &= atlas_.AddImageFromMemory("enemy_melee",
        TextureGenerator::CreateEnemySprite(EnemyType::Melee));
    ok &= atlas_.AddImageFromMemory("enemy_ranged",
        TextureGenerator::CreateEnemySprite(EnemyType::Ranged));
    ok &= atlas_.AddImageFromMemory("enemy_suicide",
        TextureGenerator::CreateEnemySprite(EnemyType::Suicide));
    ok &= atlas_.AddImageFromMemory("enemy_elite",
        TextureGenerator::CreateEnemySprite(EnemyType::Elite));
    ok &= atlas_.AddImageFromMemory("enemy_boss",
        TextureGenerator::CreateEnemySprite(EnemyType::Boss));
    // ---- 新增 4 种敌人贴图 ----
    ok &= atlas_.AddImageFromMemory("enemy_stealth",
        TextureGenerator::CreateEnemySprite(EnemyType::StealthMelee));
    ok &= atlas_.AddImageFromMemory("enemy_countdown",
        TextureGenerator::CreateEnemySprite(EnemyType::CountdownSuicide));
    ok &= atlas_.AddImageFromMemory("enemy_splitter",
        TextureGenerator::CreateEnemySprite(EnemyType::Splitter));
    ok &= atlas_.AddImageFromMemory("enemy_shielded",
        TextureGenerator::CreateEnemySprite(EnemyType::Shielded));
    // ---- 狙击远程怪贴图 ----
    ok &= atlas_.AddImageFromMemory("enemy_sniper",
        TextureGenerator::CreateEnemySprite(EnemyType::SniperRanged));
    // ---- Caster 施法者贴图 ----
    ok &= atlas_.AddImageFromMemory("enemy_caster",
        TextureGenerator::CreateEnemySprite(EnemyType::Caster));
    // ---- 商人 NPC 贴图 ----
    ok &= atlas_.AddImageFromMemory("merchant",
        TextureGenerator::CreateMerchantSprite());
    // ---- 事件房 NPC 贴图（乞丐/法师/祭坛，宝箱怪复用宝箱贴图）----
    ok &= atlas_.AddImageFromMemory("event_beggar",
        TextureGenerator::CreateBeggarSprite());
    ok &= atlas_.AddImageFromMemory("event_mage",
        TextureGenerator::CreateMageSprite());
    ok &= atlas_.AddImageFromMemory("event_altar",
        TextureGenerator::CreateAltarSprite());
    // ---- 装备图标（6 种槽位）----
    ok &= atlas_.AddImageFromMemory("icon_weapon",
        TextureGenerator::CreateItemIcon(ItemSlot::Weapon));
    ok &= atlas_.AddImageFromMemory("icon_helmet",
        TextureGenerator::CreateItemIcon(ItemSlot::Helmet));
    ok &= atlas_.AddImageFromMemory("icon_chest",
        TextureGenerator::CreateItemIcon(ItemSlot::Chest));
    ok &= atlas_.AddImageFromMemory("icon_boots",
        TextureGenerator::CreateItemIcon(ItemSlot::Boots));
    ok &= atlas_.AddImageFromMemory("icon_ring",
        TextureGenerator::CreateItemIcon(ItemSlot::Ring));
    ok &= atlas_.AddImageFromMemory("icon_amulet",
        TextureGenerator::CreateItemIcon(ItemSlot::Amulet));

    if (!ok) {
        LOG_WARN("部分图集图片添加失败，继续构建");
    }

    // 剑士武器贴图（32x32 像素长剑，用于攻击时渲染）
    ok &= atlas_.AddImageFromMemory("sword_sprite",
        TextureGenerator::CreateSwordSprite());

    // ---- Phase 5: 初始化弹幕系统 ----
    projectileSystem_.Initialize(registry_, atlas_);

    if (!atlas_.Build()) {
        LOG_ERROR("图集构建失败，演示场景无法渲染");
        return;
    }

    projectileSystem_.PostBuildInit(atlas_);

    sf::IntRect playerRect = atlas_.GetPixelRect("player_sheet");
    playerSheetInfo_.atlasX = playerRect.left;
    playerSheetInfo_.atlasY = playerRect.top;
    LOG_INFO("玩家 Sprite Sheet 图集位置: (%d, %d)", playerSheetInfo_.atlasX, playerSheetInfo_.atlasY);

    // 缓存剑士武器贴图矩形（供 renderPlaying 渲染旋转剑 sprite 使用）
    swordRect_ = atlas_.GetPixelRect("sword_sprite");

    // ---- Phase 6: 初始化 TileMap 与 RoomSystem ----
    tileMap_.Initialize(atlas_, dungeon_);
    roomSystem_.Initialize(dungeon_);
    roomSystem_.SetDungeonLevel(currentLevel_);

    // 3. 创建玩家实体
    sf::Vector2f playerStartPos(0.f, 0.f);
    if (dungeon_.startRoom >= 0 &&
        dungeon_.startRoom < static_cast<int>(dungeon_.rooms.size())) {
        playerStartPos = dungeon_.TileCenterToWorld(
            dungeon_.rooms[dungeon_.startRoom].center);
    }
    playerId_ = CreatePlayer(registry_, playerStartPos, playerSheetInfo_);

    // 5. 摄像机初始化到玩家位置
    Transform* playerTransform = registry_.GetComponent<Transform>(playerId_);
    if (playerTransform) {
        camera_.SetPosition(playerTransform->position);
        camera_.SetTarget(playerTransform->position);
    }

    // ---- Phase 4/6: 初始化流场、空间网格、敌人生成器 ----
    float worldW = dungeon_.width * static_cast<float>(kTileSize);
    float worldH = dungeon_.height * static_cast<float>(kTileSize);
    flowField_.Initialize(worldW, worldH, static_cast<float>(kTileSize));

    for (int y = 0; y < dungeon_.height; ++y) {
        for (int x = 0; x < dungeon_.width; ++x) {
            if (dungeon_.GetTile(x, y) == TileType::Wall) {
                flowField_.SetBlocked(x, y, true);
            }
        }
    }
    LOG_INFO("FlowField 已设置墙壁阻挡: %dx%d", dungeon_.width, dungeon_.height);

    if (playerTransform) {
        flowField_.SetTarget(playerTransform->position);
    }

    uniformGrid_.Resize(dungeon_.worldOffset.x, dungeon_.worldOffset.y,
                        worldW, worldH, 64.f);

    if (playerTransform) {
        enemySpawner_.Initialize(registry_, atlas_, playerTransform->position);
        enemySpawner_.SetDungeonLevel(currentLevel_);
    }

    // 重置波次与计时器
    currentWaveNumber_ = 0;
    flowFieldRecomputeTimer_ = 0.f;
    lastFlowFieldTimeMs_ = 0.f;
    lastAIUpdateTimeMs_ = 0.f;
    lastCombatTimeMs_ = 0.f;
    lastProjectileTimeMs_ = 0.f;
    totalKillCount_ = 0;
    combatSystem_.ResetKillCount();

    // ---- Phase 7: 初始化战利品与成长系统 ----
    // preserveProgress=true 时保留升级/装备/经验（用于下一层）
    lootSystem_.Initialize(registry_, atlas_);
    lootSystem_.SetDungeonLevel(currentLevel_);
    if (!preserveProgress) {
        inventorySystem_.Initialize();
        upgradeSystem_.Initialize();
    }
    expOrbSystem_.Initialize();
    coinSystem_.Initialize();
    heartSystem_.Initialize();
    merchantSystem_.Initialize(registry_, atlas_);

    // ---- 任务/成就系统初始化 ----
    // 任务系统仅在新游戏/读档时重新初始化（preserveProgress=true 下一层时保留任务进度）
    if (!preserveProgress) {
        questSystem_.Initialize();
        // 第十五轮：圣物系统也在新游戏/读档时初始化（读档由 applySaveData 反序列化覆盖）
        relicSystem_.Initialize();
        // 第十七轮：地牢变异系统在新游戏/读档时清空（读档由 applySaveData 反序列化覆盖）
        // 下一层（preserveProgress=true）时由 nextLevel 提前 RollForLevel 设置
        floorModifiers_.Clear();
        modifierBannerTimer_ = 0.f;
        regenAccumulator_ = 0.f;
        manaRegenAccumulator_ = 0.f;
    }
    // 第十七轮：将当前 floorModifiers_ 推送到各子系统（EnemySpawner/LootSystem/MerchantSystem）
    // 此处覆盖 cleared（新游戏）和 rolled（nextLevel）两种情况
    applyFloorModifiersToSubsystems();
    // 注册任务奖励发放回调（Game 层应用奖励到玩家：经验/金币/装备/技能点/等级）
    // 每次 setupPlayingScene 都注册，因为 playerId_ 会变化
    questSystem_.OnRewardGranted = [this](int questId, const QuestReward& reward) {
        LOG_INFO("应用任务 %d 奖励到玩家", questId);
        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
        if (!pc) return;

        // 经验奖励
        if (reward.exp > 0) {
            upgradeSystem_.AddExp(reward.exp);
        }
        // 金币奖励（正数=获得，负数=扣除，任务5=-1000）
        if (reward.coins != 0) {
            pc->stats.coins += reward.coins;
            if (pc->stats.coins < 0) pc->stats.coins = 0;
        }
        // 技能点奖励
        if (reward.skillPoints > 0) {
            // 直接增加技能点（不通过升级流程，需手动累加 upgradeSystem_ 的 skillPoints_）
            // 由于 skillPoints_ 是 private，使用 AddExp(0) 不行；
            // 这里通过给予 0 经验但调用 RollUpgrades/ApplyUpgrade 的方式不合适，
            // 简化方案：给等量经验让玩家"自然升级"获得技能点
            // 更准确方案：让 UpgradeSystem 提供 AddSkillPoints 方法
            // 框架阶段先给经验：每技能点约需 1 次升级（150 经验），给予 reward.skillPoints * 200 经验
            upgradeSystem_.AddExp(reward.skillPoints * 200);
        }
        // 直接提升等级（任务5：等级+5）
        if (reward.addLevels > 0) {
            upgradeSystem_.AddLevels(reward.addLevels);
        }
        // 装备奖励（随机类型，按指定品质生成）
        if (reward.itemCount > 0 && reward.itemQuality != ItemQuality::White) {
            for (int i = 0; i < reward.itemCount; ++i) {
                Item item = lootSystem_.GenerateRandomItem(5, reward.itemQuality);
                if (!inventorySystem_.PickupItem(item)) {
                    // 背包已满，直接掉落到地上
                    Transform* t = registry_.GetComponent<Transform>(playerId_);
                    if (t) lootSystem_.DropSpecificItem(item, t->position);
                }
            }
        }

        // 重新计算玩家属性（升级/装备变化后）
        recomputePlayerStats();
    };
    // 成就系统仅在首次启动时初始化+加载（跨存档共享，不随存档重置）
    static bool achInitialized = false;
    if (!achInitialized) {
        achievementSystem_.Initialize();
        if (!achievementSystem_.LoadFromFile()) {
            LOG_INFO("成就文件不存在，使用初始状态");
        }
        // 注册成就解锁回调（Game 层显示 Toast 通知）
        achievementSystem_.OnUnlocked = [this](int id, const AchievementDef& def) {
            (void)id;
            LOG_INFO("成就解锁通知: %s", def.name.c_str());
            AudioManager::Instance().PlaySFX(AudioManager::kSFXChallengeComplete);
            // 推入 HUD Toast 通知队列（右上角横幅，4s 后自动淡出）
            hud_.AddAchievementToast(def.name, def.description);
            // 成就解锁后立即持久化，避免崩溃丢失进度
            if (!achievementSystem_.SaveToFile()) {
                LOG_WARN("成就解锁后持久化失败: %s", def.name.c_str());
            }
        };
        achInitialized = true;
    }

    // 第二十四轮新增：灵魂之忆系统初始化（仅首次启动，跨存档共享）
    // 与成就系统一致，使用 static bool 守护确保只加载一次
    static bool soulMemoryInitialized = false;
    if (!soulMemoryInitialized) {
        soulMemory_.Initialize();
        soulMemoryInitialized = true;
    }

    upgradeChoiceActive_ = false;
    inventoryMenuVisible_ = false;
    merchantMenuVisible_ = false;

    // ---- 商人生成逻辑 ----
    // 首层必现，其余层 50% 概率出现；商人位于玩家起始位置右侧偏移
    {
        bool spawnMerchant = (currentLevel_ == 1) || (std::rand() % 100 < 50);
        if (spawnMerchant) {
            sf::Vector2f merchantPos = playerStartPos + sf::Vector2f(48.f, 0.f);
            merchantSystem_.SpawnMerchant(merchantPos, currentLevel_);
        } else {
            merchantSystem_.ClearMerchant();
        }
    }

    // ---- 首层初始金币 50 ----
    if (!preserveProgress && currentLevel_ == 1) {
        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
        if (pc) {
            pc->stats.coins = 50;
            LOG_INFO("首层初始金币: %d", pc->stats.coins);
        }
    }

    // ---- Phase 5: 注册战斗系统事件回调 ----
    combatSystem_.OnHit = [this](const DamageInfo& info) {
        Transform* targetTransform = registry_.GetComponent<Transform>(info.target);
        if (targetTransform) {
            SpawnHitEffect(particles_, targetTransform->position, info.knockback);

            float damage = info.amount;
            if (info.isCritical) {
                damage *= CombatSystem::kDefaultCritMultiplier;
            }
            SpawnDamageText(registry_, targetTransform->position, damage, info.isCritical);

            // Phase 8: 播放命中音效
            AudioManager::Instance().PlaySFX(AudioManager::kSFXHit);

            // ---- 第三十一轮新增：屏幕震动 + 顿帧反馈 ----
            if (info.isCritical && info.attacker == playerId_) {
                // 暴击：轻微震动 + 短暂顿帧
                camera_.Shake(4.f, 0.15f);
                hitStopTimer_ = std::max(hitStopTimer_, kHitStopCrit);
            }
        }

        // ---- 第十八轮新增：玩家受伤时重置连击 ----
        if (info.target == playerId_) {
            PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
            if (pc && pc->comboCount > 0) {
                LOG_INFO("玩家受伤，连击中断（之前 %d 连击）", pc->comboCount);
                pc->comboCount = 0;
                pc->comboTimer = 0.f;
            }
            // ---- 第三十一轮新增：受伤屏幕震动 ----
            camera_.Shake(6.f, 0.2f);
        }
    };

    combatSystem_.OnKill = [this](EntityId victim, EntityId killer) {
        ++totalKillCount_;
        if (killer == playerId_) {
            PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
            if (pc) {
                ++pc->killCount;

                // ---- 第十八轮新增：连击系统累积 ----
                // 仅由玩家击杀才累积 combo（敌人间互伤/DoT 击杀不计）。
                // 每次击杀重置 comboTimer 为 3.0s，超时则归零（在 updatePlaying 中处理）。
                // Boss 击杀直接给 25 连击起步，作为对 Boss 战的奖励。
                pc->comboCount += 1;
                if (pc->comboCount > pc->comboMaxThisLife) {
                    pc->comboMaxThisLife = pc->comboCount;
                }
                pc->comboTimer = 3.0f; // 重置保持时间
            }
        }

        Transform* victimTransform = registry_.GetComponent<Transform>(victim);
        EnemyComponent* victimEnemy = registry_.GetComponent<EnemyComponent>(victim);
        if (victimTransform && victimEnemy) {
            lootSystem_.OnEnemyKilled(victim, victimTransform->position, victimEnemy->type, victimEnemy->isChampion);

            // ---- 第三十一轮新增：击杀精英/Boss 屏幕震动 + 顿帧 ----
            if (killer == playerId_) {
                if (victimEnemy->type == EnemyType::Boss) {
                    camera_.Shake(10.f, 0.4f);
                    hitStopTimer_ = std::max(hitStopTimer_, kHitStopBossKill);
                } else if (victimEnemy->isChampion || victimEnemy->type == EnemyType::Elite) {
                    camera_.Shake(6.f, 0.2f);
                    hitStopTimer_ = std::max(hitStopTimer_, kHitStopEliteKill);
                }
            }

            // ---- 上报到任务/成就系统（框架）----
            questSystem_.OnEnemyKilled(victimEnemy->type, victimEnemy->isChampion);
            achievementSystem_.OnEnemyKilled(victimEnemy->type, victimEnemy->isChampion);

            // ---- 第二十一轮新增：词缀精英击杀上报到成就系统 ----
            // 若敌人挂载了 EnemyAffix 且 affixMask != 0，上报词缀击杀事件
            // fullAffix=true 表示 4 词缀全开（满词缀精英，仅 Champion 5% 概率触发）
            const EnemyAffix* victimAffix = registry_.GetComponent<EnemyAffix>(victim);
            if (victimAffix && victimAffix->affixMask != 0u) {
                // 满词缀：4 个词缀位全部置位（0b1111 = 15）
                constexpr uint32_t kFullAffixMask =
                    static_cast<uint32_t>(EliteAffix::HpBoost) |
                    static_cast<uint32_t>(EliteAffix::DamageBoost) |
                    static_cast<uint32_t>(EliteAffix::SpeedBoost) |
                    static_cast<uint32_t>(EliteAffix::Regenerating);
                const bool fullAffix = (victimAffix->affixMask == kFullAffixMask);
                achievementSystem_.OnAffixEnemyKilled(1, fullAffix);
            }

            int expValue = 5;
            int coinValue = 1;
            switch (victimEnemy->type) {
                case EnemyType::Melee:   expValue = 5;   coinValue = 1;   break;
                case EnemyType::Ranged:  expValue = 8;   coinValue = 2;   break;
                case EnemyType::Suicide: expValue = 10;  coinValue = 3;   break;
                case EnemyType::Elite:   expValue = 50;  coinValue = 15;  break;
                case EnemyType::Boss:    expValue = 500; coinValue = 100; break;
            }
            // 精英强化版：经验/金币 ×3（与 HP 倍率一致，奖励匹配挑战）
            if (victimEnemy->isChampion) {
                expValue *= 3;
                coinValue *= 3;
            }
            // 第十七轮新增：应用变异系统经验/金币 multiplier（默认 1.0=无影响）
            // 福星高照 ×1.5 经验，贪婪之雾 ×2.0 金币；多修饰符累乘
            expValue  = static_cast<int>(expValue  * floorModifiers_.GetExpMul());
            coinValue = static_cast<int>(coinValue * floorModifiers_.GetCoinMul());
            // 防御性下限：multiplier 不应让奖励归零
            if (expValue  < 1) expValue  = 1;
            if (coinValue < 1) coinValue = 1;
            expOrbSystem_.Spawn(victimTransform->position, expValue);
            coinSystem_.Spawn(victimTransform->position, coinValue);

            // Boss 召唤物死亡时概率掉落爱心（30% 概率）
            // 第十七轮新增：变异系统"生命之涌"禁用爱心掉落（因玩家已每秒自动回血）
            if (victimEnemy->isBossMinion && !floorModifiers_.IsHeartDropDisabled()) {
                if (std::rand() % 100 < 30) {
                    heartSystem_.Spawn(victimTransform->position);
                }
            }

            // Phase 8: 播放击杀音效
            if (victimEnemy->type == EnemyType::Boss) {
                AudioManager::Instance().PlaySFX(AudioManager::kSFXExplosion);
                // ---- 第十五轮：Boss 击败必给 1 个圣物（3 选 1）----
                // 仅当玩家未拥有所有圣物时弹出选择菜单，避免空选项
                if (!relicSystem_.IsFull()) {
                    currentRelicOptions_ = relicSystem_.RollUnownedRelics(3);
                    if (!currentRelicOptions_.empty()) {
                        relicChoiceActive_ = true;
                        relicPanelVisible_ = false; // 关闭圣物查看面板，避免与选择菜单叠加
                        relicMenu_.SetOptions(currentRelicOptions_);
                        relicMenu_.SetVisible(true);
                        // 触发击败提示的同时弹出圣物选择
                        bossDefeatedHintTimer_ = 6.0f;
                        LOG_INFO("Boss 已击败，弹出圣物选择菜单（%zu 个选项）",
                                 currentRelicOptions_.size());
                    }
                }
            }
        }
    };

    // ---- Phase 6: 注册房间系统事件回调 ----
    roomSystem_.OnRoomEnter = [this](int roomIndex) {
        if (roomIndex >= 0 && roomIndex < static_cast<int>(dungeon_.rooms.size())) {
            const Room& room = dungeon_.rooms[roomIndex];
            LOG_INFO("进入房间 %d (类型=%s, 中心=(%d,%d))",
                     roomIndex, RoomTypeName(room.type),
                     room.center.x, room.center.y);
        }
    };

    // ---- 罐子破坏回调：掉落物品 + 经验球 + 金币 ----
    projectileSystem_.onPotBroken = [this](sf::Vector2f pos) {
        // 罐子最多掉落 1 件装备（品质随层数调整）
        lootSystem_.OnPotBroken(pos);
        // 生成经验球（5-15 经验）
        int exp = 5 + (std::rand() % 11);
        expOrbSystem_.Spawn(pos, exp);
        // 掉落金币（3-10 金币）
        int coins = 3 + (std::rand() % 8);
        coinSystem_.Spawn(pos, coins);
        // 标记 TileMap 顶点缓存为脏（罐子 tile 已变为 Floor）
        tileMap_.MarkDirty();
        LOG_INFO("罐子破坏，掉落物品 + %d 经验球 + %d 金币", exp, coins);
    };

    // ---- 门破坏回调：标记 TileMap 重建顶点 ----
    projectileSystem_.onDoorBroken = [this](sf::Vector2f pos) {
        tileMap_.MarkDirty();
        LOG_INFO("门已破坏 (%.1f, %.1f)", pos.x, pos.y);
    };

    // ---- 注入 CombatSystem 指针（第十六轮新增）----
    // 让 ProjectileSystem 在 handleHit 中根据 proj.element 触发元素状态效果
    // （Fire 燃烧 / Ice 冰冻 / Poison 中毒）。两者均为 Game 持有的成员，
    // 地址稳定，无需在 setupPlayingScene 重复设置。
    projectileSystem_.SetCombatSystem(&combatSystem_);

    roomSystem_.OnRoomClear = [this](int roomIndex) {
        LOG_INFO("房间 %d 已清理 (%d/%d)",
                 roomIndex,
                 roomSystem_.GetClearedRoomCount(),
                 roomSystem_.GetTotalRoomCount());
        // 诅咒房清理：解除诅咒
        if (roomIndex >= 0 && roomIndex < static_cast<int>(dungeon_.rooms.size())) {
            const Room& room = dungeon_.rooms[roomIndex];
            if (room.type == RoomType::Cursed) {
                removeCurse();
                // 诅咒房清理后额外掉落高级装备（黄色/史诗品质）
                sf::Vector2f roomCenter = dungeon_.TileCenterToWorld(room.center);
                lootSystem_.DropItem(roomCenter, ItemQuality::Yellow, currentLevel_);
                particles_.LootGlow(roomCenter);
                SpawnFloatText(registry_, roomCenter, "诅咒解除！获得史诗装备",
                               sf::Color(200, 100, 255), 22, 2.0f);
                LOG_INFO("诅咒房 %d 清理，掉落史诗装备", roomIndex);
            }
        }

        // ---- 上报到任务/成就系统：清理一个房间 ----
        questSystem_.OnRoomCleared();
        achievementSystem_.OnRoomCleared();
    };

    // ---- 事件房进入回调：显示交互提示 ----
    roomSystem_.OnEventRoomEnter = [this](int roomIndex, EventType evtType) {
        activeEventRoomIdx_ = roomIndex;
        activeEventType_ = evtType;
        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
        if (pc) {
            pc->eventPromptActive = true;
            pc->eventRoomIndex = roomIndex;
        }
        // 显示事件提示（宝箱怪事件不显示提示，避免剧透）
        const char* evtName = EventTypeName(evtType);
        std::string hint;
        if (evtType == EventType::Beggar) hint = "按 E 与乞丐对话";
        else if (evtType == EventType::Mage) hint = "按 E 与神秘法师交谈";
        else if (evtType == EventType::Altar) hint = "按 E 使用祭坛";
        else if (evtType == EventType::Forge) hint = "按 E 使用锻造台";
        // ChestMimic 不显示任何提示（宝箱伪装成普通宝箱，玩家按 E 直接触发）

        if (!hint.empty()) {
            SpawnFloatText(registry_,
                           dungeon_.TileCenterToWorld(dungeon_.rooms[roomIndex].center),
                           hint, sf::Color(255, 220, 100), 20, 2.5f);
        }
        LOG_INFO("事件房提示: %s (房间 %d)", evtName, roomIndex);

        // ---- 上报到任务/成就系统：事件房进入即视为触发事件 ----
        // 仅对真正可交互事件（祭坛/乞丐/法师/宝箱怪）触发，None 不上报
        if (evtType != EventType::None) {
            questSystem_.OnEventTriggered(evtType);
            achievementSystem_.OnEventTriggered();
        }
    };

    // ---- 诅咒房进入回调：施加诅咒 ----
    roomSystem_.OnCursedRoomEnter = [this](int roomIndex) {
        applyCurse(roomIndex);
    };

    // ---- 掉落物粒子特效回调 ----
    // 物品掉落时生成品质颜色粒子爆裂效果
    lootSystem_.OnItemDropped = [this](sf::Vector2f pos, ItemQuality quality) {
        sf::Color color = LootSystem::GetQualityColor(quality);
        EmitConfig cfg;
        cfg.radial = true;
        cfg.speedMin = 60.f;
        cfg.speedMax = 180.f;
        cfg.colorMin = sf::Color(color.r, color.g, color.b, 255);
        cfg.colorMax = sf::Color(
            static_cast<sf::Uint8>((color.r + 255) / 2),
            static_cast<sf::Uint8>((color.g + 255) / 2),
            static_cast<sf::Uint8>((color.b + 255) / 2), 255);
        cfg.sizeMin = 3.f;
        cfg.sizeMax = 7.f;
        cfg.lifeMin = 0.4f;
        cfg.lifeMax = 0.8f;
        particles_.Emit(pos, 15, cfg);
    };

    // ---- 物品拾取回调：上报到任务/成就系统 ----
    lootSystem_.OnItemPickedUp = [this](ItemQuality quality) {
        questSystem_.OnItemPickedUp(quality);
        achievementSystem_.OnItemPickedUp(quality);
    };

    // ---- 金币拾取回调：上报到成就系统（任务系统不追踪金币）----
    coinSystem_.OnCoinGained = [this](int amount) {
        achievementSystem_.OnCoinsGained(amount);
    };

    // ---- 升级回调：上报到成就系统（每次升级 +1 技能点）+ 补满法力值 ----
    upgradeSystem_.OnLevelUp = [this](int /*newLevel*/) {
        achievementSystem_.OnSkillPointsGained(1);
        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
        if (pc) {
            pc->stats.currentMp = pc->stats.maxMp;
        }
    };

    // 经验球生成时生成亮绿色粒子爆裂效果
    expOrbSystem_.OnOrbSpawned = [this](sf::Vector2f pos) {
        EmitConfig cfg;
        cfg.radial = true;
        cfg.speedMin = 40.f;
        cfg.speedMax = 120.f;
        cfg.colorMin = sf::Color(50, 255, 50, 255);
        cfg.colorMax = sf::Color(150, 255, 150, 255);
        cfg.sizeMin = 2.f;
        cfg.sizeMax = 5.f;
        cfg.lifeMin = 0.3f;
        cfg.lifeMax = 0.6f;
        particles_.Emit(pos, 10, cfg);
    };

    // ---- 首次游戏教程对话 ----
    // 检查 settings_.IsFirstPlay()，如果为 true 则启动教程对话并标记为 false
    if (!preserveProgress && settings_.IsFirstPlay() && dialogueTreeId_Tutorial_ >= 0) {
        dialogueSystem_.StartDialogue(dialogueTreeId_Tutorial_);
        dialogueBoxUI_.SetVisible(true);
        settings_.SetFirstPlay(false);
        settings_.Save();
        LOG_INFO("首次游戏，显示教程对话");
    }

    LOG_INFO("演示场景已创建: %zu 个实体, 图集 %zu 张图片",
             registry_.GetEntityCount(), atlas_.GetImageCount());
}

// ============================================================================
// loadPlayerSpriteSheet —— 加载玩家 Sprite Sheet
// ============================================================================
PlayerSheetInfo Game::loadPlayerSpriteSheet() {
    const std::string playerTexturePath = "assets/sprites/player.png";
    sf::Image playerImage;
    bool useAiTexture = false;

    if (playerImage.loadFromFile(playerTexturePath)) {
        sf::Vector2u size = playerImage.getSize();
        LOG_INFO("玩家贴图已加载: %s (%ux%u)", playerTexturePath.c_str(), size.x, size.y);

        if (size.x == 128 && size.y == 128) {
            useAiTexture = true;
        } else {
            LOG_WARN("玩家贴图尺寸 %ux%u 非 128x128，回退过程化生成", size.x, size.y);
            useAiTexture = false;
        }
    } else {
        LOG_WARN("无法加载玩家贴图: %s，使用过程化生成", playerTexturePath.c_str());
        useAiTexture = false;
    }

    if (!useAiTexture) {
        LOG_INFO("使用过程化生成玩家 Sprite Sheet");
        playerImage = TextureGenerator::CreatePlayerSpriteSheet();

        if (!playerImage.saveToFile("assets/generated/player_procedural.png")) {
            LOG_WARN("过程化玩家贴图保存失败（不影响运行）");
        }
    }

    if (!atlas_.AddImageFromMemory("player_sheet", playerImage)) {
        LOG_ERROR("玩家 Sprite Sheet 添加到图集失败");
    }

    PlayerSheetInfo info;
    info.atlasX = 0;
    info.atlasY = 0;
    info.frameSize = 32;
    info.framesPerRow = 4;

    return info;
}

// ============================================================================
// nextLevel —— 进入下一层
// ----------------------------------------------------------------------------
// 保留玩家属性（等级、经验、升级、装备），重新生成地牢。
// ============================================================================
void Game::nextLevel() {
    // 保存玩家关键属性（含金币、技能，因玩家实体会被重建）
    Health* oldHealth = registry_.GetComponent<Health>(playerId_);
    float savedHp = oldHealth ? oldHealth->current : 100.f;
    int savedKills = totalKillCount_;
    float savedSurvivalTime = survivalTime_;
    int savedCoins = 0;
    // 保存技能数据（技能槽 + 技能背包），避免下一层丢失
    std::array<SkillInstance, kSkillSlotCount> savedSkillSlots{};
    std::array<SkillType, kSkillBackpackSize> savedSkillBackpack{};
    // 第十八轮新增：跨层保留 comboMaxThisLife（用于成就统计/调试）
    // 注意：comboCount 与 comboTimer 不保留——换层时新地牢意味着新挑战，
    //       连击应重新累积。仅保留历史最大值作为玩家战绩记录。
    int savedComboMax = 0;
    // 第二十轮新增：跨层保留 perfectDodgeCount（成就统计）与 perfectDodgeMaxThisLife（调试）
    // perfectDodgeBuffTimer/Cooldown 不保留——换层时新地牢重新累积，避免携带 buff 进入下层破坏平衡
    int savedPerfectDodgeCount = 0;
    int savedPerfectDodgeMax = 0;
    PlayerComponent* oldPc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (oldPc) {
        savedCoins = oldPc->stats.coins;
        savedSkillSlots = oldPc->skillSlots;
        savedSkillBackpack = oldPc->skillBackpack;
        savedComboMax = oldPc->comboMaxThisLife;
        savedPerfectDodgeCount = oldPc->perfectDodgeCount;
        savedPerfectDodgeMax = oldPc->perfectDodgeMaxThisLife;
    }

    // 关闭商人菜单（若打开）
    if (merchantMenuVisible_) {
        merchantMenuVisible_ = false;
        merchantMenu_.SetVisible(false);
    }

    // 增加层数
    ++currentLevel_;

    // 第十七轮新增：滚动新层的地牢变异 modifier
    // level=1 无修饰符，level=2-4 一个，level>=5 两个
    // 必须在 setupPlayingScene 之前调用，因 setupPlayingScene 会将 modifier 推送到子系统
    floorModifiers_.RollForLevel(currentLevel_);
    if (floorModifiers_.GetActiveCount() > 0) {
        modifierBannerTimer_ = kModifierBannerDuration; // 触发 5 秒 Banner 提示
        LOG_INFO("第 %d 层变异: %s", currentLevel_, floorModifiers_.GetActiveSummary().c_str());
    } else {
        modifierBannerTimer_ = 0.f; // 第 1 层或异常情况不显示 Banner
    }
    regenAccumulator_ = 0.f;
    manaRegenAccumulator_ = 0.f;

    // 重新生成场景（保留升级/装备/经验/任务进度）
    setupPlayingScene(true);

    // ---- 上报到任务/成就系统：到达新层数 ----
    // 必须在 setupPlayingScene 之后调用，否则 Initialize 会清空任务进度
    questSystem_.OnLevelReached(currentLevel_);
    achievementSystem_.OnLevelReached(currentLevel_);

    // 恢复玩家属性
    Health* newHealth = registry_.GetComponent<Health>(playerId_);
    if (newHealth) {
        newHealth->current = savedHp;
    }
    // 恢复金币与技能数据（setupPlayingScene 仅在首层且非保留模式下设置 50 金，此处恢复实际数据）
    PlayerComponent* newPc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (newPc) {
        newPc->stats.coins = savedCoins;
        newPc->skillSlots = savedSkillSlots;
        newPc->skillBackpack = savedSkillBackpack;
        // 第十八轮新增：恢复跨层保留的 comboMaxThisLife
        // comboCount/comboTimer 默认初始化为 0（新地牢重新累积），无需显式重置
        newPc->comboMaxThisLife = savedComboMax;
        // 第二十轮新增：恢复跨层保留的 perfectDodgeCount/Max
        // buffTimer/Cooldown 默认 0（新地牢重新触发），lastPerfectDodgeCount_ 也同步更新
        newPc->perfectDodgeCount = savedPerfectDodgeCount;
        newPc->perfectDodgeMaxThisLife = savedPerfectDodgeMax;
        lastPerfectDodgeCount_ = savedPerfectDodgeCount;
    }
    totalKillCount_ = savedKills;
    survivalTime_ = savedSurvivalTime;

    // 重新计算玩家属性（应用升级和装备词缀到新玩家实体）
    recomputePlayerStats();

    // 重置 BOSS 状态
    bossActive_ = false;
    bossEntityId_ = kInvalidEntity;
    bossRoomEntered_ = false;
    bossNoDamageTimer_ = 0.f;
    bossDefeatedHintTimer_ = 0.f;

    // 自动保存到当前槽位（保留存档时状态：HP/金币/装备/技能/层数/击杀/时长）
    autoSaveCurrent();

    LOG_INFO("已进入第 %d 层（保留等级/经验/属性/装备/技能）", currentLevel_);
}

// ============================================================================
// resetAllUIFlags —— 重置所有 UI 可见性标志
// ============================================================================
void Game::resetAllUIFlags() {
    inventoryMenuVisible_ = false;
    merchantMenuVisible_ = false;
    questMenuVisible_ = false;
    achievementMenuVisible_ = false;
    relicPanelVisible_ = false;
    debugPanelVisible_ = false;
    settingsMenuVisible_ = false;
    saveLoadMenuVisible_ = false;
    soulWellMenuVisible_ = false;
    upgradeChoiceActive_ = false;
    relicChoiceActive_ = false;
    tutorialVisible_ = false;
    if (saveLoadMenu_.IsVisible()) saveLoadMenu_.SetVisible(false);
    if (soulWellMenu_.IsVisible()) soulWellMenu_.SetVisible(false);
    if (settingsMenu_.IsVisible()) settingsMenu_.SetVisible(false);
    if (inventoryMenu_.IsVisible()) inventoryMenu_.SetVisible(false);
    if (merchantMenu_.IsVisible()) merchantMenu_.SetVisible(false);
    if (questMenu_.IsVisible()) questMenu_.SetVisible(false);
    if (achievementMenu_.IsVisible()) achievementMenu_.SetVisible(false);
    // 对话系统清理
    dialogueSystem_.EndDialogue();
    dialogueBoxUI_.SetVisible(false);
    pendingEventRoomIdx_ = -1;
    pendingMerchantOpen_ = false;
    pendingTutorialQuestOpen_ = false;
}

// ============================================================================
// restartGame —— 重新开始游戏（重置所有状态）
// ============================================================================
void Game::restartGame() {
    LOG_INFO("重新开始游戏");

    // 重置游戏状态
    currentLevel_ = 1;
    totalKillCount_ = 0;
    survivalTime_ = 0.f;
    currentWaveNumber_ = 0;
    bossKillCountThisRun_ = 0; // 第二十四轮新增：重置本局 Boss 击杀计数

    // 第三十轮新增：重置死亡回顾字段
    lastKillerName_ = "";
    totalDamageDealt_ = 0.f;
    comboAtDeath_ = 0;

    // 第三十一轮新增：重置顿帧
    hitStopTimer_ = 0.f;

    // 重置 BOSS 状态
    bossActive_ = false;
    bossEntityId_ = kInvalidEntity;
    bossRoomEntered_ = false;
    bossNoDamageTimer_ = 0.f;
    bossDefeatedHintTimer_ = 0.f;

    // 第十七轮新增：重置地牢变异系统状态
    floorModifiers_.Clear();
    modifierBannerTimer_ = 0.f;
    regenAccumulator_ = 0.f;
    manaRegenAccumulator_ = 0.f;

    // 重置所有 UI 标志
    resetAllUIFlags();
    
    // 直接切换到 Playing 状态并重建场景（不保留进度）
    state_ = GameState::Playing;
    setupPlayingScene(false);
    AudioManager::Instance().PlayBGM(AudioManager::kBGMDungeon);
}

} // namespace cu