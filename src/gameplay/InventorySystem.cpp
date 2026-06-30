#include "gameplay/InventorySystem.h"
#include "gameplay/Player.h"
#include "utils/Logger.h"

namespace cu {

// ============================================================================
// 第二十三轮新增：applySetBonusToStats  单条套装加成应用到 PlayerStats（文件内静态辅助）
// ----------------------------------------------------------------------------
// 在 ApplySetBonuses 之前定义，确保调用点可见
// ============================================================================
namespace {
void applySetBonusToStats(PlayerStats& s, SetBonusType type, float val) {
    switch (type) {
        case SetBonusType::DamageMul:      s.damage *= (1.f + val); break;
        case SetBonusType::MaxHpMul:        s.maxHp *= (1.f + val);  break;
        case SetBonusType::MoveSpeedMul:    s.moveSpeed *= (1.f + val); break;
        case SetBonusType::AttackSpeedMul: s.attackSpeed *= (1.f + val); break;
        case SetBonusType::CritRateAdd:     s.critChance += val;    break;
        case SetBonusType::CritDamageAdd:  s.critDamage += val;    break;
        case SetBonusType::ExpMulAdd:      s.expMultiplier += val; break;
        case SetBonusType::DefenseAdd:     s.defense += val;       break;
        case SetBonusType::None:           break;
    }
}
} // anonymous namespace


// ============================================================================
// InventorySystem 构造与初始化
// ============================================================================
InventorySystem::InventorySystem() {
    Initialize();
}

void InventorySystem::Initialize() {
    for (int i = 0; i < 6; ++i) {
        slots_[i].slot = static_cast<ItemSlot>(i);
        slots_[i].item = std::nullopt;
    }
    for (int i = 0; i < kBackpackSize; ++i) {
        backpack_[i] = std::nullopt;
    }
    LOG_INFO("InventorySystem 已初始化（6 装备槽 + %d 格背包）", kBackpackSize);
}

void InventorySystem::LoadFromData(const std::array<EquipmentSlot, 6>& equipped,
                                    const std::array<std::optional<Item>, kBackpackSize>& backpack) {
    for (int i = 0; i < 6; ++i) {
        // 强制槽位类型与索引一致（防御性：存档可能损坏）
        slots_[i].slot = static_cast<ItemSlot>(i);
        slots_[i].item = equipped[i].item;
    }
    backpack_ = backpack;
    LOG_INFO("InventorySystem 已从存档恢复（6 装备槽 + %d 格背包）", kBackpackSize);
}

// ============================================================================
// Equip  装备物品到对应槽位
// ----------------------------------------------------------------------------
// 若槽位已有装备，旧装备被替换并返回
// 注意：返回值是旧装备。若槽位为空，返回的 Item.quality 为 White 且 affixes 为空
// 调用者可通过 old.affixes.empty() 判断是否为空
// ============================================================================
std::optional<Item> InventorySystem::Equip(const Item& newItem) {
    int idx = static_cast<int>(newItem.slot);
    if (idx < 0 || idx >= 6) return std::nullopt; // 防御性边界检查

    std::optional<Item> oldItem;
    if (slots_[idx].item.has_value()) {
        oldItem = slots_[idx].item.value();
    }

    slots_[idx].item = newItem;

    LOG_INFO("装备 %s 到槽位 %s%s",
             newItem.name.c_str(),
             LootSystem::GetSlotName(newItem.slot),
             oldItem.has_value() ? ("（替换旧装备: " + oldItem->name + "）").c_str() : "");

    return oldItem;
}

// ============================================================================
// Unequip  卸下指定槽位的装备
// ============================================================================
std::optional<Item> InventorySystem::Unequip(ItemSlot slot) {
    int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= 6) return std::nullopt;
    if (!slots_[idx].item.has_value()) return std::nullopt;

    Item old = slots_[idx].item.value();
    slots_[idx].item = std::nullopt;
    LOG_INFO("卸下槽位 %s 的装备: %s",
             LootSystem::GetSlotName(slot), old.name.c_str());
    return old;
}

// ============================================================================
// AddToBackpack  添加物品到大背包（寻找第一个空位）
// ============================================================================
bool InventorySystem::AddToBackpack(const Item& item) {
    for (int i = 0; i < kBackpackSize; ++i) {
        if (!backpack_[i].has_value()) {
            backpack_[i] = item;
            LOG_INFO("物品 %s 已放入大背包格 %d", item.name.c_str(), i);
            return true;
        }
    }
    LOG_WARN("大背包已满，无法放入 %s", item.name.c_str());
    return false;
}

