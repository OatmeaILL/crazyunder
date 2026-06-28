#include "gameplay/SoulMemorySystem.h"
#include "utils/Logger.h"
#include <fstream>
#ifdef _WIN32
#include <direct.h> // _mkdir（Windows）
#else
#include <sys/stat.h> // mkdir（POSIX）
#endif

namespace cu {

// 灵魂之忆文件魔数 "SOUL"
static constexpr uint32_t kSoulMemoryMagic = 0x534F554C;
static constexpr uint8_t kSoulMemoryVersion = 1;

// ============================================================================
// 静态定义表（与头文件 enum 顺序对应）
// ============================================================================
namespace {

struct SoulUpgradeDef {
    const char* name;           // 中文名
    const char* description;    // 描述
    int effectPerLevel;         // 每级效果数值（用于 UI 显示，含义随类型而异）
    int baseCost;               // 第一级成本（后续 ×(currentLevel+1) 递增）
};

// 6 条强化路径定义
// 设计意图：覆盖六维成长方向，每条对应不同 build 偏好
constexpr SoulUpgradeDef kUpgradeDefs[kSoulUpgradeCount] = {
    {"永韧之骨", "每级最大生命 +20",          20, 20}, // Vitality
    {"智者之魂", "每级经验获取 +10%",         10, 25}, // Wisdom
    {"贪婪血脉", "每级金币掉落 +15%",         15, 25}, // Fortune
    {"武器大师", "每级伤害 +5%",              5,  30}, // Strength
    {"疾风传承", "每级移速 +5%",              5,  30}, // Swiftness
    {"守护之灵", "每级防御 +3",               3,  25}, // Aegis
};

} // anonymous namespace

// ============================================================================
// Initialize —— 从文件加载（仅首次启动调用一次）
// ============================================================================
void SoulMemorySystem::Initialize() {
    if (initialized_) return;
    initialized_ = true;

    if (!LoadFromFile()) {
        LOG_INFO("灵魂之忆: 使用初始空数据（新玩家或文件不存在）");
        data_ = SoulMemoryData{};
    }
}

// ============================================================================
// 碎片获取
// ============================================================================
int SoulMemorySystem::CalculateShardsGained(int level, int kills, int bossKills) noexcept {
    // 公式设计：
    //   层数贡献：每层 5 碎片（鼓励深入探索）
    //   击杀贡献：每 10 个击杀 1 碎片（鼓励积极战斗）
    //   Boss 贡献：每个 Boss 20 碎片（Boss 战是高风险高回报事件）
    // 首次死亡保底 10 碎片（避免低层数死亡零收益）
    int shards = level * 5 + kills / 10 + bossKills * 20;
    if (shards < 10) shards = 10;
    return shards;
}

void SoulMemorySystem::AddShards(int amount) {
    if (amount <= 0) return;
    data_.shards += amount;
    data_.totalShardsEarned += amount;
    LOG_INFO("灵魂之忆: 获得 %d 碎片（当前 %d，累计 %d）",
             amount, data_.shards, data_.totalShardsEarned);
}

// ============================================================================
// 强化购买
// ============================================================================
int SoulMemorySystem::GetUpgradeLevel(SoulUpgradeType type) const noexcept {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= kSoulUpgradeCount) return 0;
    return data_.upgradeLevels[idx];
}

int SoulMemorySystem::GetUpgradeCost(SoulUpgradeType type) const noexcept {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= kSoulUpgradeCount) return -1;
    int currentLevel = data_.upgradeLevels[idx];
    if (currentLevel >= kSoulUpgradeMaxLevel) return -1; // 已满级
    // 成本递增：baseCost × (currentLevel + 1)
    // 例：baseCost=20 → 20/40/60/80/100
    return kUpgradeDefs[idx].baseCost * (currentLevel + 1);
}

bool SoulMemorySystem::PurchaseUpgrade(SoulUpgradeType type) {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= kSoulUpgradeCount) return false;

    int currentLevel = data_.upgradeLevels[idx];
    if (currentLevel >= kSoulUpgradeMaxLevel) {
        LOG_WARN("灵魂之忆: %s 已满级", kUpgradeDefs[idx].name);
        return false;
    }

    int cost = GetUpgradeCost(type);
    if (cost <= 0 || data_.shards < cost) {
        LOG_WARN("灵魂之忆: 碎片不足（需 %d，当前 %d）", cost, data_.shards);
        return false;
    }

    data_.shards -= cost;
    data_.upgradeLevels[idx] = static_cast<uint8_t>(currentLevel + 1);
    LOG_INFO("灵魂之忆: 购买 %s Lv%d→Lv%d（消耗 %d，剩余 %d）",
             kUpgradeDefs[idx].name, currentLevel, currentLevel + 1,
             cost, data_.shards);

    // 立即保存，避免崩溃丢失进度
    if (!SaveToFile()) {
        LOG_WARN("灵魂之忆: 购买后持久化失败");
    }
    return true;
}

// ============================================================================
// 数值应用
// ============================================================================
void SoulMemorySystem::ApplyToPlayerStats(float& maxHp, float& damage, float& moveSpeed,
                                          float& expMul, float& coinMul,
                                          float& defense) const noexcept {
    // 设计：加法式叠加，作为"基础属性的一部分"
    // 在 recomputePlayerStats 中位于"每级微调"之后、"升级加成"之前，
    // 后续所有 multiplier（升级/装备/圣物/变异）乘法叠加
    int vitLv  = data_.upgradeLevels[static_cast<int>(SoulUpgradeType::Vitality)];
    int wisLv  = data_.upgradeLevels[static_cast<int>(SoulUpgradeType::Wisdom)];
    int forLv  = data_.upgradeLevels[static_cast<int>(SoulUpgradeType::Fortune)];
    int strLv  = data_.upgradeLevels[static_cast<int>(SoulUpgradeType::Strength)];
    int swiLv  = data_.upgradeLevels[static_cast<int>(SoulUpgradeType::Swiftness)];
    int aegLv  = data_.upgradeLevels[static_cast<int>(SoulUpgradeType::Aegis)];

    maxHp     += 20.f * static_cast<float>(vitLv);
    damage    *= (1.f + 0.05f * static_cast<float>(strLv));
    moveSpeed *= (1.f + 0.05f * static_cast<float>(swiLv));
    expMul    += 0.10f * static_cast<float>(wisLv);
    coinMul   += 0.15f * static_cast<float>(forLv);
    defense   += 3.f * static_cast<float>(aegLv);
}

