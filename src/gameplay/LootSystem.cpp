#include "gameplay/LootSystem.h"
#include "gameplay/InventorySystem.h"
#include "gameplay/CombatEffects.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "rendering/TextureAtlas.h"
#include "rendering/Renderer.h"
#include "core/Input.h"
#include "core/AudioManager.h"
#include "utils/Logger.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace cu {

// ============================================================================
// 品质颜色/名称查询（数据驱动）
// ============================================================================
sf::Color LootSystem::GetQualityColor(ItemQuality q) noexcept {
    switch (q) {
        // 普通：浅灰白
        case ItemQuality::White:    return sf::Color(220, 220, 220, 255);
        // 稀有：蓝色
        case ItemQuality::Blue:     return sf::Color( 80, 140, 255, 255);
        // 史诗：紫色（与其他系统颜色区分：红=伤害、绿=回复、金=经验、黄=暴击）
        case ItemQuality::Yellow:   return sf::Color(180,  80, 255, 255);
        // 传说：亮金色（偏橙金，与经验金 255,215,0 区分）
        case ItemQuality::DarkGold: return sf::Color(255, 180,  30, 255);
    }
    return sf::Color::White;
}

const char* LootSystem::GetQualityName(ItemQuality q) noexcept {
    switch (q) {
        case ItemQuality::White:    return "普通";
        case ItemQuality::Blue:     return "稀有";
        case ItemQuality::Yellow:   return "史诗";
        case ItemQuality::DarkGold: return "传说";
    }
    return "?";
}

// 品质前缀（用于飘字显示）
const char* LootSystem::GetQualityPrefix(ItemQuality q) noexcept {
    switch (q) {
        case ItemQuality::White:    return "普通级";
        case ItemQuality::Blue:     return "稀有级";
        case ItemQuality::Yellow:   return "史诗级";
        case ItemQuality::DarkGold: return "传说级";
    }
    return "";
}

const char* LootSystem::GetSlotName(ItemSlot s) noexcept {
    switch (s) {
        case ItemSlot::Weapon: return "武器";
        case ItemSlot::Helmet: return "头盔";
        case ItemSlot::Chest:  return "胸甲";
        case ItemSlot::Boots:  return "靴子";
        case ItemSlot::Ring:   return "戒指";
        case ItemSlot::Amulet: return "项链";
    }
    return "?";
}

const char* LootSystem::GetAffixTypeName(AffixType t) noexcept {
    switch (t) {
        case AffixType::AddedDamage:  return "附加伤害";
        case AffixType::AddedDefense: return "附加防御";
        case AffixType::CritRate:     return "暴击率";
        case AffixType::CritDamage:   return "暴击伤害";
        case AffixType::MoveSpeed:    return "移动速度";
        case AffixType::AttackSpeed:  return "攻击速度";
        case AffixType::Lifesteal:    return "吸血";
        case AffixType::MaxHp:        return "最大生命";
        case AffixType::MaxMp:        return "最大法力";
    }
    return "?";
}

// ============================================================================
// 第二十三轮新增：装备套装系统辅助接口实现
// ----------------------------------------------------------------------------
// 4 套装数据表（每个套装覆盖 3 个固定槽位 + 2/3 件加成）
// 设计意图：每个槽位恰属 2 个套装，玩家无法同时凑齐 2 个完整套装（强制取舍）
// ============================================================================
const char* LootSystem::GetSetName(EquipmentSet s) noexcept {
    switch (s) {
        case EquipmentSet::None:     return "";
        case EquipmentSet::Warrior:  return "战士之怒";
        case EquipmentSet::Sage:     return "智者之识";
        case EquipmentSet::Wind:     return "疾风行者";
        case EquipmentSet::Guardian: return "永恒守护";
    }
    return "";
}

