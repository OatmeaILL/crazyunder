#include "gameplay/MerchantSystem.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "rendering/TextureAtlas.h"
#include "rendering/Renderer.h"
#include "gameplay/InventorySystem.h"
#include "gameplay/Player.h"
#include "utils/Logger.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace cu {

// ---- 商人价格常量 ----
// 基础价 × 品质倍率 × (1 + ilvl * 0.1)
static constexpr int kBasePrice = 15;
static constexpr float kQualityPriceMul[] = {1.0f, 4.0f, 7.0f, 16.0f}; // White/Blue/Yellow/DarkGold
static constexpr float kSellPriceRatio = 0.5f; // 出售价 = 购买价 × 0.5

MerchantSystem::MerchantSystem() {
    for (auto& s : stock_) {
        s.sold = false;
        s.price = 0;
    }
    for (auto& s : skillStock_) {
        s.sold = false;
        s.price = 0;
        s.type = SkillType::Count;
    }
}

void MerchantSystem::Initialize(Registry& registry, const TextureAtlas& atlas) {
    registry_ = &registry;
    atlas_ = &atlas;
    active_ = false;
    merchantEntity_ = kInvalidEntity;
}

// ============================================================================
// SpawnMerchant  在指定位置生成商人并刷新库存
// ============================================================================
void MerchantSystem::SpawnMerchant(sf::Vector2f position, int dungeonLevel) {
    if (!registry_) {
        LOG_ERROR("MerchantSystem 未初始化，无法生成商人");
        return;
    }

    // 若已有商人实体，先清除
    if (merchantEntity_ != kInvalidEntity && active_) {
        ClearMerchant();
    }

    merchantPos_ = position;
    active_ = true;

    // 创建商人实体（挂载 Transform + Sprite）
    merchantEntity_ = registry_->CreateEntity();
    auto& transform = registry_->AddComponent<Transform>(merchantEntity_);
    transform.position = position;
    transform.scale = sf::Vector2f(1.f, 1.f);

    auto& sprite = registry_->AddComponent<Sprite>(merchantEntity_);
    sprite.color = sf::Color::White;
    sprite.origin = sf::Vector2f(16.f, 16.f);
    // 商人贴图名 "merchant"
    if (atlas_) {
        sprite.sourceRect = atlas_->GetPixelRect("merchant");
    }

    registry_->AddComponent<Tag>(merchantEntity_).flags = TagFlag::Prop;

    // 生成库存
    generateStock(dungeonLevel);
    generateSkillStock(dungeonLevel);

    LOG_INFO("商人已生成于 (%.1f, %.1f)，层数=%d，装备 %d 件 + 技能 %d 个",
             position.x, position.y, dungeonLevel, kMerchantStockSize, kMerchantSkillSize);
}

// ============================================================================
// ClearMerchant  清除商人（进入下一层时调用）
// ============================================================================
void MerchantSystem::ClearMerchant() {
    if (merchantEntity_ != kInvalidEntity && registry_) {
        registry_->DestroyEntity(merchantEntity_);
        merchantEntity_ = kInvalidEntity;
    }
    active_ = false;
    for (auto& s : stock_) {
        s.sold = false;
        s.price = 0;
        s.item = Item{};
    }
    for (auto& s : skillStock_) {
        s.sold = false;
        s.price = 0;
        s.type = SkillType::Count;
    }
}

// ============================================================================
// IsPlayerInRange  检查玩家是否在商人交互范围内
// ============================================================================
bool MerchantSystem::IsPlayerInRange(sf::Vector2f playerPos) const {
    if (!active_) return false;
    sf::Vector2f diff = playerPos - merchantPos_;
    float distSq = diff.x * diff.x + diff.y * diff.y;
    return distSq <= kInteractRange * kInteractRange;
}

// ============================================================================
// CalcBuyPrice  计算物品购买价格
// ============================================================================
int MerchantSystem::CalcBuyPrice(const Item& item) noexcept {
    int qIdx = static_cast<int>(item.quality);
    if (qIdx < 0 || qIdx > 3) qIdx = 0;
    // 第十七轮新增：价格乘以变异系统 multiplier（默认 1.0=无影响）
    float price = kBasePrice * kQualityPriceMul[qIdx] * (1.f + item.ilvl * 0.1f) * sPriceMul_;
    return std::max(1, static_cast<int>(price));
}

