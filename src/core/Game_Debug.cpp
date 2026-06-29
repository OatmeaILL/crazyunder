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

} // namespace cu