std::array<ItemSlot, 3> LootSystem::GetSetSlots(EquipmentSet s) noexcept {
    switch (s) {
        case EquipmentSet::Warrior:  return {ItemSlot::Weapon, ItemSlot::Chest,  ItemSlot::Ring};
        case EquipmentSet::Sage:     return {ItemSlot::Helmet, ItemSlot::Amulet, ItemSlot::Ring};
        case EquipmentSet::Wind:      return {ItemSlot::Boots,  ItemSlot::Weapon, ItemSlot::Helmet};
        case EquipmentSet::Guardian:  return {ItemSlot::Chest,  ItemSlot::Boots,  ItemSlot::Amulet};
        case EquipmentSet::None:     break;
    }
    return {ItemSlot::Weapon, ItemSlot::Weapon, ItemSlot::Weapon};
}

sf::Color LootSystem::GetSetColor(EquipmentSet s) noexcept {
    switch (s) {
        case EquipmentSet::Warrior:  return sf::Color(220,  80,  60, 255); // 红
        case EquipmentSet::Sage:     return sf::Color(100, 160, 240, 255); // 蓝
        case EquipmentSet::Wind:     return sf::Color(120, 220, 240, 255); // 青
        case EquipmentSet::Guardian: return sf::Color(180, 200, 120, 255); // 暗金绿
        case EquipmentSet::None:     break;
    }
    return sf::Color::White;
}

std::pair<SetBonusType, float> LootSystem::GetSetBonus(EquipmentSet s, int pieces) noexcept {
    if (pieces == 2) {
        switch (s) {
            // 2 件套：方向性小幅奖励，让玩家凑齐前 2 件就有正反馈
            case EquipmentSet::Warrior:  return {SetBonusType::DamageMul,    0.10f}; // +10% 伤害
            case EquipmentSet::Sage:     return {SetBonusType::ExpMulAdd,    0.20f}; // +20% 经验
            case EquipmentSet::Wind:     return {SetBonusType::MoveSpeedMul, 0.12f}; // +12% 移速
            case EquipmentSet::Guardian: return {SetBonusType::MaxHpMul,     0.15f}; // +15% 生命
            case EquipmentSet::None:     break;
        }
    } else if (pieces == 3) {
        switch (s) {
            // 3 件套：build 核心差异化，与套装主题深度契合
            case EquipmentSet::Warrior:  return {SetBonusType::CritDamageAdd, 0.25f}; // +25% 暴击伤害
            case EquipmentSet::Sage:     return {SetBonusType::CritRateAdd,   0.08f}; // +8% 暴击率
            case EquipmentSet::Wind:     return {SetBonusType::AttackSpeedMul, 0.20f}; // +20% 攻速
            case EquipmentSet::Guardian: return {SetBonusType::DefenseAdd,    10.f};  // +10 防御
            case EquipmentSet::None:     break;
        }
    }
    return {SetBonusType::None, 0.f};
}

EquipmentSet LootSystem::RollSetForSlot(ItemSlot slot) noexcept {
    // 每个槽位恰属 2 个套装，等概率随机选择（无套装=None 不在此处产生）
    // Weapon: Warrior / Wind
    // Helmet: Sage  / Wind
    // Chest:  Warrior / Guardian
    // Boots:  Wind  / Guardian
    // Ring:   Warrior / Sage
    // Amulet: Sage  / Guardian
    switch (slot) {
        case ItemSlot::Weapon: return (std::rand() & 1) ? EquipmentSet::Warrior : EquipmentSet::Wind;
        case ItemSlot::Helmet: return (std::rand() & 1) ? EquipmentSet::Sage    : EquipmentSet::Wind;
        case ItemSlot::Chest:  return (std::rand() & 1) ? EquipmentSet::Warrior : EquipmentSet::Guardian;
        case ItemSlot::Boots:  return (std::rand() & 1) ? EquipmentSet::Wind    : EquipmentSet::Guardian;
        case ItemSlot::Ring:   return (std::rand() & 1) ? EquipmentSet::Warrior : EquipmentSet::Sage;
        case ItemSlot::Amulet: return (std::rand() & 1) ? EquipmentSet::Sage    : EquipmentSet::Guardian;
    }
    return EquipmentSet::None;
}

