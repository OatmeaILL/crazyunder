#pragma once

// ============================================================================
// InventorySystem  背包与装备穿戴系统（Phase 7）
// ----------------------------------------------------------------------------
// 职责：
//   1. 管理 6 个装备槽位（Weapon/Helmet/Chest/Boots/Ring/Amulet）
//   2. 管理 25 格大背包（存放未装备的装备）
//   3. Equip/Unequip 装备，返回被替换的旧装备
//   4. AddToBackpack/RemoveFromBackpack 背包物品管理
//   5. PickupItem 智能拾取：空槽自动装备，否则放入背包
//   6. GetTotalAffixes 聚合所有装备词缀（O(1)，6 槽位固定）
//   7. ApplyToPlayerStats 将词缀加成应用到玩家属性
//
// 拾取规则：
//   - 装备栏对应槽位为空 → 自动装备
//   - 装备栏对应槽位已满 → 放入大背包（若背包已满则无法拾取）
//
// 词缀聚合规则：
//   - AddedDamage：+damage（固定值）
//   - AddedDefense：+maxHp（固定值，作为防御力体现）
//   - CritRate：+critChance（百分比累加）
//   - CritDamage：+critDamage（百分比累加）
//   - MoveSpeed：+moveSpeed（百分比加成）
//   - AttackSpeed：+attackSpeed（百分比加成）
//   - Lifesteal：+lifesteal（百分比累加）
//   - MaxHp：+maxHp（固定值）
//   - MaxMp：+maxMp（固定值）
//
// 性能：
//   - 6 槽位固定，词缀聚合 O(1)
//   - 装备/卸载 O(1)
//   - 背包操作 O(N)，N=25（可忽略）
// ============================================================================

#include <array>
#include <optional>
#include <unordered_map>
#include <cstdint>
#include "gameplay/LootSystem.h"

namespace cu {

struct PlayerStats; // 前向声明

// ---- 装备槽位结构体 ----
struct EquipmentSlot {
    ItemSlot slot = ItemSlot::Weapon;
    std::optional<Item> item; // nullopt = 空槽位
};

// ============================================================================
// InventorySystem  背包系统
// ============================================================================
class InventorySystem {
public:
    // 大背包格数
    static constexpr int kBackpackSize = 25;

    InventorySystem();
    ~InventorySystem() = default;

    // 初始化：6 个槽位 + 25 格背包清空
    void Initialize();

    // 从存档数据恢复（读档用，直接覆盖内部状态）
    void LoadFromData(const std::array<EquipmentSlot, 6>& equipped,
                      const std::array<std::optional<Item>, kBackpackSize>& backpack);

    // 装备物品到对应槽位
    // 若槽位已有装备，旧装备被替换并返回
    // 返回被替换的旧装备（nullopt 表示槽位原本为空）
    std::optional<Item> Equip(const Item& newItem);

    // 卸下指定槽位的装备
    // 返回被卸下的装备（nullopt 表示槽位为空）
    std::optional<Item> Unequip(ItemSlot slot);

    // ---- 大背包接口 ----

    // 添加物品到大背包（寻找第一个空位）
    // 返回 true 表示成功放入，false 表示背包已满
    bool AddToBackpack(const Item& item);

    // 从大背包移除指定索引的物品
    // 返回被移除的物品（nullopt 表示索引无效或空位）
    std::optional<Item> RemoveFromBackpack(int index);

    // 获取大背包指定索引的物品（只读）
    [[nodiscard]] std::optional<Item> GetBackpackItem(int index) const;

    // 替换大背包指定索引的物品，返回旧物品
    std::optional<Item> ReplaceBackpackItem(int index, const Item& newItem);

    // 智能拾取：若对应装备槽为空则自动装备，否则放入大背包
    // 返回 true 表示拾取成功（装备或入背包），false 表示背包已满且槽位已占用
    bool PickupItem(const Item& item);

    // 聚合所有装备词缀
    // 返回 unordered_map<AffixType, 总数值>
    // 百分比词缀累加为总和百分比，固定词缀累加为总和固定值
    [[nodiscard]] std::unordered_map<AffixType, float> GetTotalAffixes() const;

    // 将词缀加成应用到玩家属性
    // base: 基础属性（会被修改，加入装备词缀加成）
    void ApplyToPlayerStats(PlayerStats& stats) const;

    // 第二十三轮新增：装备套装系统聚合接口
    // 返回每个激活套装（ setId != None）已装备件数（仅 1 件不计入，需 >=2 才视为激活）
    // 调用者根据件数查 GetSetBonus 获取加成
    [[nodiscard]] std::array<std::pair<cu::EquipmentSet, int>, 4> GetActiveSetCounts() const noexcept;

    // 第二十三轮新增：将所有激活套装的加成（>=2 件）应用到玩家属性
    // 在 ApplyToPlayerStats 之后调用，与圣物/变异 multiplier 乘法叠加
    void ApplySetBonuses(PlayerStats& stats) const;

    // 获取所有装备槽位（只读）
    [[nodiscard]] const std::array<EquipmentSlot, 6>& GetEquippedItems() const noexcept {
        return slots_;
    }

    // 获取大背包所有物品（只读）
    [[nodiscard]] const std::array<std::optional<Item>, kBackpackSize>& GetBackpackItems() const noexcept {
        return backpack_;
    }

    // 获取已装备数量
    [[nodiscard]] int GetEquippedCount() const noexcept;

    // 获取大背包已用格数
    [[nodiscard]] int GetBackpackCount() const noexcept;

    // 大背包是否已满
    [[nodiscard]] bool IsBackpackFull() const noexcept;

    // 调试输出：打印所有装备信息
    void DebugPrint() const;

private:
    // 6 个装备槽位，索引与 ItemSlot 枚举值对应
    std::array<EquipmentSlot, 6> slots_;
    // 25 格大背包
    std::array<std::optional<Item>, kBackpackSize> backpack_;
};

} // namespace cu