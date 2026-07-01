#include "gameplay/UpgradeSystem.h"
#include "utils/Logger.h"
#include <cstdlib>
#include <algorithm>
#include <cmath>

namespace cu {

// ============================================================================
// UpgradeSystem 构造与初始化
// ============================================================================
UpgradeSystem::UpgradeSystem() {
    Initialize();
}

void UpgradeSystem::Initialize() {
    level_ = 1;
    exp_ = 0;
    expToNext_ = calculateExpToNext(level_);
    skillPoints_ = 0;
    upgradeLevels_.fill(0);
    LOG_INFO("UpgradeSystem 已初始化: level=1, expToNext=%d", expToNext_);
}

void UpgradeSystem::LoadFromData(int level, int exp, int expToNext, int skillPoints,
                                  const std::array<int, static_cast<size_t>(UpgradeType::Count)>& upgradeLevels) {
    level_ = (level > 0) ? level : 1;
    exp_ = (exp >= 0) ? exp : 0;
    expToNext_ = (expToNext > 0) ? expToNext : calculateExpToNext(level_);
    skillPoints_ = (skillPoints >= 0) ? skillPoints : 0;
    upgradeLevels_ = upgradeLevels;
    LOG_INFO("UpgradeSystem 已从存档恢复: level=%d, exp=%d, skillPoints=%d",
             level_, exp_, skillPoints_);
}

// ============================================================================
// 升级公式：expToNext = 100 * level * 1.5
// ============================================================================
int UpgradeSystem::calculateExpToNext(int level) const noexcept {
    return static_cast<int>(100.f * level * 1.5f);
}

// ============================================================================
// AddExp  增加经验，检测升级
// ----------------------------------------------------------------------------
// 经验达到阈值后升级，重置经验并设置 upgradePending_ 标志
// 支持连续升级（一次获得大量经验跨多级）
// ============================================================================
bool AddExp(UpgradeSystem& sys, int amount); // forward

bool UpgradeSystem::AddExp(int amount) {
    exp_ += amount;
    bool leveledUp = false;
    while (exp_ >= expToNext_) {
        exp_ -= expToNext_;
        ++level_;
        expToNext_ = calculateExpToNext(level_);
        ++skillPoints_;  // 每次升级累积 1 个技能点（玩家按 J 主动开启选择界面）
        leveledUp = true;
        LOG_INFO("升级! level=%d, expToNext=%d, skillPoints=%d", level_, expToNext_, skillPoints_);
        // 触发升级回调（任务/成就系统统计累计技能点）
        if (OnLevelUp) {
            OnLevelUp(level_);
        }
    }
    return leveledUp;
}

// ============================================================================
// GetMaxLevel  获取指定升级的最大等级
// ============================================================================
int UpgradeSystem::GetMaxLevel(UpgradeType type) noexcept {
    switch (type) {
        case UpgradeType::DamageUp:          return 10;
        case UpgradeType::AttackSpeedUp:     return 5;
        case UpgradeType::MoveSpeedUp:       return 5;
        case UpgradeType::MaxHpUp:           return 10;
        case UpgradeType::CritRateUp:        return 8;
        case UpgradeType::CritDamageUp:      return 5;
        case UpgradeType::LifestealUp:       return 5;
        case UpgradeType::ProjectileSplit:   return 3;
        case UpgradeType::ProjectilePierce:  return 3;
        case UpgradeType::ChainLightning:    return 3;
        case UpgradeType::AoeCooldownReduce: return 5;
        case UpgradeType::DashCooldownReduce:return 4;
        // 技能升级最高 3 级（重复获取升级）
        case UpgradeType::SkillGroundSlam:
        case UpgradeType::SkillLeechStrike:
        case UpgradeType::SkillBerserk:
        case UpgradeType::SkillGravityWell:
        case UpgradeType::SkillSpikeGround:
            return kSkillMaxLevel;
        default: return 1;
    }
}

// ============================================================================
// GetUpgradeName  获取升级名称
// ============================================================================
const char* UpgradeSystem::GetUpgradeName(UpgradeType type) noexcept {
    switch (type) {
        case UpgradeType::DamageUp:          return "伤害提升";
        case UpgradeType::AttackSpeedUp:     return "攻速提升";
        case UpgradeType::MoveSpeedUp:       return "移速提升";
        case UpgradeType::MaxHpUp:           return "生命提升";
        case UpgradeType::CritRateUp:        return "暴击率提升";
        case UpgradeType::CritDamageUp:      return "暴击伤害提升";
        case UpgradeType::LifestealUp:       return "吸血提升";
        case UpgradeType::ProjectileSplit:   return "子弹分裂";
        case UpgradeType::ProjectilePierce:  return "子弹穿透";
        case UpgradeType::ChainLightning:    return "连锁闪电";
        case UpgradeType::AoeCooldownReduce: return "AOE 冷却缩减";
        case UpgradeType::DashCooldownReduce:return "闪避冷却缩减";
        case UpgradeType::SkillGroundSlam:   return "震地波";
        case UpgradeType::SkillLeechStrike:  return "吸血打击";
        case UpgradeType::SkillBerserk:      return "狂暴";
        case UpgradeType::SkillGravityWell:  return "引力井";
        case UpgradeType::SkillSpikeGround:  return "地刺";
        default: return "?";
    }
}

// ============================================================================
// GetUpgradeDescription  获取升级描述
// ============================================================================
std::string UpgradeSystem::GetUpgradeDescription(UpgradeType type) noexcept {
    switch (type) {
        case UpgradeType::DamageUp:          return "+5 伤害";
        case UpgradeType::AttackSpeedUp:     return "+15% 攻击速度";
        case UpgradeType::MoveSpeedUp:       return "+10% 移动速度";
        case UpgradeType::MaxHpUp:           return "+20 最大生命";
        case UpgradeType::CritRateUp:        return "+5% 暴击率";
        case UpgradeType::CritDamageUp:      return "+30% 暴击伤害";
        case UpgradeType::LifestealUp:       return "+3% 吸血";
        case UpgradeType::ProjectileSplit:   return "+1 子弹分裂";
        case UpgradeType::ProjectilePierce:  return "+1 子弹穿透";
        case UpgradeType::ChainLightning:    return "+1 连锁闪电";
        case UpgradeType::AoeCooldownReduce: return "-1s AOE 冷却";
        case UpgradeType::DashCooldownReduce:return "-0.5s 闪避冷却";
        case UpgradeType::SkillGroundSlam:   return "范围伤害+击退+破甲 CD7s";
        case UpgradeType::SkillLeechStrike:  return "下次攻击1.5x伤害+30%吸血 CD11s";
        case UpgradeType::SkillBerserk:      return "+50%攻击+30%移速 受伤+20% 持续6s CD20s";
        case UpgradeType::SkillGravityWell:  return "拉扯周围敌人4秒 CD15s";
        case UpgradeType::SkillSpikeGround:  return "地面伤害区域5秒 减速 CD12s";
        default: return "?";
    }
}

// ============================================================================
// GetUpgradeLevel  获取指定升级的当前等级
// ============================================================================
int UpgradeSystem::GetUpgradeLevel(UpgradeType type) const noexcept {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(UpgradeType::Count)) return 0;
    return upgradeLevels_[idx];
}