// ============================================================================
// LootSystem 构造与初始化
// ============================================================================
LootSystem::LootSystem() = default;

void LootSystem::Initialize(Registry& registry, TextureAtlas& atlas) {
    registry_ = &registry;
    atlas_ = &atlas;
    drops_.clear();
    drops_.reserve(64);
    LOG_INFO("LootSystem 已初始化");
}

// ============================================================================
// 随机数辅助
// ============================================================================
float LootSystem::randomFloat(float min, float max) const {
    return min + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (max - min);
}

int LootSystem::randomInt(int min, int max) const {
    if (max <= min) return min;
    return min + (std::rand() % (max - min + 1));
}

bool LootSystem::randomChance(float probability) const {
    return randomFloat(0.f, 1.f) < probability;
}

// ============================================================================
// 品质权重随机选择
// ----------------------------------------------------------------------------
// blueWeight/yellowWeight/darkGoldWeight: 各品质权重（0~1）
// 白装权重 = 1 - sum(其他)
// ============================================================================
ItemQuality LootSystem::rollQuality(float blueWeight,
                                    float yellowWeight,
                                    float darkGoldWeight) const {
    float r = randomFloat(0.f, 1.f);
    float acc = 0.f;
    acc += darkGoldWeight;
    if (r < acc) return ItemQuality::DarkGold;
    acc += yellowWeight;
    if (r < acc) return ItemQuality::Yellow;
    acc += blueWeight;
    if (r < acc) return ItemQuality::Blue;
    return ItemQuality::White;
}

// ============================================================================
// rollQualityByLevel  基于当前层数的品质随机
// ----------------------------------------------------------------------------
// 规则：
//   - 前3层：白色为主（85%），蓝色少量（15%）
//   - 3层起：白色概率下降，蓝色增加
//   - 6层后：紫色出现
//   - 10层后：金色出现
// 公式（L = dungeonLevel_）：
//   white  = max(0.10, 0.85 - L*0.05)
//   blue   = min(0.55, 0.10 + L*0.04)
//   yellow = L >= 6 ? min(0.25, (L-5)*0.03) : 0
//   darkGold = L >= 10 ? min(0.15, (L-9)*0.02) : 0
// ============================================================================
ItemQuality LootSystem::rollQualityByLevel() const {
    int L = dungeonLevel_;
    float white  = std::max(0.10f, 0.85f - L * 0.05f);
    float blue   = std::min(0.55f, 0.10f + L * 0.04f);
    float yellow = (L >= 6) ? std::min(0.25f, (L - 5) * 0.03f) : 0.f;
    float darkGold = (L >= 10) ? std::min(0.15f, (L - 9) * 0.02f) : 0.f;

    // 归一化（确保总和为 1）
    float sum = white + blue + yellow + darkGold;
    if (sum <= 0.f) return ItemQuality::White;

    float r = randomFloat(0.f, sum);
    float acc = 0.f;
    acc += darkGold;
    if (r < acc) return ItemQuality::DarkGold;
    acc += yellow;
    if (r < acc) return ItemQuality::Yellow;
    acc += blue;
    if (r < acc) return ItemQuality::Blue;
    return ItemQuality::White;
}

// ============================================================================
// rollBossQualityByLevel  Boss 专用：基于层数的品质随机（提升一档）
// ----------------------------------------------------------------------------
// 在 rollQualityByLevel 基础上将品质向上提升一级：
//   White -> Blue, Blue -> Yellow, Yellow -> DarkGold, DarkGold -> DarkGold
// 这样前几层 Boss 主要掉蓝装，随层数增加出现紫/金，符合"前几层掉不了好东西"
// 但比普通怪更好的设定。
// ============================================================================
ItemQuality LootSystem::rollBossQualityByLevel() const {
    ItemQuality q = rollQualityByLevel();
    switch (q) {
        case ItemQuality::White:    return ItemQuality::Blue;
        case ItemQuality::Blue:     return ItemQuality::Yellow;
        case ItemQuality::Yellow:   return ItemQuality::DarkGold;
        case ItemQuality::DarkGold: return ItemQuality::DarkGold;
    }
    return ItemQuality::Blue;
}

