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

    // ---- 第三十三轮新增：对话系统鼠标交互（Playing 状态，对话活跃时）----
    if (state_ == GameState::Playing && dialogueSystem_.IsActive()) {
        dialogueBoxUI_.UpdateHover(mousePos);
        if (mousePressed) {
            int choiceIdx = dialogueBoxUI_.HandleClick(mousePos);
            if (choiceIdx >= 0) {
                dialogueSystem_.SelectChoice(choiceIdx);
            } else {
                // 点击非选项区域 → 推进对话（同 E 键）
                dialogueSystem_.Advance();
            }
        }
        return; // 对话活跃时屏蔽其他 UI 输入
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
                            auto oldItem = inventorySystem_.Equip(*backpackItem);
                            if (slotOccupied && oldItem.has_value()) {
                                inventorySystem_.ReplaceBackpackItem(targetIdx, *oldItem);
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
                    auto oldItem = inventorySystem_.Equip(*backpackItem);
                    if (slotOccupied && oldItem.has_value()) {
                        inventorySystem_.ReplaceBackpackItem(index, *oldItem);
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

    // ---- 第三十三轮新增：NPC 对话交互（优先于事件房）----
    // 遍历所有 NPC 实体，检查玩家是否在交互范围内
    if (!dialogueSystem_.IsActive()) {
        registry_.ForEach<Transform, NPCComponent>([&](EntityId id) {
            Transform* npcT = registry_.GetComponent<Transform>(id);
            NPCComponent* npc = registry_.GetComponent<NPCComponent>(id);
            if (!npcT || !npc) return;
            if (npc->dialogueTreeId < 0) return;

            float dx = npcT->position.x - pT->position.x;
            float dy = npcT->position.y - pT->position.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            const float kInteractRange = 64.f; // 64px 交互范围

            if (dist < kInteractRange) {
                dialogueSystem_.StartDialogue(npc->dialogueTreeId);
                dialogueBoxUI_.SetVisible(true);
                LOG_INFO("与 NPC 对话: treeId=%d", npc->dialogueTreeId);
            }
        });
        // 如果在对话中，跳过后续交互
        if (dialogueSystem_.IsActive()) return;
    }

    // ---- 事件房交互（改为对话系统驱动）----
    // 玩家在事件房内且事件未触发时，按 E 启动对话
    if (activeEventRoomIdx_ >= 0 && activeEventType_ != EventType::None) {
        int curRoom = roomSystem_.GetCurrentRoomIndex();
        if (curRoom == activeEventRoomIdx_ && !dialogueSystem_.IsActive()) {
            // 根据事件类型选择对话树
            int treeId = -1;
            switch (activeEventType_) {
                case EventType::Beggar: treeId = dialogueTreeId_Beggar_; break;
                case EventType::Mage:   treeId = dialogueTreeId_Mage_;   break;
                default: break;
            }
            if (treeId >= 0) {
                dialogueSystem_.StartDialogue(treeId);
                dialogueBoxUI_.SetVisible(true);
                // 对话结束后标记事件已触发
                pendingEventRoomIdx_ = activeEventRoomIdx_;
                LOG_INFO("事件房对话启动: type=%d treeId=%d",
                         static_cast<int>(activeEventType_), treeId);
                return;
            }
            // 非对话事件（ChestMimic/Altar/Forge 仍走原有逻辑）
            handleEventInteraction();
            return;
        }
    }

    // ---- 商人交互（第三十三轮：改为对话系统驱动）----
    if (merchantSystem_.IsActive() && merchantSystem_.IsPlayerInRange(pT->position)) {
        if (!dialogueSystem_.IsActive() && !merchantMenuVisible_) {
            // 启动商人对话
            dialogueSystem_.StartDialogue(dialogueTreeId_MerchantNpc_);
            dialogueBoxUI_.SetVisible(true);
            pendingMerchantOpen_ = true; // 对话结束后打开商人菜单
            LOG_INFO("商人对话启动");
        }
        return;
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

} // namespace cu