// ============================================================================
// AddLevels  直接增加等级（任务奖励用）
// ----------------------------------------------------------------------------
// 与 AddExp 升级不同：
//   - 不消耗经验（直接提升 level_）
//   - 不触发 OnLevelUp 回调（任务奖励由 Game 层自行处理技能点统计）
//   - 但仍按"每级 +1 技能点"规则累加 skillPoints_
//   - 重算 expToNext_ = 100 * level * 1.5
// ============================================================================
void UpgradeSystem::AddLevels(int levels) {
    if (levels <= 0) return;
    level_ += levels;
    skillPoints_ += levels; // 每级 +1 技能点
    expToNext_ = calculateExpToNext(level_);
    // 经验归零避免溢出
    exp_ = 0;
    LOG_INFO("任务奖励升级: +%d 级 (当前等级=%d, 技能点=%d)", levels, level_, skillPoints_);
}

// ============================================================================
// GetExpProgress  获取经验进度（0~1）
// ============================================================================
float UpgradeSystem::GetExpProgress() const noexcept {
    if (expToNext_ <= 0) return 0.f;
    return static_cast<float>(exp_) / static_cast<float>(expToNext_);
}

// ============================================================================
// RollUpgrades  随机抽 3 个未满级升级选项
// ----------------------------------------------------------------------------
// 算法：
//   1. 收集所有未满级的升级类型（按职业过滤不适用项）
//   2. 随机打乱
//   3. 取前 3 个
//   4. 不足 3 个则用 Count 填充
//
// 职业过滤规则：
//   剑士 (Warrior)：排除 ProjectileSplit / ProjectilePierce / ChainLightning
//                   （这些仅对远程弹幕生效，剑士近战无法受益）
//   法师 (Mage)   ：不排除任何升级（所有升级均对法师有效）
// ============================================================================
std::array<UpgradeOption, 3> UpgradeSystem::RollUpgrades(PlayerClass cls) {
    std::array<UpgradeOption, 3> result;
    // 初始化为无效
    for (auto& opt : result) {
        opt.type = UpgradeType::Count;
    }

    // 收集未满级升级（按职业过滤）
    std::vector<UpgradeType> available;
    for (int i = 0; i < static_cast<int>(UpgradeType::Count); ++i) {
        UpgradeType t = static_cast<UpgradeType>(i);
        if (GetUpgradeLevel(t) >= GetMaxLevel(t)) continue;

        // 剑士排除远程弹幕专属升级
        if (cls == PlayerClass::Warrior) {
            if (t == UpgradeType::ProjectileSplit ||
                t == UpgradeType::ProjectilePierce ||
                t == UpgradeType::ChainLightning) {
                continue;
            }
        }

        available.push_back(t);
    }

    if (available.empty()) {
        return result;
    }

    // 随机打乱（Fisher-Yates）
    for (int i = static_cast<int>(available.size()) - 1; i > 0; --i) {
        int j = std::rand() % (i + 1);
        std::swap(available[i], available[j]);
    }

    // 取前 3 个
    int count = std::min(3, static_cast<int>(available.size()));
    for (int i = 0; i < count; ++i) {
        result[i].type = available[i];
        result[i].name = GetUpgradeName(available[i]);
        result[i].description = GetUpgradeDescription(available[i]);
        result[i].currentLevel = GetUpgradeLevel(available[i]);
        result[i].maxLevel = GetMaxLevel(available[i]);
    }

    // 低概率（20%）替换最后一个选项为随机技能（未满级即可，支持重复升级）
    if (std::rand() % 100 < 20) {
        // 收集未满级的技能（玩家未拥有或已拥有但等级<3）
        static const UpgradeType skillUpgrades[] = {
            UpgradeType::SkillGroundSlam,
            UpgradeType::SkillLeechStrike,
            UpgradeType::SkillBerserk,
            UpgradeType::SkillGravityWell,
            UpgradeType::SkillSpikeGround
        };
        std::vector<UpgradeType> availSkills;
        for (auto st : skillUpgrades) {
            if (GetUpgradeLevel(st) < GetMaxLevel(st)) {
                availSkills.push_back(st);
            }
        }
        if (!availSkills.empty()) {
            UpgradeType chosen = availSkills[std::rand() % availSkills.size()];
            int replaceIdx = count - 1; // 替换最后一个
            if (replaceIdx >= 0) {
                result[replaceIdx].type = chosen;
                result[replaceIdx].name = GetUpgradeName(chosen);
                result[replaceIdx].description = GetUpgradeDescription(chosen);
                result[replaceIdx].currentLevel = GetUpgradeLevel(chosen);
                result[replaceIdx].maxLevel = GetMaxLevel(chosen);
            }
        }
    }

    return result;
}

