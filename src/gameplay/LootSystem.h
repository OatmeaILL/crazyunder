#pragma once

// ============================================================================
// LootSystem  战利品掉落与词缀生成系统（Phase 7）
// ----------------------------------------------------------------------------
// 职责：
//   1. 敌人死亡时根据类型决定掉落物品质与数量
//   2. 生成装备 Item（含词缀 Affix），按品质颜色绘制光圈
//   3. 处理玩家自动拾取（距离 < 40px）
//
// 品质权重（数据驱动，可配置）：
//   - 普通怪：10% 掉 1 件白装
//   - 精英怪：100% 掉 1-2 件，70% 蓝 / 25% 黄 / 5% 暗金
//   - Boss：100% 掉 3 件，30% 黄 / 70% 暗金
//   - 宝箱房：100% 掉 2 件，品质提升
//
// 词缀系统：
//   每件装备含 1-4 个词缀，数量与品质正相关：
//     White   -> 1 词缀，数值低（x0.8）
//     Blue    -> 2 词缀，数值标准（x1.0）
//     Yellow  -> 3 词缀，数值较高（x1.2）
//     DarkGold-> 4 词缀，数值高（x1.5）
//   词缀数值按 ilvl 缩放，加随机浮动 +-20%
//
// 性能：
//   - 同屏掉落物 < 50，使用 vector 存储，遍历开销可忽略
//   - 拾取检测 O(N)，N = 活跃掉落物数
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <string>
#include <vector>
#include <array>
#include <utility>
#include <cstdint>
#include <functional>
#include "ecs/Entity.h"
#include "gameplay/EnemyAI.h"

namespace cu {

class Registry;
class TextureAtlas;
class Renderer;
class Input;
class InventorySystem;

enum class ItemQuality : uint8_t {
    White    = 0,
    Blue     = 1,
    Yellow   = 2,
    DarkGold = 3
};

enum class ItemSlot : uint8_t {
    Weapon = 0,
    Helmet  = 1,
    Chest   = 2,
    Boots   = 3,
    Ring    = 4,
    Amulet  = 5
};

// ============================================================================
// 第二十三轮新增：装备套装系统
// ----------------------------------------------------------------------------
// 4 种套装，每种覆盖 3 个固定槽位（每个槽位恰属 2 个套装，强制取舍）
// 2 件套/3 件套分阶奖励，与现有"升级+装备+圣物+元素+变异+连击"正交叠加
// 设计意图：让玩家在"更高 ilvl 单件"与"较低 ilvl 但凑齐套装"间做策略选择
// ============================================================================
enum class EquipmentSet : uint8_t {
    None     = 0,
    Warrior  = 1, // 战士之怒：武器 + 胸甲 + 戒指
    Sage     = 2, // 智者之识：头盔 + 项链 + 戒指
    Wind     = 3, // 疾风行者：靴子 + 武器 + 头盔
    Guardian = 4  // 永恒守护：胸甲 + 靴子 + 项链
};

// 套装加成类型
enum class SetBonusType : uint8_t {
    None           = 0,
    DamageMul      = 1, // 伤害乘法
    MaxHpMul       = 2, // 最大生命乘法
    MoveSpeedMul   = 3, // 移速乘法
    AttackSpeedMul = 4, // 攻速乘法
    CritRateAdd    = 5, // 暴击率加法（百分比小数）
    CritDamageAdd  = 6, // 暴击伤害加法（百分比小数）
    ExpMulAdd      = 7, // 经验倍率加法
    DefenseAdd     = 8  // 防御加法（固定值）
};

enum class AffixType : uint8_t {
    AddedDamage   = 0,
    AddedDefense  = 1,
    CritRate      = 2,
    CritDamage    = 3,
    MoveSpeed     = 4,
    AttackSpeed   = 5,
    Lifesteal     = 6,
    MaxHp         = 7,
    MaxMp         = 8
};

struct Affix {
    AffixType type = AffixType::AddedDamage;
    float value = 0.f;
    bool isPercent = false;
};

struct Item {
    ItemSlot slot = ItemSlot::Weapon;
    ItemQuality quality = ItemQuality::White;
    std::string name;
    std::vector<Affix> affixes;
    int ilvl = 1;
    EquipmentSet setId = EquipmentSet::None; // 第二十三轮新增：所属套装（None=无套装）
};

struct LootDropEntry {
    Item item;
    sf::Vector2f position{0.f, 0.f};
    bool pickedUp = false;
    float lifetime = 60.f;
    float glowPhase = 0.f;
    float fullMessageCooldown = 0.f;  // "背包已满" 提示冷却（避免刷屏）
};

class LootSystem {
public:
    LootSystem();
    ~LootSystem() = default;

    void Initialize(Registry& registry, TextureAtlas& atlas);

    void OnEnemyKilled(EntityId enemy, sf::Vector2f pos, EnemyType type, bool isChampion = false);

    void OnChestOpened(sf::Vector2f pos);

    // 罐子破坏掉落：最多 1 件装备，品质随层数调整
    void OnPotBroken(sf::Vector2f pos);

