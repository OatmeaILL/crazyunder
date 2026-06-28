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

Game::Game()
    : resources_(ResourceManager::Instance())
    , state_(GameState::Menu) {
    // 加载持久化设置（音量、分辨率），失败则使用默认值
    settings_.Load();

    // 创建主窗口：按设置分辨率，标题固定，仅关闭按钮 + 标题栏
    window_.create(sf::VideoMode(settings_.GetWidth(), settings_.GetHeight()),
                   "CrazyUnder - 2.5D Pixel Roguelike",
                   sf::Style::Close | sf::Style::Titlebar);
    // 启用垂直同步，消除画面撕裂，提供更流畅的视觉体验
    window_.setVerticalSyncEnabled(true);

    // 摄像机视口尺寸固定为逻辑分辨率 1280x720（不随窗口物理尺寸变化）
    // 窗口尺寸不同时，由 SFML View 自动等比缩放显示
    camera_.SetViewportSize(sf::Vector2f(1280.f, 720.f));
    // 告知摄像机窗口物理尺寸，供 ScreenToWorld 把鼠标像素坐标转换为逻辑坐标
    camera_.SetWindowPhysicalSize(sf::Vector2f(static_cast<float>(settings_.GetWidth()),
                                               static_cast<float>(settings_.GetHeight())));

    hintText_.setFont(resources_.GetDefaultFont());
    hintText_.setCharacterSize(28);
    hintText_.setFillColor(sf::Color::White);
    hintText_.setPosition(300.f, 320.f);

    fpsText_.setFont(resources_.GetDefaultFont());
    fpsText_.setCharacterSize(18);
    fpsText_.setFillColor(sf::Color::Yellow);
    fpsText_.setPosition(10.f, 10.f);

    debugText_.setFont(resources_.GetDefaultFont());
    debugText_.setCharacterSize(16);
    debugText_.setFillColor(sf::Color::Cyan);
    debugText_.setPosition(10.f, 40.f);

    // 事件房交互提示文字（世界空间渲染）
    eventHintText_.setFont(resources_.GetDefaultFont());
    eventHintText_.setCharacterSize(20);
    eventHintText_.setFillColor(sf::Color(255, 220, 100));
    eventHintText_.setOutlineColor(sf::Color(0, 0, 0, 200));
    eventHintText_.setOutlineThickness(2.f);

    // 初始化随机数种子（粒子与场景生成用）
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // Phase 3: 注册默认按键映射（WASD + 方向键等）
    input_.RegisterDefaultMappings();

    // Phase 8: 初始化 UI 与音频系统
    initializeUI();
    AudioManager::Instance().Initialize();
    // 应用音量设置到 AudioManager
    AudioManager::Instance().SetBGMVolume(settings_.GetBGMVolume());
    AudioManager::Instance().SetSFXVolume(settings_.GetSFXVolume());

    registerStates();
    ChangeState(GameState::Menu);
}

// ============================================================================
// Phase 8: UI 初始化
// ============================================================================
void Game::initializeUI() {
    const sf::Font& font = resources_.GetDefaultFont();

    // 初始化 HUD
    hud_.Initialize(font);

    // 初始化各菜单
    mainMenu_.Initialize(font);
    pauseMenu_.Initialize(font);
    deathScreen_.Initialize(font);
    victoryScreen_.Initialize(font);
    upgradeMenu_.Initialize(font);
    relicMenu_.Initialize(font);
    inventoryMenu_.Initialize(font);
    merchantMenu_.Initialize(font);
    debugPanel_.Initialize(font);
    settingsMenu_.Initialize(font);
    questMenu_.Initialize(font);
    achievementMenu_.Initialize(font);
    saveLoadMenu_.Initialize(font);
    soulWellMenu_.Initialize(font); // 第二十四轮新增：灵魂之井面板
    // 同步当前设置到设置菜单显示
    settingsMenu_.SetBGMVolume(settings_.GetBGMVolume());
    settingsMenu_.SetSFXVolume(settings_.GetSFXVolume());
    settingsMenu_.SetResolution(settings_.GetWidth(), settings_.GetHeight());

    // 注册主菜单按钮回调
    mainMenu_.GetStartButton()->SetOnClick([this]() {
        // 新游戏：弹出槽位选择菜单（SaveNew 模式，会覆盖已有存档）
        showSaveLoadMenu(SaveLoadMenu::Mode::SaveNew);
    });
    mainMenu_.GetLoadGameButton()->SetOnClick([this]() {
        // 读取存档：弹出槽位选择菜单（Load 模式）
        showSaveLoadMenu(SaveLoadMenu::Mode::Load);
    });
    mainMenu_.GetQuitButton()->SetOnClick([this]() {
        window_.close();
    });
    mainMenu_.GetSettingsButton()->SetOnClick([this]() {
        // 打开设置菜单时同步当前值
        settingsMenu_.SetBGMVolume(settings_.GetBGMVolume());
        settingsMenu_.SetSFXVolume(settings_.GetSFXVolume());
        settingsMenu_.SetResolution(settings_.GetWidth(), settings_.GetHeight());
        settingsMenuVisible_ = true;
        settingsMenu_.SetVisible(true);
    });
    // 第二十四轮新增：灵魂之井按钮回调（打开永久强化面板）
    mainMenu_.GetSoulWellButton()->SetOnClick([this]() {
        soulMemory_.ReloadFromFile();
        soulWellMenu_.SetSoulMemoryData(soulMemory_);
        soulWellMenuVisible_ = true;
        soulWellMenu_.SetVisible(true);
    });

    // 注册暂停菜单按钮回调
    pauseMenu_.GetResumeButton()->SetOnClick([this]() {
        ChangeState(GameState::Playing);
    });
    pauseMenu_.GetSaveButton()->SetOnClick([this]() {
        // 保存进度到当前槽位
        autoSaveCurrent();
    });
    pauseMenu_.GetRestartButton()->SetOnClick([this]() {
        restartGame();
    });
    pauseMenu_.GetQuitToMenuButton()->SetOnClick([this]() {
        ChangeState(GameState::Menu);
    });

    // 注册死亡结算按钮回调
    deathScreen_.GetRestartButton()->SetOnClick([this]() {
        restartGame();
    });
    deathScreen_.GetMainMenuButton()->SetOnClick([this]() {
        ChangeState(GameState::Menu);
    });

    // 注册胜利结算按钮回调
    victoryScreen_.GetContinueButton()->SetOnClick([this]() {
        ChangeState(GameState::Playing);
    });
    victoryScreen_.GetMainMenuButton()->SetOnClick([this]() {
        ChangeState(GameState::Menu);
    });

    LOG_INFO("UI 初始化完成");
}

void Game::registerStates() {
    // ---- Menu：主菜单 ----
    states_[GameState::Menu] = {
        [this]() {
            LOG_INFO("进入状态: Menu");
            mainMenu_.SetVisible(true);
            AudioManager::Instance().PlayBGM(AudioManager::kBGMMenu);
        },
        [this]() {
            mainMenu_.SetVisible(false);
        },
        [this](float dt) {
            mainMenu_.Update(dt);
        },
        [this](float) {
            window_.clear(sf::Color(15, 20, 40));
            mainMenu_.Render(window_);
        }
    };

    // ---- Playing：游戏中 ----
    states_[GameState::Playing] = {
        [this]() {
            LOG_INFO("进入状态: Playing");
            setupPlayingScene();
            survivalTime_ = 0.f;
            AudioManager::Instance().PlayBGM(AudioManager::kBGMDungeon);
        },
        [this]() { registry_.Clear(); },
        [this](float dt) { updatePlaying(dt); },
        [this](float alpha) { renderPlaying(alpha); }
    };

    // ---- Paused：暂停 ----
    states_[GameState::Paused] = {
        [this]() {
            LOG_INFO("进入状态: Paused");
            pauseMenu_.SetVisible(true);
            AudioManager::Instance().StopBGM();
        },
        [this]() {
            pauseMenu_.SetVisible(false);
        },
        [this](float dt) {
            pauseMenu_.Update(dt);
        },
        [this](float) {
            // 先渲染游戏画面（冻结状态），再渲染暂停菜单
            renderPlaying(0.f);
            pauseMenu_.Render(window_);
        }
    };

    // ---- Dead：死亡 ----
    states_[GameState::Dead] = {
        [this]() {
            LOG_INFO("进入状态: Dead");
            deathScreen_.SetStats(totalKillCount_,
                                  upgradeSystem_.GetLevel(),
                                  survivalTime_);
            // 第二十四轮新增：计算并发放灵魂碎片（Meta Progression）
            // 公式：层数×5 + 击杀/10 + Boss×20，保底 10 碎片
            // 设计意图：让每次死亡都有进展，将挫败感转化为"下一局更强"的期待
            int shards = SoulMemorySystem::CalculateShardsGained(
                currentLevel_, totalKillCount_, bossKillCountThisRun_);
            lastShardsGained_ = shards;
            soulMemory_.AddShards(shards);
            if (!soulMemory_.SaveToFile()) {
                LOG_WARN("灵魂碎片持久化失败（本次获得 %d 碎片）", shards);
            }
            deathScreen_.SetShardsGained(shards);
            // 第三十轮新增：死亡回顾信息（使用预保存数据，因为 registry 已被 onExit 清空）
            float dps = (survivalTime_ > 0.f) ? totalDamageDealt_ / survivalTime_ : 0.f;
            deathScreen_.SetDeathReview(lastKillerName_, comboAtDeath_, dps);
            LOG_INFO("死亡回顾: 击杀者=%s combo=%d dps=%.0f",
                     lastKillerName_.empty() ? "无" : lastKillerName_.c_str(), comboAtDeath_, dps);
            LOG_INFO("本局死亡结算: 层数=%d 击杀=%d Boss=%d → 获得 %d 灵魂碎片",
                     currentLevel_, totalKillCount_, bossKillCountThisRun_, shards);
            deathScreen_.SetVisible(true);
            AudioManager::Instance().StopBGM();
            AudioManager::Instance().PlaySFX(AudioManager::kSFXPlayerDeath);
        },
        [this]() {
            deathScreen_.SetVisible(false);
        },
        [this](float dt) {
            deathScreen_.Update(dt);
        },
        [this](float) {
            window_.clear(sf::Color(40, 10, 10));
            deathScreen_.Render(window_);
        }
    };

    // ---- Victory：胜利 ----
    states_[GameState::Victory] = {
        [this]() {
            LOG_INFO("进入状态: Victory");
            victoryScreen_.SetStats(totalKillCount_,
                                     upgradeSystem_.GetLevel(),
                                     survivalTime_);
            victoryScreen_.SetVisible(true);
            AudioManager::Instance().StopBGM();
            AudioManager::Instance().PlaySFX(AudioManager::kSFXVictory);
        },
        [this]() {
            victoryScreen_.SetVisible(false);
        },
        [this](float dt) {
            victoryScreen_.Update(dt);
        },
        [this](float) {
            window_.clear(sf::Color(40, 30, 10));
            victoryScreen_.Render(window_);
        }
    };
}

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
// Phase 8: 显示升级选择菜单
// ============================================================================
void Game::showUpgradeChoice() {
    currentUpgradeOptions_ = upgradeSystem_.RollUpgrades();
    upgradeChoiceActive_ = true;
    relicPanelVisible_ = false; // 升级菜单打开时关闭圣物面板，避免 UI 叠加
    upgradeMenu_.SetOptions(currentUpgradeOptions_);
    upgradeMenu_.SetVisible(true);
    upgradeUI_.Show(currentUpgradeOptions_);
    AudioManager::Instance().PlaySFX(AudioManager::kSFXLevelUp);
    LOG_INFO("升级选择菜单已显示");
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
// Phase 8: 处理升级选择逻辑
// ============================================================================
void Game::handleUpgradeChoice() {
    if (!upgradeChoiceActive_) return;

    // 鼠标坐标转换：窗口物理像素 → 1280x720 逻辑坐标（与升级卡片渲染坐标系一致）
    sf::View uiView(sf::FloatRect(0.f, 0.f, 1280.f, 720.f));
    sf::Vector2f mousePos = window_.mapPixelToCoords(input_.GetMousePosition(), uiView);

    // 检查鼠标点击
    if (input_.IsMousePressed(sf::Mouse::Left)) {
        int idx = upgradeMenu_.HandleMouseClick(mousePos);
        if (idx >= 0 && idx < 3 &&
            currentUpgradeOptions_[idx].type != UpgradeType::Count) {
            UpgradeType chosenType = currentUpgradeOptions_[idx].type;

            // 检查是否是技能升级
            static auto isSkillUpgrade = [](UpgradeType t) -> bool {
                return t == UpgradeType::SkillGroundSlam ||
                       t == UpgradeType::SkillLeechStrike ||
                       t == UpgradeType::SkillBerserk ||
                       t == UpgradeType::SkillGravityWell ||
                       t == UpgradeType::SkillSpikeGround;
            };
            static auto upgradeToSkill = [](UpgradeType t) -> SkillType {
                switch (t) {
                    case UpgradeType::SkillGroundSlam:  return SkillType::GroundSlam;
                    case UpgradeType::SkillLeechStrike: return SkillType::LeechStrike;
                    case UpgradeType::SkillBerserk:     return SkillType::Berserk;
                    case UpgradeType::SkillGravityWell: return SkillType::GravityWell;
                    case UpgradeType::SkillSpikeGround: return SkillType::SpikeGround;
                    default: return SkillType::Count;
                }
            };

            if (isSkillUpgrade(chosenType)) {
                // 技能升级：已拥有则升级等级，未拥有则添加到背包
                PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
                if (pc) {
                    SkillType skill = upgradeToSkill(chosenType);
                    if (PlayerHasSkill(*pc, skill)) {
                        // 已拥有：升级技能等级（level++，最高 3 级）
                        if (UpgradeSkillLevel(*pc, skill)) {
                            upgradeSystem_.ApplyUpgrade(chosenType);
                            AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                            LOG_INFO("技能升级: %s -> Lv.%d", GetSkillName(skill),
                                     GetSkillLevel(*pc, skill));
                        } else {
                            LOG_WARN("技能 %s 已满级，无法继续升级", GetSkillName(skill));
                        }
                    } else {
                        // 未拥有：添加到技能背包
                        AddSkillToBackpack(*pc, skill);
                        upgradeSystem_.ApplyUpgrade(chosenType);
                        AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                        LOG_INFO("获得技能: %s", GetSkillName(skill));
                    }
                }
            } else {
                upgradeSystem_.ApplyUpgrade(chosenType);
            }
            recomputePlayerStats();

            // 若仍有剩余技能点，重新滚动选项保持菜单打开；否则关闭
            if (upgradeSystem_.GetSkillPoints() > 0) {
                currentUpgradeOptions_ = upgradeSystem_.RollUpgrades();
                upgradeMenu_.SetOptions(currentUpgradeOptions_);
                upgradeUI_.Show(currentUpgradeOptions_);
                LOG_INFO("仍有 %d 个技能点未使用，继续选择", upgradeSystem_.GetSkillPoints());
            } else {
                upgradeChoiceActive_ = false;
                upgradeMenu_.SetVisible(false);
                LOG_INFO("升级选择完成（鼠标点击），游戏继续");
            }
        }
    }

    // 更新悬停状态（mousePos 已在函数开头转换为 1280x720 逻辑坐标）
    int hoverIdx = -1;
    for (int i = 0; i < 3; ++i) {
        // 检查鼠标是否在某张卡片上
        if (upgradeMenu_.HandleMouseClick(mousePos) == i) {
            hoverIdx = i;
            break;
        }
    }
    upgradeMenu_.SetHoveredCard(hoverIdx);
}

// ============================================================================
// handleRelicChoice —— 处理圣物选择菜单（Boss 击败后 3 选 1）
// ----------------------------------------------------------------------------
// 与 handleUpgradeChoice 逻辑类似：监听鼠标点击与悬停，选中后添加圣物到玩家
// 构筑并重算属性。圣物菜单只选一次（不像升级菜单可连续选多个技能点）。
// ============================================================================
void Game::handleRelicChoice() {
    if (!relicChoiceActive_) return;

    sf::View uiView(sf::FloatRect(0.f, 0.f, 1280.f, 720.f));
    sf::Vector2f mousePos = window_.mapPixelToCoords(input_.GetMousePosition(), uiView);

    if (input_.IsMousePressed(sf::Mouse::Left)) {
        int idx = relicMenu_.HandleMouseClick(mousePos);
        if (idx >= 0 && idx < static_cast<int>(currentRelicOptions_.size())) {
            RelicType chosen = currentRelicOptions_[idx];
            if (chosen != RelicType::None) {
                relicSystem_.AddRelic(chosen);
                recomputePlayerStats();
                AudioManager::Instance().PlaySFX(AudioManager::kSFXLevelUp);
                LOG_INFO("圣物已选择: %s，当前共 %d 个圣物",
                         GetRelicName(chosen), relicSystem_.GetOwnedCount());
                relicChoiceActive_ = false;
                relicMenu_.SetVisible(false);
            }
        }
    }

    // 更新悬停状态
    int hoverIdx = -1;
    for (int i = 0; i < static_cast<int>(currentRelicOptions_.size()); ++i) {
        if (relicMenu_.HandleMouseClick(mousePos) == i) {
            hoverIdx = i;
            break;
        }
    }
    relicMenu_.SetHoveredCard(hoverIdx);
}

// ============================================================================
// 渲染圣物查看面板（R 键切换）
// ----------------------------------------------------------------------------
// 显示玩家当前已获得的圣物列表与构筑概览，让玩家随时审视自己的 Build。
// 布局：3x2 共 6 个槽位，已拥有的槽位显示图标色块+名称+描述，空槽显示"空缺"占位。
// ============================================================================
void Game::renderRelicPanel() {
    const sf::Font& font = resources_.GetDefaultFont();

    // 半透明遮罩
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(0, 0, 0, 180));
    window_.draw(overlay);

    // 主面板（居中）
    const float panelX = 280.f;
    const float panelY = 130.f;
    const float panelW = 720.f;
    const float panelH = 460.f;
    sf::RectangleShape panel(sf::Vector2f(panelW, panelH));
    panel.setPosition(panelX, panelY);
    panel.setFillColor(sf::Color(20, 25, 40, 240));
    panel.setOutlineColor(sf::Color(180, 150, 80));
    panel.setOutlineThickness(2.f);
    window_.draw(panel);

    // 标题
    sf::Text title;
    title.setFont(font);
    title.setString(U8("圣物 (Build 构筑)"));
    title.setCharacterSize(28);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(sf::Color(255, 220, 100));
    sf::FloatRect tb = title.getLocalBounds();
    title.setPosition(panelX + (panelW - tb.width) * 0.5f, panelY + 16.f);
    window_.draw(title);

    // 副标题：当前数量
    const auto& owned = relicSystem_.GetOwnedRelics();
    int ownedCount = relicSystem_.GetOwnedCount();
    sf::Text countText;
    countText.setFont(font);
    countText.setString(U8("已获得: ") + std::to_string(ownedCount) + U8(" / 6"));
    countText.setCharacterSize(18);
    countText.setFillColor(sf::Color(200, 220, 240));
    sf::FloatRect cb = countText.getLocalBounds();
    countText.setPosition(panelX + (panelW - cb.width) * 0.5f, panelY + 56.f);
    window_.draw(countText);

    // 6 个圣物槽位 (3 列 x 2 行)
    const float slotW = 200.f;
    const float slotH = 140.f;
    const float gap = 20.f;
    const float totalW = 3 * slotW + 2 * gap;       // 640
    const float startX = panelX + (panelW - totalW) * 0.5f;  // 320
    const float startY = panelY + 100.f;

    for (int i = 0; i < kRelicMaxCount; ++i) {
        int col = i % 3;
        int row = i / 3;
        float x = startX + col * (slotW + gap);
        float y = startY + row * (slotH + gap);

        RelicType rt = owned[i];

        // 槽位背景
        sf::RectangleShape slotBg(sf::Vector2f(slotW, slotH));
        slotBg.setPosition(x, y);
        if (rt != RelicType::None) {
            const RelicData& rd = GetRelicData(rt);
            slotBg.setFillColor(sf::Color(30, 30, 40, 220));
            slotBg.setOutlineColor(sf::Color(rd.r, rd.g, rd.b));
            slotBg.setOutlineThickness(2.f);
        } else {
            slotBg.setFillColor(sf::Color(20, 20, 25, 180));
            slotBg.setOutlineColor(sf::Color(80, 80, 80, 150));
            slotBg.setOutlineThickness(1.f);
        }
        window_.draw(slotBg);

        if (rt != RelicType::None) {
            const RelicData& rd = GetRelicData(rt);

            // 图标色块（圣物主色调）
            sf::RectangleShape icon(sf::Vector2f(64.f, 64.f));
            icon.setPosition(x + (slotW - 64.f) * 0.5f, y + 12.f);
            icon.setFillColor(sf::Color(rd.r, rd.g, rd.b, 220));
            icon.setOutlineColor(sf::Color::White);
            icon.setOutlineThickness(1.f);
            window_.draw(icon);

            // 圣物名称
            sf::Text nameText;
            nameText.setFont(font);
            nameText.setString(utf8ToSfString(rd.name));
            nameText.setCharacterSize(16);
            nameText.setStyle(sf::Text::Bold);
            nameText.setFillColor(sf::Color(rd.r, rd.g, rd.b));
            sf::FloatRect nb = nameText.getLocalBounds();
            nameText.setPosition(x + (slotW - nb.width) * 0.5f, y + 84.f);
            window_.draw(nameText);

            // 圣物描述
            sf::Text descText;
            descText.setFont(font);
            descText.setString(utf8ToSfString(rd.desc));
            descText.setCharacterSize(13);
            descText.setFillColor(sf::Color(220, 220, 220));
            sf::FloatRect db = descText.getLocalBounds();
            descText.setPosition(x + (slotW - db.width) * 0.5f, y + 108.f);
            window_.draw(descText);

            // ---- 第三十一轮新增：圣物叙事文本（lore）----
            if (rd.lore && rd.lore[0] != '\0') {
                sf::Text loreText;
                loreText.setFont(font);
                loreText.setString(utf8ToSfString(rd.lore));
                loreText.setCharacterSize(10);
                loreText.setFillColor(sf::Color(160, 160, 180));
                loreText.setStyle(sf::Text::Italic);
                // 自动换行：每行最多 28 个字符
                std::string loreStr(rd.lore);
                std::string wrapped;
                int charCount = 0;
                for (size_t ci = 0; ci < loreStr.size();) {
                    unsigned char c = static_cast<unsigned char>(loreStr[ci]);
                    int charLen = 1;
                    if ((c & 0xE0) == 0xC0) charLen = 2;
                    else if ((c & 0xF0) == 0xE0) charLen = 3;
                    else if ((c & 0xF8) == 0xF0) charLen = 4;
                    wrapped += loreStr.substr(ci, charLen);
                    ci += charLen;
                    ++charCount;
                    if (charCount >= 28 && ci < loreStr.size()) {
                        wrapped += '\n';
                        charCount = 0;
                    }
                }
                loreText.setString(utf8ToSfString(wrapped));
                sf::FloatRect lb = loreText.getLocalBounds();
                loreText.setPosition(x + (slotW - lb.width) * 0.5f, y + 122.f);
                window_.draw(loreText);
            }
        } else {
            // 空缺占位
            sf::Text emptyText;
            emptyText.setFont(font);
            emptyText.setString(U8("— 空缺 —"));
            emptyText.setCharacterSize(14);
            emptyText.setFillColor(sf::Color(120, 120, 120));
            sf::FloatRect eb = emptyText.getLocalBounds();
            emptyText.setPosition(x + (slotW - eb.width) * 0.5f,
                                  y + (slotH - eb.height) * 0.5f - eb.top);
            window_.draw(emptyText);
        }
    }

    // 底部提示
    sf::Text hint;
    hint.setFont(font);
    hint.setString(U8("按 R 或 ESC 关闭"));
    hint.setCharacterSize(14);
    hint.setFillColor(sf::Color(160, 160, 160));
    sf::FloatRect hb = hint.getLocalBounds();
    hint.setPosition(panelX + (panelW - hb.width) * 0.5f, panelY + panelH - 30.f);
    window_.draw(hint);
}

