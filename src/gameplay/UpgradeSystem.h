#pragma once

// ============================================================================
// UpgradeSystem  经验值与 3 选 1 升级系统（Phase 7）
// ----------------------------------------------------------------------------
// 职责：
//   1. AddExp：增加经验，检测升级（exp >= expToNext）
//   2. RollUpgrades：随机抽 3 个未满级升级选项
//   3. ApplyUpgrade：应用升级到 PlayerStats
//
// 升级公式：
//   expToNext = 100 * level * 1.5
//   level 1 -> 150 exp
//   level 2 -> 300 exp
//   level 3 -> 450 exp
//   ...
//
// 升级类型（12 种）：
//   DamageUp          +5 伤害       maxLevel=10
//   AttackSpeedUp     +15% 攻速     maxLevel=5
//   MoveSpeedUp       +10% 移速     maxLevel=5
//   MaxHpUp           +20 最大生命  maxLevel=10
//   CritRateUp        +5% 暴击率    maxLevel=8
//   CritDamageUp      +30% 暴击伤害 maxLevel=5
//   LifestealUp       +3% 吸血      maxLevel=5
//   ProjectileSplit   子弹分裂+1    maxLevel=3
//   ProjectilePierce  子弹穿透+1    maxLevel=3
//   ChainLightning    连锁闪电+1    maxLevel=3
//   AoeCooldownReduce AOE 冷却-1s   maxLevel=5
//   DashCooldownReduce 闪避冷却-0.5s maxLevel=4
// ============================================================================

#include <array>
#include <cstdint>
#include <functional>
#include "gameplay/Player.h"
#include "gameplay/SkillSystem.h"
#include "gameplay/PlayerClassTypes.h"

namespace cu {

// ---- 升级类型枚举 ----
enum class UpgradeType : uint8_t {
    DamageUp          = 0,  // +5 伤害
    AttackSpeedUp     = 1,  // +15% 攻速
    MoveSpeedUp       = 2,  // +10% 移速
    MaxHpUp           = 3,  // +20 最大生命
    CritRateUp        = 4,  // +5% 暴击率
    CritDamageUp      = 5,  // +30% 暴击伤害
    LifestealUp       = 6,  // +3% 吸血
    ProjectileSplit   = 7,  // 子弹分裂+1
    ProjectilePierce  = 8,  // 子弹穿透+1
    ChainLightning    = 9,  // 连锁闪电+1
    AoeCooldownReduce = 10, // AOE 冷却-1s
    DashCooldownReduce= 11, // 闪避冷却-0.5s
    // 技能升级（低概率出现，maxLevel=1 表示只能获得一次）
    SkillGroundSlam   = 12, // 震地波
    SkillLeechStrike  = 13, // 吸血打击
    SkillBerserk      = 14, // 狂暴
    SkillGravityWell  = 15, // 引力井
    SkillSpikeGround  = 16, // 地刺
    Count             = 17  // 升级类型总数（哨兵）
};

// ---- 升级选项结构体 ----
struct UpgradeOption {
    UpgradeType type = UpgradeType::DamageUp;
    std::string name;
    std::string description;
    int currentLevel = 0;
    int maxLevel = 1;
};

// ============================================================================
// UpgradeSystem  升级系统
// ============================================================================
class UpgradeSystem {
public:
    UpgradeSystem();
    ~UpgradeSystem() = default;

    // 初始化
    void Initialize();

    // 从存档数据恢复（读档用，直接覆盖内部状态）
    void LoadFromData(int level, int exp, int expToNext, int skillPoints,
                      const std::array<int, static_cast<size_t>(UpgradeType::Count)>& upgradeLevels);

    // 增加经验值，检测升级
    // amount: 获得的经验值
    // 返回 true 表示触发了升级（IsUpgradePending 将为 true）
    bool AddExp(int amount);

    // 是否有待处理的升级选择（技能点 > 0）
    [[nodiscard]] bool IsUpgradePending() const noexcept { return skillPoints_ > 0; }

    // 获取未使用的技能点数
    [[nodiscard]] int GetSkillPoints() const noexcept { return skillPoints_; }

    // 随机抽 3 个未满级升级选项
    // cls: 玩家职业，用于过滤不适用升级（如剑士不会随机到远程弹幕升级）
    // 若可用升级不足 3 个，则返回的数组中部分选项的 type 为 Count（无效）
    [[nodiscard]] std::array<UpgradeOption, 3> RollUpgrades(PlayerClass cls = PlayerClass::Mage);

    // 应用升级到 PlayerStats
    // type: 选择的升级类型
    // 应用后消耗 1 个技能点；若仍有剩余技能点，IsUpgradePending 仍为 true
    void ApplyUpgrade(UpgradeType type);

    // 获取经验进度（0~1）
    [[nodiscard]] float GetExpProgress() const noexcept;

    // 获取当前等级
    [[nodiscard]] int GetLevel() const noexcept { return level_; }

    // 获取当前经验
    [[nodiscard]] int GetExp() const noexcept { return exp_; }

    // 获取升级所需经验
    [[nodiscard]] int GetExpToNext() const noexcept { return expToNext_; }

    // 获取指定升级的当前等级
    [[nodiscard]] int GetUpgradeLevel(UpgradeType type) const noexcept;

    // 获取指定升级的最大等级
    [[nodiscard]] static int GetMaxLevel(UpgradeType type) noexcept;

    // 获取升级名称
    [[nodiscard]] static const char* GetUpgradeName(UpgradeType type) noexcept;

    // 获取升级描述
    [[nodiscard]] static std::string GetUpgradeDescription(UpgradeType type) noexcept;

    // 直接增加等级（任务奖励用，不触发 OnLevelUp 回调避免重复统计技能点）
    // levels: 增加的等级数（>0）
    // 每级会同步 +1 技能点，并重算 expToNext_
    void AddLevels(int levels);

    // 升级回调（每次升级时触发，参数：新等级）
    // 用于任务/成就系统统计累计获得的技能点
    std::function<void(int newLevel)> OnLevelUp;

private:
    int level_ = 1;          // 当前等级
    int exp_ = 0;            // 当前经验
    int expToNext_ = 150;    // 升级所需经验（= 100 * level * 1.5）
    int skillPoints_ = 0;    // 未使用的技能点数（每次升级 +1，玩家按 J 主动开启选择界面）

    // 各升级的当前等级（索引 = UpgradeType 枚举值）
    std::array<int, static_cast<size_t>(UpgradeType::Count)> upgradeLevels_{};

    // 计算升级所需经验
    [[nodiscard]] int calculateExpToNext(int level) const noexcept;
};

} // namespace cu