    // 强制掉落指定品质装备（事件房/诅咒房奖励用）
    // pos: 掉落位置, quality: 强制品质, ilvl: 物品等级（影响词缀数值）
    void DropItem(sf::Vector2f pos, ItemQuality quality, int ilvl);

    // 设置当前层数（影响品质权重）
    void SetDungeonLevel(int level) noexcept { dungeonLevel_ = level; }

    // 第十七轮新增：地牢变异系统——装备掉率 multiplier（默认 1.0=无影响）
    // 由 Game::setupPlayingScene 注入，作用于 OnEnemyKilled/OnPotBroken 的概率判定
    void SetModifierItemDropChanceMul(float m) noexcept { modItemDropChanceMul_ = m; }

    void Update(Registry& registry, EntityId player,
                const Input& input, InventorySystem& inventory, float dt);

    void Render(Renderer& renderer);

    [[nodiscard]] const std::vector<LootDropEntry>& GetDroppedItems() const noexcept {
        return drops_;
    }

    [[nodiscard]] int GetActiveCount() const noexcept {
        return static_cast<int>(drops_.size());
    }

    [[nodiscard]] static sf::Color GetQualityColor(ItemQuality q) noexcept;
    [[nodiscard]] static const char* GetQualityName(ItemQuality q) noexcept;
    [[nodiscard]] static const char* GetQualityPrefix(ItemQuality q) noexcept;
    [[nodiscard]] static const char* GetSlotName(ItemSlot s) noexcept;
    [[nodiscard]] static const char* GetAffixTypeName(AffixType t) noexcept;

    // ---- 第二十三轮新增：装备套装系统辅助接口 ----
    // 获取套装中文名（None 返回空字符串）
    [[nodiscard]] static const char* GetSetName(EquipmentSet s) noexcept;
    // 获取套装覆盖的 3 个槽位（None 返回全 Weapon 占位）
    [[nodiscard]] static std::array<ItemSlot, 3> GetSetSlots(EquipmentSet s) noexcept;
    // 获取套装主色调（UI 显示用）
    [[nodiscard]] static sf::Color GetSetColor(EquipmentSet s) noexcept;
    // 获取套装在指定件数（2 或 3）下的加成类型与数值；其他件数或 None 返回 {None, 0}
    [[nodiscard]] static std::pair<SetBonusType, float> GetSetBonus(EquipmentSet s, int pieces) noexcept;
    // 为指定槽位随机分配一个所属套装（每个槽位恰有 2 个可选套装，等概率）
    [[nodiscard]] static EquipmentSet RollSetForSlot(ItemSlot slot) noexcept;

    // 物品掉落回调（用于触发粒子特效）
    std::function<void(sf::Vector2f pos, ItemQuality quality)> OnItemDropped;
    // 物品拾取回调（用于任务/成就系统统计拾取数量与品质）
    // 参数：拾取的物品品质
    std::function<void(ItemQuality quality)> OnItemPickedUp;

    // ---- 公开接口（供任务奖励等外部调用）----
    // 生成一件随机装备（指定 ilvl 和品质，类型随机）
    [[nodiscard]] Item GenerateRandomItem(int ilvl, ItemQuality forcedQuality) const {
        return generateRandomItem(ilvl, forcedQuality);
    }
    // 掉落一件现成物品到地上（背包已满时用）
    void DropSpecificItem(const Item& item, sf::Vector2f pos) { dropItem(item, pos); }

private:
    Item generateRandomItem(int ilvl, ItemQuality forcedQuality) const;
    // 基于固定权重的品质随机（旧接口，保留给 Boss/宝箱等固定权重场景）
    [[nodiscard]] ItemQuality rollQuality(float blueWeight,
                                          float yellowWeight,
                                          float darkGoldWeight) const;
    // 基于当前层数的品质随机（普通怪/罐子使用）
    // 规则：前3层白色为主；3层起蓝色增加；6层后紫色；10层金色
    [[nodiscard]] ItemQuality rollQualityByLevel() const;
    // Boss 专用：基于层数的品质随机，整体比普通怪提升一档
    // 规则：在 rollQualityByLevel 基础上，将品质向上提升一级（White→Blue→Yellow→DarkGold）
    // 前几层 Boss 主要掉蓝装，随层数增加出现紫/金
    [[nodiscard]] ItemQuality rollBossQualityByLevel() const;
    void generateAffixes(Item& item) const;
    [[nodiscard]] Affix rollSingleAffix(const Item& item, float qualityMultiplier) const;
    [[nodiscard]] std::string generateItemName(ItemQuality q, ItemSlot s) const;
    void dropItem(const Item& item, sf::Vector2f pos);

    [[nodiscard]] float randomFloat(float min, float max) const;
    [[nodiscard]] int randomInt(int min, int max) const;
    [[nodiscard]] bool randomChance(float probability) const;

private:
    Registry* registry_ = nullptr;
    TextureAtlas* atlas_ = nullptr;
    std::vector<LootDropEntry> drops_;
    int dungeonLevel_ = 1;  // 当前地牢层数（影响品质权重）

    // 第十七轮新增：地牢变异系统——装备掉率 multiplier（默认 1.0=无影响）
    float modItemDropChanceMul_ = 1.f;

    static constexpr float kPickupRadius = 40.f;
};

} // namespace cu