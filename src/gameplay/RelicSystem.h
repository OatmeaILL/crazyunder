#pragma once

// ============================================================================
// RelicSystem —— 圣物/遗物系统（第十五轮新增）
// ----------------------------------------------------------------------------
// 设计意图：
//   Roguelike 核心的"构筑（Build）维度"。圣物为被动效果，跨层保留，从 Boss
//   击败奖励与隐藏房发现奖励获取。每个圣物对玩家属性有不同方向的加成，玩家
//   通过 3 选 1 决策构筑差异化的角色 build，提升重玩价值与策略深度。
//
// 获取渠道：
//   1. Boss 击败后弹出 3 选 1 圣物选择（必给 1 个，未拥有的圣物中抽取）
//   2. 隐藏房首次发现时直接给予 1 个随机未拥有圣物
//
// 持久化：
//   随 SaveData 序列化（relicIds 数组），跨层与跨存档均保留
//
// 与现有系统的关系：
//   - 数值加成在 Game::recomputePlayerStats 中调用 ApplyToPlayerStats 应用
//   - 与升级系统、装备词缀三重叠加，形成"升级+装备+圣物"的复合成长曲线
//   - 圣物上限 6 个，避免后期属性膨胀失控
// ============================================================================

#include <array>
#include <cstdint>
#include <vector>
#include "gameplay/Player.h"  // PlayerStats 定义

namespace cu {

// ---- 圣物类型枚举 ----
enum class RelicType : uint8_t {
    None           = 0,  // 哨兵/空
    WarriorCrest   = 1,  // 战士之证：伤害 +15%
    GuardianHeart  = 2,  // 守卫之心：最大生命 +20%
    HunterEye      = 3,  // 猎手之眼：暴击率 +10%，暴击伤害 +20%
    WindBoots      = 4,  // 疾风之靴：移速 +15%
    ScholarBook   = 5,  // 学者之书：经验获取 +30%
    GreedyEye      = 6,  // 贪婪之眼：金币掉落 +50%
    VampireFang    = 7,  // 吸血鬼之牙：吸血 +5%
    Aegis          = 8,  // 守护之心：防御 +15，最大生命 +10%
    // ---- 第十九轮新增：雷霆系圣物（闪电流 build 核心）----
    ThunderHeart   = 9,  // 雷霆之心：连锁闪电 +1，伤害 +10%
    StormEye       = 10, // 风暴之眼：麻痹时间 +50%，暴击率 +5%
    // ---- 第二十轮新增：极限闪避系圣物（防御反击 build 核心）----
    MoonAmulet     = 11, // 月光护符：极限闪避检测窗口 +50%
    VengeanceBlade = 12, // 复仇之刃：极限闪避 buff 期间所有攻击必暴击
    Count          = 13  // 圣物类型总数（哨兵）
};

// ---- 圣物静态数据 ----
struct RelicData {
    RelicType  type      = RelicType::None;
    const char* name     = "";  // 中文名
    const char* desc     = "";  // 简短描述（用于选择 UI 与面板）
    const char* lore     = "";  // 叙事文本（世界观背景故事）
    uint8_t    r         = 255; // 边框/图标色（用于 UI 区分）
    uint8_t    g         = 255;
    uint8_t    b         = 255;
};

// ---- 圣物系统常量 ----
inline constexpr int kRelicMaxCount = 6;  // 玩家最多拥有的圣物数

// ---- 获取圣物静态数据 ----
[[nodiscard]] const RelicData& GetRelicData(RelicType type);

// ---- 获取圣物中文名 ----
[[nodiscard]] const char* GetRelicName(RelicType type);

// ============================================================================
// RelicSystem —— 圣物系统
// ============================================================================
class RelicSystem {
public:
    RelicSystem();
    ~RelicSystem() = default;

    // 初始化（清空已拥有圣物）
    void Initialize();

    // ---- 查询接口 ----
    [[nodiscard]] bool HasRelic(RelicType type) const noexcept;
    [[nodiscard]] int  GetOwnedCount() const noexcept { return ownedCount_; }
    [[nodiscard]] bool IsFull() const noexcept { return ownedCount_ >= kRelicMaxCount; }
    // 获取已拥有圣物列表（按获得顺序，未拥有的位置为 RelicType::None）
    [[nodiscard]] const std::array<RelicType, kRelicMaxCount>& GetOwnedRelics() const noexcept { return owned_; }

    // ---- 修改接口 ----
    // 添加圣物（若已拥有或已满返回 false）
    bool AddRelic(RelicType type);
    // 清空所有圣物（重新开始游戏时调用）
    void Clear();

    // ---- 随机抽取未拥有的圣物 ----
    // count: 抽取数量（不足时返回实际数量）
    [[nodiscard]] std::vector<RelicType> RollUnownedRelics(int count) const;

    // ---- 应用圣物加成到玩家属性 ----
    // 在 Game::recomputePlayerStats 末尾调用，叠加圣物的数值加成
    void ApplyToPlayerStats(PlayerStats& stats) const;

    // ---- 序列化/反序列化（存档用）----
    [[nodiscard]] std::array<uint8_t, kRelicMaxCount> Serialize() const;
    void Deserialize(const std::array<uint8_t, kRelicMaxCount>& data);

private:
    // 已拥有的圣物列表（前 ownedCount_ 个有效，其余为 None）
    std::array<RelicType, kRelicMaxCount> owned_{};
    int ownedCount_ = 0;
};

} // namespace cu
