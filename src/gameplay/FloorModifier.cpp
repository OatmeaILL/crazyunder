// ============================================================================
// FloorModifier —— 地牢变异系统实现（第十七轮新增）
// ============================================================================

#include "gameplay/FloorModifier.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <vector>

namespace cu {

namespace {

// ---- 修饰符静态数据表 ----
// 索引 [0..Count-1]，索引 0 = None（无修饰符）
// 数值平衡：所有 multiplier 控制在 0.6-2.0，每个修饰符必有正负效果
const FloorModifierData kModifierTable[] = {
    // 0: None
    FloorModifierData{
        FloorModifierType::None, "", "", 255, 255, 255,
        1.f, 1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, false
    },
    // 1: Bloodlust 嗜血狂暴 — 红色
    // 设计：敌人攻击更快但血薄，玩家割草节奏更快但被命中风险高
    FloorModifierData{
        FloorModifierType::Bloodlust, "嗜血狂暴",
        "敌人攻速 +30%，但敌人生命 -30%",
        220, 80, 80,
        0.7f, 1.f, 1.f, 1.3f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, false
    },
    // 2: Frenzy 狂乱冲刺 — 橙色
    // 设计：敌人冲得快但血薄，考验玩家走位
    FloorModifierData{
        FloorModifierType::Frenzy, "狂乱冲刺",
        "敌人移速 +30%，但敌人生命 -20%",
        240, 160, 60,
        0.8f, 1.f, 1.3f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, false
    },
    // 3: Greed 贪婪之雾 — 金色
    // 设计：经济增益 + 商人价格上升，鼓励打怪而非购物
    FloorModifierData{
        FloorModifierType::Greed, "贪婪之雾",
        "金币掉落 +100%，但商人价格 +50%",
        240, 200, 80,
        1.f, 1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        2.0f, 1.f, 1.5f, 1.f,
        0.f, false
    },
    // 4: Fortune 福星高照 — 青色
    // 设计：经验增益但敌人更耐打，成长与挑战并存
    FloorModifierData{
        FloorModifierType::Fortune, "福星高照",
        "经验获取 +50%，但敌人生命 +25%",
        100, 220, 220,
        1.25f, 1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        1.f, 1.5f, 1.f, 1.f,
        0.f, false
    },
    // 5: Glass 玻璃大炮 — 紫色
    // 设计：高风险高回报，秒杀敌人但也容易被秒杀
    FloorModifierData{
        FloorModifierType::Glass, "玻璃大炮",
        "玩家伤害 +50%，但玩家最大生命 -25%",
        180, 120, 220,
        1.f, 1.f, 1.f, 1.f, 1.f,
        1.5f, 0.75f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, false
    },
    // 6: Swarm 虫群涌动 — 绿色
    // 设计：敌人伤害低但数量多，割草快感
    FloorModifierData{
        FloorModifierType::Swarm, "虫群涌动",
        "敌人伤害 -30%，但刷怪间隔 -40%（数量更多）",
        140, 220, 100,
        1.f, 0.7f, 1.f, 1.f, 0.6f,
        1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, false
    },
    // 7: Haste 疾风步法 — 浅蓝
    // 设计：双方都加速，节奏加快
    FloorModifierData{
        FloorModifierType::Haste, "疾风步法",
        "玩家移速 +20%，但敌人移速 +15%",
        120, 220, 240,
        1.f, 1.f, 1.15f, 1.f, 1.f,
        1.f, 1.f, 1.2f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, false
    },
    // 8: Regen 生命之涌 — 粉色
    // 设计：持续续航 + 失去爱心掉落，简化回血路径
    FloorModifierData{
        FloorModifierType::Regen, "生命之涌",
        "每秒回复 1% 最大生命，但爱心掉落禁用",
        220, 120, 180,
        1.f, 1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.01f, true
    },
    // 9: Curse 诅咒之地 — 暗紫
    // 设计：敌人伤害高但装备掉率上升，鼓励冒险
    FloorModifierData{
        FloorModifierType::Curse, "诅咒之地",
        "敌人伤害 +20%，但装备掉率 +40%",
        160, 100, 200,
        1.f, 1.2f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.4f,
        0.f, false
    },
    // 10: Wrath 暴怒之力 — 暗红
    // 设计：高伤害但走位受限，鼓励精准操作
    FloorModifierData{
        FloorModifierType::Wrath, "暴怒之力",
        "玩家伤害 +30%，但玩家移速 -15%",
        200, 60, 60,
        1.f, 1.f, 1.f, 1.f, 1.f,
        1.3f, 1.f, 0.85f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, false
    },
    // 11: Thunderstorm 雷暴领域 — 亮黄（第十九轮新增）
    // 设计：闪电流 build 的"层修饰符"维度。
    // 正效果：玩家所有普攻附加 Lightning 元素 + 0.6s 麻痹（无需 chainLightning 升级）
    // 负效果：敌人伤害 +15%，被麻痹敌人周边的同伴威胁更高
    // 策略点：临时获得控制能力，但容错率下降；与 Glass/Wrath 叠加形成"玻璃闪电流"
    FloorModifierData{
        FloorModifierType::Thunderstorm, "雷暴领域",
        "玩家攻击附加闪电麻痹，但敌人伤害 +15%",
        255, 230, 80,
        1.f, 1.15f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, false, true
    },
};

static_assert(sizeof(kModifierTable) / sizeof(kModifierTable[0]) ==
              static_cast<size_t>(FloorModifierType::Count),
              "kModifierTable 必须覆盖所有 FloorModifierType 枚举值");

// Fisher-Yates 洗牌（局部区间 [first, last)）
// 使用 std::rand 保证与项目其他随机数使用风格一致
void fisherYatesShuffle(std::vector<FloorModifierType>& arr, int first, int last) {
    for (int i = last - 1; i > first; --i) {
        int j = first + (std::rand() % (i - first + 1));
        std::swap(arr[i], arr[j]);
    }
}

} // anonymous namespace

// ---- 全局查询接口实现 ----

const FloorModifierData& GetFloorModifierData(FloorModifierType type) noexcept {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(FloorModifierType::Count)) {
        return kModifierTable[0]; // 越界回退到 None
    }
    return kModifierTable[idx];
}