// ============================================================================
// CalcSellPrice  计算物品出售价格（半价回收）
// ============================================================================
int MerchantSystem::CalcSellPrice(const Item& item) noexcept {
    return std::max(1, static_cast<int>(CalcBuyPrice(item) * kSellPriceRatio));
}

// ============================================================================
// 第十七轮新增：变异系统商人价格 multiplier 静态接口
// ============================================================================
float MerchantSystem::sPriceMul_ = 1.f; // 静态成员定义，默认 1.0=无影响

void MerchantSystem::SetModifierPriceMul(float m) noexcept {
    // 防御性下限：multiplier 必须 > 0，避免价格为 0 导致可白嫖
    sPriceMul_ = (m > 0.1f) ? m : 1.f;
}

float MerchantSystem::GetModifierPriceMul() noexcept {
    return sPriceMul_;
}

// ============================================================================
// BuyItem  购买指定索引的物品
// ----------------------------------------------------------------------------
// 返回 true 表示购买成功（金币足够、未售出、背包未满）
// ============================================================================
bool MerchantSystem::BuyItem(int index, InventorySystem& inventory, PlayerStats& stats) {
    if (index < 0 || index >= kMerchantStockSize) return false;
    MerchantItem& mi = stock_[index];
    if (mi.sold) return false;
    if (stats.coins < mi.price) return false;
    if (inventory.IsBackpackFull()) return false;

    // 扣金币
    stats.coins -= mi.price;
    // 物品放入背包（不自动装备，直接入背包）
    inventory.AddToBackpack(mi.item);
    mi.sold = true;

    LOG_INFO("购买 %s，花费 %d 金币，剩余 %d 金币",
             mi.item.name.c_str(), mi.price, stats.coins);
    return true;
}

// ============================================================================
// SellBackpackItem  出售背包内指定索引的物品
// ----------------------------------------------------------------------------
// 返回出售所得金币（0 表示失败）
// ============================================================================
int MerchantSystem::SellBackpackItem(int backpackIndex, InventorySystem& inventory, PlayerStats& stats) {
    auto opt = inventory.GetBackpackItem(backpackIndex);
    if (!opt.has_value()) return 0;

    Item item = opt.value();
    int sellPrice = CalcSellPrice(item);

    // 从背包移除
    inventory.RemoveFromBackpack(backpackIndex);
    // 增加金币
    stats.coins += sellPrice;

    LOG_INFO("出售 %s，获得 %d 金币，总金币 %d",
             item.name.c_str(), sellPrice, stats.coins);
    return sellPrice;
}

// ============================================================================
// generateStock  生成商人库存（6 件随机装备，品质随层数）
// ============================================================================
void MerchantSystem::generateStock(int dungeonLevel) {
    for (int i = 0; i < kMerchantStockSize; ++i) {
        ItemQuality q = rollQualityByLevel(dungeonLevel);
        // ilvl 随层数增长（1-5层 ilvl=1-3，6-10层 ilvl=3-5，11+层 ilvl=5-8）
        int ilvl = std::max(1, std::min(8, 1 + dungeonLevel / 2));
        Item item = generateRandomItem(ilvl, q);
        stock_[i].item = item;
        stock_[i].price = CalcBuyPrice(item);
        stock_[i].sold = false;
    }
}