// ============================================================================
// 生成物品名称（品质前缀 + 槽位名）
// ----------------------------------------------------------------------------
// 名称格式："<品质前缀> <槽位名>"，如 "传说级 武器"、"史诗级 头盔"
// 普通品质无前缀，直接显示槽位名
// ============================================================================
std::string LootSystem::generateItemName(ItemQuality q, ItemSlot s) const {
    const char* prefix = "";
    switch (q) {
        case ItemQuality::White:    prefix = ""; break;
        case ItemQuality::Blue:     prefix = "稀有 "; break;
        case ItemQuality::Yellow:   prefix = "史诗 "; break;
        case ItemQuality::DarkGold: prefix = "传说 "; break;
    }
    return std::string(prefix) + GetSlotName(s);
}

// ============================================================================
// 生成单个词缀（避免重复类型）
// ----------------------------------------------------------------------------
// qualityMultiplier: 品质数值倍率（White=0.8, Blue=1.0, Yellow=1.2, DarkGold=1.5）
// ilvl: 物品等级，影响词缀基础数值
// 随机浮动 +-20%
// ============================================================================
Affix LootSystem::rollSingleAffix(const Item& item, float qualityMultiplier) const {
    Affix affix;
    // 随机选择词缀类型（避免与已有词缀重复）
    std::vector<AffixType> available;
    for (int i = 0; i <= 8; ++i) {
        AffixType t = static_cast<AffixType>(i);
        bool dup = false;
        for (const auto& a : item.affixes) {
            if (a.type == t) { dup = true; break; }
        }
        if (!dup) available.push_back(t);
    }
    if (available.empty()) {
        affix.type = AffixType::AddedDamage;
        affix.value = 0.f;
        return affix;
    }
    affix.type = available[randomInt(0, static_cast<int>(available.size()) - 1)];

    // ilvl 缩放系数：每级 +5% 数值
    float ilvlScale = 1.f + (item.ilvl - 1) * 0.05f;
    // 随机浮动 +-20%
    float fluctuation = randomFloat(0.8f, 1.2f);
    float baseMul = qualityMultiplier * ilvlScale * fluctuation;

    // 各词缀类型的基础数值与是否百分比
    switch (affix.type) {
        case AffixType::AddedDamage:
            affix.isPercent = false;
            affix.value = 3.f * baseMul;
            break;
        case AffixType::AddedDefense:
            affix.isPercent = false;
            affix.value = 5.f * baseMul;
            break;
        case AffixType::CritRate:
            affix.isPercent = true;
            affix.value = 0.03f * baseMul;
            break;
        case AffixType::CritDamage:
            affix.isPercent = true;
            affix.value = 0.10f * baseMul;
            break;
        case AffixType::MoveSpeed:
            affix.isPercent = true;
            affix.value = 0.05f * baseMul;
            break;
        case AffixType::AttackSpeed:
            affix.isPercent = true;
            affix.value = 0.05f * baseMul;
            break;
        case AffixType::Lifesteal:
            affix.isPercent = true;
            affix.value = 0.02f * baseMul;
            break;
        case AffixType::MaxHp:
            affix.isPercent = false;
            affix.value = 10.f * baseMul;
            break;
        case AffixType::MaxMp:
            affix.isPercent = false;
            affix.value = 5.f * baseMul;
            break;
    }
    return affix;
}