const char* GetFloorModifierName(FloorModifierType type) noexcept {
    return GetFloorModifierData(type).name;
}

// ---- FloorModifierSystem 实现 ----

void FloorModifierSystem::Clear() noexcept {
    active_.fill(FloorModifierType::None);
}

void FloorModifierSystem::RollForLevel(int level) {
    active_.fill(FloorModifierType::None);

    if (level <= 1) {
        return; // 第 1 层无修饰符
    }

    // 决定本层修饰符数量
    int count = (level >= 5) ? 2 : 1;
    if (count > kFloorModifierSlotCount) count = kFloorModifierSlotCount;

    // 构建候选池：所有非 None 修饰符
    std::vector<FloorModifierType> pool;
    pool.reserve(static_cast<int>(FloorModifierType::Count) - 1);
    for (int t = 1; t < static_cast<int>(FloorModifierType::Count); ++t) {
        pool.push_back(static_cast<FloorModifierType>(t));
    }

    // Fisher-Yates 洗牌整个池
    fisherYatesShuffle(pool, 0, static_cast<int>(pool.size()));

    // 取前 count 个
    for (int i = 0; i < count && i < static_cast<int>(pool.size()); ++i) {
        active_[i] = pool[i];
    }
}

void FloorModifierSystem::Deserialize(
    const std::array<uint8_t, kFloorModifierSlotCount>& ids) noexcept {
    active_.fill(FloorModifierType::None);
    for (int i = 0; i < kFloorModifierSlotCount; ++i) {
        uint8_t v = ids[i];
        if (v == 0) continue; // None
        if (v >= static_cast<uint8_t>(FloorModifierType::Count)) {
            continue; // 越界，跳过保护
        }
        // 防重复（同一槽位已填或前槽位已含此类型）
        FloorModifierType t = static_cast<FloorModifierType>(v);
        bool dup = false;
        for (int j = 0; j < i; ++j) {
            if (active_[j] == t) { dup = true; break; }
        }
        if (dup) continue;
        active_[i] = t;
    }
}