// ============================================================================
// ApplyUpgrade  应用升级到 PlayerStats
// ----------------------------------------------------------------------------
// 注意：此函数仅修改 PlayerStats 的基础属性字段
// 装备词缀加成由 InventorySystem.ApplyToPlayerStats 单独处理
// 调用顺序：先应用升级基础属性，再应用装备词缀加成
// ============================================================================
void UpgradeSystem::ApplyUpgrade(UpgradeType type) {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(UpgradeType::Count)) return;

    // 检查是否已满级
    if (upgradeLevels_[idx] >= GetMaxLevel(type)) {
        LOG_WARN("升级 %s 已满级", GetUpgradeName(type));
        return;
    }

    ++upgradeLevels_[idx];
    // 消耗 1 个技能点（若仍有剩余，IsUpgradePending 仍为 true，玩家可继续按 J 选择）
    if (skillPoints_ > 0) {
        --skillPoints_;
    }

    LOG_INFO("应用升级: %s (Lv.%d/%d), 剩余技能点=%d",
             GetUpgradeName(type),
             upgradeLevels_[idx],
             GetMaxLevel(type),
             skillPoints_);

    // 实际属性修改由 Game 层在调用后重新计算 PlayerStats
    // 这里只记录等级，属性计算在 Game::recomputePlayerStats 中完成
}

} // namespace cu