// ============================================================================
// RemoveFromBackpack  从大背包移除指定索引的物品
// ============================================================================
std::optional<Item> InventorySystem::RemoveFromBackpack(int index) {
    if (index < 0 || index >= kBackpackSize) return std::nullopt;
    if (!backpack_[index].has_value()) return std::nullopt;

    Item item = backpack_[index].value();
    backpack_[index] = std::nullopt;
    LOG_INFO("从大背包格 %d 移除物品: %s", index, item.name.c_str());
    return item;
}

// ============================================================================
// GetBackpackItem  获取大背包指定索引的物品（只读）
// ============================================================================
std::optional<Item> InventorySystem::GetBackpackItem(int index) const {
    if (index < 0 || index >= kBackpackSize) return std::nullopt;
    return backpack_[index];
}

// ============================================================================
// ReplaceBackpackItem  替换大背包指定索引的物品，返回旧物品
// ============================================================================
std::optional<Item> InventorySystem::ReplaceBackpackItem(int index, const Item& newItem) {
    if (index < 0 || index >= kBackpackSize) return std::nullopt;
    std::optional<Item> old = backpack_[index];
    backpack_[index] = newItem;
    return old;
}

// ============================================================================
// PickupItem  智能拾取：空槽自动装备，否则放入大背包
// ----------------------------------------------------------------------------
// 规则：
//   1. 若对应装备槽为空 → 自动装备
//   2. 若对应装备槽已满 → 放入大背包
//   3. 大背包已满 → 返回 false（拾取失败）
// ============================================================================
bool InventorySystem::PickupItem(const Item& item) {
    int idx = static_cast<int>(item.slot);
    if (idx >= 0 && idx < 6 && !slots_[idx].item.has_value()) {
        // 对应装备槽为空，自动装备
        Equip(item);
        return true;
    }
    // 装备槽已满，放入大背包
    return AddToBackpack(item);
}

// ============================================================================
// GetTotalAffixes  聚合所有装备词缀
// ----------------------------------------------------------------------------
// 遍历 6 个槽位，累加所有词缀数值
// 百分比词缀与固定词缀分别累加（通过 isPercent 区分）
// 返回 unordered_map<AffixType, 总数值>
// 注意：同一类型的百分比与固定词缀会合并到同一个 key
// ============================================================================
std::unordered_map<AffixType, float> InventorySystem::GetTotalAffixes() const {
    std::unordered_map<AffixType, float> totals;
    for (const auto& slot : slots_) {
        if (!slot.item.has_value()) continue;
        for (const auto& affix : slot.item->affixes) {
            totals[affix.type] += affix.value;
        }
    }
    return totals;
}

// ============================================================================
// ApplyToPlayerStats  将词缀加成应用到玩家属性
// ----------------------------------------------------------------------------
// 聚合规则：
//   AddedDamage  -> stats.damage += value（固定值）
//   AddedDefense -> stats.maxHp += value; stats.defense += value（固定值）
//   CritRate     -> stats.critChance += value（百分比累加）
//   CritDamage   -> stats.critDamage += value（百分比累加）
//   MoveSpeed    -> stats.moveSpeed *= (1 + value)（百分比加成）
//   AttackSpeed  -> stats.attackSpeed *= (1 + value)（百分比加成）
//   Lifesteal    -> stats.lifesteal += value（百分比累加）
//   MaxHp        -> stats.maxHp += value（固定值）
//   MaxMp        -> stats.maxMp += value（固定值）
//
// 注意：调用者应先保存基础属性，每次重新计算而非累加
// ============================================================================
void InventorySystem::ApplyToPlayerStats(PlayerStats& stats) const {
    auto totals = GetTotalAffixes();

    // 固定值词缀直接累加
    if (totals.count(AffixType::AddedDamage))
        stats.damage += totals[AffixType::AddedDamage];
    if (totals.count(AffixType::AddedDefense)) {
        float def = totals[AffixType::AddedDefense];
        stats.maxHp += def;
        stats.defense += def;
    }
    if (totals.count(AffixType::MaxHp))
        stats.maxHp += totals[AffixType::MaxHp];
    if (totals.count(AffixType::MaxMp))
        stats.maxMp += totals[AffixType::MaxMp];

    // 百分比词缀
    if (totals.count(AffixType::CritRate))
        stats.critChance += totals[AffixType::CritRate];
    if (totals.count(AffixType::CritDamage))
        stats.critDamage += totals[AffixType::CritDamage];
    if (totals.count(AffixType::Lifesteal))
        stats.lifesteal += totals[AffixType::Lifesteal];
    if (totals.count(AffixType::MoveSpeed))
        stats.moveSpeed *= (1.f + totals[AffixType::MoveSpeed]);
    if (totals.count(AffixType::AttackSpeed))
        stats.attackSpeed *= (1.f + totals[AffixType::AttackSpeed]);

    // 确保当前 HP 不超过 maxHp
    if (stats.currentHp > stats.maxHp) stats.currentHp = stats.maxHp;
    if (stats.currentMp > stats.maxMp) stats.currentMp = stats.maxMp;
}

