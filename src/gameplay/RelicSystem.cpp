// ============================================================================
// RelicSystem —— 圣物系统实现（第十五轮新增）
// ============================================================================

#include "gameplay/RelicSystem.h"

#include <algorithm>
#include <cstdlib>
#include "utils/Logger.h"

namespace cu {

// ---- 圣物静态数据表（与枚举顺序一致，索引 0 = None 占位）----
static const std::array<RelicData, static_cast<size_t>(RelicType::Count)> kRelicTable = {{
    {RelicType::None,           "",    "",                                0,   0,   0},
    {RelicType::WarriorCrest,   "战士之证", "伤害 +15%",                    220, 80,  60},
    {RelicType::GuardianHeart,  "守卫之心", "最大生命 +20%",                 100, 200, 120},
    {RelicType::HunterEye,      "猎手之眼", "暴击率 +10%，暴击伤害 +20%",    180, 120, 220},
    {RelicType::WindBoots,      "疾风之靴", "移速 +15%",                    120, 220, 240},
    {RelicType::ScholarBook,    "学者之书", "经验获取 +30%",                 100, 160, 240},
    {RelicType::GreedyEye,      "贪婪之眼", "金币掉落 +50%",                240, 200, 80},
    {RelicType::VampireFang,    "吸血鬼之牙", "吸血 +5%",                    200, 60,  100},
    {RelicType::Aegis,          "守护之心", "防御 +15，最大生命 +10%",       180, 180, 200},
    // ---- 第十九轮新增：雷霆系圣物（闪电流 build 核心）----
    {RelicType::ThunderHeart,   "雷霆之心", "连锁闪电 +1，伤害 +10%",        255, 220, 80},
    {RelicType::StormEye,       "风暴之眼", "麻痹时间 +50%，暴击率 +5%",     180, 200, 255},
    // ---- 第二十轮新增：极限闪避系圣物（防御反击 build 核心）----
    {RelicType::MoonAmulet,     "月光护符", "极限闪避检测窗口 +50%",         200, 220, 255},
    {RelicType::VengeanceBlade, "复仇之刃", "极限闪避后攻击必暴击",          255, 100, 100},
}};

const RelicData& GetRelicData(RelicType type) {
    static RelicData fallback{};
    auto idx = static_cast<size_t>(type);
    if (idx >= kRelicTable.size()) return fallback;
    return kRelicTable[idx];
}

const char* GetRelicName(RelicType type) {
    return GetRelicData(type).name;
}

// ============================================================================
// RelicSystem 实现
// ============================================================================

RelicSystem::RelicSystem() = default;

void RelicSystem::Initialize() {
    owned_.fill(RelicType::None);
    ownedCount_ = 0;
}

bool RelicSystem::HasRelic(RelicType type) const noexcept {
    if (type == RelicType::None || type >= RelicType::Count) return false;
    for (int i = 0; i < ownedCount_; ++i) {
        if (owned_[i] == type) return true;
    }
    return false;
}

bool RelicSystem::AddRelic(RelicType type) {
    if (type == RelicType::None || type >= RelicType::Count) return false;
    if (HasRelic(type)) {
        LOG_WARN("圣物 %s 已拥有，无法重复添加", GetRelicName(type));
        return false;
    }
    if (IsFull()) {
        LOG_WARN("圣物栏已满 (%d/%d)，无法添加 %s",
                 ownedCount_, kRelicMaxCount, GetRelicName(type));
        return false;
    }
    owned_[ownedCount_] = type;
    ++ownedCount_;
    LOG_INFO("获得圣物: %s (%d/%d)",
             GetRelicName(type), ownedCount_, kRelicMaxCount);
    return true;
}

void RelicSystem::Clear() {
    owned_.fill(RelicType::None);
    ownedCount_ = 0;
}

std::vector<RelicType> RelicSystem::RollUnownedRelics(int count) const {
    std::vector<RelicType> pool;
    pool.reserve(static_cast<size_t>(RelicType::Count) - 1);
    // 从所有可用圣物中筛选未拥有的
    for (int i = 1; i < static_cast<int>(RelicType::Count); ++i) {
        RelicType t = static_cast<RelicType>(i);
        if (!HasRelic(t)) pool.push_back(t);
    }
    // Fisher-Yates 洗牌取前 count 个
    for (int i = static_cast<int>(pool.size()) - 1; i > 0; --i) {
        int j = std::rand() % (i + 1);
        std::swap(pool[i], pool[j]);
    }
    int actual = std::min(count, static_cast<int>(pool.size()));
    pool.resize(actual);
    return pool;
}

void RelicSystem::ApplyToPlayerStats(PlayerStats& stats) const {
    for (int i = 0; i < ownedCount_; ++i) {
        switch (owned_[i]) {
            case RelicType::WarriorCrest:
                stats.damage *= 1.15f;
                break;
            case RelicType::GuardianHeart:
                stats.maxHp *= 1.20f;
                break;
            case RelicType::HunterEye:
                stats.critChance += 0.10f;
                stats.critDamage += 0.20f;
                break;
            case RelicType::WindBoots:
                stats.moveSpeed *= 1.15f;
                break;
            case RelicType::ScholarBook:
                stats.expMultiplier += 0.30f;
                break;
            case RelicType::GreedyEye:
                stats.coinMultiplier += 0.50f;
                break;
            case RelicType::VampireFang:
                stats.lifesteal += 0.05f;
                break;
            case RelicType::Aegis:
                stats.defense += 15.f;
                stats.maxHp *= 1.10f;
                break;
            // ---- 第十九轮新增：雷霆系圣物 ----
            case RelicType::ThunderHeart:
                // 雷霆之心：连锁次数 +1，伤害 +10%
                // 设计意图：与 chainLightning 升级叠加，让玩家可构筑"纯闪电流"
                // 链次数 = 升级(3) + 圣物(1) = 4 次，覆盖更多敌人
                stats.chainLightning += 1;
                stats.damage *= 1.10f;
                break;
            case RelicType::StormEye:
                // 风暴之眼：麻痹时间 +50%，暴击率 +5%
                // 设计意图：强化 Lightning 控制效果（0.6s → 0.9s），
                // 配合暴击率提升让闪电流有爆发伤害潜力
                stats.lightningDurationMul += 0.50f;
                stats.critChance += 0.05f;
                break;
            // ---- 第二十轮新增：极限闪避系圣物 ----
            case RelicType::MoonAmulet:
                // 月光护符：极限闪避检测窗口 +50%
                // 设计意图：降低极限闪避触发难度，让技术流玩家更稳定地触发反击
                // 应用位置：PlayerCombat 闪避检测 kPerfectDodgeRange *= dodgeWindowMul
                // 数值：1.0 → 1.5，检测范围 80px → 120px，更容易捕捉到攻击前摇的敌人
                stats.dodgeWindowMul += 0.50f;
                break;
            case RelicType::VengeanceBlade:
                // 复仇之刃：极限闪避 buff 期间所有攻击必暴击
                // 设计意图：让极限闪避不仅是防御手段，更是爆发窗口
                // 应用位置：CombatSystem::ApplyDamage 中检测
                //   perfectDodgeBuffTimer > 0 && perfectDodgeGuaranteedCrit 时强制暴击
                // 数值：buff 持续 2s 内所有伤害 ×1.5（极限闪避）×1.5（暴击）= 2.25x
                stats.perfectDodgeGuaranteedCrit = true;
                break;
            case RelicType::None:
            case RelicType::Count:
            default:
                break;
        }
    }
}

std::array<uint8_t, kRelicMaxCount> RelicSystem::Serialize() const {
    std::array<uint8_t, kRelicMaxCount> data{};
    for (int i = 0; i < kRelicMaxCount; ++i) {
        data[i] = static_cast<uint8_t>(owned_[i]);
    }
    return data;
}

void RelicSystem::Deserialize(const std::array<uint8_t, kRelicMaxCount>& data) {
    owned_.fill(RelicType::None);
    ownedCount_ = 0;
    for (int i = 0; i < kRelicMaxCount; ++i) {
        RelicType t = static_cast<RelicType>(data[i]);
        if (t == RelicType::None) continue;
        if (t >= RelicType::Count) continue;
        if (HasRelic(t)) continue; // 防御性：存档数据损坏时不重复
        owned_[ownedCount_] = t;
        ++ownedCount_;
    }
    LOG_INFO("读档圣物恢复: %d/%d", ownedCount_, kRelicMaxCount);
}

} // namespace cu
