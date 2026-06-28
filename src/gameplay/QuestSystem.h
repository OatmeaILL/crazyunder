#pragma once

// ============================================================================
// QuestSystem  任务系统
// ----------------------------------------------------------------------------
// 职责：
//   1. 维护剧情任务线（5 个主线任务，按依赖关系解锁）
//   2. 接收游戏事件（击杀/拾取/进入新层/触发事件房/技能变化）推进任务进度
//   3. 检测任务完成，触发奖励（经验/金币/装备/技能点/等级提升）
//   4. 提供任务列表查询接口供 UI 渲染
//   5. 任务5（攒1000金币）的"提交"机制：领取时扣金币，奖励金色装备+等级+5
//
// 任务依赖解锁：
//   - 任务1（打通第一层）：游戏开始即解锁
//   - 任务2-5：任务1完成后才解锁
//   - 通过 QuestDef::prerequisiteQuestId 字段表示前置任务（0=无前置）
//
// 任务线剧情（与地牢世界观匹配）：
//   任务1 "破晓之始"      ：击败第一层 Boss 进入第二层
//   任务2 "古老祭坛"      ：找到并触发 1 个祭坛
//   任务3 "地下奇人"      ：找到并触发 1 个乞丐
//   任务4 "百艺兼修"      ：收集 4 个技能（技能槽 + 技能背包总数）
//   任务5 "财富之力"      ：攒够 1000 金币，提交后获得金色装备 + 等级 +5
//
// 任务5提交机制：
//   - 当玩家金币 >= 1000 时任务进入 Completed 状态
//   - 玩家在任务面板点击"领取奖励"按钮调用 ClaimReward
//   - Game 层在 ClaimReward 回调中扣除 1000 金币并发放金色装备 + 调用 AddLevels(5)
// ============================================================================

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>
#include "gameplay/EnemyAI.h"
#include "gameplay/LootSystem.h"
#include "gameplay/DungeonGenerator.h" // EventType

namespace cu {

// ---- 任务类型 ----
enum class QuestType : uint8_t {
    KillTarget     = 0, // 击杀指定敌人
    CollectItem    = 1, // 拾取指定装备
    ReachLevel     = 2, // 到达指定层数
    TriggerEvent   = 3, // 触发指定事件房事件
    SurviveTime    = 4, // 存活时间
    ClearRooms     = 5, // 清理房间
    CollectSkills  = 6, // 收集 N 个技能（任务4 用）
    AccumulateCoins= 7, // 攒够 N 金币（任务5 用，需提交领取）
};

// ---- 任务状态 ----
enum class QuestState : uint8_t {
    Locked    = 0, // 未解锁（前置任务未完成）
    Active    = 1, // 进行中
    Completed = 2, // 已完成，待领取奖励
    Claimed   = 3, // 已领取奖励
};

// ---- 任务奖励 ----
struct QuestReward {
    int exp = 0;            // 经验奖励
    int coins = 0;          // 金币奖励（正数=获得，负数=扣除，任务5=-1000）
    int skillPoints = 0;    // 技能点奖励
    int addLevels = 0;      // 直接提升等级数（任务5=5）
    ItemQuality itemQuality = ItemQuality::White; // 装备奖励品质（White=无装备奖励）
    int itemCount = 0;      // 装备奖励数量
    bool isRandomItem = false; // 装备是否随机类型（true=随机槽位生成金装）
};

// ---- 任务定义（静态模板）----
struct QuestDef {
    int id = 0;                          // 任务唯一 ID
    QuestType type = QuestType::KillTarget;
    std::string title;                   // 任务标题（中文）
    std::string description;             // 任务描述（中文）
    EnemyType targetEnemy = EnemyType::Melee;       // KillTarget 用
    ItemQuality targetQuality = ItemQuality::White; // CollectItem 用
    EventType targetEvent = EventType::None;        // TriggerEvent 用：具体事件类型
    int targetCount = 1;                 // 目标数量
    int targetLevel = 1;                 // ReachLevel 用：目标层数
    float targetTime = 0.f;              // SurviveTime 用
    int prerequisiteQuestId = 0;         // 前置任务 ID（0=无前置，游戏开始即可解锁）
    QuestReward reward;
};

// ---- 任务实例（运行时状态）----
struct QuestInstance {
    int id = 0;
    QuestState state = QuestState::Locked;
    int currentProgress = 0;  // 当前进度
    int targetProgress = 1;   // 目标进度
    float timeAccumulator = 0.f; // SurviveTime 用
};

// ---- 任务系统 ----
class QuestSystem {
public:
    static constexpr int kMaxActiveQuests = 5;

    QuestSystem();
    ~QuestSystem() = default;

    // 初始化：注册剧情任务线
    void Initialize();

    // 每帧更新：处理 SurviveTime 累计、检测解锁与完成
    // currentCoins: 玩家当前金币（任务5 用，每次变化推进进度）
    // currentSkillCount: 玩家当前技能总数（技能槽+技能背包，任务4 用）
    void Update(float dt, int currentLevel, int currentCoins = -1, int currentSkillCount = -1);

    // ---- 事件上报接口 ----
    void OnEnemyKilled(EnemyType type, bool isChampion);
    void OnItemPickedUp(ItemQuality quality);
    void OnLevelReached(int newLevel);
    // 触发事件房事件（区分乞丐/祭坛/法师/宝箱怪）
    void OnEventTriggered(EventType evtType);
    void OnRoomCleared();

    // 第二十二轮新增：玩家死亡时调用，重置 SurviveTime 任务进度
    // 设计意图：SurviveTime 任务语义为"单次生命存活 N 秒"，玩家死亡后应从 0 重新累计
    void OnPlayerDeath();

    // ---- 查询接口 ----
    [[nodiscard]] const std::vector<QuestInstance>& GetAllQuests() const noexcept {
        return quests_;
    }
    [[nodiscard]] const QuestDef* GetQuestDef(int id) const;
    [[nodiscard]] int GetCompletedUnclaimedCount() const noexcept;
    [[nodiscard]] int GetActiveCount() const noexcept;

    // 领取任务奖励
    // taskIdToUnlockAfter: 出参，返回本次领取后新解锁的任务 ID（无则 0）
    [[nodiscard]] std::optional<QuestReward> ClaimReward(int questId, int* taskIdToUnlockAfter = nullptr);

    // ---- 存档支持 ----
    [[nodiscard]] std::vector<QuestInstance> Serialize() const { return quests_; }
    void Deserialize(const std::vector<QuestInstance>& data);
    void ResetAll();

    // 奖励发放回调（Game 层注册，ClaimReward 时触发，用于应用装备/升级到玩家）
    // 参数：questId, reward
    std::function<void(int, const QuestReward&)> OnRewardGranted;

private:
    std::vector<QuestDef> questDefs_;
    std::vector<QuestInstance> quests_;

    // 检查任务解锁条件（前置任务完成 + 满足 unlockLevel）
    void checkUnlocks(int currentLevel);
    void checkCompletion();
    // 推进指定类型任务进度
    void progressQuest(QuestType type, int amount, EnemyType enemyType = EnemyType::Melee,
                       ItemQuality quality = ItemQuality::White, EventType evtType = EventType::None);
    // 任务完成后，解锁其后续任务（前置 ID == 当前任务 ID 的任务）
    void unlockDependentQuests(int completedQuestId);
};

} // namespace cu
