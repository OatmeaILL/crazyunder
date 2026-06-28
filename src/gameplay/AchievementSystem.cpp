#include "gameplay/AchievementSystem.h"
#include "utils/Logger.h"
#include <chrono>
#include <fstream>
#ifdef _WIN32
#include <direct.h> // _mkdir（Windows）
#else
#include <sys/stat.h> // mkdir（POSIX）
#endif

namespace cu {

// 成就文件魔数 "ACHV"
static constexpr uint32_t kAchievementMagic = 0x41434856;
static constexpr uint8_t kAchievementVersion = 1;

AchievementSystem::AchievementSystem() = default;

// ============================================================================
// Initialize —— 注册预定义成就
// ============================================================================
void AchievementSystem::Initialize() {
    achievementDefs_.clear();
    achievements_.clear();

    // ---- 战斗类成就 ----
    {
        AchievementDef def;
        def.id = 1;
        def.category = AchievementCategory::Combat;
        def.condition = AchievementCondition::TotalKills;
        def.name = "初露锋芒";
        def.description = "累计击杀 100 个敌人";
        def.targetValue = 100;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 2;
        def.category = AchievementCategory::Combat;
        def.condition = AchievementCondition::TotalKills;
        def.name = "杀戮者";
        def.description = "累计击杀 1000 个敌人";
        def.targetValue = 1000;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 3;
        def.category = AchievementCategory::Combat;
        def.condition = AchievementCondition::TotalBossKills;
        def.name = "巨人杀手";
        def.description = "击败 1 个地牢 Boss";
        def.targetValue = 1;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 4;
        def.category = AchievementCategory::Combat;
        def.condition = AchievementCondition::TotalBossKills;
        def.name = "屠魔者";
        def.description = "击败 10 个地牢 Boss";
        def.targetValue = 10;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 5;
        def.category = AchievementCategory::Combat;
        def.condition = AchievementCondition::TotalChampionKills;
        def.name = "精英猎人";
        def.description = "击败 50 个精英强化怪";
        def.targetValue = 50;
        achievementDefs_.push_back(def);
    }

    // ---- 探索类成就 ----
    {
        AchievementDef def;
        def.id = 10;
        def.category = AchievementCategory::Exploration;
        def.condition = AchievementCondition::ReachLevel;
        def.name = "地下探险家";
        def.description = "到达地牢第 5 层";
        def.targetValue = 5;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 11;
        def.category = AchievementCategory::Exploration;
        def.condition = AchievementCondition::ReachLevel;
        def.name = "深渊行者";
        def.description = "到达地牢第 10 层";
        def.targetValue = 10;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 12;
        def.category = AchievementCategory::Exploration;
        def.condition = AchievementCondition::TotalRoomsCleared;
        def.name = "清道夫";
        def.description = "累计清理 100 个房间";
        def.targetValue = 100;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 13;
        def.category = AchievementCategory::Exploration;
        def.condition = AchievementCondition::TotalEvents;
        def.name = "好奇心";
        def.description = "触发 20 次事件房事件";
        def.targetValue = 20;
        achievementDefs_.push_back(def);
    }

    // ---- 收集类成就 ----
    {
        AchievementDef def;
        def.id = 20;
        def.category = AchievementCategory::Collection;
        def.condition = AchievementCondition::TotalCoins;
        def.name = "小有积蓄";
        def.description = "累计获得 1000 金币";
        def.targetValue = 1000;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 21;
        def.category = AchievementCategory::Collection;
        def.condition = AchievementCondition::TotalCoins;
        def.name = "财迷";
        def.description = "累计获得 10000 金币";
        def.targetValue = 10000;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 22;
        def.category = AchievementCategory::Collection;
        def.condition = AchievementCondition::TotalItemsPicked;
        def.name = "装备收藏家";
        def.description = "累计拾取 100 件装备";
        def.targetValue = 100;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 23;
        def.category = AchievementCategory::Collection;
        def.condition = AchievementCondition::LegendaryItems;
        def.name = "传说之物";
        def.description = "拾取 1 件暗金装备";
        def.targetValue = 1;
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 24;
        def.category = AchievementCategory::Collection;
        def.condition = AchievementCondition::SkillPointsEarned;
        def.name = "技能大师";
        def.description = "累计获得 20 个技能点";
        def.targetValue = 20;
        achievementDefs_.push_back(def);
    }

    // ---- 第二十轮新增：极限闪避累计型成就 ----
    {
        AchievementDef def;
        def.id = 25;
        def.category = AchievementCategory::Combat;
        def.condition = AchievementCondition::TotalPerfectDodges;
        def.name = "幻影之舞";
        def.description = "累计触发 100 次极限闪避";
        def.targetValue = 100;
        achievementDefs_.push_back(def);
    }

    // ---- 第二十一轮新增：词缀精英击杀累计型成就 ----
    {
        AchievementDef def;
        def.id = 26;
        def.category = AchievementCategory::Combat;
        def.condition = AchievementCondition::TotalAffixKills;
        def.name = "词缀猎手";
        def.description = "累计击杀 30 个带词缀精英怪";
        def.targetValue = 30;
        achievementDefs_.push_back(def);
    }
    {
        // 满词缀征服：击败 4 词缀全开的"满词缀精英"5 个
        // 满词缀精英仅 Champion 5% 概率触发，稀有度高，故阈值设为 5
        // condition 用 NoDamageBoss 占位（特殊成就模式，仅由 OnAffixEnemyKilled(fullAffix=true) 显式推进，
        // 不依赖 progressByCondition 自动推进，避免被普通词缀击杀误触发）
        AchievementDef def;
        def.id = 27;
        def.category = AchievementCategory::Combat;
        def.condition = AchievementCondition::NoDamageBoss; // 占位（特殊成就不依赖 condition）
        def.name = "满词缀征服";
        def.description = "击败 5 个四词缀全开的满词缀精英";
        def.targetValue = 5;
        def.isHidden = true; // 隐藏成就（玩家需自行发现）
        achievementDefs_.push_back(def);
    }

    // ---- 特殊类成就 ----
    {
        AchievementDef def;
        def.id = 30;
        def.category = AchievementCategory::Special;
        def.condition = AchievementCondition::NoDamageBoss;
        def.name = "完美猎手";
        def.description = "无伤击败一个 Boss";
        def.isHidden = true; // 隐藏成就
        achievementDefs_.push_back(def);
    }
    {
        AchievementDef def;
        def.id = 31;
        def.category = AchievementCategory::Special;
        def.condition = AchievementCondition::SpeedRun;
        def.name = "疾风迅雷";
        def.description = "在 5 分钟内到达第 5 层";
        def.isHidden = true;
        achievementDefs_.push_back(def);
    }
    {
        // 第二十轮新增：极限闪避首次触发成就（特殊类，隐藏）
        AchievementDef def;
        def.id = 40;
        def.category = AchievementCategory::Special;
        def.condition = AchievementCondition::NoDamageBoss; // 占位（特殊成就不依赖 condition）
        def.name = "极限闪避";
        def.description = "首次触发极限闪避反击";
        def.isHidden = true;
        achievementDefs_.push_back(def);
    }

    // ---- 初始化成就状态 ----
    achievements_.reserve(achievementDefs_.size());
    for (const auto& def : achievementDefs_) {
        AchievementState s;
        s.id = def.id;
        s.unlocked = false;
        s.currentValue = 0;
        s.unlockedTimestamp = 0;
        achievements_.push_back(s);
    }

    LOG_INFO("成就系统已初始化: 注册 %d 个成就", static_cast<int>(achievementDefs_.size()));
}

// ============================================================================
// 事件上报接口实现
// ============================================================================
void AchievementSystem::OnEnemyKilled(EnemyType type, bool isChampion) {
    // 累计击杀数（所有类型）
    progressByCondition(AchievementCondition::TotalKills, 1);

    // Boss 击杀数
    if (type == EnemyType::Boss) {
        progressByCondition(AchievementCondition::TotalBossKills, 1);
    }

    // 精英强化怪击杀数
    if (isChampion) {
        progressByCondition(AchievementCondition::TotalChampionKills, 1);
    }
}

void AchievementSystem::OnItemPickedUp(ItemQuality quality) {
    progressByCondition(AchievementCondition::TotalItemsPicked, 1);
    if (quality == ItemQuality::DarkGold) {
        progressByCondition(AchievementCondition::LegendaryItems, 1);
    }
}

void AchievementSystem::OnLevelReached(int newLevel) {
    // ReachLevel 类型：取 max 推进
    for (size_t i = 0; i < achievementDefs_.size(); ++i) {
        if (achievementDefs_[i].condition != AchievementCondition::ReachLevel) continue;
        if (achievements_[i].unlocked) continue;
        if (newLevel > achievements_[i].currentValue) {
            achievements_[i].currentValue = newLevel;
        }
        tryUnlock(static_cast<int>(i));
    }
}

void AchievementSystem::OnEventTriggered() {
    progressByCondition(AchievementCondition::TotalEvents, 1);
}

void AchievementSystem::OnRoomCleared() {
    progressByCondition(AchievementCondition::TotalRoomsCleared, 1);
}

void AchievementSystem::OnCoinsGained(int amount) {
    if (amount <= 0) return;
    progressByCondition(AchievementCondition::TotalCoins, amount);
}

void AchievementSystem::OnSkillPointsGained(int amount) {
    if (amount <= 0) return;
    progressByCondition(AchievementCondition::SkillPointsEarned, amount);
}

void AchievementSystem::OnPerfectDodge(int amount) {
    if (amount <= 0) return;
    progressByCondition(AchievementCondition::TotalPerfectDodges, amount);
}

void AchievementSystem::OnAffixEnemyKilled(int amount, bool fullAffix) {
    if (amount <= 0) return;
    // 推进 TotalAffixKills 条件（词缀猎手成就 id=26 + 满词缀征服成就 id=27 共用此条件）
    progressByCondition(AchievementCondition::TotalAffixKills, amount);

    // 满词缀精英（4 词缀全开）：仅当 fullAffix=true 时推进满词缀征服成就
    // 通过显式推进 id=27（与 TotalAffixKills 条件共用，但仅 fullAffix 路径推进）
    if (fullAffix) {
        for (size_t i = 0; i < achievementDefs_.size(); ++i) {
            if (achievementDefs_[i].id != 27) continue;
            if (achievements_[i].unlocked) continue;
            achievements_[i].currentValue += amount;
            tryUnlock(static_cast<int>(i));
        }
    }
}

bool AchievementSystem::UnlockSpecial(int achievementId) {
    for (size_t i = 0; i < achievementDefs_.size(); ++i) {
        if (achievementDefs_[i].id != achievementId) continue;
        if (achievements_[i].unlocked) return false; // 已解锁
        // 直接解锁（特殊成就不累计进度，直接标记）
        achievements_[i].currentValue = achievementDefs_[i].targetValue;
        return tryUnlock(static_cast<int>(i));
    }
    LOG_WARN("UnlockSpecial: 未找到成就 ID %d", achievementId);
    return false;
}

// ============================================================================
// 查询接口
// ============================================================================
const AchievementDef* AchievementSystem::GetAchievementDef(int id) const {
    for (const auto& def : achievementDefs_) {
        if (def.id == id) return &def;
    }
    return nullptr;
}

int AchievementSystem::GetUnlockedCount() const noexcept {
    int count = 0;
    for (const auto& a : achievements_) {
        if (a.unlocked) ++count;
    }
    return count;
}

int AchievementSystem::GetUnlockedPercentage() const noexcept {
    if (achievementDefs_.empty()) return 0;
    return (GetUnlockedCount() * 100) / static_cast<int>(achievementDefs_.size());
}

bool AchievementSystem::IsUnlocked(int id) const noexcept {
    for (const auto& a : achievements_) {
        if (a.id == id) return a.unlocked;
    }
    return false;
}

// ============================================================================
// 持久化
// ============================================================================
std::string AchievementSystem::getFilePath() {
    return "saves/achievements.dat";
}

bool AchievementSystem::LoadFromFile() {
    std::ifstream ifs(getFilePath(), std::ios::binary);
    if (!ifs.is_open()) {
        LOG_INFO("成就文件不存在，将使用初始状态: %s", getFilePath().c_str());
        return false;
    }

    // 强制 C locale，避免 locale 污染
    ifs.imbue(std::locale::classic());

    uint32_t magic = 0;
    uint8_t version = 0;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (magic != kAchievementMagic || version != kAchievementVersion) {
        LOG_WARN("成就文件格式错误: magic=0x%X version=%d", magic, version);
        return false;
    }

    int32_t count = 0;
    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (count <= 0 || count > 1000) { // 合理性检查
        LOG_WARN("成就文件 count 异常: %d", count);
        return false;
    }

    int loaded = 0;
    for (int i = 0; i < count; ++i) {
        int32_t id = 0;
        uint8_t unlocked = 0;
        int64_t currentValue = 0;
        int64_t unlockedTs = 0;
        ifs.read(reinterpret_cast<char*>(&id), sizeof(id));
        ifs.read(reinterpret_cast<char*>(&unlocked), sizeof(unlocked));
        ifs.read(reinterpret_cast<char*>(&currentValue), sizeof(currentValue));
        ifs.read(reinterpret_cast<char*>(&unlockedTs), sizeof(unlockedTs));

        // 匹配到现有成就定义
        for (size_t j = 0; j < achievementDefs_.size(); ++j) {
            if (achievementDefs_[j].id == id) {
                achievements_[j].unlocked = (unlocked != 0);
                achievements_[j].currentValue = currentValue;
                achievements_[j].unlockedTimestamp = unlockedTs;
                ++loaded;
                break;
            }
        }
    }

    LOG_INFO("成就状态已加载: %d/%d 项", loaded, static_cast<int>(achievements_.size()));
    return true;
}

bool AchievementSystem::SaveToFile() const {
    // 确保 saves 目录存在
    // 跨平台 mkdir：Windows 用 _mkdir，POSIX 用 mkdir
#ifdef _WIN32
    (void)_mkdir("saves");
#else
    (void)mkdir("saves", 0755);
#endif

    std::ofstream ofs(getFilePath(), std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        LOG_ERROR("无法写入成就文件: %s", getFilePath().c_str());
        return false;
    }
    ofs.imbue(std::locale::classic());

    // 文件头
    ofs.write(reinterpret_cast<const char*>(&kAchievementMagic), sizeof(kAchievementMagic));
    ofs.write(reinterpret_cast<const char*>(&kAchievementVersion), sizeof(kAchievementVersion));

    // 成就数
    int32_t count = static_cast<int32_t>(achievements_.size());
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));

    // 逐项写入
    for (const auto& a : achievements_) {
        int32_t id = a.id;
        uint8_t unlocked = a.unlocked ? 1 : 0;
        int64_t currentValue = a.currentValue;
        int64_t unlockedTs = a.unlockedTimestamp;
        ofs.write(reinterpret_cast<const char*>(&id), sizeof(id));
        ofs.write(reinterpret_cast<const char*>(&unlocked), sizeof(unlocked));
        ofs.write(reinterpret_cast<const char*>(&currentValue), sizeof(currentValue));
        ofs.write(reinterpret_cast<const char*>(&unlockedTs), sizeof(unlockedTs));
    }

    LOG_INFO("成就状态已保存: %d 项", count);
    return true;
}

