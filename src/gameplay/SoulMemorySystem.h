#pragma once

// ============================================================================
// SoulMemorySystem —— 灵魂之忆系统（Meta Progression / 跨局永久成长）
// ----------------------------------------------------------------------------
// 设计意图：
//   Roguelike 标志性的"局间成长"维度。原游戏每次死亡 100% 重置，缺乏长线
//   目标，玩家挫败感强。本系统让每次死亡都获得"灵魂碎片"，碎片可在主菜单
//   "灵魂之井"中兑换永久强化，跨存档共享。与现有九维成长（升级/装备/圣物
//   /元素/变异/连击/极限闪避/词缀/套装）正交叠加，形成"meta 局外 + 局内"
//   双层策略体系。
//
// 核心机制：
//   1. 玩家死亡时根据本局表现获得灵魂碎片（层数×5 + 击杀/10 + Boss×20）
//   2. 碎片持久化到 saves/soul_memory.dat（跨存档共享，类似 achievements.dat）
//   3. 主菜单"灵魂之井"按钮打开面板，消耗碎片购买永久强化
//   4. 6 条强化路径，每条 5 级，每级成本递增
//   5. 加成在 recomputePlayerStats 中应用，作为"基础属性的一部分"，
//      所有后续 multiplier（升级/装备/圣物/变异）乘法叠加
//
// 数值平衡：
//   - 全点满需 2375 碎片（约 8-15 次完整通关才能全点满）
//   - 全满加成：HP+100 / Exp+50% / Coin+75% / Damage+25% / Speed+25% / Def+15
//   - 与单局成长对比：第 10 层玩家通常 HP=400+、Damage=80+，
//     Meta 加成仅占 20-30%，不会破坏前期难度
//   - 首通玩家 0 加成 vs 全满玩家差距可控，确保难度曲线平滑
// ============================================================================

#include <array>
#include <cstdint>
#include <string>

namespace cu {

// ---- 永久强化类型 ----
enum class SoulUpgradeType : uint8_t {
    Vitality  = 0, // 永韧之骨：最大生命 +20/级
    Wisdom    = 1, // 智者之魂：经验获取 +10%/级
    Fortune   = 2, // 贪婪血脉：金币掉落 +15%/级
    Strength  = 3, // 武器大师：伤害 +5%/级
    Swiftness = 4, // 疾风传承：移速 +5%/级
    Aegis     = 5, // 守护之灵：防御 +3/级
    Count     = 6
};

constexpr int kSoulUpgradeCount = static_cast<int>(SoulUpgradeType::Count);
constexpr int kSoulUpgradeMaxLevel = 5;

// ---- 灵魂之忆数据（持久化）----
struct SoulMemoryData {
    int shards = 0;                                   // 当前可用碎片
    int totalShardsEarned = 0;                        // 历史总获得（统计用）
    std::array<uint8_t, kSoulUpgradeCount> upgradeLevels{}; // 各强化等级
};

// ---- 灵魂之忆系统 ----
class SoulMemorySystem {
public:
    SoulMemorySystem() = default;
    ~SoulMemorySystem() = default;

    // 初始化（仅首次启动调用，从文件加载）
    // 加载失败（文件不存在/格式错误）时使用默认空数据
    void Initialize();

    // ---- 碎片获取 ----
    // 计算本局死亡应获得的碎片数
    // level: 当前层数；kills: 本局击杀数；bossKills: 本局 Boss 击杀数
    [[nodiscard]] static int CalculateShardsGained(int level, int kills, int bossKills) noexcept;

    // 增加碎片（不立即保存，由调用方决定保存时机）
    void AddShards(int amount);

    // ---- 强化购买 ----
    // 获取指定强化的当前等级
    [[nodiscard]] int GetUpgradeLevel(SoulUpgradeType type) const noexcept;

    // 获取指定强化下一级的成本（已满级返回 -1）
    [[nodiscard]] int GetUpgradeCost(SoulUpgradeType type) const noexcept;

    // 购买强化（消耗碎片提升一级）
    // 返回 true 表示购买成功（碎片足够且未满级）
    bool PurchaseUpgrade(SoulUpgradeType type);

    // ---- 数值应用（在 recomputePlayerStats 中调用）----
    // 将永久强化应用到玩家属性（加法式叠加，作为基础属性的一部分）
    // 参数为引用传入的玩家属性字段
    void ApplyToPlayerStats(float& maxHp, float& damage, float& moveSpeed,
                            float& expMul, float& coinMul, float& defense) const noexcept;

    // ---- 查询接口 ----
    [[nodiscard]] int GetShards() const noexcept { return data_.shards; }
    [[nodiscard]] int GetTotalShardsEarned() const noexcept { return data_.totalShardsEarned; }
    [[nodiscard]] int GetTotalUpgradeLevels() const noexcept;

    // 重新从文件加载（打开灵魂之井时调用，确保数据显示最新）
    void ReloadFromFile();

    // 获取强化定义信息（供 UI 显示）
    [[nodiscard]] static const char* GetUpgradeName(SoulUpgradeType type) noexcept;
    [[nodiscard]] static const char* GetUpgradeDescription(SoulUpgradeType type) noexcept;
    [[nodiscard]] static int GetUpgradeEffectPerLevel(SoulUpgradeType type) noexcept;
    [[nodiscard]] static int GetUpgradeBaseCost(SoulUpgradeType type) noexcept;

    // ---- 持久化 ----
    [[nodiscard]] bool LoadFromFile();
    [[nodiscard]] bool SaveToFile() const;

private:
    SoulMemoryData data_;
    bool initialized_ = false;

    [[nodiscard]] static std::string getFilePath();
};

} // namespace cu