// ============================================================================
// Phase 8: 处理 UI 输入
// ============================================================================
void Game::handleUIInput() {
    // 使用固定 1280x720 逻辑分辨率转换鼠标坐标，保证不同窗口尺寸下 UI 命中正确
    sf::View uiView(sf::FloatRect(0.f, 0.f, 1280.f, 720.f));
    sf::Vector2f mousePos = window_.mapPixelToCoords(input_.GetMousePosition(), uiView);
    bool mousePressed = input_.IsMousePressed(sf::Mouse::Left);

    // ---- 设置菜单处理（优先级最高，打开时屏蔽其他 UI 输入）----
    if (settingsMenuVisible_) {
        if (mousePressed) {
            int action = settingsMenu_.CheckClick(mousePos);
            if (action != 0) {
                handleSettingsMenuClick(action);
            }
        }
        return; // 设置菜单打开时不处理其他 UI 输入
    }

    // ---- 第二十四轮新增：灵魂之井面板处理（优先级最高，打开时屏蔽其他 UI 输入）----
    if (soulWellMenuVisible_) {
        soulWellMenu_.UpdateHover(mousePos);
        if (mousePressed) {
            int action = soulWellMenu_.CheckClick(mousePos);
            if (action != 0) {
                handleSoulWellMenuClick(action);
            }
        }
        return; // 灵魂之井面板打开时不处理其他 UI 输入
    }

    // ---- 存档菜单处理（优先级最高，打开时屏蔽其他 UI 输入）----
    if (saveLoadMenuVisible_) {
        if (mousePressed) {
            int action = saveLoadMenu_.CheckClick(mousePos);
            if (action != 0) {
                handleSaveLoadMenuClick(action);
            }
        }
        return; // 存档菜单打开时不处理其他 UI 输入
    }

    // ---- 商人菜单鼠标点击处理（Playing 状态）----
    if (state_ == GameState::Playing && merchantMenuVisible_) {
        // 每帧更新悬停状态（用于技能 tooltip）
        merchantMenu_.UpdateHover(mousePos);

        if (mousePressed) {
            auto [op, index] = merchantMenu_.CheckClick(mousePos);
            PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
            if (pc) {
                if (op == 1 && index >= 0 && index < MerchantSystem::kMerchantStockSize) {
                    // 购买
                    bool ok = merchantSystem_.BuyItem(index, inventorySystem_, pc->stats);
                    if (ok) {
                        AudioManager::Instance().PlaySFX(AudioManager::kSFXBuy);
                        LOG_INFO("购买成功，索引=%d", index);
                    } else {
                        LOG_WARN("购买失败，索引=%d（金币不足或已售出）", index);
                    }
                    // 刷新菜单数据
                    merchantMenu_.SetMerchantStock(merchantSystem_);
                    merchantMenu_.SetBackpack(inventorySystem_, pc->stats.coins, pc);
                } else if (op == 2 && index >= 0 && index < InventorySystem::kBackpackSize) {
                    // 出售
                    int gained = merchantSystem_.SellBackpackItem(index, inventorySystem_, pc->stats);
                    if (gained > 0) {
                        AudioManager::Instance().PlaySFX(AudioManager::kSFXSell);
                        LOG_INFO("出售成功，索引=%d，获得 %d 金币", index, gained);
                    } else {
                        LOG_WARN("出售失败，索引=%d（空槽位）", index);
                    }
                    // 刷新菜单数据
                    merchantMenu_.SetMerchantStock(merchantSystem_);
                    merchantMenu_.SetBackpack(inventorySystem_, pc->stats.coins, pc);
                } else if (op == 3 && index >= 0 && index < MerchantSystem::kMerchantSkillSize) {
                    // 购买技能
                    bool ok = merchantSystem_.BuySkill(index, *pc, pc->stats);
                    if (ok) {
                        AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                        LOG_INFO("购买技能成功，索引=%d", index);
                    } else {
                        LOG_WARN("购买技能失败，索引=%d", index);
                    }
                    merchantMenu_.SetMerchantStock(merchantSystem_);
                    merchantMenu_.SetBackpack(inventorySystem_, pc->stats.coins, pc);
                }
            }
        }
        return; // 商人菜单打开时不处理其他 UI 输入
    }

    // ---- 背包菜单鼠标点击处理（Playing 状态）----
    if (state_ == GameState::Playing && inventoryMenuVisible_) {
        // 每帧更新悬停状态（闪烁高亮）
        inventoryMenu_.UpdateHover(mousePos);

        bool rightPressed = input_.IsMousePressed(sf::Mouse::Right);

        // ---- 右键：弹出上下文菜单 ----
        if (rightPressed) {
            // 若菜单已可见，先关闭（再次右键视为取消）
            if (inventoryMenu_.IsContextMenuVisible()) {
                inventoryMenu_.CloseContextMenu();
            } else {
                inventoryMenu_.HandleRightClick(mousePos);
            }
            return;
        }

        // ---- 左键处理 ----
        if (mousePressed) {
            // 优先处理上下文菜单点击（菜单可见时）
            if (inventoryMenu_.IsContextMenuVisible()) {
                auto [action, targetInfo] = inventoryMenu_.HandleContextMenuClick(mousePos);
                int targetType = targetInfo.first;
                int targetIdx = targetInfo.second;
                inventoryMenu_.CloseContextMenu();

                if (action == 1) {
                    // 装备/卸下
                    if (targetType == 1 && targetIdx >= 0 && targetIdx < 6) {
                        // 卸下装备槽
                        if (!inventorySystem_.IsBackpackFull()) {
                            ItemSlot slotType = static_cast<ItemSlot>(targetIdx);
                            auto removed = inventorySystem_.Unequip(slotType);
                            if (removed.has_value()) {
                                inventorySystem_.AddToBackpack(*removed);
                                AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                                LOG_INFO("卸下装备槽 %d 的 %s 到背包", targetIdx, removed->name.c_str());
                            }
                        } else {
                            LOG_WARN("背包已满，无法卸下装备槽 %d", targetIdx);
                        }
                    } else if (targetType == 2 && targetIdx >= 0 && targetIdx < InventorySystem::kBackpackSize) {
                        // 装备背包物品
                        auto backpackItem = inventorySystem_.GetBackpackItem(targetIdx);
                        if (backpackItem.has_value()) {
                            const auto& equipped = inventorySystem_.GetEquippedItems();
                            int slotIdx = static_cast<int>(backpackItem->slot);
                            bool slotOccupied = equipped[slotIdx].item.has_value();
                            inventorySystem_.RemoveFromBackpack(targetIdx);
                            Item oldItem = inventorySystem_.Equip(*backpackItem);
                            if (slotOccupied) {
                                inventorySystem_.ReplaceBackpackItem(targetIdx, oldItem);
                            }
                            AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                            LOG_INFO("装备背包格 %d 的 %s", targetIdx, backpackItem->name.c_str());
                        }
                    } else if (targetType == 3 && targetIdx >= 0 && targetIdx < kSkillSlotCount) {
                        // 卸下技能槽
                        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
                        if (pc) {
                            UnequipSkill(*pc, targetIdx);
                            AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                        }
                    } else if (targetType == 4 && targetIdx >= 0 && targetIdx < kSkillBackpackSize) {
                        // 装备技能背包到第一个空技能槽
                        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
                        if (pc) {
                            int targetSlot = -1;
                            for (int i = 0; i < kSkillSlotCount; ++i) {
                                if (pc->skillSlots[i].type == SkillType::Count) {
                                    targetSlot = i;
                                    break;
                                }
                            }
                            if (targetSlot >= 0) {
                                EquipSkill(*pc, targetIdx, targetSlot);
                                AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                            }
                        }
                    }
                    // 刷新菜单数据
                    inventoryMenu_.SetInventory(inventorySystem_);
                    PlayerComponent* pc2 = registry_.GetComponent<PlayerComponent>(playerId_);
                    if (pc2) inventoryMenu_.SetSkillData(*pc2);
                } else if (action == 2) {
                    // 丢弃
                    if (targetType == 1 && targetIdx >= 0 && targetIdx < 6) {
                        // 丢弃已装备物品（直接销毁，不放入背包）
                        ItemSlot slotType = static_cast<ItemSlot>(targetIdx);
                        auto removed = inventorySystem_.Unequip(slotType);
                        if (removed.has_value()) {
                            AudioManager::Instance().PlaySFX(AudioManager::kSFXPickup);
                            LOG_INFO("已丢弃装备槽 %d 的 %s", targetIdx, removed->name.c_str());
                        }
                    } else if (targetType == 2 && targetIdx >= 0 && targetIdx < InventorySystem::kBackpackSize) {
                        // 丢弃背包物品
                        auto removed = inventorySystem_.RemoveFromBackpack(targetIdx);
                        if (removed.has_value()) {
                            AudioManager::Instance().PlaySFX(AudioManager::kSFXPickup);
                            LOG_INFO("已丢弃背包格 %d 的 %s", targetIdx, removed->name.c_str());
                        }
                    } else if (targetType == 3 && targetIdx >= 0 && targetIdx < kSkillSlotCount) {
                        // 丢弃已装备技能（直接清空槽位，不放入技能背包）
                        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
                        if (pc) {
                            SkillType discarded = pc->skillSlots[targetIdx].type;
                            pc->skillSlots[targetIdx].type = SkillType::Count;
                            pc->skillSlots[targetIdx].cooldownRemain = 0.f;
                            pc->skillSlots[targetIdx].level = 1;
                            AudioManager::Instance().PlaySFX(AudioManager::kSFXPickup);
                            LOG_INFO("已丢弃技能槽 %d 的技能", targetIdx);
                        }
                    } else if (targetType == 4 && targetIdx >= 0 && targetIdx < kSkillBackpackSize) {
                        // 丢弃技能背包中的技能
                        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
                        if (pc) {
                            pc->skillBackpack[targetIdx] = SkillType::Count;
                            AudioManager::Instance().PlaySFX(AudioManager::kSFXPickup);
                            LOG_INFO("已丢弃技能背包格 %d 的技能", targetIdx);
                        }
                    }
                    // 刷新菜单数据
                    inventoryMenu_.SetInventory(inventorySystem_);
                    PlayerComponent* pc3 = registry_.GetComponent<PlayerComponent>(playerId_);
                    if (pc3) inventoryMenu_.SetSkillData(*pc3);
                    recomputePlayerStats();
                }
                return;
            }

            // 上下文菜单不可见时，保留左键快速穿卸
            auto [op, index] = inventoryMenu_.HandleClick(mousePos);
            if (op == 1 && index >= 0 && index < 6) {
                // 卸下装备槽[index] → 放入背包
                if (inventorySystem_.IsBackpackFull()) {
                    LOG_WARN("背包已满，无法卸下装备槽 %d", index);
                } else {
                    ItemSlot slotType = static_cast<ItemSlot>(index);
                    auto removed = inventorySystem_.Unequip(slotType);
                    if (removed.has_value()) {
                        inventorySystem_.AddToBackpack(*removed);
                        AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                        LOG_INFO("卸下装备槽 %d 的 %s 到背包", index, removed->name.c_str());
                    }
                }
                inventoryMenu_.SetInventory(inventorySystem_);
                PlayerComponent* pc2 = registry_.GetComponent<PlayerComponent>(playerId_);
                if (pc2) inventoryMenu_.SetSkillData(*pc2);
            } else if (op == 2 && index >= 0 && index < InventorySystem::kBackpackSize) {
                // 装备背包[index]
                auto backpackItem = inventorySystem_.GetBackpackItem(index);
                if (backpackItem.has_value()) {
                    const auto& equipped = inventorySystem_.GetEquippedItems();
                    int slotIdx = static_cast<int>(backpackItem->slot);
                    bool slotOccupied = equipped[slotIdx].item.has_value();
                    inventorySystem_.RemoveFromBackpack(index);
                    Item oldItem = inventorySystem_.Equip(*backpackItem);
                    if (slotOccupied) {
                        inventorySystem_.ReplaceBackpackItem(index, oldItem);
                    }
                    AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                    LOG_INFO("装备背包格 %d 的 %s", index, backpackItem->name.c_str());
                }
                inventoryMenu_.SetInventory(inventorySystem_);
                PlayerComponent* pc3 = registry_.GetComponent<PlayerComponent>(playerId_);
                if (pc3) inventoryMenu_.SetSkillData(*pc3);
            }

            // ---- 技能穿卸处理（左键快速操作）----
            auto [skillOp, skillIdx] = inventoryMenu_.HandleSkillClick(mousePos);
            if (skillOp == 3 && skillIdx >= 0 && skillIdx < kSkillSlotCount) {
                PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
                if (pc) {
                    UnequipSkill(*pc, skillIdx);
                    AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                    inventoryMenu_.SetSkillData(*pc);
                }
            } else if (skillOp == 4 && skillIdx >= 0 && skillIdx < kSkillBackpackSize) {
                PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
                if (pc) {
                    int targetSlot = -1;
                    for (int i = 0; i < kSkillSlotCount; ++i) {
                        if (pc->skillSlots[i].type == SkillType::Count) {
                            targetSlot = i;
                            break;
                        }
                    }
                    if (targetSlot >= 0) {
                        EquipSkill(*pc, skillIdx, targetSlot);
                        AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                        inventoryMenu_.SetSkillData(*pc);
                    }
                }
            }
        }
        return; // 背包菜单打开时不处理其他 UI 输入
    }

    // ---- 任务面板鼠标点击处理（Playing 状态）----
    if (state_ == GameState::Playing && questMenuVisible_) {
        // 每帧更新悬停状态（领取按钮高亮）
        questMenu_.UpdateHover(mousePos);

        if (mousePressed) {
            auto [op, questId] = questMenu_.CheckClick(mousePos);
            if (op == 1 && questId > 0) {
                // 领取任务奖励
                int unlockedId = 0;
                auto reward = questSystem_.ClaimReward(questId, &unlockedId);
                if (reward.has_value()) {
                    // OnRewardGranted 回调会应用奖励到玩家
                    // （回调在 Game::initializeUI 中注册：经验/金币/装备/等级/技能点）
                    AudioManager::Instance().PlaySFX(AudioManager::kSFXPickup);
                    LOG_INFO("领取任务 %d 奖励: 经验=%d 金币=%d 装备品质=%d 等级+%d",
                             questId, reward->exp, reward->coins,
                             static_cast<int>(reward->itemQuality), reward->addLevels);
                    if (unlockedId > 0) {
                        LOG_INFO("任务 %d 完成后解锁任务 %d", questId, unlockedId);
                    }
                } else {
                    LOG_WARN("任务 %d 奖励领取失败（状态可能不是 Completed）", questId);
                }
                // 刷新菜单数据
                questMenu_.SetQuestData(questSystem_);
            }
        }
        return; // 任务菜单打开时不处理其他 UI 输入
    }

    // ---- 调试面板鼠标点击处理（Playing 状态）----
    if (state_ == GameState::Playing && debugPanelVisible_) {
        if (mousePressed) {
            int action = debugPanel_.CheckClick(mousePos);
            switch (action) {
                case 1: // 传送到出生房
                    teleportToRoom(RoomType::Spawn);
                    break;
                case 2: // 传送到宝箱房
                    teleportToRoom(RoomType::Treasure);
                    break;
                case 3: // 传送到陷阱房
                    teleportToRoom(RoomType::Trap);
                    break;
                case 4: // 传送到阻碍房
                    teleportToRoom(RoomType::Obstacle);
                    break;
                case 5: // 无敌模式
                    godMode_ = !godMode_;
                    LOG_INFO("无敌模式: %s", godMode_ ? "开启" : "关闭");
                    break;
                case 6: // 秒杀所有敌人
                    killAllEnemies();
                    break;
                case 7: // +1000 金币
                    addCoins(1000);
                    break;
                case 8: // +1000 经验
                    addExperience(1000);
                    break;
                case 9: // 清屏
                    clearScreen();
                    break;
                case 10: // 传送到 Boss 房
                    teleportToRoom(RoomType::Boss);
                    break;
                case 11: // 传送到楼梯房
                    teleportToRoom(RoomType::Stairs);
                    break;
                case 12: // 立即下一层
                    nextLevel();
                    break;
                case 13: { // +1 技能点
                    // 直接增加升级系统的技能点（通过升级触发，不消耗经验）
                    upgradeSystem_.AddExp(upgradeSystem_.GetExpToNext());
                    LOG_INFO("调试: +1 技能点");
                    break;
                }
                case 14: // 清除诅咒
                    removeCurse();
                    break;
                case 15: { // 满血
                    Health* h = registry_.GetComponent<Health>(playerId_);
                    if (h) h->current = h->max;
                    LOG_INFO("调试: 满血");
                    break;
                }
                case 16: { // 重置技能冷却
                    PlayerComponent* pc16 = registry_.GetComponent<PlayerComponent>(playerId_);
                    if (pc16) {
                        for (auto& slot : pc16->skillSlots) {
                            slot.cooldownRemain = 0.f;
                        }
                    }
                    LOG_INFO("调试: 重置技能冷却");
                    break;
                }
                case 17: // 传送到事件房
                    teleportToRoom(RoomType::Event);
                    break;
                case 18: // 传送到诅咒房
                    teleportToRoom(RoomType::Cursed);
                    break;
            }
        }
        return; // 调试面板打开时不处理其他 UI 输入
    }

    // 菜单是 Game 的直接成员，未添加到 uiManager_，因此直接检测按钮交互
    Button* buttons[6] = { nullptr };
    int buttonCount = 0;

    switch (state_) {
        case GameState::Menu:
            buttons[0] = mainMenu_.GetStartButton();
            buttons[1] = mainMenu_.GetLoadGameButton();
            buttons[2] = mainMenu_.GetSoulWellButton(); // 第二十四轮新增
            buttons[3] = mainMenu_.GetSettingsButton();
            buttons[4] = mainMenu_.GetQuitButton();
            buttonCount = 5;
            break;
        case GameState::Paused:
            buttons[0] = pauseMenu_.GetResumeButton();
            buttons[1] = pauseMenu_.GetSaveButton();
            buttons[2] = pauseMenu_.GetRestartButton();
            buttons[3] = pauseMenu_.GetQuitToMenuButton();
            buttonCount = 4;
            break;
        case GameState::Dead:
            buttons[0] = deathScreen_.GetRestartButton();
            buttons[1] = deathScreen_.GetMainMenuButton();
            buttonCount = 2;
            break;
        case GameState::Victory:
            buttons[0] = victoryScreen_.GetContinueButton();
            buttons[1] = victoryScreen_.GetMainMenuButton();
            buttonCount = 2;
            break;
        default:
            return; // Playing 状态的 UI 输入由其他逻辑处理
    }

    // 更新悬停状态 + 处理点击
    for (int i = 0; i < buttonCount; ++i) {
        Button* btn = buttons[i];
        if (!btn || !btn->IsVisible() || !btn->IsEnabled()) continue;

        if (btn->Contains(mousePos)) {
            btn->OnMouseEnter();
            if (mousePressed) {
                btn->OnClick(mousePos);
                break; // 只处理第一个命中的按钮
            }
        } else {
            btn->OnMouseLeave();
        }
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
// Phase 8: 渲染 UI
// ============================================================================
void Game::renderUI() {
    // HUD 仅在 Playing 状态渲染
    if (state_ == GameState::Playing) {
        hud_.Render(window_);

        // 第十七轮新增：地牢变异系统 UI
        //   Banner：进入新层时 5 秒淡入淡出提示（覆盖在 HUD 之上，模态菜单之下）
        //   HUD 指示器：左上角持久显示当前层激活的变异名
        renderFloorModifierHUD();
        renderFloorModifierBanner();

        if (upgradeChoiceActive_) {
            upgradeMenu_.Render(window_);
        }

        // 圣物选择菜单（Boss 击败后 3 选 1，第十五轮新增）
        if (relicChoiceActive_) {
            relicMenu_.Render(window_);
        }

        if (inventoryMenuVisible_) {
            inventoryMenu_.Render(window_);
        }

        // 圣物查看面板（R 键切换，第十五轮新增）
        if (relicPanelVisible_) {
            renderRelicPanel();
        }
    }
}

// ============================================================================
// Playing 状态：渲染
// ============================================================================
void Game::renderPlaying(float /*alpha*/) {
    // 清屏
    window_.clear(sf::Color(20, 50, 30));

    const sf::Texture* atlasTexture = atlas_.GetTexture();

    // 1. 开始场景：设置摄像机
    renderer_.BeginScene(window_, camera_);

    // ---- Phase 6: 渲染地牢 TileMap ----
    if (dungeonInitialized_) {
        tileMap_.Render(renderer_, camera_);
    }

    // 2. 绘制所有拥有 Transform + Sprite 的实体
    registry_.ForEach<Transform, Sprite>([&](EntityId id) {
        Transform* t = registry_.GetComponent<Transform>(id);
        Sprite* s = registry_.GetComponent<Sprite>(id);
        if (t && s) {
            // 跳过非活跃敌人（已死亡但未回收）
            EnemyComponent* enemy = registry_.GetComponent<EnemyComponent>(id);
            if (enemy && !enemy->active) return;

            // ---- 第十六轮新增：根据状态效果染色敌人 sprite ----
            // 让玩家直观看到哪些敌人被燃烧/冰冻/中毒/麻痹：
            //   Fire      燃烧：叠加橙红色 tint（255, 110, 50）
            //   Ice       冰冻：叠加青蓝色 tint（100, 180, 255）
            //   Poison    中毒：叠加毒绿色 tint（110, 200, 80）
            //   Lightning 麻痹：叠加亮黄色 tint（255, 230, 80）—— 第十九轮新增
            // 多状态时取优先级最高的（Lightning > Fire > Poison > Ice，按控制强度排序）
            //   Lightning 麻痹为硬控，视觉优先级最高，让玩家能识别"被控住的目标"
            // 实现：将 s->color 与 tint 做 50% 混合（保留原色识别度）
            sf::Color renderColor = s->color;
            if (enemy) {
                const StatusEffectComponent* statusComp =
                    registry_.GetComponent<StatusEffectComponent>(id);
                if (statusComp && !statusComp->effects.empty()) {
                    sf::Color tint = sf::Color::White;
                    bool hasTint = false;
                    int priority = 0; // 0=无, 1=Ice, 2=Poison, 3=Fire, 4=Lightning
                    for (const auto& eff : statusComp->effects) {
                        if (eff.type == ElementType::Lightning && priority < 4) {
                            tint = sf::Color(255, 230, 80);
                            priority = 4;
                            hasTint = true;
                        } else if (eff.type == ElementType::Fire && priority < 3) {
                            tint = sf::Color(255, 110, 50);
                            priority = 3;
                            hasTint = true;
                        } else if (eff.type == ElementType::Poison && priority < 2) {
                            tint = sf::Color(110, 200, 80);
                            priority = 2;
                            hasTint = true;
                        } else if (eff.type == ElementType::Ice && priority < 1) {
                            tint = sf::Color(100, 180, 255);
                            priority = 1;
                            hasTint = true;
                        }
                    }
                    if (hasTint) {
                        // 50% 混合：保留原色识别度，同时呈现状态色
                        renderColor.r = static_cast<sf::Uint8>((s->color.r + tint.r) / 2);
                        renderColor.g = static_cast<sf::Uint8>((s->color.g + tint.g) / 2);
                        renderColor.b = static_cast<sf::Uint8>((s->color.b + tint.b) / 2);
                        renderColor.a = s->color.a;
                    }
                }

                // ---- 第二十一轮新增：词缀精英紫色发光边缘 ----
                // 激活 EnemyAffix 词缀系统的视觉反馈：词缀敌人 sprite 与紫色 (180, 80, 255)
                // 做 30% 混合（轻度染色，比状态效果弱，避免覆盖元素状态色）。
                // 让玩家能视觉识别"词缀精英"并优先击杀，与紫色光环粒子呼应。
                // 不覆盖 Champion 金色描边（isChampion 的金色血条仍渲染在头顶）。
                const EnemyAffix* affix = registry_.GetComponent<EnemyAffix>(id);
                if (affix && affix->affixMask != 0u) {
                    constexpr float kMix = 0.3f; // 30% 紫色混合
                    renderColor.r = static_cast<sf::Uint8>(renderColor.r * (1.f - kMix) + 180.f * kMix);
                    renderColor.g = static_cast<sf::Uint8>(renderColor.g * (1.f - kMix) + 80.f  * kMix);
                    renderColor.b = static_cast<sf::Uint8>(renderColor.b * (1.f - kMix) + 255.f * kMix);
                }
            }

            renderer_.DrawSprite(atlasTexture, t->position,
                                 s->sourceRect, renderColor, t->scale);
        }
    });

    // 3. 绘制粒子
    particles_.Render(renderer_);

    // 3.5 绘制掉落物（战利品光圈）
    lootSystem_.Render(renderer_);

    // 3.6 绘制经验球（绿色发光圆点）
    expOrbSystem_.Render(renderer_);

    // 3.7 绘制金币（金色发光圆点）
    coinSystem_.Render(renderer_);

    // 3.8 绘制爱心（红色发光，Boss 召唤物掉落）
    heartSystem_.Render(renderer_);

    // 4. 结束场景
    renderer_.EndScene();

    // 4.5 渲染商人头顶文字（世界空间，此时 view 仍为摄像机视图）
    // 直接在商人世界位置上方绘制，摄像机自动处理坐标转换
    if (merchantSystem_.IsActive()) {
        sf::Vector2f merchantWorldPos = merchantSystem_.GetPosition();

        sf::Text merchantLabel;
        merchantLabel.setFont(resources_.GetDefaultFont());
        merchantLabel.setString(U8("神秘商人"));
        merchantLabel.setCharacterSize(14);
        merchantLabel.setFillColor(sf::Color(255, 220, 100));
        merchantLabel.setStyle(sf::Text::Bold);
        merchantLabel.setOutlineColor(sf::Color(0, 0, 0, 200));
        merchantLabel.setOutlineThickness(2.f);
        sf::FloatRect mb = merchantLabel.getLocalBounds();
        merchantLabel.setOrigin(mb.width * 0.5f, mb.height * 0.5f);
        merchantLabel.setPosition(merchantWorldPos.x, merchantWorldPos.y - 36.f);
        window_.draw(merchantLabel);

        // 玩家靠近时显示 "按 E 交易" 提示
        Transform* pT = registry_.GetComponent<Transform>(playerId_);
        if (pT && merchantSystem_.IsPlayerInRange(pT->position)) {
            sf::Text hint;
            hint.setFont(resources_.GetDefaultFont());
            hint.setString(U8("按 E 交易"));
            hint.setCharacterSize(12);
            hint.setFillColor(sf::Color(180, 255, 180));
            hint.setOutlineColor(sf::Color(0, 0, 0, 200));
            hint.setOutlineThickness(2.f);
            sf::FloatRect hb = hint.getLocalBounds();
            hint.setOrigin(hb.width * 0.5f, hb.height * 0.5f);
            hint.setPosition(merchantWorldPos.x, merchantWorldPos.y - 52.f);
            window_.draw(hint);
        }
    }

    // 4.6 渲染 Boss 冲撞地裂区域（世界空间，在摄像机视图下）
    renderFissureZones();

    // 4.7 渲染事件房交互提示（世界空间）
    renderEventHint();

    // 4.75 渲染精英强化怪头上小血条（世界空间，跟随摄像机）
    renderChampionHealthBars();

    // 5. 切换回屏幕空间绘制 UI（固定 1280x720 逻辑分辨率，由 SFML 自动缩放到窗口）
    window_.setView(sf::View(sf::FloatRect(0.f, 0.f, 1280.f, 720.f)));

    // 6. 渲染伤害飘字
    RenderDamageTexts(registry_, window_, camera_, resources_.GetDefaultFont());

    // 6.5 渲染门的血量条（屏幕空间）
    if (dungeonInitialized_) {
        renderDoorHealthBars();
    }

    // 6.6 渲染 BOSS 血条（屏幕顶部）
    renderBossHealthBar();

    // 6.65 渲染 BOSS 击败提示（屏幕中央上方）
    if (bossDefeatedHintTimer_ > 0.f) {
        sf::Text bossDefeatedText;
        bossDefeatedText.setFont(resources_.GetDefaultFont());
        bossDefeatedText.setString(U8("BOSS 已击败！通过通道前往下一层"));
        bossDefeatedText.setCharacterSize(24);
        bossDefeatedText.setFillColor(sf::Color(255, 220, 100));
        bossDefeatedText.setStyle(sf::Text::Bold);
        bossDefeatedText.setOutlineColor(sf::Color(0, 0, 0, 200));
        bossDefeatedText.setOutlineThickness(3.f);
        sf::FloatRect bdt = bossDefeatedText.getLocalBounds();
        bossDefeatedText.setOrigin(bdt.width * 0.5f, bdt.height * 0.5f);
        bossDefeatedText.setPosition(640.f, 120.f);
        // 闪烁效果（最后3秒闪烁）
        if (bossDefeatedHintTimer_ < 3.f) {
            float alpha = 128.f + 127.f * std::sin(bossDefeatedHintTimer_ * 10.f);
            bossDefeatedText.setFillColor(sf::Color(255, 220, 100, static_cast<uint8_t>(alpha)));
        }
        window_.draw(bossDefeatedText);
    }

    // 7. 操作提示（底部小字）
    hintText_.setCharacterSize(14);
    hintText_.setPosition(10.f, 695.f);
    hintText_.setString(U8("WASD:移动 | 左键:射击 | 右键:闪避 | 空格:AOE | E:交互/商人 | G:背包 | J:技能升级 | H:帮助 | F1:调试 | P/ESC:暂停"));
    window_.draw(hintText_);

    // 8. 调试信息（F1 切换）
    if (debugMode_) {
        std::string debug;
        debug += "FPS: " + std::to_string(time_.GetFPS()) + "\n";
        debug += "绘制调用: " + std::to_string(renderer_.GetDrawCallCount()) + "\n";
        debug += "顶点数: " + std::to_string(renderer_.GetVertexCount()) + "\n";
        debug += "实体数: " + std::to_string(registry_.GetEntityCount()) + "\n";
        debug += "粒子数: " + std::to_string(particles_.GetActiveCount()) + "\n";
        debug += "敌人数: " + std::to_string(enemySpawner_.GetAliveCount()) + "\n";
        debug += "波次: " + std::to_string(currentWaveNumber_) + "\n";
        debug += "流场耗时: " + std::to_string(lastFlowFieldTimeMs_) + " ms\n";
        debug += "AI更新: " + std::to_string(lastAIUpdateTimeMs_) + " ms\n";
        debug += "敌人池: " + std::to_string(enemySpawner_.GetFreeCount()) + "/" + std::to_string(enemySpawner_.GetPoolCapacity()) + "\n";
        debug += "子弹数: " + std::to_string(projectileSystem_.GetActiveCount()) + "\n";
        debug += "子弹池: " + std::to_string(projectileSystem_.GetFreeCount()) + "/" + std::to_string(projectileSystem_.GetPoolCapacity()) + "\n";
        debug += "子弹更新: " + std::to_string(lastProjectileTimeMs_) + " ms\n";
        debug += "战斗更新: " + std::to_string(lastCombatTimeMs_) + " ms\n";
        debug += "击杀数: " + std::to_string(totalKillCount_) + "\n";
        Transform* t = registry_.GetComponent<Transform>(playerId_);
        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
        AnimationComponent* anim = registry_.GetComponent<AnimationComponent>(playerId_);
        if (t) {
            debug += "Player Pos: (" + std::to_string(t->position.x) + ", "
                   + std::to_string(t->position.y) + ")\n";
        }
        if (pc) {
            debug += "Facing: " + std::string(FacingDirectionName(pc->facing)) + "\n";
            debug += "Anim: " + std::string(PlayerAnimStateName(pc->animState)) + "\n";
            debug += "HP: " + std::to_string(static_cast<int>(pc->stats.currentHp))
                   + "/" + std::to_string(static_cast<int>(pc->stats.maxHp)) + "\n";
            debug += "Speed: " + std::to_string(static_cast<int>(pc->stats.moveSpeed)) + "\n";
            debug += "Atk CD: " + std::to_string(pc->attackCooldown) + "\n";
            debug += "Dodge CD: " + std::to_string(pc->dodgeCooldown) + "\n";
            debug += "AOE CD: " + std::to_string(pc->aoeCooldown) + "\n";
            debug += "Level: " + std::to_string(pc->stats.level) + "\n";
            debug += "EXP: " + std::to_string(static_cast<int>(pc->stats.exp)) + "/"
                   + std::to_string(static_cast<int>(pc->stats.expToNext)) + "\n";
        }
        if (anim) {
            debug += "Frame: " + std::to_string(anim->currentFrame) + "/"
                   + std::to_string(anim->frames.size()) + "\n";
        }
        sf::Vector2f mouseWorld = input_.GetMouseWorldPosition(camera_);
        debug += "Mouse World: (" + std::to_string(static_cast<int>(mouseWorld.x))
               + ", " + std::to_string(static_cast<int>(mouseWorld.y)) + ")\n";

        if (dungeonInitialized_) {
            debug += "--- Dungeon ---\n";
            debug += "Seed: " + std::to_string(dungeonSeed_) + "\n";
            debug += "Size: " + std::to_string(dungeon_.width) + "x" + std::to_string(dungeon_.height) + "\n";
            debug += "Rooms: " + std::to_string(dungeon_.rooms.size()) + "\n";
            debug += "Cleared: " + std::to_string(roomSystem_.GetClearedRoomCount())
                   + "/" + std::to_string(static_cast<int>(dungeon_.rooms.size())) + "\n";
            debug += "Visible Tiles: " + std::to_string(tileMap_.GetVisibleTileCount()) + "\n";
            int curRoom = roomSystem_.GetCurrentRoomIndex();
            if (curRoom >= 0 && curRoom < static_cast<int>(dungeon_.rooms.size())) {
                const Room& room = dungeon_.rooms[curRoom];
                debug += "Current Room: #" + std::to_string(curRoom)
                       + " (" + RoomTypeName(room.type) + ")\n";
            } else {
                debug += "Current Room: Corridor\n";
            }
            if (t) {
                sf::Vector2i playerTile = dungeon_.WorldToTile(t->position);
                debug += "Player Tile: (" + std::to_string(playerTile.x)
                       + ", " + std::to_string(playerTile.y) + ")\n";
                TileType pt = dungeon_.GetTile(playerTile.x, playerTile.y);
                debug += "Tile Type: " + std::string(TileTypeName(pt)) + "\n";
            }
        }
        debugText_.setString(utf8ToSfString(debug));
        window_.draw(debugText_);
    }

    // Phase 8: 渲染 HUD（血条/蓝条/经验条/技能图标/小地图/波次/FPS）
    hud_.Render(window_);

    // Phase 8: 渲染升级选择菜单
    if (upgradeChoiceActive_) {
        upgradeMenu_.Render(window_);
    }

    // Phase 8: 渲染背包菜单
    if (inventoryMenuVisible_) {
        inventoryMenu_.Render(window_);
    }

    // Phase 8: 渲染商人交易菜单
    if (merchantMenuVisible_) {
        merchantMenu_.Render(window_);
    }

    // 渲染任务面板（按 Q 切换）
    if (questMenuVisible_) {
        questMenu_.Render(window_);
    }

    // 渲染成就面板（按 Tab 切换）
    if (achievementMenuVisible_) {
        achievementMenu_.Render(window_);
    }

    // 调试面板（F5）
    if (debugPanelVisible_) {
        debugPanel_.Render(window_);
    }

    // 圣物查看面板（R 键切换，第十五轮新增）
    if (relicPanelVisible_) {
        renderRelicPanel();
    }

    // 9. 渲染按键教程覆盖层（首次进入游戏时显示）
    if (tutorialVisible_) {
        renderTutorial();
    }
}

void Game::ChangeState(GameState newState) {
    if (state_ == newState) return;

    // Playing ↔ Paused 切换时非对称处理：
    //   Playing→Paused: 不调用 Playing.onExit（避免 registry_.Clear() 清空场景），
    //                    但必须调用 Paused.onEnter（显示暂停菜单、停止BGM）
    //   Paused→Playing: 调用 Paused.onExit（隐藏暂停菜单），
    //                   但不调用 Playing.onEnter（避免 setupPlayingScene() 重建场景）
    bool skipOnExit = (state_ == GameState::Playing && newState == GameState::Paused);
    bool skipOnEnter = (state_ == GameState::Paused && newState == GameState::Playing);

    if (!skipOnExit) {
        if (auto it = states_.find(state_); it != states_.end() && it->second.onExit) {
            it->second.onExit();
        }
    }
    state_ = newState;
    if (!skipOnEnter) {
        if (auto it = states_.find(state_); it != states_.end() && it->second.onEnter) {
            it->second.onEnter();
        }
    }
}

// ============================================================================
// handleInteract —— E 键交互：开关门 + 开宝箱
// ----------------------------------------------------------------------------
// 在 handleEvents 的 KeyPressed 分支中调用，确保每次按键只触发一次。
//
// 门交互：
//   1. 搜索玩家周围 8 格的 Door tile
//   2. 切换 open 状态
//   3. 关门前检查玩家是否站在门 tile 上，若是则不允许关闭（避免卡住）
//
// 宝箱交互：
//   1. 搜索玩家周围 8 格的 Chest tile
//   2. 打开宝箱：SetTile(Floor)，获得金币，显示提示
// ============================================================================
void Game::handleInteract() {
    Transform* pT = registry_.GetComponent<Transform>(playerId_);
    if (!pT) return;

    // ---- 事件房交互优先级最高 ----
    // 玩家在事件房内且事件未触发时，按 E 触发事件
    if (activeEventRoomIdx_ >= 0 && activeEventType_ != EventType::None) {
        // 检查玩家是否仍在事件房内
        int curRoom = roomSystem_.GetCurrentRoomIndex();
        if (curRoom == activeEventRoomIdx_) {
            handleEventInteraction();
            return; // 事件交互优先，不处理门/宝箱/楼梯
        }
    }

    // ---- 商人交互：靠近商人按 E 打开交易菜单 ----
    if (merchantSystem_.IsActive() && merchantSystem_.IsPlayerInRange(pT->position)) {
        if (!merchantMenuVisible_) {
            merchantMenuVisible_ = true;
            relicPanelVisible_ = false; // 商人菜单打开时关闭圣物面板
            // 刷新商人菜单数据
            PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
            int playerCoins = pc ? pc->stats.coins : 0;
            merchantMenu_.SetMerchantStock(merchantSystem_);
            merchantMenu_.SetBackpack(inventorySystem_, playerCoins, pc);
            merchantMenu_.SetVisible(true);
            AudioManager::Instance().PlaySFX(AudioManager::kSFXMerchant);
            LOG_INFO("商人菜单已打开（玩家金币=%d）", playerCoins);
        } else {
            // 再次按 E 关闭菜单
            merchantMenuVisible_ = false;
            merchantMenu_.SetVisible(false);
            LOG_INFO("商人菜单已关闭");
        }
        return; // 商人交互优先，不处理门/宝箱/楼梯
    }

    sf::Vector2i playerTile = dungeon_.WorldToTile(pT->position);
    bool interacted = false;

    // 搜索玩家周围 8 格（含自身位置）
    for (int dy = -1; dy <= 1 && !interacted; ++dy) {
        for (int dx = -1; dx <= 1 && !interacted; ++dx) {
            int cx = playerTile.x + dx;
            int cy = playerTile.y + dy;

            // ---- 门交互 ----
            if (dungeon_.GetTile(cx, cy) == TileType::Door) {
                DoorState* ds = dungeon_.GetDoorState(cx, cy);
                if (!ds) continue;

                // 上锁门无法交互
                if (ds->locked) {
                    SpawnFloatText(registry_, pT->position,
                                   "门已上锁，需清理房间才能打开",
                                   sf::Color(255, 100, 100), 18, 1.2f);
                    LOG_WARN("门 (%d,%d) 已上锁，无法交互", cx, cy);
                    interacted = true;
                    continue;
                }

                // 关门前检查：玩家是否在该门 tile 上或碰撞半径覆盖门 tile
                if (!ds->open) {
                    // 当前是关闭状态，将切换为打开，无需检查
                } else {
                    // 当前是打开状态，将切换为关闭
                    // 检查玩家是否在门 tile 上或碰撞半径覆盖门 tile 中心
                    sf::Vector2f doorCenter = dungeon_.TileCenterToWorld(sf::Vector2i(cx, cy));
                    sf::Vector2f toDoor = doorCenter - pT->position;
                    float distToDoor = std::sqrt(toDoor.x * toDoor.x + toDoor.y * toDoor.y);
                    Collider* playerCol = registry_.GetComponent<Collider>(playerId_);
                    float playerRadius = playerCol ? playerCol->radius : 16.f;
                    // 玩家碰撞半径 + tile 半宽（16px）作为阈值
                    if (distToDoor < playerRadius + 16.f) {
                        LOG_WARN("玩家太靠近门 (%d,%d)，无法关闭（避免卡住）", cx, cy);
                        SpawnDamageText(registry_, pT->position, 0.f, false);
                        continue;
                    }
                }

                ds->open = !ds->open;
                interacted = true;
                // 门开关改变贴图，标记 TileMap 重建顶点
                tileMap_.MarkDirty();
                LOG_INFO("门 (%d,%d) %s", cx, cy, ds->open ? "打开" : "关闭");
                AudioManager::Instance().PlaySFX(ds->open ? AudioManager::kSFXDoorOpen : AudioManager::kSFXDoorClose);
                continue;
            }

            // ---- 宝箱交互 ----
            if (dungeon_.GetTile(cx, cy) == TileType::Chest) {
                // 打开宝箱：变为地板
                dungeon_.SetTile(cx, cy, TileType::Floor);
                interacted = true;
                // 宝箱 tile 已改变，标记 TileMap 重建顶点
                tileMap_.MarkDirty();

                // 获得经验（随机 20-100）
                int exp = 20 + (std::rand() % 81);
                upgradeSystem_.AddExp(exp);

                // 宝箱掉落金币（20-50 金币）
                sf::Vector2f chestPos = dungeon_.TileCenterToWorld(sf::Vector2i(cx, cy));
                int coins = 20 + (std::rand() % 31);
                coinSystem_.Spawn(chestPos, coins);

                // 生成宝箱打开粒子效果
                particles_.LootGlow(chestPos);

                // 显示获得物品提示（用伤害飘字显示经验值）
                SpawnDamageText(registry_, chestPos, static_cast<float>(exp), true);

                LOG_INFO("宝箱 (%d,%d) 已打开，获得 %d 经验 + %d 金币", cx, cy, exp, coins);
                AudioManager::Instance().PlaySFX(AudioManager::kSFXChestOpen);
                continue;
            }

            // ---- 楼梯交互（进入下一层）----
            if (dungeon_.GetTile(cx, cy) == TileType::Stairs) {
                // BOSS 存活时禁止进入下一层
                if (bossActive_ && bossEntityId_ != kInvalidEntity) {
                    Health* bossHp = registry_.GetComponent<Health>(bossEntityId_);
                    if (bossHp && bossHp->current > 0.f) {
                        // 显示提示：需先击败 BOSS
                        SpawnFloatText(registry_, pT->position,
                                       "先击败 BOSS 才能进入下一层",
                                       sf::Color(255, 100, 100), 20, 1.5f);
                        LOG_WARN("BOSS 未击杀，禁止进入下一层");
                        interacted = true;
                        continue;
                    }
                }
                LOG_INFO("进入下一层（当前层 %d → %d）", currentLevel_, currentLevel_ + 1);
                nextLevel();
                return; // 场景已重置，直接返回
            }
        }
    }
}

// ============================================================================
// renderDoorHealthBars —— 渲染门的血量条（屏幕空间）
// ----------------------------------------------------------------------------
// 遍历可见区域的门，若门未满血则在其上方渲染血量条。
// 仅渲染 hp < maxHp 的门，避免满血门也显示血量条。
// ============================================================================
void Game::renderDoorHealthBars() {
    // 获取摄像机视图边界（世界坐标）
    const sf::View& view = camera_.GetView();
    sf::Vector2f viewCenter = view.getCenter();
    sf::Vector2f viewSize = view.getSize();
    sf::FloatRect viewBounds(
        viewCenter.x - viewSize.x * 0.5f,
        viewCenter.y - viewSize.y * 0.5f,
        viewSize.x,
        viewSize.y
    );

    // 转换为 tile 坐标范围
    sf::Vector2i minTile = dungeon_.WorldToTile(sf::Vector2f(viewBounds.left, viewBounds.top));
    sf::Vector2i maxTile = dungeon_.WorldToTile(
        sf::Vector2f(viewBounds.left + viewBounds.width, viewBounds.top + viewBounds.height)
    );

    // 裁剪到地牢范围
    minTile.x = std::max(0, minTile.x);
    minTile.y = std::max(0, minTile.y);
    maxTile.x = std::min(dungeon_.width - 1, maxTile.x);
    maxTile.y = std::min(dungeon_.height - 1, maxTile.y);

    // 遍历可见 tile，渲染门的血量条
    for (int ty = minTile.y; ty <= maxTile.y; ++ty) {
        for (int tx = minTile.x; tx <= maxTile.x; ++tx) {
            if (dungeon_.GetTile(tx, ty) != TileType::Door) continue;

            const DoorState* ds = dungeon_.GetDoorState(tx, ty);
            if (!ds) continue;
            // 仅未满血的门显示血量条
            if (ds->hp >= ds->maxHp) continue;

            // 门的世界坐标（中心）
            sf::Vector2f doorWorldPos = dungeon_.TileCenterToWorld(sf::Vector2i(tx, ty));
            // 转换为屏幕坐标
            sf::Vector2f screenPos = camera_.WorldToScreen(doorWorldPos);

            // 血量条尺寸
            float barWidth = 28.f;
            float barHeight = 4.f;
            float barX = screenPos.x - barWidth * 0.5f;
            float barY = screenPos.y - 24.f; // 门上方

            // 计算血量比例
            float hpRatio = ds->hp / ds->maxHp;
            if (hpRatio < 0.f) hpRatio = 0.f;
            if (hpRatio > 1.f) hpRatio = 1.f;

            // 背景（深色边框）
            sf::RectangleShape bg(sf::Vector2f(barWidth, barHeight));
            bg.setPosition(barX, barY);
            bg.setFillColor(sf::Color(40, 40, 40, 200));
            bg.setOutlineColor(sf::Color::Black);
            bg.setOutlineThickness(1.f);
            window_.draw(bg);

            // 前景（红色血量）
            float fgWidth = barWidth * hpRatio;
            if (fgWidth > 0.f) {
                sf::RectangleShape fg(sf::Vector2f(fgWidth, barHeight));
                fg.setPosition(barX, barY);
                // 血量低时变黄，极低时变红
                sf::Color fgColor;
                if (hpRatio > 0.5f) {
                    fgColor = sf::Color(220, 80, 80, 255);
                } else if (hpRatio > 0.25f) {
                    fgColor = sf::Color(220, 180, 50, 255);
                } else {
                    fgColor = sf::Color(220, 50, 50, 255);
                }
                fg.setFillColor(fgColor);
                window_.draw(fg);
            }
        }
    }
}

// ============================================================================
// renderChampionHealthBars —— 渲染精英强化怪头上小血条（世界空间）
// ----------------------------------------------------------------------------
// 遍历所有 isChampion 标记的活跃敌人，在其头顶绘制小血条
// 血条尺寸比 Boss 屏幕顶部血条小，跟随敌人移动
// 视觉规范：背景半透明黑色，前景金色（与 Boss 红色区分），高度 4px
// 第二十一轮扩展：词缀敌人（EnemyAffix::affixMask != 0）也渲染血条 + 词缀名
// ============================================================================
void Game::renderChampionHealthBars() {
    if (!dungeonInitialized_) return;

    registry_.ForEach<Transform, EnemyComponent>([&](EntityId id) {
        EnemyComponent* enemy = registry_.GetComponent<EnemyComponent>(id);
        if (!enemy || !enemy->active) return;

        // 第二十一轮：词缀敌人也显示血条（不仅是 Champion）
        const EnemyAffix* affix = registry_.GetComponent<EnemyAffix>(id);
        const bool hasAffix = (affix && affix->affixMask != 0u);
        if (!enemy->isChampion && !hasAffix) return;

        Transform* t = registry_.GetComponent<Transform>(id);
        Health* health = registry_.GetComponent<Health>(id);
        if (!t || !health) return;
        // 已死亡的不渲染（Health.current <= 0）
        if (health->current <= 0.f) return;

        // 计算血量比例
        float hpRatio = (health->max > 0.f) ? health->current / health->max : 0.f;
        if (hpRatio < 0.f) hpRatio = 0.f;
        if (hpRatio > 1.f) hpRatio = 1.f;

        // 血条尺寸（小血条，比 Boss 屏幕顶部血条小）
        // 宽度 36px，高度 4px，位于敌人头顶上方 22px
        constexpr float kBarWidth = 36.f;
        constexpr float kBarHeight = 4.f;
        constexpr float kBarOffsetY = 22.f;

        float barX = t->position.x - kBarWidth * 0.5f;
        float barY = t->position.y - kBarOffsetY;

        // 背景（半透明深色）
        sf::RectangleShape bg(sf::Vector2f(kBarWidth, kBarHeight));
        bg.setPosition(barX, barY);
        bg.setFillColor(sf::Color(20, 20, 20, 200));
        bg.setOutlineColor(sf::Color(0, 0, 0, 220));
        bg.setOutlineThickness(1.f);
        window_.draw(bg);

        // 前景血量：Champion 用金色，纯 Elite 词缀怪用紫色（区分两种精英）
        float fgWidth = kBarWidth * hpRatio;
        if (fgWidth > 0.f) {
            sf::RectangleShape fg(sf::Vector2f(fgWidth, kBarHeight));
            fg.setPosition(barX, barY);
            if (enemy->isChampion) {
                fg.setFillColor(sf::Color(255, 215, 80, 240));  // 金色
            } else {
                fg.setFillColor(sf::Color(180, 80, 255, 240));  // 紫色（词缀精英）
            }
            window_.draw(fg);
        }

        // 精英标识小三角形（左侧，标识"精英"）
        sf::ConvexShape mark;
        mark.setPointCount(3);
        mark.setPoint(0, sf::Vector2f(barX - 6.f, barY - 1.f));
        mark.setPoint(1, sf::Vector2f(barX - 1.f, barY + kBarHeight * 0.5f));
        mark.setPoint(2, sf::Vector2f(barX - 6.f, barY + kBarHeight + 1.f));
        mark.setFillColor(enemy->isChampion ? sf::Color(255, 215, 80, 240)
                                            : sf::Color(180, 80, 255, 240));
        window_.draw(mark);

        // ---- 第二十一轮新增：词缀敌人头顶显示词缀名（中文）----
        // 让玩家知道这个精英带了哪些词缀，决定是否优先击杀
        // 词缀名格式："厚血+狂暴"（多词缀用 + 连接）
        if (hasAffix) {
            std::string affixName;
            if (HasEliteAffix(affix->affixMask, EliteAffix::HpBoost))      affixName += "厚血";
            if (HasEliteAffix(affix->affixMask, EliteAffix::DamageBoost))  affixName += (affixName.empty() ? "" : "+") + std::string("狂暴");
            if (HasEliteAffix(affix->affixMask, EliteAffix::SpeedBoost))   affixName += (affixName.empty() ? "" : "+") + std::string("迅捷");
            if (HasEliteAffix(affix->affixMask, EliteAffix::Regenerating)) affixName += (affixName.empty() ? "" : "+") + std::string("回血");

            if (!affixName.empty()) {
                sf::Text affixLabel;
                affixLabel.setFont(resources_.GetDefaultFont());
                affixLabel.setString(utf8ToSfString(affixName));
                affixLabel.setCharacterSize(10);
                affixLabel.setFillColor(sf::Color(220, 180, 255));
                affixLabel.setOutlineColor(sf::Color(0, 0, 0, 220));
                affixLabel.setOutlineThickness(1.f);
                sf::FloatRect ab = affixLabel.getLocalBounds();
                affixLabel.setOrigin(ab.width * 0.5f, ab.height * 0.5f);
                affixLabel.setPosition(t->position.x, barY - 8.f);
                window_.draw(affixLabel);
            }
        }
    });
}

// ============================================================================
// renderBossHealthBar —— 渲染 BOSS 血条（屏幕顶部中央）
// ----------------------------------------------------------------------------
// 当 BOSS 存活时，在屏幕顶部中央显示半透明血条。
// ============================================================================
void Game::renderBossHealthBar() {
    if (!bossActive_ || bossEntityId_ == kInvalidEntity) return;

    Health* bossHealth = registry_.GetComponent<Health>(bossEntityId_);
    if (!bossHealth) return;

    // BOSS 已死亡，清除标志
    if (bossHealth->current <= 0.f) {
        bossActive_ = false;
        bossEntityId_ = kInvalidEntity;
        return;
    }

    // 血条尺寸（屏幕顶部中央，使用固定 1280 逻辑宽度居中）
    float barWidth = 400.f;
    float barHeight = 20.f;
    float barX = (1280.f - barWidth) * 0.5f;
    float barY = 20.f;

    // 计算血量比例
    float hpRatio = bossHealth->current / bossHealth->max;
    if (hpRatio < 0.f) hpRatio = 0.f;
    if (hpRatio > 1.f) hpRatio = 1.f;

    // 背景（半透明深色）
    sf::RectangleShape bg(sf::Vector2f(barWidth, barHeight));
    bg.setPosition(barX, barY);
    bg.setFillColor(sf::Color(20, 20, 30, 180));
    bg.setOutlineColor(sf::Color(200, 50, 50, 200));
    bg.setOutlineThickness(2.f);
    window_.draw(bg);

    // 前景（红色血量）
    float fgWidth = barWidth * hpRatio;
    if (fgWidth > 0.f) {
        sf::RectangleShape fg(sf::Vector2f(fgWidth, barHeight));
        fg.setPosition(barX, barY);
        fg.setFillColor(sf::Color(180, 30, 30, 220));
        window_.draw(fg);
    }

    // BOSS 名称
    sf::Text bossName;
    bossName.setFont(resources_.GetDefaultFont());
    bossName.setString(U8("首领"));
    bossName.setCharacterSize(14);
    bossName.setFillColor(sf::Color::White);
    bossName.setPosition(barX + barWidth * 0.5f - 20.f, barY + 2.f);
    window_.draw(bossName);
}

// ============================================================================
// renderTutorial —— 渲染按键教程覆盖层
// ----------------------------------------------------------------------------
// 半透明遮罩 + 居中标题 + 按键说明网格
// 任意键/鼠标点击后关闭
// ============================================================================
void Game::renderTutorial() {
    const sf::Font& font = resources_.GetDefaultFont();
    // 使用固定 1280x720 逻辑分辨率（与当前 View 一致），不随窗口物理尺寸变化
    float screenW = 1280.f;
    float screenH = 720.f;

    // 半透明遮罩
    sf::RectangleShape overlay(sf::Vector2f(screenW, screenH));
    overlay.setFillColor(sf::Color(0, 0, 0, 180));
    window_.draw(overlay);

    // 背景卡片
    float cardW = 720.f;
    float cardH = 500.f;
    float cardX = (screenW - cardW) * 0.5f;
    float cardY = (screenH - cardH) * 0.5f;
    sf::RectangleShape card(sf::Vector2f(cardW, cardH));
    card.setPosition(cardX, cardY);
    card.setFillColor(sf::Color(25, 25, 35, 230));
    card.setOutlineColor(sf::Color(120, 100, 60));
    card.setOutlineThickness(3.f);
    window_.draw(card);

    // 标题：首次显示"操作指南"，H 键重新打开时显示"帮助手册"
    sf::Text title;
    title.setFont(font);
    title.setString(tutorialShown_ ? U8("帮助手册") : U8("操作指南"));
    title.setCharacterSize(40);
    title.setFillColor(sf::Color(255, 220, 100));
    title.setStyle(sf::Text::Bold);
    title.setOutlineColor(sf::Color(80, 40, 0));
    title.setOutlineThickness(2.f);
    sf::FloatRect tb = title.getLocalBounds();
    title.setPosition((screenW - tb.width) * 0.5f, cardY + 30.f);
    window_.draw(title);

    // 按键说明：键位 + 说明
    struct TutorialItem {
        const char* key;
        const char* desc;
    };
    TutorialItem items[] = {
        { "W A S D",        "移动" },
        { "鼠标左键",       "普通攻击" },
        { "鼠标右键",       "闪避（带无敌帧）" },
        { "空格键",         "AOE 爆炸（清怪）" },
        { "E 键",           "交互 / 商人交易" },
        { "G 键",           "打开 / 关闭背包" },
        { "数字键 1-4",     "释放技能" },
        { "P / ESC",        "暂停游戏" },
        { "J 键",           "升级选择界面" },
        { "Q 键",           "切换任务面板" },
        { "Tab 键",         "切换成就面板" },
        { "R 键",           "切换圣物面板" },
        { "H 键",           "打开帮助手册" },
        { "Enter 键",       "开始下一波" },
    };
    constexpr int kItemCount = sizeof(items) / sizeof(items[0]);
    constexpr int kCol1Count = 7; // 左列 7 项，右列 6 项

    float rowH = 38.f;
    float col1X = cardX + 40.f;
    float col2X = cardX + 390.f;
    float startY = cardY + 100.f;
    float keyBgW = 130.f;

    // 左列：items[0..kCol1Count-1]
    for (int i = 0; i < kCol1Count; ++i) {
        float y = startY + static_cast<float>(i) * rowH;
        const auto& item = items[i];

        sf::RectangleShape keyBg(sf::Vector2f(keyBgW, 32.f));
        keyBg.setPosition(col1X, y);
        keyBg.setFillColor(sf::Color(60, 50, 30, 220));
        keyBg.setOutlineColor(sf::Color(255, 220, 100));
        keyBg.setOutlineThickness(1.f);
        window_.draw(keyBg);

        sf::Text keyText;
        keyText.setFont(font);
        keyText.setString(utf8ToSfString(item.key));
        keyText.setCharacterSize(15);
        keyText.setFillColor(sf::Color(255, 220, 100));
        keyText.setStyle(sf::Text::Bold);
        sf::FloatRect kb = keyText.getLocalBounds();
        keyText.setPosition(col1X + (keyBgW - kb.width) * 0.5f,
                            y + (32.f - kb.height) * 0.5f - 2.f);
        window_.draw(keyText);

        sf::Text descText;
        descText.setFont(font);
        descText.setString(utf8ToSfString(item.desc));
        descText.setCharacterSize(16);
        descText.setFillColor(sf::Color(220, 220, 220));
        descText.setPosition(col1X + keyBgW + 12.f, y + 5.f);
        window_.draw(descText);
    }

    // 右列：items[kCol1Count..kItemCount-1]
    for (int i = kCol1Count; i < kItemCount; ++i) {
        float y = startY + static_cast<float>(i - kCol1Count) * rowH;
        const auto& item = items[i];

        sf::RectangleShape keyBg(sf::Vector2f(keyBgW, 32.f));
        keyBg.setPosition(col2X, y);
        keyBg.setFillColor(sf::Color(60, 50, 30, 220));
        keyBg.setOutlineColor(sf::Color(255, 220, 100));
        keyBg.setOutlineThickness(1.f);
        window_.draw(keyBg);

        sf::Text keyText;
        keyText.setFont(font);
        keyText.setString(utf8ToSfString(item.key));
        keyText.setCharacterSize(15);
        keyText.setFillColor(sf::Color(255, 220, 100));
        keyText.setStyle(sf::Text::Bold);
        sf::FloatRect kb = keyText.getLocalBounds();
        keyText.setPosition(col2X + (keyBgW - kb.width) * 0.5f,
                            y + (32.f - kb.height) * 0.5f - 2.f);
        window_.draw(keyText);

        sf::Text descText;
        descText.setFont(font);
        descText.setString(utf8ToSfString(item.desc));
        descText.setCharacterSize(16);
        descText.setFillColor(sf::Color(220, 220, 220));
        descText.setPosition(col2X + keyBgW + 12.f, y + 5.f);
        window_.draw(descText);
    }

    // 底部提示
    sf::Text closeHint;
    closeHint.setFont(font);
    closeHint.setString(U8("按任意键或点击鼠标关闭  |  游戏中按 H 重新打开"));
    closeHint.setCharacterSize(16);
    closeHint.setFillColor(sf::Color(180, 255, 180));
    closeHint.setStyle(sf::Text::Italic);
    sf::FloatRect cb = closeHint.getLocalBounds();
    closeHint.setPosition((screenW - cb.width) * 0.5f, cardY + cardH - 50.f);
    window_.draw(closeHint);
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

// ============================================================================
// 设置菜单：应用分辨率变化（音量已在点击时实时应用）
// ============================================================================
void Game::applySettings() {
    // 应用分辨率（若与当前窗口不同则重建窗口）
    int targetW = settingsMenu_.GetResW();
    int targetH = settingsMenu_.GetResH();
    sf::Vector2u currentSize = window_.getSize();
    if (static_cast<int>(currentSize.x) != targetW ||
        static_cast<int>(currentSize.y) != targetH) {
        window_.create(sf::VideoMode(targetW, targetH),
                       "CrazyUnder - 2.5D Pixel Roguelike",
                       sf::Style::Close | sf::Style::Titlebar);
        window_.setVerticalSyncEnabled(true);
        // 更新摄像机物理尺寸，保证鼠标世界坐标转换正确
        camera_.SetWindowPhysicalSize(sf::Vector2f(static_cast<float>(targetW),
                                                    static_cast<float>(targetH)));
        // 同步到持久化 settings_
        settings_.SetResolution(targetW, targetH);
        LOG_INFO("窗口分辨率已切换为 %dx%d", targetW, targetH);
    }
}

// ============================================================================
// 设置菜单：处理按钮点击（1-8）
// ----------------------------------------------------------------------------
// 1=BGM- 2=BGM+ 3=SFX- 4=SFX+ 5=分辨率上一个 6=分辨率下一个 7=应用 8=返回
// 音量调节实时生效（立即应用到 AudioManager），分辨率在"应用"时重建窗口
// ============================================================================
void Game::handleSettingsMenuClick(int action) {
    auto clampVol = [](float v) { return (v < 0.f) ? 0.f : (v > 100.f) ? 100.f : v; };

    switch (action) {
        case 1: { // BGM-
            float v = clampVol(settingsMenu_.GetBGMVolume() - 10.f);
            settingsMenu_.SetBGMVolume(v);
            settings_.SetBGMVolume(v);
            AudioManager::Instance().SetBGMVolume(v);
            break;
        }
        case 2: { // BGM+
            float v = clampVol(settingsMenu_.GetBGMVolume() + 10.f);
            settingsMenu_.SetBGMVolume(v);
            settings_.SetBGMVolume(v);
            AudioManager::Instance().SetBGMVolume(v);
            break;
        }
        case 3: { // SFX-
            float v = clampVol(settingsMenu_.GetSFXVolume() - 10.f);
            settingsMenu_.SetSFXVolume(v);
            settings_.SetSFXVolume(v);
            AudioManager::Instance().SetSFXVolume(v);
            AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip); // 试听
            break;
        }
        case 4: { // SFX+
            float v = clampVol(settingsMenu_.GetSFXVolume() + 10.f);
            settingsMenu_.SetSFXVolume(v);
            settings_.SetSFXVolume(v);
            AudioManager::Instance().SetSFXVolume(v);
            AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip); // 试听
            break;
        }
        case 5: { // 分辨率上一个（循环）
            int idx = settingsMenu_.GetCurrentResolutionIndex();
            idx = (idx - 1 + SettingsMenu::kResolutionCount) % SettingsMenu::kResolutionCount;
            settingsMenu_.SetResolution(SettingsMenu::kResolutions[idx].w,
                                        SettingsMenu::kResolutions[idx].h);
            break;
        }
        case 6: { // 分辨率下一个（循环）
            int idx = settingsMenu_.GetCurrentResolutionIndex();
            idx = (idx + 1) % SettingsMenu::kResolutionCount;
            settingsMenu_.SetResolution(SettingsMenu::kResolutions[idx].w,
                                        SettingsMenu::kResolutions[idx].h);
            break;
        }
        case 7: { // 应用（分辨率变化重建窗口 + 保存到文件）
            applySettings();
            settings_.Save();
            LOG_INFO("设置已应用并保存: BGM=%.0f SFX=%.0f %dx%d",
                     settings_.GetBGMVolume(), settings_.GetSFXVolume(),
                     settings_.GetWidth(), settings_.GetHeight());
            break;
        }
        case 8: { // 返回（关闭设置菜单）
            settingsMenuVisible_ = false;
            settingsMenu_.SetVisible(false);
            break;
        }
        default: break;
    }
}

// ============================================================================
// 第二十四轮新增：灵魂之井面板点击处理
// ----------------------------------------------------------------------------
// action: 1-6=购买对应强化（Vitality/Wisdom/Fortune/Strength/Swiftness/Aegis）
//         7=返回（关闭面板）
// 购买成功后立即刷新面板数据显示，让玩家看到碎片扣除和等级提升
// ============================================================================
void Game::handleSoulWellMenuClick(int action) {
    if (action == 7) {
        // 返回按钮
        soulWellMenuVisible_ = false;
        soulWellMenu_.SetVisible(false);
        LOG_INFO("灵魂之井面板已关闭（按钮）");
        return;
    }

    if (action >= 1 && action <= 6) {
        SoulUpgradeType type = static_cast<SoulUpgradeType>(action - 1);
        bool ok = soulMemory_.PurchaseUpgrade(type);
        if (ok) {
            // 购买成功，立即刷新面板数据
            soulWellMenu_.SetSoulMemoryData(soulMemory_);
            // 播放装备音效作为正反馈（复用现有音效，无新增资源）
            AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
            LOG_INFO("灵魂之井: 购买 %s 成功",
                     SoulMemorySystem::GetUpgradeName(type));
        } else {
            // 购买失败（碎片不足或已满级），播放错误音效
            AudioManager::Instance().PlaySFX(AudioManager::kSFXHit);
        }
    }
}

// ============================================================================
// 存档系统实现
// ============================================================================

SaveData Game::buildSaveData() {
    SaveData data;
    data.level = currentLevel_;
    data.kills = totalKillCount_;
    data.survivalTime = survivalTime_;

    // 玩家状态
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    Health* hp = registry_.GetComponent<Health>(playerId_);
    if (pc) {
        data.coins = pc->stats.coins;
        data.skillSlots = pc->skillSlots;
        data.skillBackpack = pc->skillBackpack;
    }
    if (hp) {
        data.playerHp = hp->current;
    }

    // 升级系统
    data.playerLevel = upgradeSystem_.GetLevel();
    data.exp = upgradeSystem_.GetExp();
    data.expToNext = upgradeSystem_.GetExpToNext();
    data.skillPoints = upgradeSystem_.GetSkillPoints();
    for (int i = 0; i < static_cast<int>(UpgradeType::Count); ++i) {
        data.upgradeLevels[i] = upgradeSystem_.GetUpgradeLevel(static_cast<UpgradeType>(i));
    }

    // 装备系统
    data.equipped = inventorySystem_.GetEquippedItems();
    data.backpack = inventorySystem_.GetBackpackItems();

    // 任务系统状态（5 个任务，按数组索引对应任务 ID 1-5）
    {
        auto quests = questSystem_.Serialize();
        for (size_t i = 0; i < quests.size() && i < data.questStates.size(); ++i) {
            data.questStates[i].state = static_cast<uint8_t>(quests[i].state);
            data.questStates[i].currentProgress = quests[i].currentProgress;
            data.questStates[i].timeAccumulator = quests[i].timeAccumulator;
        }
    }

    // 圣物系统状态（第十五轮新增，6 个圣物槽位）
    data.relicIds = relicSystem_.Serialize();

    // 地牢变异系统状态（第十七轮新增，2 个修饰符槽位）
    data.floorModifierIds = floorModifiers_.Serialize();

    // 时间戳
    data.timestamp = static_cast<int64_t>(std::time(nullptr));
    return data;
}

void Game::applySaveData(const SaveData& data) {
    // 设置当前层数
    currentLevel_ = data.level;

    // 重新生成场景（不保留进度，会重置 inventory/upgrade/totalKillCount）
    setupPlayingScene(false);

    // 恢复升级系统
    upgradeSystem_.LoadFromData(data.playerLevel, data.exp, data.expToNext,
                                 data.skillPoints, data.upgradeLevels);

    // 恢复装备系统
    inventorySystem_.LoadFromData(data.equipped, data.backpack);

    // 恢复任务系统状态（setupPlayingScene 已 Initialize 注册任务定义，此处覆盖运行时进度）
    {
        std::vector<QuestInstance> questData;
        questData.reserve(data.questStates.size());
        for (size_t i = 0; i < data.questStates.size(); ++i) {
            QuestInstance qi;
            qi.id = static_cast<int>(i + 1); // 任务 ID 从 1 开始
            qi.state = static_cast<QuestState>(data.questStates[i].state);
            qi.currentProgress = data.questStates[i].currentProgress;
            qi.timeAccumulator = data.questStates[i].timeAccumulator;
            // targetProgress 由 Deserialize 内部根据任务定义重填
            questData.push_back(std::move(qi));
        }
        questSystem_.Deserialize(questData);
        int claimedCount = 0, activeCount = 0;
        for (const auto& q : questSystem_.GetAllQuests()) {
            if (q.state == QuestState::Claimed) ++claimedCount;
            else if (q.state == QuestState::Active) ++activeCount;
        }
        LOG_INFO("读档任务恢复: 已领取=%d 进行中=%d", claimedCount, activeCount);
    }

    // 恢复圣物系统状态（第十五轮新增）
    relicSystem_.Deserialize(data.relicIds);

    // 第十七轮新增：恢复地牢变异系统状态
    // setupPlayingScene 中已清空（preserveProgress=false），此处反序列化覆盖
    floorModifiers_.Deserialize(data.floorModifierIds);
    applyFloorModifiersToSubsystems(); // 推送到 EnemySpawner/LootSystem/MerchantSystem
    if (floorModifiers_.GetActiveCount() > 0) {
        // 读档后短暂显示当前层的变异状态（2 秒，避免长时间打扰）
        modifierBannerTimer_ = std::min(kModifierBannerDuration, 2.5f);
        LOG_INFO("读档恢复变异: %s", floorModifiers_.GetActiveSummary().c_str());
    }

    // 恢复玩家状态
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    Health* hp = registry_.GetComponent<Health>(playerId_);
    if (pc) {
        pc->stats.coins = data.coins;
        pc->skillSlots = data.skillSlots;
        pc->skillBackpack = data.skillBackpack;
        // 清零 buff 计时器（瞬时战斗状态不存档）
        pc->berserkTimer = 0.f;
        pc->leechStrikeActive = 0.f;
        pc->gravityWellTimer = 0.f;
        pc->spikeGroundTimer = 0.f;
        // 清零技能冷却（读档后冷却重置，避免读档后无法立即战斗）
        for (auto& sk : pc->skillSlots) {
            sk.cooldownRemain = 0.f;
        }
        // 诊断日志：确认技能槽已恢复
        int slotCount = 0;
        for (const auto& sk : pc->skillSlots) {
            if (sk.type != SkillType::Count) ++slotCount;
        }
        int bpCount = 0;
        for (const auto& st : pc->skillBackpack) {
            if (st != SkillType::Count) ++bpCount;
        }
        LOG_INFO("读档技能恢复: 技能槽=%d/4 技能背包=%d/5", slotCount, bpCount);
    }
    if (hp) {
        hp->current = data.playerHp;
    }

    // 恢复统计（setupPlayingScene 会重置 totalKillCount_，需在此之后恢复）
    totalKillCount_ = data.kills;
    survivalTime_ = data.survivalTime;

    // 重新计算玩家属性（应用升级和装备词缀到新玩家实体）
    recomputePlayerStats();

    // 重置 BOSS 状态
    bossActive_ = false;
    bossEntityId_ = kInvalidEntity;
    bossRoomEntered_ = false;
    bossNoDamageTimer_ = 0.f;
    bossDefeatedHintTimer_ = 0.f;

    LOG_INFO("存档已应用: 层数=%d 击杀=%d HP=%.1f 金币=%d",
             data.level, data.kills, data.playerHp, data.coins);
}

void Game::refreshSaveSlotInfo() {
    auto allInfo = saveSystem_.GetAllSlotInfo();
    saveLoadMenu_.SetSlotInfo(allInfo);
}

void Game::showSaveLoadMenu(SaveLoadMenu::Mode mode) {
    refreshSaveSlotInfo();
    saveLoadMenu_.SetMode(mode);
    saveLoadMenu_.SetVisible(true);
    saveLoadMenuVisible_ = true;
}

void Game::autoSaveCurrent() {
    if (!SaveSystem::IsValidSlot(currentSlot_)) {
        LOG_WARN("当前槽位无效: %d，无法保存", currentSlot_);
        return;
    }
    SaveData data = buildSaveData();
    if (saveSystem_.SaveToSlot(currentSlot_, data)) {
        LOG_INFO("已自动保存到槽位 %d", currentSlot_);
    } else {
        LOG_WARN("自动保存到槽位 %d 失败", currentSlot_);
    }
}

void Game::handleSaveLoadMenuClick(int action) {
    if (action == 0) return;

    // 4=返回
    if (action == 4) {
        saveLoadMenuVisible_ = false;
        saveLoadMenu_.SetVisible(false);
        return;
    }

    // 5-7=删除槽位
    if (action >= 5 && action <= 7) {
        int slot = action - 4; // 5->1, 6->2, 7->3
        if (saveSystem_.DeleteSlot(slot)) {
            LOG_INFO("已删除槽位 %d 的存档", slot);
            refreshSaveSlotInfo();
        }
        return;
    }

    // 1-3=选择槽位
    if (action >= 1 && action <= 3) {
        int slot = action;
        SaveLoadMenu::Mode mode = saveLoadMenu_.GetMode();

        if (mode == SaveLoadMenu::Mode::Load) {
            // 读档
            SaveData data;
            if (saveSystem_.LoadFromSlot(slot, data)) {
                currentSlot_ = slot;
                // 重置所有 UI 标志，避免二次读档后残留
                resetAllUIFlags();
                mainMenu_.SetVisible(false);
                state_ = GameState::Playing;
                applySaveData(data);
                // 首次进入游戏时显示按键教程
                if (!tutorialShown_) {
                    tutorialVisible_ = true;
                    tutorialShown_ = true;
                }
                AudioManager::Instance().PlayBGM(AudioManager::kBGMDungeon);
                LOG_INFO("已从槽位 %d 读档开始游戏", slot);
            } else {
                LOG_WARN("槽位 %d 读档失败", slot);
            }
        } else { // SaveNew 模式
            // 新游戏：设置当前槽位，开始新游戏
            currentSlot_ = slot;
            // 重置游戏状态
            currentLevel_ = 1;
            totalKillCount_ = 0;
            survivalTime_ = 0.f;
            bossKillCountThisRun_ = 0;
            lastKillerName_ = "";
            totalDamageDealt_ = 0.f;
            comboAtDeath_ = 0;
            // 重置所有 UI 标志
            resetAllUIFlags();
            mainMenu_.SetVisible(false);
            state_ = GameState::Playing;
            setupPlayingScene(false);
            // 首次进入游戏时显示按键教程
            if (!tutorialShown_) {
                tutorialVisible_ = true;
                tutorialShown_ = true;
            }
            AudioManager::Instance().PlayBGM(AudioManager::kBGMDungeon);
            // 立即保存（记录新游戏起始状态）
            autoSaveCurrent();
            LOG_INFO("新游戏已开始，存档到槽位 %d", slot);
        }
    }
}

// ============================================================================
// 调试面板辅助方法实现
// ============================================================================

void Game::teleportToRoom(RoomType type) {
    if (!dungeonInitialized_) return;
    
    // 查找指定类型的房间
    int targetRoomIndex = -1;
    for (size_t i = 0; i < dungeon_.rooms.size(); ++i) {
        if (dungeon_.rooms[i].type == type) {
            targetRoomIndex = static_cast<int>(i);
            break;
        }
    }
    
    if (targetRoomIndex < 0) {
        LOG_WARN("未找到类型为 %d 的房间", static_cast<int>(type));
        return;
    }
    
    // 获取房间中心位置（tile 坐标转世界坐标）
    const Room& room = dungeon_.rooms[targetRoomIndex];
    sf::Vector2f targetPos = dungeon_.TileCenterToWorld(room.center);
    
    // 传送玩家
    Transform* playerTransform = registry_.GetComponent<Transform>(playerId_);
    if (playerTransform) {
        playerTransform->position = targetPos;
        camera_.SetPosition(targetPos);
        LOG_INFO("已传送到房间 %d (类型=%d)", targetRoomIndex, static_cast<int>(type));
    }
}

void Game::killAllEnemies() {
    if (!dungeonInitialized_) return;
    
    int killCount = 0;
    registry_.ForEach<EnemyComponent>([&](EntityId id) {
        EnemyComponent* enemy = registry_.GetComponent<EnemyComponent>(id);
        Health* health = registry_.GetComponent<Health>(id);
        if (enemy && health && enemy->active) {
            health->current = 0;
            ++killCount;
        }
    });
    LOG_INFO("已秒杀 %d 个敌人", killCount);
}

void Game::addCoins(int amount) {
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (pc) {
        pc->stats.coins += amount;
        LOG_INFO("已增加 %d 金币，当前金币: %d", amount, pc->stats.coins);
    }
}

void Game::addExperience(int amount) {
    upgradeSystem_.AddExp(amount);
    LOG_INFO("已增加 %d 经验", amount);
}

void Game::clearScreen() {
    if (!dungeonInitialized_) return;
    
    // 清除所有敌人
    enemySpawner_.ClearAllEnemies();
    
    // 清除所有弹幕
    projectileSystem_.ClearAll();
    
    LOG_INFO("已清屏（移除所有敌人和弹幕）");
}

void Game::handleEvents() {
    sf::Event event;
    while (window_.pollEvent(event)) {
        input_.HandleEvent(event);

        // 按键教程显示期间，任意按键/鼠标点击关闭教程（但不处理其他游戏输入）
        // H 键除外：H 键专门用于切换帮助手册
        if (tutorialVisible_) {
            if (event.type == sf::Event::KeyPressed && event.key.code != sf::Keyboard::H) {
                tutorialVisible_ = false;
                LOG_INFO("按键教程已关闭");
            } else if (event.type == sf::Event::MouseButtonPressed) {
                tutorialVisible_ = false;
                LOG_INFO("按键教程已关闭");
            }
        }

        if (event.type == sf::Event::Closed) {
            window_.close();
        } else if (event.type == sf::Event::MouseWheelScrolled) {
            // 第二十二轮新增：任务面板可见时转发鼠标滚轮事件（垂直滚动任务列表）
            if (state_ == GameState::Playing && questMenuVisible_) {
                questMenu_.OnMouseWheel(event.mouseWheelScroll.delta);
            }
        } else if (event.type == sf::Event::KeyPressed) {
            auto key = event.key.code;
            // Phase 8: ESC 键状态切换
            if (key == sf::Keyboard::Escape) {
                // 存档菜单打开时，ESC 优先关闭存档菜单
                if (saveLoadMenuVisible_) {
                    saveLoadMenuVisible_ = false;
                    saveLoadMenu_.SetVisible(false);
                    LOG_INFO("存档菜单已关闭（ESC）");
                }
                // 第二十四轮新增：灵魂之井面板打开时，ESC 优先关闭
                else if (soulWellMenuVisible_) {
                    soulWellMenuVisible_ = false;
                    soulWellMenu_.SetVisible(false);
                    LOG_INFO("灵魂之井面板已关闭（ESC）");
                }
                // 设置菜单打开时，ESC 优先关闭设置菜单
                else if (settingsMenuVisible_) {
                    settingsMenuVisible_ = false;
                    settingsMenu_.SetVisible(false);
                    LOG_INFO("设置菜单已关闭（ESC）");
                }
                // 若商人/背包菜单打开，ESC 先关闭菜单而非暂停
                else if (state_ == GameState::Playing &&
                    (merchantMenuVisible_ || inventoryMenuVisible_ ||
                     questMenuVisible_ || achievementMenuVisible_ ||
                     relicPanelVisible_)) {
                    if (merchantMenuVisible_) {
                        merchantMenuVisible_ = false;
                        merchantMenu_.SetVisible(false);
                        LOG_INFO("商人菜单已关闭（ESC）");
                    }
                    if (inventoryMenuVisible_) {
                        inventoryMenuVisible_ = false;
                        inventoryMenu_.SetVisible(false);
                        inventoryMenu_.CloseContextMenu();
                        LOG_INFO("背包菜单已关闭（ESC）");
                    }
                    if (questMenuVisible_) {
                        questMenuVisible_ = false;
                        questMenu_.SetVisible(false);
                        LOG_INFO("任务面板已关闭（ESC）");
                    }
                    if (achievementMenuVisible_) {
                        achievementMenuVisible_ = false;
                        achievementMenu_.SetVisible(false);
                        LOG_INFO("成就面板已关闭（ESC）");
                    }
                    if (relicPanelVisible_) {
                        relicPanelVisible_ = false;
                        LOG_INFO("圣物面板已关闭（ESC）");
                    }
                } else {
                    switch (state_) {
                        case GameState::Menu:    window_.close(); break;
                        case GameState::Playing: ChangeState(GameState::Paused); break;
                        case GameState::Paused:  ChangeState(GameState::Playing); break;
                        case GameState::Dead:    ChangeState(GameState::Menu); break;
                        case GameState::Victory: ChangeState(GameState::Menu); break;
                    }
                }
            } else if (key == sf::Keyboard::Space) {
                switch (state_) {
                    case GameState::Menu:    ChangeState(GameState::Playing); break;
                    case GameState::Dead:    ChangeState(GameState::Playing); break;
                    case GameState::Victory: ChangeState(GameState::Menu);    break;
                    default: break;
                }
            } else if (key == sf::Keyboard::P) {
                if (state_ == GameState::Playing)      ChangeState(GameState::Paused);
                else if (state_ == GameState::Paused)  ChangeState(GameState::Playing);
            } else if (key == sf::Keyboard::X) {
                if (state_ == GameState::Playing) ChangeState(GameState::Dead);
            } else if (key == sf::Keyboard::V) {
                if (state_ == GameState::Playing) ChangeState(GameState::Victory);
            } else if (key == sf::Keyboard::F1) {
                debugMode_ = !debugMode_;
                LOG_INFO("调试模式: %s", debugMode_ ? "开启" : "关闭");
            } else if (key == sf::Keyboard::H) {
                // H 键切换帮助手册（可重复查看按键教程）
                if (state_ == GameState::Playing && !upgradeChoiceActive_ &&
                    !inventoryMenuVisible_ && !merchantMenuVisible_ &&
                    !questMenuVisible_ && !achievementMenuVisible_ &&
                    !relicPanelVisible_) {
                    tutorialVisible_ = !tutorialVisible_;
                    LOG_INFO("帮助手册: %s", tutorialVisible_ ? "打开" : "关闭");
                }
            } else if (key == sf::Keyboard::F5) {
                // F5 键切换调试面板
                if (state_ == GameState::Playing) {
                    debugPanelVisible_ = !debugPanelVisible_;
                    debugPanel_.SetVisible(debugPanelVisible_);
                    LOG_INFO("调试面板: %s", debugPanelVisible_ ? "开启" : "关闭");
                }
            } else if (key == sf::Keyboard::Enter) {
                if (state_ == GameState::Playing && !upgradeChoiceActive_ && !inventoryMenuVisible_) {
                    ++currentWaveNumber_;
                    enemySpawner_.StartWave(currentWaveNumber_);
                    LOG_INFO("开始波次 %d", currentWaveNumber_);
                }
            } else if (key == sf::Keyboard::K) {
                if (state_ == GameState::Playing) {
                    enemySpawner_.ClearAllEnemies();
                    LOG_INFO("已清除所有敌人");
                }
            } else if (key == sf::Keyboard::R) {
                // Shift+R 保留原"重新生成地牢"调试功能；纯 R 键切换圣物查看面板
                if (event.key.shift && state_ == GameState::Playing) {
                    LOG_INFO("重新生成地牢...");
                    setupPlayingScene();
                } else if (state_ == GameState::Playing && !upgradeChoiceActive_ &&
                           !inventoryMenuVisible_ && !merchantMenuVisible_ &&
                           !questMenuVisible_ && !achievementMenuVisible_) {
                    relicPanelVisible_ = !relicPanelVisible_;
                    LOG_INFO("圣物查看面板: %s", relicPanelVisible_ ? "打开" : "关闭");
                }
            } else if (key == sf::Keyboard::G) {
                // Phase 8: G 键切换背包菜单（大背包系统）/ 关闭商人菜单
                if (state_ == GameState::Playing && !upgradeChoiceActive_) {
                    if (merchantMenuVisible_) {
                        // G 键关闭商人菜单
                        merchantMenuVisible_ = false;
                        merchantMenu_.SetVisible(false);
                        LOG_INFO("商人菜单已关闭（G 键）");
                    } else {
                        inventoryMenuVisible_ = !inventoryMenuVisible_;
                        if (inventoryMenuVisible_) {
                            inventoryMenu_.SetInventory(inventorySystem_);
                            PlayerComponent* pcG = registry_.GetComponent<PlayerComponent>(playerId_);
                            if (pcG) inventoryMenu_.SetSkillData(*pcG);
                            inventoryMenu_.SetVisible(true);
                            relicPanelVisible_ = false; // 背包打开时关闭圣物面板
                            LOG_INFO("背包菜单已打开");
                        } else {
                            inventoryMenu_.SetVisible(false);
                            inventoryMenu_.CloseContextMenu();
                            LOG_INFO("背包菜单已关闭");
                        }
                    }
                }
            } else if (key == sf::Keyboard::J) {
                // J 键：主动开启升级选择界面（当有未使用的技能点时）
                if (state_ == GameState::Playing && !upgradeChoiceActive_ &&
                    !inventoryMenuVisible_ && !merchantMenuVisible_ &&
                    !questMenuVisible_ && !achievementMenuVisible_ &&
                    upgradeSystem_.GetSkillPoints() > 0) {
                    showUpgradeChoice();
                    LOG_INFO("J 键开启升级选择（剩余技能点=%d）", upgradeSystem_.GetSkillPoints());
                }
            } else if (key == sf::Keyboard::Q) {
                // Q 键：切换任务面板
                if (state_ == GameState::Playing && !upgradeChoiceActive_ &&
                    !inventoryMenuVisible_ && !merchantMenuVisible_) {
                    questMenuVisible_ = !questMenuVisible_;
                    if (questMenuVisible_) {
                        questMenu_.SetQuestData(questSystem_);
                        questMenu_.SetVisible(true);
                        achievementMenuVisible_ = false;
                        achievementMenu_.SetVisible(false);
                        relicPanelVisible_ = false; // 任务面板打开时关闭圣物面板
                        LOG_INFO("任务面板已打开");
                    } else {
                        questMenu_.SetVisible(false);
                        LOG_INFO("任务面板已关闭");
                    }
                }
            } else if (key == sf::Keyboard::Tab) {
                // Tab 键：切换成就面板
                if (state_ == GameState::Playing && !upgradeChoiceActive_ &&
                    !inventoryMenuVisible_ && !merchantMenuVisible_) {
                    achievementMenuVisible_ = !achievementMenuVisible_;
                    if (achievementMenuVisible_) {
                        achievementMenu_.SetAchievementData(achievementSystem_);
                        achievementMenu_.SetVisible(true);
                        questMenuVisible_ = false;
                        questMenu_.SetVisible(false);
                        relicPanelVisible_ = false; // 成就面板打开时关闭圣物面板
                        LOG_INFO("成就面板已打开");
                    } else {
                        achievementMenu_.SetVisible(false);
                        LOG_INFO("成就面板已关闭");
                    }
                }
            } else if (key == sf::Keyboard::E) {
                // E 键交互：开关门 + 开宝箱
                // 放在 handleEvents 中确保每次按键只触发一次（避免固定步长多次调用导致抖动）
                if (state_ == GameState::Playing && dungeonInitialized_) {
                    handleInteract();
                }
            } else if (key == sf::Keyboard::Num1 || key == sf::Keyboard::Num2 ||
                       key == sf::Keyboard::Num3) {
                // ---- 第十五轮：圣物选择优先（Boss 击败后弹出时）----
                if (state_ == GameState::Playing && relicChoiceActive_) {
                    int idx = relicMenu_.HandleKeyInput(static_cast<int>(key));
                    if (idx >= 0 && idx < static_cast<int>(currentRelicOptions_.size())) {
                        RelicType chosen = currentRelicOptions_[idx];
                        if (chosen != RelicType::None) {
                            relicSystem_.AddRelic(chosen);
                            recomputePlayerStats();
                            AudioManager::Instance().PlaySFX(AudioManager::kSFXLevelUp);
                            LOG_INFO("圣物已选择（按键）: %s，当前共 %d 个圣物",
                                     GetRelicName(chosen), relicSystem_.GetOwnedCount());
                            relicChoiceActive_ = false;
                            relicMenu_.SetVisible(false);
                        }
                    }
                }
                // Phase 8: 1/2/3 键选择升级（图形界面）
                else if (state_ == GameState::Playing && upgradeChoiceActive_) {
                    int idx = upgradeMenu_.HandleKeyInput(static_cast<int>(key));
                    if (idx >= 0 && idx < 3 &&
                        currentUpgradeOptions_[idx].type != UpgradeType::Count) {
                        UpgradeType chosenType = currentUpgradeOptions_[idx].type;
                        // 检查是否是技能升级
                        bool isSkill = (chosenType == UpgradeType::SkillGroundSlam ||
                                       chosenType == UpgradeType::SkillLeechStrike ||
                                       chosenType == UpgradeType::SkillBerserk ||
                                       chosenType == UpgradeType::SkillGravityWell ||
                                       chosenType == UpgradeType::SkillSpikeGround);
                        if (isSkill) {
                            PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
                            if (pc) {
                                SkillType skill = SkillType::Count;
                                switch (chosenType) {
                                    case UpgradeType::SkillGroundSlam:  skill = SkillType::GroundSlam; break;
                                    case UpgradeType::SkillLeechStrike: skill = SkillType::LeechStrike; break;
                                    case UpgradeType::SkillBerserk:     skill = SkillType::Berserk; break;
                                    case UpgradeType::SkillGravityWell: skill = SkillType::GravityWell; break;
                                    case UpgradeType::SkillSpikeGround: skill = SkillType::SpikeGround; break;
                                    default: break;
                                }
                                if (skill != SkillType::Count) {
                                    // 已拥有则升级，未拥有则添加
                                    if (PlayerHasSkill(*pc, skill)) {
                                        UpgradeSkillLevel(*pc, skill);
                                        LOG_INFO("技能升级: %s -> Lv.%d",
                                                 GetSkillName(skill), GetSkillLevel(*pc, skill));
                                    } else {
                                        AddSkillToBackpack(*pc, skill);
                                        LOG_INFO("获得技能: %s", GetSkillName(skill));
                                    }
                                    AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                                }
                            }
                        }
                        upgradeSystem_.ApplyUpgrade(chosenType);
                        recomputePlayerStats();

                        // 若仍有剩余技能点，重新滚动选项保持菜单打开；否则关闭
                        if (upgradeSystem_.GetSkillPoints() > 0) {
                            currentUpgradeOptions_ = upgradeSystem_.RollUpgrades();
                            upgradeMenu_.SetOptions(currentUpgradeOptions_);
                            upgradeUI_.Show(currentUpgradeOptions_);
                            LOG_INFO("仍有 %d 个技能点未使用，继续选择", upgradeSystem_.GetSkillPoints());
                        } else {
                            upgradeChoiceActive_ = false;
                            upgradeMenu_.SetVisible(false);
                            LOG_INFO("升级选择完成（按键），游戏继续");
                        }
                    }
                }
            }
        }
    }
}

void Game::Run() {
    time_.Reset();
    while (window_.isOpen()) {
        // 0. 帧开始：更新输入系统的双缓冲
        input_.NewFrame();

        // 1. 事件处理
        handleEvents();

        // Phase 8: 处理 UI 鼠标点击
        handleUIInput();

        // 2. 时间更新
        int steps = time_.Update();

        // 3. 固定步长更新
        auto it = states_.find(state_);
        if (it != states_.end()) {
            auto& cb = it->second;
            for (int i = 0; i < steps; ++i) {
                if (cb.update) cb.update(static_cast<float>(Time::FixedDeltaTime));
            }

            // Phase 8: 更新 UI
            updateUI(static_cast<float>(time_.GetDeltaTime()));

            // 4. 插值渲染
            // 渲染前统一设置固定 1280x720 逻辑分辨率 View，保证不同窗口尺寸下
            // UI 渲染坐标与鼠标点击检测坐标（mapPixelToCoords 1280x720）一致。
            // renderPlaying 内部会自行切换摄像机 View 渲染世界，此处设置对其无影响。
            window_.setView(sf::View(sf::FloatRect(0.f, 0.f, 1280.f, 720.f)));
            float alpha = static_cast<float>(time_.GetAlpha());
            if (cb.render) cb.render(alpha);
        }

        // 4.5 设置菜单覆盖层（任意状态都可显示，使用固定 1280x720 逻辑分辨率）
        window_.setView(sf::View(sf::FloatRect(0.f, 0.f, 1280.f, 720.f)));
        if (settingsMenuVisible_) {
            settingsMenu_.Render(window_);
        }

        // 4.6 存档菜单覆盖层（任意状态都可显示，使用固定 1280x720 逻辑分辨率）
        if (saveLoadMenuVisible_) {
            saveLoadMenu_.Render(window_);
        }

        // 4.7 第二十四轮新增：灵魂之井面板覆盖层（仅主菜单状态显示）
        if (soulWellMenuVisible_) {
            soulWellMenu_.Render(window_);
        }

        // FPS 叠加显示（使用固定 1280x720 逻辑分辨率）
        window_.setView(sf::View(sf::FloatRect(0.f, 0.f, 1280.f, 720.f)));
        fpsText_.setString("FPS: " + std::to_string(time_.GetFPS()));
        window_.draw(fpsText_);

        // 5. 交换缓冲
        window_.display();

        // 控制台每秒输出一次 FPS
        fpsLogTimer_ += time_.GetDeltaTime();
        if (fpsLogTimer_ >= 1.0) {
            LOG_INFO("FPS: %d  DrawCalls: %d  Entities: %zu",
                     time_.GetFPS(), renderer_.GetDrawCallCount(), registry_.GetEntityCount());
            fpsLogTimer_ = 0.0;
        }
    }
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
    s.manaRegen = 2.f * upgradeSystem_.GetUpgradeLevel(UpgradeType::ManaRegen);
    s.defense += 3.f * upgradeSystem_.GetUpgradeLevel(UpgradeType::DefenseUp);

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

// ============================================================================
// 第十七轮新增：applyFloorModifiersToSubsystems
// ----------------------------------------------------------------------------
// 将当前 floorModifiers_ 的乘法系数推送到 EnemySpawner/LootSystem/MerchantSystem
// 在 setupPlayingScene 与 applySaveData 中调用，确保所有子系统与本层 modifier 同步
// 默认 modifier 均为 1.0（无影响），仅在 RollForLevel / Deserialize 后才有非 1 值
// ============================================================================
void Game::applyFloorModifiersToSubsystems() {
    enemySpawner_.SetModifierEnemyHpMul(floorModifiers_.GetEnemyHpMul());
    enemySpawner_.SetModifierEnemyDamageMul(floorModifiers_.GetEnemyDamageMul());
    enemySpawner_.SetModifierEnemyMoveSpeedMul(floorModifiers_.GetEnemyMoveSpeedMul());
    enemySpawner_.SetModifierEnemyAttackSpeedMul(floorModifiers_.GetEnemyAttackSpeedMul());
    enemySpawner_.SetModifierSpawnIntervalMul(floorModifiers_.GetSpawnIntervalMul());

    lootSystem_.SetModifierItemDropChanceMul(floorModifiers_.GetItemDropChanceMul());

    MerchantSystem::SetModifierPriceMul(floorModifiers_.GetMerchantPriceMul());
}

// ============================================================================
// 第十七轮新增：renderFloorModifierBanner
// ----------------------------------------------------------------------------
// 进入新层时显示 5 秒淡入淡出 Banner，提示当前层的变异效果
// 视觉：屏幕中上方 720x80 半透明背景板 + 主色调边框 + 标题 + 描述
// 动画：前 0.4s 淡入，中间 4.2s 稳定，最后 0.4s 淡出
// ============================================================================
void Game::renderFloorModifierBanner() {
    if (modifierBannerTimer_ <= 0.f) return;
    if (floorModifiers_.GetActiveCount() == 0) return;

    // 计算 alpha：前 0.4s 淡入，中间稳定，最后 0.4s 淡出
    const float fadeIn = 0.4f;
    const float fadeOut = 0.4f;
    float elapsed = kModifierBannerDuration - modifierBannerTimer_;
    float alpha = 1.f;
    if (elapsed < fadeIn) {
        alpha = elapsed / fadeIn; // 0 → 1
    } else if (modifierBannerTimer_ < fadeOut) {
        alpha = modifierBannerTimer_ / fadeOut; // 1 → 0
    }
    if (alpha < 0.f) alpha = 0.f;
    if (alpha > 1.f) alpha = 1.f;

    // 主色调：取第一个激活修饰符的颜色（多个时混合简化为首个）
    auto mods = floorModifiers_.GetActiveModifiers();
    sf::Color mainColor(255, 255, 255);
    for (auto t : mods) {
        if (t != FloorModifierType::None) {
            const auto& d = GetFloorModifierData(t);
            mainColor = sf::Color(d.r, d.g, d.b);
            break;
        }
    }

    // 备份当前 view，切换到默认 UI view
    sf::View prevView = window_.getView();
    window_.setView(window_.getDefaultView());

    // 半透明背景板：720x100 居中，y=180
    const float bgW = 720.f, bgH = 100.f;
    const float bgX = (1280.f - bgW) * 0.5f;
    const float bgY = 180.f;
    sf::RectangleShape bg(sf::Vector2f(bgW, bgH));
    bg.setPosition(bgX, bgY);
    bg.setFillColor(sf::Color(15, 15, 25, static_cast<sf::Uint8>(220 * alpha)));
    bg.setOutlineColor(sf::Color(mainColor.r, mainColor.g, mainColor.b,
                                  static_cast<sf::Uint8>(255 * alpha)));
    bg.setOutlineThickness(2.f);
    window_.draw(bg);

    // 顶部装饰条
    sf::RectangleShape topBar(sf::Vector2f(bgW - 8.f, 4.f));
    topBar.setPosition(bgX + 4.f, bgY + 4.f);
    topBar.setFillColor(sf::Color(mainColor.r, mainColor.g, mainColor.b,
                                   static_cast<sf::Uint8>(255 * alpha)));
    window_.draw(topBar);

    // 标题："本层变异"
    sf::Text title;
    title.setFont(resources_.GetDefaultFont());
    title.setCharacterSize(20);
    title.setFillColor(sf::Color(255, 220, 100, static_cast<sf::Uint8>(255 * alpha)));
    title.setString(U8("本层变异"));
    title.setPosition(bgX + 16.f, bgY + 12.f);
    window_.draw(title);

    // 修饰符名（主色调）
    sf::Text name;
    name.setFont(resources_.GetDefaultFont());
    name.setCharacterSize(26);
    name.setFillColor(sf::Color(mainColor.r, mainColor.g, mainColor.b,
                                 static_cast<sf::Uint8>(255 * alpha)));
    name.setStyle(sf::Text::Bold);
    name.setString(utf8ToSfString(floorModifiers_.GetActiveSummary()));
    // 计算文本宽度以居中
    float nameW = name.getLocalBounds().width;
    name.setPosition(bgX + (bgW - nameW) * 0.5f, bgY + 36.f);
    window_.draw(name);

    // 描述：拼接所有激活修饰符的描述
    std::string desc;
    bool first = true;
    for (auto t : mods) {
        if (t == FloorModifierType::None) continue;
        if (!first) desc += "  |  ";
        desc += GetFloorModifierData(t).description;
        first = false;
    }
    sf::Text descText;
    descText.setFont(resources_.GetDefaultFont());
    descText.setCharacterSize(14);
    descText.setFillColor(sf::Color(220, 220, 220, static_cast<sf::Uint8>(220 * alpha)));
    descText.setString(utf8ToSfString(desc));
    float descW = descText.getLocalBounds().width;
    descText.setPosition(bgX + (bgW - descW) * 0.5f, bgY + 72.f);
    window_.draw(descText);

    window_.setView(prevView);
}

// ============================================================================
// 第十七轮新增：renderFloorModifierHUD
// ----------------------------------------------------------------------------
// HUD 持久指示器：屏幕左上角（FPS 下方）显示当前激活的变异名
// 始终显示（即使 Banner 已消失），便于玩家随时知晓本层规则
// ============================================================================
void Game::renderFloorModifierHUD() {
    if (floorModifiers_.GetActiveCount() == 0) return;

    sf::View prevView = window_.getView();
    window_.setView(window_.getDefaultView());

    // 标签 "变异：" + 修饰符名（主色调拼接）
    auto mods = floorModifiers_.GetActiveModifiers();
    sf::Text label;
    label.setFont(resources_.GetDefaultFont());
    label.setCharacterSize(14);
    label.setFillColor(sf::Color(180, 180, 200));
    label.setString(U8("变异："));
    label.setPosition(8.f, 96.f); // FPS 文本下方（FPS 通常在 y=80）
    window_.draw(label);

    float offsetX = label.getLocalBounds().width + 12.f;
    float posY = 96.f;
    bool first = true;
    for (auto t : mods) {
        if (t == FloorModifierType::None) continue;
        const auto& d = GetFloorModifierData(t);
        sf::Text modName;
        modName.setFont(resources_.GetDefaultFont());
        modName.setCharacterSize(14);
        modName.setFillColor(sf::Color(d.r, d.g, d.b));
        modName.setStyle(sf::Text::Bold);
        modName.setString(utf8ToSfString(d.name));
        if (!first) {
            offsetX += 10.f; // 多个修饰符间距
        }
        modName.setPosition(offsetX, posY);
        window_.draw(modName);
        offsetX += modName.getLocalBounds().width + 6.f;
        first = false;
    }

    window_.setView(prevView);
}

// ============================================================================
// applyCurse —— 施加诅咒效果（诅咒房进入时）
// ============================================================================
void Game::applyCurse(int roomIndex) {
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (!pc) return;
    pc->cursed = true;
    pc->cursedRoomIndex = roomIndex;
    recomputePlayerStats();
    // 视觉提示
    Transform* pT = registry_.GetComponent<Transform>(playerId_);
    if (pT) {
        SpawnFloatText(registry_, pT->position, "被诅咒！移速与攻速降低",
                       sf::Color(180, 80, 220), 22, 2.0f);
    }
    LOG_INFO("玩家被诅咒（房间 %d）", roomIndex);
}

// ============================================================================
// removeCurse —— 解除诅咒效果（诅咒房清理时）
// ============================================================================
void Game::removeCurse() {
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (!pc || !pc->cursed) return;
    pc->cursed = false;
    pc->cursedRoomIndex = -1;
    recomputePlayerStats();
    LOG_INFO("玩家诅咒已解除");
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
// renderFissureZones —— 渲染地裂区域视觉（暗红色半透明圆 + 边缘亮色）
// ============================================================================
void Game::renderFissureZones() {
    if (fissureZones_.empty()) return;

    // 切换到世界坐标 View（由 renderPlaying 调用，已是世界 View）
    for (const auto& fz : fissureZones_) {
        // 半透明暗红色圆形（地裂区域）
        sf::CircleShape zone(fz.radius);
        zone.setOrigin(fz.radius, fz.radius);
        zone.setPosition(fz.position);
        // 寿命越短越透明
        float alpha = std::min(1.f, fz.lifetime / 5.f) * 0.5f;
        sf::Uint8 a = static_cast<sf::Uint8>(alpha * 255);
        zone.setFillColor(sf::Color(120, 30, 20, a));
        zone.setOutlineColor(sf::Color(255, 100, 50, a));
        zone.setOutlineThickness(2.f);
        window_.draw(zone);

        // 中心暗棕色圆（裂缝核心）
        sf::CircleShape core(fz.radius * 0.4f);
        core.setOrigin(fz.radius * 0.4f, fz.radius * 0.4f);
        core.setPosition(fz.position);
        core.setFillColor(sf::Color(60, 20, 10, a));
        window_.draw(core);
    }
}

// ============================================================================
// handleEventInteraction —— 处理事件房 E 键交互
// ----------------------------------------------------------------------------
// 玩家在事件房内按 E 键时调用，根据事件类型弹出对话框或直接触发效果。
// 由于本游戏无完整对话框系统，采用直接触发 + 飘字反馈的简化方案：
//   - Beggar：消耗 50 金币 → 获得 100 经验 + 回复 20% HP
//   - Mage：消耗 30% 当前 HP → 获得随机品质装备
//   - ChestMimic：假宝箱变成精英怪（死亡掉落大量金币）
//   - Altar：消耗 30% 当前金币 → 永久 +5 攻击力
// ============================================================================
void Game::handleEventInteraction() {
    if (activeEventRoomIdx_ < 0) return;
    if (activeEventRoomIdx_ >= static_cast<int>(dungeon_.rooms.size())) return;

    Room& room = dungeon_.rooms[activeEventRoomIdx_];
    if (room.eventTriggered) {
        LOG_INFO("事件 %d 已触发过", activeEventRoomIdx_);
        return;
    }

    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    Transform* pT = registry_.GetComponent<Transform>(playerId_);
    if (!pc || !pT) return;

    room.eventTriggered = true; // 标记为已触发

    sf::Vector2f roomCenter = dungeon_.TileCenterToWorld(room.center);

    switch (activeEventType_) {
        case EventType::Beggar: {
            // 乞丐：给 50 金币换 100 经验 + 回血
            if (pc->stats.coins >= 50) {
                pc->stats.coins -= 50;
                upgradeSystem_.AddExp(100);
                // 回复 20% 最大 HP
                Health* h = registry_.GetComponent<Health>(playerId_);
                if (h) {
                    float heal = pc->stats.maxHp * 0.2f;
                    h->current = std::min(h->max, h->current + heal);
                    SpawnFloatText(registry_, pT->position, "+" + std::to_string(static_cast<int>(heal)) + " HP",
                                   sf::Color(80, 255, 80), 20, 1.5f);
                }
                // ---- 第三十一轮新增：动态事件叙述 ----
                std::string beggarText = "乞丐: 谢谢你的善心！";
                if (relicSystem_.HasRelic(RelicType::GreedyEye)) {
                    beggarText = "乞丐: 你眼神贪婪...但心地善良。";
                } else if (pc->stats.damage > 80.f) {
                    beggarText = "乞丐: 你身上的杀气太重了...但谢谢你。";
                }
                SpawnFloatText(registry_, roomCenter, beggarText,
                               sf::Color(255, 220, 100), 20, 2.0f);
                AudioManager::Instance().PlaySFX(AudioManager::kSFXPickup);
                LOG_INFO("乞丐事件: -50 金币, +100 经验, +20%% HP");
            } else {
                SpawnFloatText(registry_, roomCenter, "乞丐: 你金币不够...",
                               sf::Color(200, 200, 200), 20, 2.0f);
                room.eventTriggered = false; // 金币不够可重试
            }
            break;
        }
        case EventType::Mage: {
            // 神秘法师：消耗 30% 当前 HP 换取随机品质装备
            Health* h = registry_.GetComponent<Health>(playerId_);
            if (h && h->current > pc->stats.maxHp * 0.3f + 1.f) {
                float cost = h->current * 0.3f;
                h->current -= cost;
                SpawnFloatText(registry_, pT->position, "-" + std::to_string(static_cast<int>(cost)) + " HP",
                               sf::Color(255, 80, 80), 20, 1.5f);
                // 随机品质（蓝色/黄色/暗金色）
                int q = std::rand() % 3;
                ItemQuality quality = (q == 0) ? ItemQuality::Blue :
                                      (q == 1) ? ItemQuality::Yellow : ItemQuality::DarkGold;
                lootSystem_.DropItem(pT->position, quality, currentLevel_);
                // ---- 第三十一轮新增：动态事件叙述 ----
                std::string mageText = "神秘法师: 这件宝物归你了...";
                if (relicSystem_.HasRelic(RelicType::VampireFang)) {
                    mageText = "神秘法师: 你身上的黑暗气息...有趣。收下吧。";
                } else if (pc->comboCount >= 25) {
                    mageText = "神秘法师: 你的战斗技艺令人赞叹。这是奖赏。";
                }
                SpawnFloatText(registry_, roomCenter, mageText,
                               sf::Color(180, 100, 255), 20, 2.0f);
                AudioManager::Instance().PlaySFX(AudioManager::kSFXLevelUp);
                LOG_INFO("法师事件: -30%% HP, 获得品质=%d 装备", static_cast<int>(quality));
            } else {
                SpawnFloatText(registry_, roomCenter, "神秘法师: 你的生命太弱了...",
                               sf::Color(200, 200, 200), 20, 2.0f);
                room.eventTriggered = false; // HP 不够可重试
            }
            break;
        }
        case EventType::ChestMimic: {
            // 宝箱怪：假宝箱变成精英怪
            // 在房间中心放置的位置生成 1 个精英怪，死亡掉落大量金币
            SpawnFloatText(registry_, roomCenter, "宝箱是怪物伪装的！",
                           sf::Color(255, 80, 80), 22, 2.0f);
            EntityId mimicId = enemySpawner_.SpawnEnemyAt(EnemyType::Elite, roomCenter);
            if (mimicId != kInvalidEntity) {
                EnemyComponent* mimic = registry_.GetComponent<EnemyComponent>(mimicId);
                if (mimic) {
                    mimic->isBossMinion = true; // 标记为特殊掉落
                    mimic->isElite = true;
                }
            }
            // 清除假宝箱 tile（变成 Floor）
            // 搜索房间内的 Chest tile 并移除
            for (int ty = room.bounds.top; ty < room.bounds.top + room.bounds.height; ++ty) {
                for (int tx = room.bounds.left; tx < room.bounds.left + room.bounds.width; ++tx) {
                    if (dungeon_.GetTile(tx, ty) == TileType::Chest) {
                        dungeon_.SetTile(tx, ty, TileType::Floor);
                    }
                }
            }
            tileMap_.MarkDirty();
            AudioManager::Instance().PlaySFX(AudioManager::kSFXExplosion);
            LOG_INFO("宝箱怪事件: 生成 1 精英怪");
            break;
        }
        case EventType::Altar: {
            // 祭坛：消耗 30% 当前金币换永久 +5 攻击力
            int cost = static_cast<int>(pc->stats.coins * 0.3f);
            if (pc->stats.coins >= 10) { // 至少需要 10 金币才能触发
                pc->stats.coins -= cost;
                // 永久增加攻击力（通过 ApplyUpgrade 增加 DamageUp 等级）
                // 注意：若玩家有技能点会消耗 1 个，无技能点则不消耗
                upgradeSystem_.ApplyUpgrade(UpgradeType::DamageUp);
                recomputePlayerStats();
                // ---- 第三十一轮新增：动态事件叙述 ----
                std::string altarText = "祭坛吸收了 " + std::to_string(cost) + " 金币，攻击力 +5";
                if (relicSystem_.HasRelic(RelicType::Aegis)) {
                    altarText = "祭坛与守护之心共鸣！攻击力 +5";
                } else if (pc->stats.damage > 100.f) {
                    altarText = "祭坛: 你的力量已经很强大了...再强一些吧。";
                }
                SpawnFloatText(registry_, roomCenter, altarText,
                               sf::Color(255, 180, 80), 20, 2.5f);
                AudioManager::Instance().PlaySFX(AudioManager::kSFXLevelUp);
                LOG_INFO("祭坛事件: -%d 金币, +5 攻击力", cost);
            } else {
                SpawnFloatText(registry_, roomCenter, "祭坛: 你的金币太少...",
                               sf::Color(200, 200, 200), 20, 2.0f);
                room.eventTriggered = false; // 金币不够可重试
            }
            break;
        }
        case EventType::Forge: {
            // 锻造房：花费 200 金币随机升级穿戴中一件装备品质（白→蓝→黄→暗金）
            if (pc->stats.coins >= 200) {
                bool upgraded = false;
                // 先收集所有可升级槽位
                std::vector<int> upgradeableSlots;
                const auto& equipped = inventorySystem_.GetEquippedItems();
                for (int s = 0; s < static_cast<int>(equipped.size()); ++s) {
                    if (equipped[s].item.has_value() && equipped[s].item->quality < ItemQuality::DarkGold) {
                        upgradeableSlots.push_back(s);
                    }
                }
                if (!upgradeableSlots.empty()) {
                    // 随机选一个槽位
                    int s = upgradeableSlots[std::rand() % upgradeableSlots.size()];
                    ItemSlot slot = static_cast<ItemSlot>(s);
                    auto unequipped = inventorySystem_.Unequip(slot);
                    if (unequipped.has_value()) {
                        Item upgraded = unequipped.value();
                        upgraded.quality = static_cast<ItemQuality>(static_cast<int>(upgraded.quality) + 1);
                        inventorySystem_.Equip(upgraded);
                        pc->stats.coins -= 200;
                        recomputePlayerStats();
                        SpawnFloatText(registry_, roomCenter,
                                       "锻造台: 装备已升级为" + std::string(LootSystem::GetQualityName(upgraded.quality)) + "品质!",
                                       sf::Color(255, 200, 80), 20, 2.5f);
                        AudioManager::Instance().PlaySFX(AudioManager::kSFXEquip);
                        LOG_INFO("锻造房事件: -200 金币, 槽位 %d 品质升级到 %d", s, static_cast<int>(upgraded.quality));
                    }
                } else {
                    SpawnFloatText(registry_, roomCenter, "锻造台: 没有可升级的装备...",
                                   sf::Color(200, 200, 200), 20, 2.0f);
                    room.eventTriggered = false;
                }
            } else {
                SpawnFloatText(registry_, roomCenter, "锻造台: 需要 200 金币...",
                               sf::Color(200, 200, 200), 20, 2.0f);
                room.eventTriggered = false;
            }
            break;
        }
        default:
            break;
    }

    // 清除事件提示
    pc->eventPromptActive = false;
    activeEventRoomIdx_ = -1;
    activeEventType_ = EventType::None;
}

// ============================================================================
// renderEventHint —— 渲染事件房交互提示（房间上方文字）
// ============================================================================
void Game::renderEventHint() {
    if (activeEventRoomIdx_ < 0) return;
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (!pc || !pc->eventPromptActive) return;

    // 在事件房中心上方显示提示
    if (activeEventRoomIdx_ >= static_cast<int>(dungeon_.rooms.size())) return;
    const Room& room = dungeon_.rooms[activeEventRoomIdx_];
    if (room.eventTriggered) return;

    sf::Vector2f roomCenter = dungeon_.TileCenterToWorld(room.center);

    // ---- 渲染事件房 NPC 贴图（世界空间）----
    // 宝箱怪事件不渲染 NPC（假宝箱由 TileType::Chest 渲染）
    const char* spriteKey = nullptr;
    switch (activeEventType_) {
        case EventType::Beggar:     spriteKey = "event_beggar"; break;
        case EventType::Mage:       spriteKey = "event_mage";   break;
        case EventType::Altar:      spriteKey = "event_altar";  break;
        case EventType::ChestMimic: spriteKey = nullptr;        break; // 复用宝箱 tile
        case EventType::Forge:      spriteKey = "event_altar";  break; // 锻造台复用祭坛贴图
        default: return;
    }
    if (spriteKey) {
        sf::IntRect rect = atlas_.GetPixelRect(spriteKey);
        const sf::Texture* tex = atlas_.GetTexture();
        if (rect.width > 0 && rect.height > 0 && tex) {
            sf::Sprite npcSprite;
            npcSprite.setTexture(*tex);
            npcSprite.setTextureRect(rect);
            npcSprite.setOrigin(rect.width * 0.5f, rect.height * 0.5f);
            // NPC 放在房间中心，放大 1.5 倍与玩家一致
            npcSprite.setPosition(roomCenter.x, roomCenter.y);
            npcSprite.setScale(1.5f, 1.5f);
            window_.draw(npcSprite);
        }
    }

    // ---- 渲染交互提示文字（NPC 上方）----
    std::string hint;
    switch (activeEventType_) {
        case EventType::Beggar:     hint = "按 E 与乞丐对话"; break;
        case EventType::Mage:       hint = "按 E 与神秘法师交谈"; break;
        case EventType::ChestMimic: hint = ""; break; // 宝箱怪不显示提示（避免剧透）
        case EventType::Altar:      hint = "按 E 使用祭坛"; break;
        case EventType::Forge:      hint = "按 E 使用锻造台"; break;
        default: return;
    }
    if (hint.empty()) return; // 宝箱怪事件不显示提示

    eventHintText_.setString(utf8ToSfString(hint));
    // 提示文字放在 NPC 上方
    eventHintText_.setPosition(roomCenter.x - 100.f, roomCenter.y - 60.f);
    window_.draw(eventHintText_);
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

} // namespace cu