void AchievementSystem::ResetAll() {
    for (auto& a : achievements_) {
        a.unlocked = false;
        a.currentValue = 0;
        a.unlockedTimestamp = 0;
    }
    LOG_INFO("成就系统已重置");
}

// ============================================================================
// 私有辅助方法
// ============================================================================
void AchievementSystem::progressByCondition(AchievementCondition cond, int64_t amount) {
    for (size_t i = 0; i < achievementDefs_.size(); ++i) {
        if (achievementDefs_[i].condition != cond) continue;
        if (achievements_[i].unlocked) continue; // 已解锁不再累计
        achievements_[i].currentValue += amount;
        tryUnlock(static_cast<int>(i));
    }
}

bool AchievementSystem::tryUnlock(int index) {
    if (index < 0 || index >= static_cast<int>(achievements_.size())) return false;
    if (achievements_[index].unlocked) return false;

    const auto& def = achievementDefs_[index];
    auto& state = achievements_[index];

    if (state.currentValue >= def.targetValue) {
        state.unlocked = true;
        state.unlockedTimestamp = getCurrentTimestamp();
        LOG_INFO("成就解锁: %s (%s)", def.name.c_str(), def.description.c_str());

        // 触发回调（Game 层显示 Toast 通知）
        if (OnUnlocked) {
            OnUnlocked(def.id, def);
        }
        return true;
    }
    return false;
}

int64_t AchievementSystem::getCurrentTimestamp() noexcept {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace cu