// ============================================================================
// 生成词缀（按品质数量）
// ----------------------------------------------------------------------------
// White   -> 1 词缀，数值倍率 0.8
// Blue    -> 2 词缀，数值倍率 1.0
// Yellow  -> 3 词缀，数值倍率 1.2
// DarkGold-> 4 词缀，数值倍率 1.5
// ============================================================================
void LootSystem::generateAffixes(Item& item) const {
    int count = 1;
    float qualityMul = 0.8f;
    switch (item.quality) {
        case ItemQuality::White:    count = 1; qualityMul = 0.8f; break;
        case ItemQuality::Blue:     count = 2; qualityMul = 1.0f; break;
        case ItemQuality::Yellow:   count = 3; qualityMul = 1.2f; break;
        case ItemQuality::DarkGold: count = 4; qualityMul = 1.5f; break;
    }
    for (int i = 0; i < count; ++i) {
        item.affixes.push_back(rollSingleAffix(item, qualityMul));
    }
}

// ============================================================================
// 生成一件随机物品
// ============================================================================
Item LootSystem::generateRandomItem(int ilvl, ItemQuality forcedQuality) const {
    Item item;
    item.ilvl = ilvl;
    item.quality = forcedQuality;
    item.slot = static_cast<ItemSlot>(randomInt(0, 5));
    item.name = generateItemName(item.quality, item.slot);
    generateAffixes(item);
    // 第二十三轮新增：按槽位分配所属套装（每件装备必属 1 个套装）
    // 设计意图：让所有掉落装备都参与套装 build，玩家无需寻找"特殊套装掉落"
    item.setId = RollSetForSlot(item.slot);
    return item;
}

// ============================================================================
// 掉落一件物品到世界
// ============================================================================
void LootSystem::dropItem(const Item& item, sf::Vector2f pos) {
    if (drops_.size() >= 50) {
        // 超过上限，移除最旧的
        drops_.erase(drops_.begin());
    }
    LootDropEntry drop;
    drop.item = item;
    drop.position = pos;
    drop.pickedUp = false;
    drop.lifetime = 60.f;
    drop.glowPhase = randomFloat(0.f, 6.28f);
    drops_.push_back(std::move(drop));

    // 触发掉落回调（粒子特效）
    if (OnItemDropped) OnItemDropped(pos, item.quality);
}

// ============================================================================
// OnEnemyKilled  敌人击杀掉落
// ----------------------------------------------------------------------------
// 掉落规则：
//   普通怪：10% 掉 1 件白装
//   精英怪：100% 掉 1-2 件，70% 蓝/25% 黄/5% 暗金
//   Boss：100% 掉 3 件，30% 黄/70% 暗金
// ============================================================================
void LootSystem::OnEnemyKilled(EntityId enemy, sf::Vector2f pos, EnemyType type, bool isChampion) {
    if (!registry_) return;

    // ilvl 根据敌人类型决定（Champion 按 Elite 等级 ilvl=3）
    int ilvl = 1;
    switch (type) {
        case EnemyType::Melee:   ilvl = 1; break;
        case EnemyType::Ranged:  ilvl = 2; break;
        case EnemyType::Suicide: ilvl = 2; break;
        case EnemyType::Elite:   ilvl = 3; break;
        case EnemyType::Boss:    ilvl = 5; break;
    }
    if (isChampion) ilvl = std::max(ilvl, 3); // Champion 至少 ilvl=3

    switch (type) {
        case EnemyType::Melee:
        case EnemyType::Ranged:
        case EnemyType::Suicide: {
            if (isChampion) {
                // 精英强化版普通怪：100% 掉 1-2 件，保底蓝色以上（与 EnemyType::Elite 同等掉落）
                int count = randomInt(1, 2);
                for (int i = 0; i < count; ++i) {
                    ItemQuality q = rollQualityByLevel();
                    if (q == ItemQuality::White) q = ItemQuality::Blue;
                    Item item = generateRandomItem(ilvl, q);
                    sf::Vector2f offset(randomFloat(-20.f, 20.f), randomFloat(-20.f, 20.f));
                    dropItem(item, pos + offset);
                }
            } else {
                // 普通怪：10% 掉 1 件装备，品质随层数递增
                // 第十七轮新增：装备掉率乘以变异系统 multiplier（默认 1.0=无影响）
                // 上限 1.0，避免 100%+ 概率导致每怪必掉
                float dropChance = std::min(1.0f, 0.10f * modItemDropChanceMul_);
                if (randomChance(dropChance)) {
                    ItemQuality q = rollQualityByLevel();
                    Item item = generateRandomItem(ilvl, q);
                    dropItem(item, pos);
                }
            }
            break;
        }
        case EnemyType::Elite: {
            // 精英怪：100% 掉 1-2 件，品质随层数提升但保底蓝色以上
            int count = randomInt(1, 2);
            for (int i = 0; i < count; ++i) {
                // 精英怪：在层数权重基础上提升一档
                ItemQuality q = rollQualityByLevel();
                // 保底蓝色
                if (q == ItemQuality::White) q = ItemQuality::Blue;
                Item item = generateRandomItem(ilvl, q);
                // 小偏移避免重叠
                sf::Vector2f offset(randomFloat(-20.f, 20.f), randomFloat(-20.f, 20.f));
                dropItem(item, pos + offset);
            }
            break;
        }
        case EnemyType::Boss: {
            // Boss：100% 掉 3 件，品质遵循层数递增规则（rollBossQualityByLevel 提升一档）
            // 前几层主要掉蓝装，随层数增加出现紫/金
            for (int i = 0; i < 3; ++i) {
                ItemQuality q = rollBossQualityByLevel();
                Item item = generateRandomItem(ilvl, q);
                sf::Vector2f offset(randomFloat(-30.f, 30.f), randomFloat(-30.f, 30.f));
                dropItem(item, pos + offset);
            }
            break;
        }
    }
}