// ============================================================================
// rollQualityByLevel  基于层数的品质随机（与 LootSystem 一致）
// ----------------------------------------------------------------------------
// 前面层也有小概率出好货（与掉落规则一致）
// ============================================================================
ItemQuality MerchantSystem::rollQualityByLevel(int level) const {
    int L = level;
    float white  = std::max(0.10f, 0.85f - L * 0.05f);
    float blue   = std::min(0.55f, 0.10f + L * 0.04f);
    float yellow = (L >= 6) ? std::min(0.25f, (L - 5) * 0.03f) : 0.f;
    float darkGold = (L >= 10) ? std::min(0.15f, (L - 9) * 0.02f) : 0.f;

    float sum = white + blue + yellow + darkGold;
    if (sum <= 0.f) return ItemQuality::White;

    float r = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * sum;
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
// generateRandomItem  生成单件随机物品（复用 LootSystem 的词缀逻辑）
// ============================================================================
Item MerchantSystem::generateRandomItem(int ilvl, ItemQuality quality) const {
    Item item;
    item.ilvl = ilvl;
    item.quality = quality;
    item.slot = static_cast<ItemSlot>(std::rand() % 6);

    // 生成名称
    const char* prefix = "";
    switch (quality) {
        case ItemQuality::White:    prefix = ""; break;
        case ItemQuality::Blue:     prefix = "稀有 "; break;
        case ItemQuality::Yellow:   prefix = "史诗 "; break;
        case ItemQuality::DarkGold: prefix = "传说 "; break;
    }
    item.name = std::string(prefix) + LootSystem::GetSlotName(item.slot);

    // 生成词缀（按品质数量）
    int count = 1;
    float qualityMul = 0.8f;
    switch (quality) {
        case ItemQuality::White:    count = 1; qualityMul = 0.8f; break;
        case ItemQuality::Blue:     count = 2; qualityMul = 1.0f; break;
        case ItemQuality::Yellow:   count = 3; qualityMul = 1.2f; break;
        case ItemQuality::DarkGold: count = 4; qualityMul = 1.5f; break;
    }

    for (int i = 0; i < count; ++i) {
        // 随机选择词缀类型（避免重复）
        std::vector<AffixType> available;
        for (int t = 0; t <= 8; ++t) {
            AffixType at = static_cast<AffixType>(t);
            bool dup = false;
            for (const auto& a : item.affixes) {
                if (a.type == at) { dup = true; break; }
            }
            if (!dup) available.push_back(at);
        }
        if (available.empty()) break;

        Affix affix;
        affix.type = available[std::rand() % available.size()];

        float ilvlScale = 1.f + (ilvl - 1) * 0.05f;
        float fluctuation = 0.8f + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 0.4f;
        float baseMul = qualityMul * ilvlScale * fluctuation;

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
        item.affixes.push_back(affix);
    }

    return item;
}

// ============================================================================
// Render  渲染商人实体
// ============================================================================
void MerchantSystem::Render(Renderer& renderer) {
    // 商人实体已挂载 Sprite + Transform，由 Renderer 统一批量渲染
    // 此处无需额外操作，保留接口供未来扩展（如商人头顶图标）
    (void)renderer;
}

// ============================================================================
// BuySkill  购买指定索引的技能
// ============================================================================
bool MerchantSystem::BuySkill(int index, PlayerComponent& pc, PlayerStats& stats) {
    if (index < 0 || index >= kMerchantSkillSize) return false;
    MerchantSkill& ms = skillStock_[index];
    if (ms.sold || ms.type == SkillType::Count) return false;
    if (stats.coins < ms.price) return false;
    if (PlayerHasSkill(pc, ms.type)) return false; // 已拥有
    if (IsSkillBackpackFull(pc)) {
        // 背包满，检查技能槽是否有空位
        bool hasEmptySlot = false;
        for (int i = 0; i < kSkillSlotCount; ++i) {
            if (pc.skillSlots[i].type == SkillType::Count) { hasEmptySlot = true; break; }
        }
        if (!hasEmptySlot) return false;
    }

    // 扣金币
    stats.coins -= ms.price;
    // 添加技能到玩家
    AddSkillToBackpack(pc, ms.type);
    ms.sold = true;

    LOG_INFO("购买技能 %s，花费 %d 金币，剩余 %d 金币",
             GetSkillName(ms.type), ms.price, stats.coins);
    return true;
}

// ============================================================================
// generateSkillStock  生成商人技能库存（2 个随机技能）
// ============================================================================
void MerchantSystem::generateSkillStock(int dungeonLevel) {
    // 技能价格：基础 150 金币（x5倍），随层数增长
    static constexpr int kSkillBasePrice = 150;

    // 随机选择2个不重复的技能
    int skillTypes[static_cast<int>(SkillType::Count)];
    for (int i = 0; i < static_cast<int>(SkillType::Count); ++i) {
        skillTypes[i] = i;
    }
    // Fisher-Yates 洗牌
    for (int i = static_cast<int>(SkillType::Count) - 1; i > 0; --i) {
        int j = std::rand() % (i + 1);
        int tmp = skillTypes[i];
        skillTypes[i] = skillTypes[j];
        skillTypes[j] = tmp;
    }

    for (int i = 0; i < kMerchantSkillSize; ++i) {
        skillStock_[i].type = static_cast<SkillType>(skillTypes[i]);
        skillStock_[i].price = kSkillBasePrice + dungeonLevel * 25; // x5倍：原30+level*5 → 150+level*25
        skillStock_[i].sold = false;
    }
}

} // namespace cu
