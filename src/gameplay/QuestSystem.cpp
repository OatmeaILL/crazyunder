#include "gameplay/QuestSystem.h"
#include "core/AudioManager.h"
#include "utils/Logger.h"

namespace cu {

QuestSystem::QuestSystem() = default;

// ============================================================================
// Initialize —— 注册剧情任务线
// ----------------------------------------------------------------------------
// 5 个主线任务，按 ID 顺序解锁：
//   1. 破晓之始（首层通关）→ 解锁 2,3,4,5
//   2. 古老祭坛（找到1个祭坛）
//   3. 地下奇人（找到1个乞丐）
//   4. 百艺兼修（收集4个技能）
//   5. 财富之力（攒1000金币，提交换金装+5级）
// ============================================================================
void QuestSystem::Initialize() {
    questDefs_.clear();
    quests_.clear();

    // ---- 任务 1：破晓之始（首层通关）----
    // 剧情：玩家初入地牢，需击败首层 Boss 才能进入下层。
    // 目标：到达地牢第 2 层（即首层已通关）
    // 奖励：200 经验 + 100 金币 + 1 件蓝色装备
    {
        QuestDef def;
        def.id = 1;
        def.type = QuestType::ReachLevel;
        def.title = "破晓之始";
        def.description = "击败第一层 Boss，进入地牢更深处";
        def.targetLevel = 2;
        def.targetCount = 2;
        def.prerequisiteQuestId = 0; // 无前置，游戏开始即可
        def.reward.exp = 200;
        def.reward.coins = 100;
        def.reward.itemQuality = ItemQuality::Blue;
        def.reward.itemCount = 1;
        def.reward.isRandomItem = true;
        questDefs_.push_back(def);
    }

    // ---- 任务 2：古老祭坛 ----
    // 剧情：地牢深处散布着远古祭坛，献祭金币可获得永久强化。
    // 目标：找到并触发 1 个祭坛
    // 奖励：300 经验 + 150 金币 + 1 件史诗装备
    {
        QuestDef def;
        def.id = 2;
        def.type = QuestType::TriggerEvent;
        def.title = "古老祭坛";
        def.description = "在地牢中找到一个祭坛并触发它";
        def.targetEvent = EventType::Altar;
        def.targetCount = 1;
        def.prerequisiteQuestId = 1; // 任务1 完成后解锁
        def.reward.exp = 300;
        def.reward.coins = 150;
        def.reward.itemQuality = ItemQuality::Yellow; // 史诗品质（枚举无 Purple，用 Yellow 表示）
        def.reward.itemCount = 1;
        def.reward.isRandomItem = true;
        questDefs_.push_back(def);
    }

    // ---- 任务 3：地下奇人 ----
    // 剧情：地下世界聚集着各色奇人异士，有乞丐以金币换情报。
    // 目标：找到并触发 1 个乞丐
    // 奖励：300 经验 + 150 金币 + 1 件史诗装备
    {
        QuestDef def;
        def.id = 3;
        def.type = QuestType::TriggerEvent;
        def.title = "地下奇人";
        def.description = "在地牢中找到一个乞丐并与之交谈";
        def.targetEvent = EventType::Beggar;
        def.targetCount = 1;
        def.prerequisiteQuestId = 1;
        def.reward.exp = 300;
        def.reward.coins = 150;
        def.reward.itemQuality = ItemQuality::Yellow; // 史诗品质
        def.reward.itemCount = 1;
        def.reward.isRandomItem = true;
        questDefs_.push_back(def);
    }

    // ---- 任务 4：百艺兼修 ----
    // 剧情：真正的勇者需掌握多种技能，应对地牢中的各种威胁。
    // 目标：收集 4 个技能（技能槽 + 技能背包总数）
    // 奖励：500 经验 + 200 金币 + 1 技能点 + 1 件暗金装备
    {
        QuestDef def;
        def.id = 4;
        def.type = QuestType::CollectSkills;
        def.title = "百艺兼修";
        def.description = "收集 4 个技能（含已装备和背包中）";
        def.targetCount = 4;
        def.prerequisiteQuestId = 1;
        def.reward.exp = 500;
        def.reward.coins = 200;
        def.reward.skillPoints = 1;
        def.reward.itemQuality = ItemQuality::DarkGold;
        def.reward.itemCount = 1;
        def.reward.isRandomItem = true;
        questDefs_.push_back(def);
    }

    // ---- 任务 5：财富之力 ----
    // 剧情：地牢商人对你的财富感兴趣，提出以 1000 金币换取传说装备。
    // 目标：攒够 1000 金币（玩家当前持有）
    // 提交奖励：金色装备1件（随机）+ 等级 +5
    // 注意：领取时扣除 1000 金币（reward.coins = -1000）
    {
        QuestDef def;
        def.id = 5;
        def.type = QuestType::AccumulateCoins;
        def.title = "财富之力";
        def.description = "攒够 1000 金币，提交给商人换取传说装备与力量";
        def.targetCount = 1000;
        def.prerequisiteQuestId = 1;
        def.reward.coins = -1000;       // 扣除 1000 金币
        def.reward.addLevels = 5;        // 等级 +5
        def.reward.itemQuality = ItemQuality::Yellow; // 金色装备
        def.reward.itemCount = 1;
        def.reward.isRandomItem = true;
        questDefs_.push_back(def);
    }

    // ============================================================================
    // ---- 第二十二轮新增：激活 QuestType 死接口（KillTarget/CollectItem/
    //                        SurviveTime/ClearRooms 四种类型）
    // ----------------------------------------------------------------------------
    // 设计意图：
    //   - 当前任务线只有 5 个主线任务，玩家前期可选任务少，长线内容单薄
    //   - 4 种 QuestType 枚举、QuestDef 字段、progressQuest 匹配逻辑、Update
    //     时间累计、Game.cpp 事件上报——五层基础设施全部就位但从未被注册使用
    //   - 新增 5 个支线任务（id 6-10），与主线 2-5 并行解锁（前置=任务 1）
    //   - 与第二十一轮词缀系统协同：任务 6 引导玩家关注 Elite 类型敌人
    //   - 横向内容扩展，不修改任何数值公式，零平衡风险
    // ============================================================================

    // ---- 任务 6：精英猎人（KillTarget，激活击杀指定敌人类型）----
    // 目标：击杀 5 个 Elite 类型敌人（EnemyType::Elite）
    // 奖励：400 经验 + 200 金币 + 1 件史诗装备
    {
        QuestDef def;
        def.id = 6;
        def.type = QuestType::KillTarget;
        def.title = "精英猎人";
        def.description = "击败 5 个精英类型敌人，证明你的实力";
        def.targetEnemy = EnemyType::Elite;
        def.targetCount = 5;
        def.prerequisiteQuestId = 1; // 任务1 完成后与任务 2-5 并行解锁
        def.reward.exp = 400;
        def.reward.coins = 200;
        def.reward.itemQuality = ItemQuality::Yellow; // 史诗
        def.reward.itemCount = 1;
        def.reward.isRandomItem = true;
        questDefs_.push_back(def);
    }

    // ---- 任务 7：远程杀手（KillTarget，引导玩家熟悉不同敌人类型）----
    // 目标：击杀 10 个 Ranged 类型敌人
    // 奖励：350 经验 + 180 金币 + 1 件蓝色装备
    {
        QuestDef def;
        def.id = 7;
        def.type = QuestType::KillTarget;
        def.title = "远程杀手";
        def.description = "击败 10 个远程敌人，瓦解它们的火力网";
        def.targetEnemy = EnemyType::Ranged;
        def.targetCount = 10;
        def.prerequisiteQuestId = 1;
        def.reward.exp = 350;
        def.reward.coins = 180;
        def.reward.itemQuality = ItemQuality::Blue;
        def.reward.itemCount = 1;
        def.reward.isRandomItem = true;
        questDefs_.push_back(def);
    }

    // ---- 任务 8：史诗收藏家（CollectItem，激活拾取指定品质装备）----
    // 目标：拾取 3 件史诗（Yellow）品质装备
    // 奖励：500 经验 + 250 金币 + 1 技能点
    {
        QuestDef def;
        def.id = 8;
        def.type = QuestType::CollectItem;
        def.title = "史诗收藏家";
        def.description = "拾取 3 件史诗品质装备，构筑你的装备 build";
        def.targetQuality = ItemQuality::Yellow; // 史诗品质
        def.targetCount = 3;
        def.prerequisiteQuestId = 1;
        def.reward.exp = 500;
        def.reward.coins = 250;
        def.reward.skillPoints = 1;
        def.reward.itemQuality = ItemQuality::White; // 无装备奖励
        def.reward.itemCount = 0;
        questDefs_.push_back(def);
    }

    // ---- 任务 9：不死行者（SurviveTime，激活存活时间累计）----
    // 目标：单次生命存活 180 秒（3 分钟）
    // 奖励：600 经验 + 300 金币 + 1 件暗金装备
    // 注意：SurviveTime 通过 Update 中 timeAccumulator 累计，无需事件上报
    //       进度 = floor(timeAccumulator)，targetProgress = 180
    {
        QuestDef def;
        def.id = 9;
        def.type = QuestType::SurviveTime;
        def.title = "不死行者";
        def.description = "单次生命存活 180 秒，展现你的生存能力";
        def.targetTime = 180.f; // 语义标注（实际逻辑用 targetCount）
        def.targetCount = 180;  // Update 中 currentProgress = floor(timeAccumulator)
        def.prerequisiteQuestId = 1;
        def.reward.exp = 600;
        def.reward.coins = 300;
        def.reward.itemQuality = ItemQuality::DarkGold; // 暗金
        def.reward.itemCount = 1;
        def.reward.isRandomItem = true;
        questDefs_.push_back(def);
    }

    // ---- 任务 10：地牢清道夫（ClearRooms，激活清理房间计数）----
    // 目标：清理 10 个房间
    // 奖励：450 经验 + 220 金币 + 1 件史诗装备
    {
        QuestDef def;
        def.id = 10;
        def.type = QuestType::ClearRooms;
        def.title = "地牢清道夫";
        def.description = "清理 10 个房间，扫清地牢的每一寸土地";
        def.targetCount = 10;
        def.prerequisiteQuestId = 1;
        def.reward.exp = 450;
        def.reward.coins = 220;
        def.reward.itemQuality = ItemQuality::Yellow; // 史诗
        def.reward.itemCount = 1;
        def.reward.isRandomItem = true;
        questDefs_.push_back(def);
    }

    // ---- 初始化任务实例 ----
    quests_.reserve(questDefs_.size());
    for (const auto& def : questDefs_) {
        QuestInstance inst;
        inst.id = def.id;
        inst.state = QuestState::Locked;
        inst.currentProgress = 0;
        inst.targetProgress = def.targetCount;
        inst.timeAccumulator = 0.f;
        quests_.push_back(inst);
    }

    LOG_INFO("任务系统已初始化: 注册 %d 个主线任务", static_cast<int>(questDefs_.size()));
}

// ============================================================================
// Update —— 每帧更新
// ============================================================================
void QuestSystem::Update(float dt, int currentLevel, int currentCoins, int currentSkillCount) {
    // 1. 检查解锁条件
    checkUnlocks(currentLevel);

    // 2. 推进 AccumulateCoins 类任务（任务5：玩家当前金币）
    if (currentCoins >= 0) {
        for (auto& q : quests_) {
            if (q.state != QuestState::Active) continue;
            const QuestDef* def = GetQuestDef(q.id);
            if (!def || def->type != QuestType::AccumulateCoins) continue;
            q.currentProgress = currentCoins;
        }
    }

    // 3. 推进 CollectSkills 类任务（任务4：技能总数）
    if (currentSkillCount >= 0) {
        for (auto& q : quests_) {
            if (q.state != QuestState::Active) continue;
            const QuestDef* def = GetQuestDef(q.id);
            if (!def || def->type != QuestType::CollectSkills) continue;
            if (currentSkillCount > q.currentProgress) {
                q.currentProgress = currentSkillCount;
            }
        }
    }

    // 4. SurviveTime 类任务时间累计（当前任务线未用，保留接口）
    for (auto& q : quests_) {
        if (q.state != QuestState::Active) continue;
        const QuestDef* def = GetQuestDef(q.id);
        if (!def || def->type != QuestType::SurviveTime) continue;
        q.timeAccumulator += dt;
        int newProgress = static_cast<int>(q.timeAccumulator);
        if (newProgress > q.currentProgress) {
            q.currentProgress = newProgress;
        }
    }

    // 5. 检查完成条件
    checkCompletion();
}

// ============================================================================
// 事件上报接口
// ============================================================================
void QuestSystem::OnEnemyKilled(EnemyType type, bool isChampion) {
    (void)isChampion;
    progressQuest(QuestType::KillTarget, 1, type);
}

void QuestSystem::OnItemPickedUp(ItemQuality quality) {
    progressQuest(QuestType::CollectItem, 1, EnemyType::Melee, quality);
}

void QuestSystem::OnLevelReached(int newLevel) {
    // ReachLevel 任务：进度直接设置为当前层数
    for (auto& q : quests_) {
        if (q.state != QuestState::Active) continue;
        const QuestDef* def = GetQuestDef(q.id);
        if (!def || def->type != QuestType::ReachLevel) continue;
        if (newLevel > q.currentProgress) {
            q.currentProgress = newLevel;
        }
    }
}

void QuestSystem::OnEventTriggered(EventType evtType) {
    progressQuest(QuestType::TriggerEvent, 1, EnemyType::Melee, ItemQuality::White, evtType);
}

void QuestSystem::OnRoomCleared() {
    progressQuest(QuestType::ClearRooms, 1);
}

// ============================================================================
// 第二十二轮新增：OnPlayerDeath —— 玩家死亡时重置 SurviveTime 任务进度
// ----------------------------------------------------------------------------
// 设计意图：SurviveTime 任务（如任务9 "不死行者"）语义为"单次生命存活 N 秒"。
// 玩家死亡后若不重置 timeAccumulator，玩家可跨多次生命累计达成目标，
// 与"单次生命"语义不符。此接口在玩家死亡时被调用，重置所有 Active 状态的
// SurviveTime 任务的 timeAccumulator 和 currentProgress，确保语义正确。
// ============================================================================
void QuestSystem::OnPlayerDeath() {
    for (auto& q : quests_) {
        if (q.state != QuestState::Active) continue;
        const QuestDef* def = GetQuestDef(q.id);
        if (!def || def->type != QuestType::SurviveTime) continue;
        q.timeAccumulator = 0.f;
        q.currentProgress = 0;
        LOG_INFO("玩家死亡，重置 SurviveTime 任务 %d 进度（单次生命语义）", q.id);
    }
}

// ============================================================================
// 查询接口
// ============================================================================
const QuestDef* QuestSystem::GetQuestDef(int id) const {
    for (const auto& def : questDefs_) {
        if (def.id == id) return &def;
    }
    return nullptr;
}

int QuestSystem::GetCompletedUnclaimedCount() const noexcept {
    int count = 0;
    for (const auto& q : quests_) {
        if (q.state == QuestState::Completed) ++count;
    }
    return count;
}

int QuestSystem::GetActiveCount() const noexcept {
    int count = 0;
    for (const auto& q : quests_) {
        if (q.state == QuestState::Active || q.state == QuestState::Completed) ++count;
    }
    return count;
}

// ============================================================================
// ClaimReward —— 领取任务奖励
// ============================================================================
std::optional<QuestReward> QuestSystem::ClaimReward(int questId, int* taskIdToUnlockAfter) {
    if (taskIdToUnlockAfter) *taskIdToUnlockAfter = 0;

    for (auto& q : quests_) {
        if (q.id != questId) continue;
        if (q.state != QuestState::Completed) {
            LOG_WARN("任务 %d 未完成或已领取，无法领取奖励", questId);
            return std::nullopt;
        }
        const QuestDef* def = GetQuestDef(questId);
        if (!def) return std::nullopt;

        q.state = QuestState::Claimed;
        LOG_INFO("任务 %d 奖励已领取: %s", questId, def->title.c_str());

        AudioManager::Instance().PlaySFX(AudioManager::kSFXQuestReward);

        // 触发回调（Game 层应用奖励到玩家）
        if (OnRewardGranted) {
            OnRewardGranted(questId, def->reward);
        }

        // 解锁依赖此任务的后继任务
        unlockDependentQuests(questId);

        // 返回首个新解锁的任务 ID（若有）
        if (taskIdToUnlockAfter) {
            for (const auto& other : quests_) {
                if (other.state == QuestState::Active) {
                    const QuestDef* otherDef = GetQuestDef(other.id);
                    if (otherDef && otherDef->prerequisiteQuestId == questId) {
                        *taskIdToUnlockAfter = other.id;
                        break;
                    }
                }
            }
        }
        return def->reward;
    }
    return std::nullopt;
}

// ============================================================================
// 存档支持
// ============================================================================
void QuestSystem::Deserialize(const std::vector<QuestInstance>& data) {
    for (const auto& saved : data) {
        for (auto& q : quests_) {
            if (q.id == saved.id) {
                q.state = saved.state;
                q.currentProgress = saved.currentProgress;
                q.timeAccumulator = saved.timeAccumulator;
                break;
            }
        }
    }
    LOG_INFO("任务状态已恢复: %d 个任务", static_cast<int>(data.size()));
}

void QuestSystem::ResetAll() {
    for (auto& q : quests_) {
        q.state = QuestState::Locked;
        q.currentProgress = 0;
        q.timeAccumulator = 0.f;
    }
    LOG_INFO("任务系统已重置");
}

// ============================================================================
// 私有辅助方法
// ============================================================================
void QuestSystem::checkUnlocks(int currentLevel) {
    (void)currentLevel; // 当前任务线仅靠前置任务解锁，无需层数判定
    // 任务1 无前置，初始化时为 Locked，需在游戏开始时主动激活
    // 这里仅在游戏开始时解锁任务1（无前置的任务）
    for (auto& q : quests_) {
        if (q.state != QuestState::Locked) continue;
        const QuestDef* def = GetQuestDef(q.id);
        if (!def) continue;
        if (def->prerequisiteQuestId == 0) {
            q.state = QuestState::Active;
            LOG_INFO("任务 %d 已解锁: %s", q.id, def->title.c_str());
        }
    }
}

void QuestSystem::checkCompletion() {
    for (auto& q : quests_) {
        if (q.state != QuestState::Active) continue;
        if (q.currentProgress >= q.targetProgress) {
            q.state = QuestState::Completed;
            const QuestDef* def = GetQuestDef(q.id);
            LOG_INFO("任务 %d 已完成: %s（待领取奖励）", q.id,
                     def ? def->title.c_str() : "?");
            AudioManager::Instance().PlaySFX(AudioManager::kSFXQuestTip);
        }
    }
}

void QuestSystem::progressQuest(QuestType type, int amount, EnemyType enemyType,
                                ItemQuality quality, EventType evtType) {
    for (auto& q : quests_) {
        if (q.state != QuestState::Active) continue;
        const QuestDef* def = GetQuestDef(q.id);
        if (!def || def->type != type) continue;

        // KillTarget: 需匹配敌人类型
        if (type == QuestType::KillTarget && def->targetEnemy != enemyType) continue;
        // CollectItem: 需匹配品质
        if (type == QuestType::CollectItem && def->targetQuality != quality) continue;
        // TriggerEvent: 需匹配事件类型
        if (type == QuestType::TriggerEvent && def->targetEvent != evtType) continue;

        q.currentProgress += amount;
        if (q.currentProgress > q.targetProgress) {
            q.currentProgress = q.targetProgress;
        }
    }
}

void QuestSystem::unlockDependentQuests(int completedQuestId) {
    for (auto& q : quests_) {
        if (q.state != QuestState::Locked) continue;
        const QuestDef* def = GetQuestDef(q.id);
        if (!def) continue;
        if (def->prerequisiteQuestId == completedQuestId) {
            q.state = QuestState::Active;
            LOG_INFO("任务 %d 已解锁（前置任务 %d 完成）: %s",
                     q.id, completedQuestId, def->title.c_str());
        }
    }
}

} // namespace cu