// ============================================================================
// OnChestOpened  宝箱开启掉落
// ----------------------------------------------------------------------------
// 宝箱房：100% 掉 2 件，品质随层数提升（保底蓝色以上）
// ============================================================================
void LootSystem::OnChestOpened(sf::Vector2f pos) {
    for (int i = 0; i < 2; ++i) {
        // 宝箱：在层数权重基础上提升，保底蓝色
        ItemQuality q = rollQualityByLevel();
        if (q == ItemQuality::White) q = ItemQuality::Blue;
        Item item = generateRandomItem(4, q);
        sf::Vector2f offset(randomFloat(-20.f, 20.f), randomFloat(-20.f, 20.f));
        dropItem(item, pos + offset);
    }
    LOG_INFO("宝箱开启，掉落 2 件装备");
}

// ============================================================================
// OnPotBroken  罐子破坏掉落
// ----------------------------------------------------------------------------
// 罐子：最多掉落 1 件装备，品质随层数调整
// ============================================================================
void LootSystem::OnPotBroken(sf::Vector2f pos) {
    // 罐子掉落概率 50%，品质随层数递增
    // 第十七轮新增：装备掉率乘以变异系统 multiplier（默认 1.0=无影响），上限 1.0
    float dropChance = std::min(1.0f, 0.50f * modItemDropChanceMul_);
    if (randomChance(dropChance)) {
        ItemQuality q = rollQualityByLevel();
        Item item = generateRandomItem(1, q);
        dropItem(item, pos);
    }
}

// ============================================================================
// DropItem  强制掉落指定品质装备（事件房/诅咒房奖励用）
// ============================================================================
void LootSystem::DropItem(sf::Vector2f pos, ItemQuality quality, int ilvl) {
    Item item = generateRandomItem(ilvl, quality);
    sf::Vector2f offset(randomFloat(-20.f, 20.f), randomFloat(-20.f, 20.f));
    dropItem(item, pos + offset);
    LOG_INFO("强制掉落: 品质=%d, ilvl=%d", static_cast<int>(quality), ilvl);
}

