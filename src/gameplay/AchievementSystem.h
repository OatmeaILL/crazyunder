#pragma once

// ============================================================================
// AchievementSystem  成就系统（框架）
// ----------------------------------------------------------------------------
// 职责：
//   1. 维护全局成就表（成就定义 + 解锁状态）
//   2. 接收游戏事件（击杀/拾取/层数/特殊行为等），检测成就解锁条件
//   3. 成就解锁时触发通知（OnUnlocked 回调，UI 层显示 Toast）
//   4. 提供"已解锁/总数"统计接口供 UI 显示
//
// 与任务系统（QuestSystem）的区别：
//   - 任务：有期限、可领取奖励、状态机（Locked->Active->Completed->Claimed）
//   - 成就：永久解锁、无奖励（或纯装饰性奖励）、二元状态（未解锁/已解锁）
//   - 成就跟踪的是"累计型"长期目标，如"累计击杀 1000 个敌人"
//
// 成就分类：
//   Combat    —— 战斗类（击杀数/Boss/连击等）
//   Exploration —— 探索类（层数/房间数/事件数等）
//   Collection —— 收集类（装备/金币/技能等）
//   Special   —— 特殊类（无伤通关/限时挑战/隐藏成就等）
//
// 框架说明：
//   本文件仅搭建框架，核心解锁检测逻辑在 AchievementSystem.cpp 中实现。
//   成就数据持久化到 achievements.dat（独立于存档，跨存档共享）。
// ============================================================================

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include "gameplay/EnemyAI.h"
#include "gameplay/LootSystem.h"

namespace cu {

// ---- 成就分类 ----
enum class AchievementCategory : uint8_t {
    Combat      = 0, // 战斗类
    Exploration = 1, // 探索类
    Collection  = 2, // 收集类
    Special     = 3, // 特殊类
};

// ---- 成就解锁条件类型 ----
enum class AchievementCondition : uint8_t {
    TotalKills        = 0,  // 累计击杀数 >= N
    TotalBossKills    = 1,  // 累计 Boss 击杀数 >= N
    TotalChampionKills = 2, // 累计精英强化怪击杀数 >= N
    ReachLevel        = 3,  // 到达第 N 层
    TotalRoomsCleared = 4,  // 累计清理房间数 >= N
    TotalCoins        = 5,  // 累计获得金币 >= N
    TotalItemsPicked  = 6,  // 累计拾取装备数 >= N
    LegendaryItems    = 7,  // 拾取暗金装备 >= N
    TotalEvents       = 8,  // 累计触发事件房事件 >= N
    SkillPointsEarned = 9,  // 累计获得技能点 >= N
    // 特殊成就（一次性触发，由 Game 层显式调用 UnlockSpecial）
    NoDamageBoss      = 10, // 无伤击败 Boss（特殊判定）
    SpeedRun          = 11, // 限时通关（特殊判定）
    // ---- 第二十轮新增：极限闪避成就 ----
    TotalPerfectDodges = 12, // 累计极限闪避次数 >= N
    // ---- 第二十一轮新增：词缀精英击杀成就 ----
    TotalAffixKills  = 13, // 累计击杀带词缀精英数 >= N
};

// ---- 成就定义 ----
struct AchievementDef {
    int id = 0;                                   // 成就唯一 ID
    AchievementCategory category = AchievementCategory::Combat;
    AchievementCondition condition = AchievementCondition::TotalKills;
    std::string name;                             // 成就名称（中文）
    std::string description;                      // 成就描述（中文）
    int64_t targetValue = 0;                      // 解锁阈值（如 1000 击杀）
    bool isHidden = false;                        // 是否为隐藏成就（未解锁时 UI 显示???）
};

// ---- 成就运行时状态 ----
struct AchievementState {
    int id = 0;
    bool unlocked = false;        // 是否已解锁
    int64_t currentValue = 0;     // 当前进度（未解锁时记录进度，便于 UI 显示百分比）
    int64_t unlockedTimestamp = 0; // 解锁时间戳（Unix 秒，0=未解锁）
};

// ---- 成就系统 ----
class AchievementSystem {
public:
    AchievementSystem();
    ~AchievementSystem() = default;