// ============================================================================
// 第二十三轮新增：GetActiveSetCounts  聚合当前装备的每个套装件数
// ----------------------------------------------------------------------------
// 遍历 6 个装备槽位，对 setId != None 的物品计数
// 返回固定 4 项数组（Warrior/Sage/Wind/Guardian 顺序），件数 0-3
// 调用者根据件数查 LootSystem::GetSetBonus 获取加成（>=2 件才激活）
// ============================================================================
std::array<std::pair<cu::EquipmentSet, int>, 4> InventorySystem::GetActiveSetCounts() const noexcept {
    std::array<std::pair<cu::EquipmentSet, int>, 4> result = {{
        {cu::EquipmentSet::Warrior,  0},
        {cu::EquipmentSet::Sage,     0},
        {cu::EquipmentSet::Wind,      0},
        {cu::EquipmentSet::Guardian, 0},
    }};
    for (const auto& slot : slots_) {
        if (!slot.item.has_value()) continue;
        const cu::EquipmentSet sid = slot.item->setId;
        if (sid == cu::EquipmentSet::None) continue;
        for (auto& entry : result) {
            if (entry.first == sid) { ++entry.second; break; }
        }
    }
    return result;
}

// ============================================================================
// 第二十三轮新增：ApplySetBonuses  将激活套装（>=2 件）的加成应用到玩家属性
// ----------------------------------------------------------------------------
// 应用顺序：装备词缀（ApplyToPlayerStats）→ 套装加成（本方法）
// 与圣物/层变异 multiplier 是乘法叠加关系
// 注意：maxHp 修改不影响 currentHp（Health 同步在 Game::recomputePlayerStats 末尾统一处理）
// ============================================================================
void InventorySystem::ApplySetBonuses(PlayerStats& stats) const {
    auto counts = GetActiveSetCounts();
    for (const auto& [setId, pieces] : counts) {
        if (pieces < 2) continue;
        // 2 件套加成
        auto [type2, val2] = LootSystem::GetSetBonus(setId, 2);
        if (type2 != SetBonusType::None) {
            applySetBonusToStats(stats, type2, val2);
        }
        // 3 件套加成（仅满 3 件时）
        if (pieces >= 3) {
            auto [type3, val3] = LootSystem::GetSetBonus(setId, 3);
            if (type3 != SetBonusType::None) {
                applySetBonusToStats(stats, type3, val3);
            }
        }
    }
}

// ============================================================================
// GetEquippedCount  获取已装备数量
// ============================================================================
int InventorySystem::GetEquippedCount() const noexcept {
    int count = 0;
    for (const auto& slot : slots_) {
        if (slot.item.has_value()) ++count;
    }
    return count;
}

// ============================================================================
// GetBackpackCount  获取大背包已用格数
// ============================================================================
int InventorySystem::GetBackpackCount() const noexcept {
    int count = 0;
    for (const auto& item : backpack_) {
        if (item.has_value()) ++count;
    }
    return count;
}

// ============================================================================
// IsBackpackFull  大背包是否已满
// ============================================================================
bool InventorySystem::IsBackpackFull() const noexcept {
    return GetBackpackCount() >= kBackpackSize;
}

// ============================================================================
// DebugPrint  调试输出所有装备信息
// ============================================================================
void InventorySystem::DebugPrint() const {
    LOG_INFO("==== 当前装备 ====");
    for (const auto& slot : slots_) {
        if (slot.item.has_value()) {
            const Item& item = slot.item.value();
            LOG_INFO("  [%s] %s (品质=%s, ilvl=%d, 词缀=%zu)",
                     LootSystem::GetSlotName(slot.slot),
                     item.name.c_str(),
                     LootSystem::GetQualityName(item.quality),
                     item.ilvl,
                     item.affixes.size());
            for (const auto& affix : item.affixes) {
                LOG_INFO("    - %s: %.2f%s",
                         LootSystem::GetAffixTypeName(affix.type),
                         affix.value,
                         affix.isPercent ? "%" : "");
            }
        } else {
            LOG_INFO("  [%s] (空)", LootSystem::GetSlotName(slot.slot));
        }
    }
    LOG_INFO("==================");
}

} // namespace cu