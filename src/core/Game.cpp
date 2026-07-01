#include "core/Game.h"
#include "utils/Logger.h"
#include "utils/TextureGenerator.h"
#include "ecs/Component.h"
#include "gameplay/PlayerCombat.h"
#include "gameplay/CombatEffects.h"
#include "gameplay/SkillSystem.h"
#include "gameplay/DialogueData.h"
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
    classSelectMenu_.Initialize(font); // 职业选择菜单
    // 同步当前设置到设置菜单显示
    settingsMenu_.SetBGMVolume(settings_.GetBGMVolume());
    settingsMenu_.SetSFXVolume(settings_.GetSFXVolume());
    settingsMenu_.SetResolution(settings_.GetWidth(), settings_.GetHeight());

    // 注册主菜单按钮回调
  mainMenu_.GetStartButton()->SetOnClick([this]() {
        // 新游戏：先弹出职业选择菜单，确认后再弹出槽位选择
        showClassSelectMenu();
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

    // ---- 第三十三轮新增：对话系统初始化 ----
    dialogueBoxUI_.Initialize(font);
    registerDialogueCallbacks();
    LOG_INFO("对话系统初始化完成");
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
            LOG_INFO("[Dead] SetVisible 开始");
            deathScreen_.SetVisible(true);
            LOG_INFO("[Dead] SetVisible 完成");
            LOG_INFO("[Dead] StopBGM 开始");
            AudioManager::Instance().StopBGM();
            LOG_INFO("[Dead] StopBGM 完成");
            LOG_INFO("[Dead] PlaySFX 开始");
            AudioManager::Instance().PlaySFX(AudioManager::kSFXPlayerDeath);
            LOG_INFO("[Dead] PlaySFX 完成");
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

void Game::ChangeState(GameState newState) {
    if (state_ == newState) return;

    // Playing ↔ Paused 切换时非对称处理：
    //   Playing→Paused: 不调用 Playing.onExit（避免 registry_.Clear() 清空场景），
    //                    但必须调用 Paused.onEnter（显示暂停菜单、停止BGM）
    //   Paused→Playing: 调用 Paused.onExit（隐藏暂停菜单），
    //                   但不调用 Playing.onEnter（避免 setupPlayingScene() 重建场景）
    bool skipOnExit = (state_ == GameState::Playing && newState == GameState::Paused);
    bool skipOnEnter = (state_ == GameState::Paused && newState == GameState::Playing);

    // 切换状态时清理对话系统（Playing ↔ Paused 除外）
    if (!skipOnExit && !skipOnEnter && dialogueSystem_.IsActive()) {
        dialogueSystem_.EndDialogue();
        dialogueBoxUI_.SetVisible(false);
        pendingEventRoomIdx_ = -1;
        pendingMerchantOpen_ = false;
    }

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
                // 职业选择菜单打开时，ESC 优先关闭并返回主菜单
                if (classSelectMenuVisible_) {
                    classSelectMenuVisible_ = false;
                    classSelectMenu_.SetVisible(false);
                    LOG_INFO("职业选择菜单已关闭（ESC）");
                }
                // 存档菜单打开时，ESC 优先关闭存档菜单
                else if (saveLoadMenuVisible_) {
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
            } else if (key == sf::Keyboard::Enter) {
                // 职业选择菜单确认
                if (classSelectMenuVisible_ && classSelectMenu_.GetSelectedIndex() >= 0) {
                    handleClassSelectMenuClick(classSelectMenu_.GetSelectedIndex());
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
                // 对话系统优先：对话活跃时，E 键推进对话
                if (dialogueSystem_.IsActive()) {
                    dialogueSystem_.Advance();
                } else if (state_ == GameState::Playing && dungeonInitialized_) {
                    handleInteract();
                }
            } else if (key == sf::Keyboard::Num1 || key == sf::Keyboard::Num2 ||
                       key == sf::Keyboard::Num3 || key == sf::Keyboard::Num4) {
                // ---- 职业选择菜单键盘输入（1/2 选择职业）----
                if (classSelectMenuVisible_) {
                    int idx = classSelectMenu_.HandleKeyInput(static_cast<int>(key));
                    if (idx >= 0) {
                        selectedClass_ = static_cast<PlayerClass>(idx);
                        AudioManager::Instance().PlaySFX(AudioManager::kSFXPickup);
                    }
                    return;
                }
                // ---- 第三十三轮：对话选项优先（对话活跃时，1-4 选择选项）----
                if (dialogueSystem_.IsActive() && dialogueSystem_.GetState().showChoices) {
                    int idx = -1;
                    if (key == sf::Keyboard::Num1) idx = 0;
                    else if (key == sf::Keyboard::Num2) idx = 1;
                    else if (key == sf::Keyboard::Num3) idx = 2;
                    else if (key == sf::Keyboard::Num4) idx = 3;
                    if (idx >= 0) {
                        dialogueSystem_.SelectChoice(idx);
                    }
                }
                // ---- 第十五轮：圣物选择优先（Boss 击败后弹出时）----
                else if (state_ == GameState::Playing && relicChoiceActive_) {
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
                            PlayerComponent* pcR = registry_.GetComponent<PlayerComponent>(playerId_);
                            PlayerClass clsR = pcR ? pcR->playerClass : PlayerClass::Mage;
                            currentUpgradeOptions_ = upgradeSystem_.RollUpgrades(clsR);
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

        // 4.5 职业选择菜单覆盖层（任意状态都可显示）
        if (classSelectMenuVisible_) {
            classSelectMenu_.Render(window_);
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
// 第三十三轮新增：对话系统回调注册与渲染
// ============================================================================

void Game::registerDialogueCallbacks() {
    // 注册对话树
    dialogueTreeId_Beggar_ = dialogueSystem_.RegisterTree(kBeggarDialogue);
    dialogueTreeId_MerchantNpc_ = dialogueSystem_.RegisterTree(kMerchantDialogue);
    dialogueTreeId_Mage_ = dialogueSystem_.RegisterTree(kMageDialogue);
    dialogueTreeId_Tutorial_ = dialogueSystem_.RegisterTree(kTutorialDialogue);

    // 动作处理器：对话中的 Action 节点执行的具体游戏操作
    dialogueSystem_.SetActionHandler([this](DialogueAction action, int param) -> bool {
        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
        Transform* pT = registry_.GetComponent<Transform>(playerId_);
        if (!pc || !pT) return false;

        switch (action) {
            case DialogueAction::GiveGold:
                pc->stats.coins += param;
                LOG_INFO("对话动作: 获得 %d 金币", param);
                return true;
            case DialogueAction::TakeGold:
                if (pc->stats.coins >= param) {
                    pc->stats.coins -= param;
                    LOG_INFO("对话动作: 扣除 %d 金币", param);
                    return true;
                }
                LOG_WARN("对话动作: 金币不足 (%d < %d)", pc->stats.coins, param);
                return false;
            case DialogueAction::GiveExp:
                upgradeSystem_.AddExp(param);
                LOG_INFO("对话动作: 获得 %d 经验", param);
                return true;
            case DialogueAction::HealPlayer: {
                Health* h = registry_.GetComponent<Health>(playerId_);
                if (h) {
                    float heal = pc->stats.maxHp * (param / 100.f);
                    h->current = std::min(h->max, h->current + heal);
                    LOG_INFO("对话动作: 回复 %d%% HP (%.0f)", param, heal);
                }
                return true;
            }
            case DialogueAction::GiveItem: {
                ItemQuality q = static_cast<ItemQuality>(std::clamp(param, 1, 4));
                lootSystem_.DropItem(pT->position, q, currentLevel_);
                LOG_INFO("对话动作: 获得品质=%d 装备", static_cast<int>(q));
                return true;
            }
            case DialogueAction::ApplyCurse:
                if (activeEventRoomIdx_ >= 0) {
                    applyCurse(activeEventRoomIdx_);
                } else {
                    pc->cursed = true;
                }
                LOG_INFO("对话动作: 施加诅咒");
                return true;
            case DialogueAction::RemoveCurse:
                removeCurse();
                LOG_INFO("对话动作: 解除诅咒");
                return true;
            case DialogueAction::GrantLevels:
                for (int i = 0; i < param; ++i) {
                    upgradeSystem_.AddExp(upgradeSystem_.GetExpToNext());
                }
                LOG_INFO("对话动作: 提升 %d 级", param);
                return true;
            case DialogueAction::MarkEventDone:
                // 标记事件完成（参数为事件ID，在事件系统中使用）
                LOG_INFO("对话动作: 标记事件 %d 已完成", param);
                return true;
            case DialogueAction::SacrificeHP: {
                // 献祭当前 HP 的 param%（法师事件用）
                Health* h = registry_.GetComponent<Health>(playerId_);
                if (h) {
                    float cost = h->current * (param / 100.f);
                    h->current = std::max(1.f, h->current - cost);
                    SpawnDamageText(registry_, pT->position, cost, false);
                    LOG_INFO("对话动作: 献祭 %d%% HP (%.0f)", param, cost);
                }
                return true;
            }
            case DialogueAction::GiveRandomItem: {
                // 给予随机品质装备（param=品质上限，0=白 1=蓝 2=黄 3=暗金）
                int q = (std::rand() % (param + 1)) + 1;
                ItemQuality quality = static_cast<ItemQuality>(std::clamp(q, 1, 3));
                lootSystem_.DropItem(pT->position, quality, currentLevel_);
                LOG_INFO("对话动作: 获得随机品质=%d 装备", static_cast<int>(quality));
                return true;
            }
            case DialogueAction::OpenQuestMenu:
                questMenuVisible_ = true;
                questMenu_.SetQuestData(questSystem_);
                questMenu_.SetVisible(true);
                LOG_INFO("对话动作: 打开任务栏");
                return true;
            default:
                return false;
        }
    });

    // 条件求值器：对话 Branch 节点的条件检查
    dialogueSystem_.SetConditionEvaluator([this](BranchCondition cond, int param) -> bool {
        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
        if (!pc) return false;

        switch (cond) {
            case BranchCondition::HasGold:
                return pc->stats.coins >= param;
            case BranchCondition::HasItem:
                // 检查背包中是否有 >= param 品质的物品
                for (int i = 0; i < InventorySystem::kBackpackSize; ++i) {
                    auto item = inventorySystem_.GetBackpackItem(i);
                    if (item.has_value() && static_cast<int>(item->quality) >= param) {
                        return true;
                    }
                }
                // 也检查已装备
                for (const auto& slot : inventorySystem_.GetEquippedItems()) {
                    if (slot.item.has_value() && static_cast<int>(slot.item->quality) >= param) {
                        return true;
                    }
                }
                return false;
            case BranchCondition::HasRelic:
                return relicSystem_.HasRelic(static_cast<RelicType>(param));
            case BranchCondition::HasSkill:
                return PlayerHasSkill(*pc, static_cast<SkillType>(param));
            case BranchCondition::HPBelow: {
                Health* h = registry_.GetComponent<Health>(playerId_);
                if (!h || h->max <= 0.f) return false;
                float hpPct = (h->current / h->max) * 100.f;
                return hpPct < static_cast<float>(param);
            }
            case BranchCondition::IsCursed:
                return pc->cursed;
            case BranchCondition::QuestCompleted: {
                // 检查该 ID 的任务是否处于 Completed 状态
                for (const auto& qi : questSystem_.GetAllQuests()) {
                    if (qi.id == param && qi.state == QuestState::Completed) {
                        return true;
                    }
                }
                return false;
            }
            case BranchCondition::ComboAbove:
                return pc->comboCount >= param;
            default:
                return false;
        }
    });
}

void Game::renderDialogueBox() {
    if (!dialogueSystem_.IsActive()) return;
    dialogueBoxUI_.Render(window_);
}

} // namespace cu