std::array<uint8_t, kFloorModifierSlotCount>
FloorModifierSystem::Serialize() const noexcept {
    std::array<uint8_t, kFloorModifierSlotCount> out{};
    for (int i = 0; i < kFloorModifierSlotCount; ++i) {
        out[i] = static_cast<uint8_t>(active_[i]);
    }
    return out;
}

// ---- 乘法聚合接口（多个修饰符累乘）----

float FloorModifierSystem::GetEnemyHpMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).enemyHpMul;
    }
    return m;
}

float FloorModifierSystem::GetEnemyDamageMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).enemyDamageMul;
    }
    return m;
}

float FloorModifierSystem::GetEnemyMoveSpeedMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).enemyMoveSpeedMul;
    }
    return m;
}

float FloorModifierSystem::GetEnemyAttackSpeedMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).enemyAttackSpeedMul;
    }
    return m;
}

float FloorModifierSystem::GetSpawnIntervalMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).spawnIntervalMul;
    }
    return m;
}

float FloorModifierSystem::GetPlayerDamageMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).playerDamageMul;
    }
    return m;
}

float FloorModifierSystem::GetPlayerMaxHpMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).playerMaxHpMul;
    }
    return m;
}

float FloorModifierSystem::GetPlayerMoveSpeedMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).playerMoveSpeedMul;
    }
    return m;
}

float FloorModifierSystem::GetPlayerPickupRangeMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).playerPickupRangeMul;
    }
    return m;
}

float FloorModifierSystem::GetCoinMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).coinMul;
    }
    return m;
}

float FloorModifierSystem::GetExpMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).expMul;
    }
    return m;
}

float FloorModifierSystem::GetMerchantPriceMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).merchantPriceMul;
    }
    return m;
}

float FloorModifierSystem::GetItemDropChanceMul() const noexcept {
    float m = 1.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) m *= GetFloorModifierData(t).itemDropChanceMul;
    }
    return m;
}

float FloorModifierSystem::GetPlayerRegenPerSec() const noexcept {
    float total = 0.f;
    for (auto t : active_) {
        if (t != FloorModifierType::None) total += GetFloorModifierData(t).playerRegenPerSec;
    }
    return total;
}

bool FloorModifierSystem::IsHeartDropDisabled() const noexcept {
    for (auto t : active_) {
        if (t != FloorModifierType::None && GetFloorModifierData(t).heartDropDisabled) {
            return true;
        }
    }
    return false;
}

// 第十九轮新增：雷暴领域查询
bool FloorModifierSystem::IsPlayerAttackLightning() const noexcept {
    for (auto t : active_) {
        if (t != FloorModifierType::None && GetFloorModifierData(t).playerAttackLightning) {
            return true;
        }
    }
    return false;
}

void FloorModifierSystem::ApplyToPlayerStats(float& damage, float& maxHp,
                                              float& moveSpeed,
                                              float& pickupRange) const noexcept {
    damage     *= GetPlayerDamageMul();
    maxHp      *= GetPlayerMaxHpMul();
    moveSpeed  *= GetPlayerMoveSpeedMul();
    pickupRange *= GetPlayerPickupRangeMul();
}

// ---- 状态查询 ----

int FloorModifierSystem::GetActiveCount() const noexcept {
    int n = 0;
    for (auto t : active_) {
        if (t != FloorModifierType::None) ++n;
    }
    return n;
}

bool FloorModifierSystem::HasModifier(FloorModifierType type) const noexcept {
    for (auto t : active_) {
        if (t == type) return true;
    }
    return false;
}

FloorModifierType FloorModifierSystem::GetSlot(int index) const noexcept {
    if (index < 0 || index >= kFloorModifierSlotCount) return FloorModifierType::None;
    return active_[index];
}

std::array<FloorModifierType, kFloorModifierSlotCount>
FloorModifierSystem::GetActiveModifiers() const noexcept {
    return active_;
}

std::string FloorModifierSystem::GetActiveSummary() const {
    std::ostringstream oss;
    bool first = true;
    for (auto t : active_) {
        if (t == FloorModifierType::None) continue;
        if (!first) oss << " + ";
        oss << GetFloorModifierName(t);
        first = false;
    }
    if (first) oss << "无变异";
    return oss.str();
}

} // namespace cu