// ============================================================================
// Update  处理拾取与生命周期
// ----------------------------------------------------------------------------
// 拾取逻辑：玩家走近自动拾取（距离 < 40px）
// 拾取后调用 InventorySystem.Equip 自动装备，旧装备返回到背包
// ============================================================================
void LootSystem::Update(Registry& registry, EntityId player,
                        const Input& input, InventorySystem& inventory, float dt) {
    (void)input;
    if (!registry_) return;

    Transform* playerTransform = registry.GetComponent<Transform>(player);
    if (!playerTransform) return;

    for (auto& drop : drops_) {
        if (drop.pickedUp) continue;

        // 生命周期衰减
        drop.lifetime -= dt;
        drop.glowPhase += dt * 3.f;

        // "背包已满" 提示冷却递减
        if (drop.fullMessageCooldown > 0.f) {
            drop.fullMessageCooldown -= dt;
        }

        // 拾取检测
        sf::Vector2f diff = playerTransform->position - drop.position;
        float distSq = diff.x * diff.x + diff.y * diff.y;
        if (distSq < kPickupRadius * kPickupRadius) {
            // 智能拾取：空槽自动装备，否则放入大背包
            bool picked = inventory.PickupItem(drop.item);
            if (!picked) {
                // 背包已满且槽位已占用，无法拾取
                // 仅在冷却结束时显示一次提示（避免刷屏）
                if (drop.fullMessageCooldown <= 0.f) {
                    SpawnFloatText(registry, drop.position, "背包已满",
                                   sf::Color(255, 100, 100), 16, 1.5f);
                    drop.fullMessageCooldown = 2.0f; // 2秒冷却
                }
                continue;
            }
            drop.pickedUp = true;

            // 触发拾取回调（任务/成就系统统计拾取数量与品质）
            if (OnItemPickedUp) {
                OnItemPickedUp(drop.item.quality);
            }

            // 拾取装备音效
            AudioManager::Instance().PlaySFX(AudioManager::kSFXCoinPickup);

            // 拾取飘字：显示 "品质级 物品名"，颜色按品质区分
            // 传说级=亮金色，史诗级=紫色，稀有级=蓝色，普通级=白色
            // 与其他系统颜色区分：红=伤害、绿=回复、亮绿=经验、黄=暴击
            // 生命周期 2.5s + 淡入淡出效果
            sf::Color qualityColor = GetQualityColor(drop.item.quality);
            std::string floatText = std::string(GetQualityPrefix(drop.item.quality))
                                  + " " + drop.item.name;
            SpawnFloatText(registry, drop.position, floatText, qualityColor, 18, 2.5f);

            LOG_INFO("拾取装备: %s (品质=%s, ilvl=%d, 词缀数=%zu)",
                     drop.item.name.c_str(),
                     GetQualityName(drop.item.quality),
                     drop.item.ilvl,
                     drop.item.affixes.size());
        }
    }

    // 清理已拾取或过期的掉落物
    drops_.erase(
        std::remove_if(drops_.begin(), drops_.end(),
            [](const LootDropEntry& d) {
                return d.pickedUp || d.lifetime <= 0.f;
            }),
        drops_.end());
}

// ============================================================================
// Render  渲染掉落物（不同品质不同颜色光圈）
// ============================================================================
void LootSystem::Render(Renderer& renderer) {
    for (const auto& drop : drops_) {
        if (drop.pickedUp) continue;

        sf::Color color = GetQualityColor(drop.item.quality);
        // 呼吸效果：光圈大小随时间波动
        float pulse = 0.8f + 0.2f * std::sin(drop.glowPhase);
        float radius = 12.f * pulse;

        // 绘制外圈光晕（大半透明四边形）
        renderer.DrawQuad(drop.position, sf::Vector2f(radius * 2.5f, radius * 2.5f),
                          sf::Color(color.r, color.g, color.b, 60),
                          Layer::Entity);
        // 绘制核心（小不透明四边形）
        renderer.DrawQuad(drop.position, sf::Vector2f(radius, radius),
                          sf::Color(color.r, color.g, color.b, 220),
                          Layer::Entity);
    }
}

} // namespace cu