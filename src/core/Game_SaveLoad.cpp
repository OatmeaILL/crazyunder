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

} // namespace cu