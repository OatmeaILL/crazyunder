#pragma once

// ============================================================================
// MerchantSystem —— 商人系统
// ----------------------------------------------------------------------------
// 职责：
//   1. 在每层开始房间按概率生成商人（首层必现，其余 50%）
//   2. 生成商人售卖物品清单（6 件，品质随层数增加而提升）
//   3. 处理购买/出售逻辑
//   4. 渲染商人实体
//
// 售卖规则：
//   - 商人每次出现刷新 6 件随机装备
//   - 品质概率与装备掉落一致（rollQualityByLevel），但前面层数也有小概率出好货
//   - 售价 = 基础价 × 品质倍率 × (1 + ilvl * 0.1)
//   - 出售价 = 购买价 × 0.5（半价回收）
//
// 交互：
//   - 玩家靠近商人（距离 < 50px）按 E 键打开商人菜单
//   - 商人菜单中左键点击购买，右键点击出售
// ============================================================================
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <array>
#include <vector>
#include <cstdint>
#include "ecs/Entity.h"
#include "gameplay/LootSystem.h" // Item/ItemQuality 定义
#include "gameplay/SkillSystem.h" // SkillType 定义

namespace cu {

class Registry;
class TextureAtlas;
class Renderer;
class InventorySystem;
struct PlayerStats;

// ---- 商人售卖条目 ----
struct MerchantItem {
    Item item;          // 售卖物品（装备）
    int price = 0;      // 购买价格（金币）
    bool sold = false;  // 是否已售出
};

// ---- 商人售卖技能条目 ----
struct MerchantSkill {
    SkillType type = SkillType::Count; // 技能类型（Count=空/已售）
    int price = 0;                     // 购买价格（金币）
    bool sold = false;                 // 是否已售出
};

class MerchantSystem {
public:
    static constexpr int kMerchantStockSize = 6;       // 商人装备售卖格数
    static constexpr int kMerchantSkillSize = 2;       // 商人技能售卖格数
    static constexpr float kInteractRange = 80.f;        // 交互距离（像素）

    MerchantSystem();
    ~MerchantSystem() = default;

    // 初始化（注册商人实体到 ECS，初始为非活跃）
    void Initialize(Registry& registry, const TextureAtlas& atlas);

    // 在指定位置生成商人并刷新库存
    // dungeonLevel: 当前地牢层数（影响品质权重）
    void SpawnMerchant(sf::Vector2f position, int dungeonLevel);

    // 清除当前商人（进入下一层时调用）
    void ClearMerchant();

    // 检查玩家是否在商人交互范围内
    [[nodiscard]] bool IsPlayerInRange(sf::Vector2f playerPos) const;

    // 获取商人位置
    [[nodiscard]] sf::Vector2f GetPosition() const noexcept { return merchantPos_; }

    // 商人是否活跃（当前层有商人）
    [[nodiscard]] bool IsActive() const noexcept { return active_; }

    // 获取商人库存（只读，供 UI 显示）
    [[nodiscard]] const std::array<MerchantItem, kMerchantStockSize>& GetStock() const noexcept {
        return stock_;
    }

    // 获取商人技能库存（只读，供 UI 显示）
    [[nodiscard]] const std::array<MerchantSkill, kMerchantSkillSize>& GetSkillStock() const noexcept {
        return skillStock_;
    }

    // 购买指定索引的物品
    // 返回 true 表示购买成功（金币足够且未售出）
    bool BuyItem(int index, InventorySystem& inventory, PlayerStats& stats);

    // 购买指定索引的技能
    // 返回 true 表示购买成功
    bool BuySkill(int index, PlayerComponent& pc, PlayerStats& stats);

    // 出售背包内指定索引的物品
    // 返回出售所得金币（0 表示失败）
    int SellBackpackItem(int backpackIndex, InventorySystem& inventory, PlayerStats& stats);

    // 计算物品的购买价格（静态工具函数）
    [[nodiscard]] static int CalcBuyPrice(const Item& item) noexcept;
    // 计算物品的出售价格（静态工具函数）
    [[nodiscard]] static int CalcSellPrice(const Item& item) noexcept;

    // 第十七轮新增：地牢变异系统——商人价格 multiplier（静态，默认 1.0=无影响）
    // 由 Game::setupPlayingScene 在每层开始时设置，作用于 CalcBuyPrice
    // 采用静态成员而非实例字段，因为 Menus.cpp 中以静态方式调用 CalcSellPrice
    static void SetModifierPriceMul(float m) noexcept;
    [[nodiscard]] static float GetModifierPriceMul() noexcept;

    // 渲染商人实体
    void Render(Renderer& renderer);

private:
    // 生成商人库存（6 件随机装备，品质随层数）
    void generateStock(int dungeonLevel);
    // 生成商人技能库存（2 个随机技能）
    void generateSkillStock(int dungeonLevel);
    // 基于层数的品质随机（与 LootSystem 一致，但前面层也有小概率出好货）
    [[nodiscard]] ItemQuality rollQualityByLevel(int level) const;
    // 生成单件随机物品
    [[nodiscard]] Item generateRandomItem(int ilvl, ItemQuality quality) const;

    Registry* registry_ = nullptr;
    const TextureAtlas* atlas_ = nullptr;
    EntityId merchantEntity_ = kInvalidEntity;
    sf::Vector2f merchantPos_{0.f, 0.f};
    bool active_ = false;
    std::array<MerchantItem, kMerchantStockSize> stock_;
    std::array<MerchantSkill, kMerchantSkillSize> skillStock_;

    // 第十七轮新增：变异系统商人价格 multiplier（静态，默认 1.0=无影响）
    static float sPriceMul_;
};

} // namespace cu