// ============================================================================
// 查询接口
// ============================================================================
int SoulMemorySystem::GetTotalUpgradeLevels() const noexcept {
    int total = 0;
    for (int i = 0; i < kSoulUpgradeCount; ++i) {
        total += data_.upgradeLevels[i];
    }
    return total;
}

void SoulMemorySystem::ReloadFromFile() {
    if (!LoadFromFile()) {
        LOG_INFO("灵魂之忆: 重新加载失败，使用当前内存数据");
    }
}

const char* SoulMemorySystem::GetUpgradeName(SoulUpgradeType type) noexcept {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= kSoulUpgradeCount) return "未知";
    return kUpgradeDefs[idx].name;
}

const char* SoulMemorySystem::GetUpgradeDescription(SoulUpgradeType type) noexcept {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= kSoulUpgradeCount) return "";
    return kUpgradeDefs[idx].description;
}

int SoulMemorySystem::GetUpgradeEffectPerLevel(SoulUpgradeType type) noexcept {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= kSoulUpgradeCount) return 0;
    return kUpgradeDefs[idx].effectPerLevel;
}

int SoulMemorySystem::GetUpgradeBaseCost(SoulUpgradeType type) noexcept {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= kSoulUpgradeCount) return 0;
    return kUpgradeDefs[idx].baseCost;
}

// ============================================================================
// 持久化
// ============================================================================
std::string SoulMemorySystem::getFilePath() {
    return "saves/soul_memory.dat";
}

bool SoulMemorySystem::LoadFromFile() {
    std::ifstream ifs(getFilePath(), std::ios::binary);
    if (!ifs.is_open()) {
        LOG_INFO("灵魂之忆文件不存在，将使用初始状态: %s", getFilePath().c_str());
        return false;
    }

    // 强制 C locale，避免 locale 污染
    ifs.imbue(std::locale::classic());

    uint32_t magic = 0;
    uint8_t version = 0;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (magic != kSoulMemoryMagic || version != kSoulMemoryVersion) {
        LOG_WARN("灵魂之忆文件格式错误: magic=0x%X version=%d", magic, version);
        return false;
    }

    int32_t shards = 0;
    int32_t totalEarned = 0;
    ifs.read(reinterpret_cast<char*>(&shards), sizeof(shards));
    ifs.read(reinterpret_cast<char*>(&totalEarned), sizeof(totalEarned));

    // 防御性检查：负数或异常大值视为损坏
    if (shards < 0 || totalEarned < 0 || totalEarned < shards) {
        LOG_WARN("灵魂之忆数据异常: shards=%d totalEarned=%d", shards, totalEarned);
        return false;
    }

    data_.shards = shards;
    data_.totalShardsEarned = totalEarned;

    // 读取 6 个强化等级（每字节一个）
    for (int i = 0; i < kSoulUpgradeCount; ++i) {
        uint8_t lv = 0;
        ifs.read(reinterpret_cast<char*>(&lv), sizeof(lv));
        // 防御性检查：等级不能超过最大值
        if (lv > kSoulUpgradeMaxLevel) {
            LOG_WARN("灵魂之忆: 强化 %d 等级异常 %d，重置为 0", i, lv);
            lv = 0;
        }
        data_.upgradeLevels[i] = lv;
    }

    LOG_INFO("灵魂之忆已加载: 碎片=%d 累计=%d 总强化等级=%d/%d",
             data_.shards, data_.totalShardsEarned,
             GetTotalUpgradeLevels(), kSoulUpgradeCount * kSoulUpgradeMaxLevel);
    return true;
}

bool SoulMemorySystem::SaveToFile() const {
    // 确保 saves 目录存在
#ifdef _WIN32
    (void)_mkdir("saves");
#else
    (void)mkdir("saves", 0755);
#endif

    std::ofstream ofs(getFilePath(), std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        LOG_ERROR("无法写入灵魂之忆文件: %s", getFilePath().c_str());
        return false;
    }
    ofs.imbue(std::locale::classic());

    // 文件头
    ofs.write(reinterpret_cast<const char*>(&kSoulMemoryMagic), sizeof(kSoulMemoryMagic));
    ofs.write(reinterpret_cast<const char*>(&kSoulMemoryVersion), sizeof(kSoulMemoryVersion));

    // 碎片与累计
    int32_t shards = data_.shards;
    int32_t totalEarned = data_.totalShardsEarned;
    ofs.write(reinterpret_cast<const char*>(&shards), sizeof(shards));
    ofs.write(reinterpret_cast<const char*>(&totalEarned), sizeof(totalEarned));

    // 6 个强化等级
    for (int i = 0; i < kSoulUpgradeCount; ++i) {
        uint8_t lv = data_.upgradeLevels[i];
        ofs.write(reinterpret_cast<const char*>(&lv), sizeof(lv));
    }

    LOG_INFO("灵魂之忆已保存: 碎片=%d 累计=%d", data_.shards, data_.totalShardsEarned);
    return true;
}

} // namespace cu