    // 初始化：注册所有预定义成就
    void Initialize();

    // 每帧更新：框架阶段无每帧逻辑，保留接口供后续扩展（如限时成就检测）
    void Update(float dt) { (void)dt; }

    // ---- 事件上报接口（由 Game 层调用）----
    // 与 QuestSystem 类似，但成就累计的是全局总量（跨存档）

    // 敌人被击杀
    void OnEnemyKilled(EnemyType type, bool isChampion);

    // 玩家拾取装备
    void OnItemPickedUp(ItemQuality quality);

    // 玩家进入新层
    void OnLevelReached(int newLevel);

    // 玩家触发事件房事件
    void OnEventTriggered();

    // 玩家清理一个房间
    void OnRoomCleared();

    // 玩家获得金币
    void OnCoinsGained(int amount);

    // 玩家获得技能点（升级或任务奖励）
    void OnSkillPointsGained(int amount);

    // ---- 第二十轮新增：极限闪避事件上报 ----
    // amount: 本次触发的极限闪避次数（通常为 1）
    // 内部推进 TotalPerfectDodges 条件的进度
    void OnPerfectDodge(int amount);

    // ---- 第二十一轮新增：词缀精英击杀事件上报 ----
    // amount: 本次击杀的词缀精英数（通常为 1）
    // fullAffix: 是否为 4 词缀全开的"满词缀精英"（用于满词缀征服成就）
    // 内部推进 TotalAffixKills 条件的进度；fullAffix=true 时额外推进满词缀成就
    void OnAffixEnemyKilled(int amount, bool fullAffix);

    // 特殊成就显式解锁（由 Game 层在特殊事件发生时调用）
    // achievementId: 成就 ID
    // 返回 true 表示首次解锁
    bool UnlockSpecial(int achievementId);

    // ---- 查询接口 ----

    // 获取所有成就状态（含未解锁）
    [[nodiscard]] const std::vector<AchievementState>& GetAllAchievements() const noexcept {
        return achievements_;
    }

    // 获取成就定义
    [[nodiscard]] const AchievementDef* GetAchievementDef(int id) const;

    // 获取已解锁成就数
    [[nodiscard]] int GetUnlockedCount() const noexcept;

    // 获取总成就数
    [[nodiscard]] int GetTotalCount() const noexcept {
        return static_cast<int>(achievementDefs_.size());
    }

    // 获取解锁百分比（0-100）
    [[nodiscard]] int GetUnlockedPercentage() const noexcept;

    // 检查指定成就是否已解锁
    [[nodiscard]] bool IsUnlocked(int id) const noexcept;

    // ---- 持久化（独立于存档，跨存档共享）----

    // 从 achievements.dat 加载成就状态
    // 返回 true 表示加载成功（文件存在且格式正确）
    [[nodiscard]] bool LoadFromFile();

    // 保存成就状态到 achievements.dat
    // 返回 true 表示保存成功
    [[nodiscard]] bool SaveToFile() const;

    // 重置所有成就（调试用，正常游戏不应调用）
    void ResetAll();

    // 成就解锁回调（Game 层注册，触发时显示 Toast 通知）
    // 参数：achievementId, def
    std::function<void(int, const AchievementDef&)> OnUnlocked;

private:
    // 成就定义表（静态）
    std::vector<AchievementDef> achievementDefs_;
    // 运行时状态（与定义一一对应，按 id 索引）
    std::vector<AchievementState> achievements_;

    // 文件路径：saves/achievements.dat
    [[nodiscard]] static std::string getFilePath();

    // 根据条件类型推进所有相关成就的进度
    void progressByCondition(AchievementCondition cond, int64_t amount);

    // 检查指定成就是否达成解锁条件，达成则解锁
    // 返回 true 表示本次调用触了解锁
    bool tryUnlock(int index);

    // 获取当前 Unix 时间戳（秒）
    [[nodiscard]] static int64_t getCurrentTimestamp() noexcept;
};

} // namespace cu
