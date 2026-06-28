#pragma once

// ============================================================================
// FloorModifier —— 地牢变异系统（Roguelike 维度拓展，第十七轮新增）
// ----------------------------------------------------------------------------
// 设计意图：
//   Roguelike 标志性的"层修饰符"机制。每层地牢随机获得 1-2 个修饰符，
//   每个修饰符都有正负效果配对（双刃剑设计），强制玩家调整策略。
//   与现有"升级 + 装备 + 圣物"三重成长正交叠加，让每局游戏体验差异化，
//   产生"本层该如何应对"的策略决策点，提升重玩价值。
//
// 规则：
//   - 第 1 层：无修饰符（让玩家熟悉基础玩法）
//   - 第 2-4 层：1 个修饰符
//   - 第 5+ 层：2 个修饰符（效果叠加，乘法复合）
//   - 从池中 Fisher-Yates 洗牌随机抽取，不重复
//   - 跨层重新滚动（每次进入新层重新决定），存档时持久化当前层的修饰符
//
// 数值平衡原则：
//   - 所有 multiplier 控制在 0.6-2.0 范围内，避免极端数值
//   - 每个修饰符必有正负两面，纯增益/纯减益都不被接受
//   - 修饰符之间可叠加，但同种 multiplier 取乘法（如两个 +20% HP → ×1.44）
//
// 应用位置（最小侵入）：
//   - EnemySpawner: SpawnEnemyAt 应用 enemyHpMul/Damage/MoveSpeed/AttackSpeed
//   - EnemySpawner: StartWave 应用 spawnIntervalMul（影响刷怪节奏）
//   - Game::recomputePlayerStats: 末尾应用 playerDamageMul/MaxHp/MoveSpeed/PickupRange
//   - Game::OnKill: expValue/coinValue 乘以 expMul/coinMul
//   - LootSystem::OnEnemyKilled: 掉落概率乘以 itemDropChanceMul
//   - MerchantSystem::CalcBuyPrice: 价格乘以 merchantPriceMul
//   - Game::updatePlaying: 每秒按 playerRegenPerSec 回血
//   - Game::OnKill heart drop: heartDropDisabled 跳过
// ============================================================================

#include <array>
#include <cstdint>
#include <string>

namespace sf { class Color; }

namespace cu {

// ---- 修饰符类型枚举 ----
enum class FloorModifierType : uint8_t {
    None        = 0, // 无修饰符（第 1 层 / 未激活）
    Bloodlust   = 1, // 嗜血狂暴：敌人攻速+30%，但敌人生命-30%
    Frenzy      = 2, // 狂乱冲刺：敌人移速+30%，但敌人生命-20%
    Greed       = 3, // 贪婪之雾：金币掉落+100%，但商人价格+50%
    Fortune     = 4, // 福星高照：经验获取+50%，但敌人生命+25%
    Glass       = 5, // 玻璃大炮：玩家伤害+50%，但玩家最大生命-25%
    Swarm       = 6, // 虫群涌动：敌人伤害-30%，但刷怪间隔-40%（更多怪）
    Haste       = 7, // 疾风步法：玩家移速+20%，但敌人移速+15%
    Regen       = 8, // 生命之涌：每秒回 1% HP，但爱心掉落禁用
    Curse       = 9, // 诅咒之地：敌人伤害+20%，但装备掉率+40%
    Wrath       = 10, // 暴怒之力：玩家伤害+30%，但玩家移速-15%
    // ---- 第十九轮新增：雷霆系变异（闪电流 build 的"层修饰符"维度）----
    Thunderstorm= 11, // 雷暴领域：玩家攻击附加 Lightning 元素，但敌人伤害+15%

    Count           // 枚举边界（用于数组大小）
};

// 修饰符槽位数（单层最多 2 个修饰符）
inline constexpr int kFloorModifierSlotCount = 2;

// ---- 修饰符静态数据 ----
struct FloorModifierData {
    FloorModifierType type = FloorModifierType::None;
    const char* name = "";        // 中文名称
    const char* description = ""; // 中文描述（含正负效果）
    uint8_t r = 255, g = 255, b = 255; // UI 主色调（用于 Banner / HUD 指示）

    // ---- 乘法字段（默认 1.0 = 无影响）----
    float enemyHpMul          = 1.f;
    float enemyDamageMul      = 1.f;
    float enemyMoveSpeedMul   = 1.f;
    float enemyAttackSpeedMul = 1.f; // 影响攻击冷却（>1=更快攻击）
    float spawnIntervalMul    = 1.f; // 影响刷怪间隔（<1=更频繁）

    float playerDamageMul     = 1.f;
    float playerMaxHpMul      = 1.f;
    float playerMoveSpeedMul  = 1.f;
    float playerPickupRangeMul = 1.f;

    float coinMul             = 1.f;
    float expMul              = 1.f;
    float merchantPriceMul    = 1.f;
    float itemDropChanceMul   = 1.f;

    // ---- 非乘法字段 ----
    float playerRegenPerSec   = 0.f; // 每秒回血比例（占最大生命），0=无
    bool  heartDropDisabled   = false;
    // ---- 第十九轮新增：雷暴领域专用标志 ----
    // true = 玩家所有普攻子弹自动附加 Lightning 元素 + 麻痹效果
    // 设计意图：让"闪电流 build"从"圣物+升级"二维扩展到"层修饰符"第三维度，
    // 即使玩家未抽到 chainLightning 升级/雷霆圣物，进入雷暴领域层也可
    // 临时体验闪电流玩法，鼓励适应性策略调整。
    bool  playerAttackLightning = false;
};

// ---- 全局查询接口（数据表查找，O(1)）----
[[nodiscard]] const FloorModifierData& GetFloorModifierData(FloorModifierType type) noexcept;

// 获取修饰符中文名（用于 UI 显示）
[[nodiscard]] const char* GetFloorModifierName(FloorModifierType type) noexcept;

// ============================================================================
// FloorModifierSystem —— 当前层的修饰符状态
// ============================================================================
class FloorModifierSystem {
public:
    FloorModifierSystem() = default;
    ~FloorModifierSystem() = default;

    // 清空当前修饰符（重置回 None）
    void Clear() noexcept;

    // 根据层数随机滚动修饰符
    //   level=1: 清空（无修饰符）
    //   level=2-4: 1 个
    //   level>=5: 2 个（不重复）
    // 已拥有的修饰符不会重复抽取（同层内）
    void RollForLevel(int level);

    // 从存档恢复（覆盖当前状态）
    void Deserialize(const std::array<uint8_t, kFloorModifierSlotCount>& ids) noexcept;

    // 序列化到存档
    [[nodiscard]] std::array<uint8_t, kFloorModifierSlotCount> Serialize() const noexcept;

    // ---- 查询接口（聚合所有激活修饰符的乘法复合）----
    // 多个修饰符的乘法按"累乘"复合：两个 ×1.2 → ×1.44
    [[nodiscard]] float GetEnemyHpMul() const noexcept;
    [[nodiscard]] float GetEnemyDamageMul() const noexcept;
    [[nodiscard]] float GetEnemyMoveSpeedMul() const noexcept;
    [[nodiscard]] float GetEnemyAttackSpeedMul() const noexcept;
    [[nodiscard]] float GetSpawnIntervalMul() const noexcept;

    [[nodiscard]] float GetPlayerDamageMul() const noexcept;
    [[nodiscard]] float GetPlayerMaxHpMul() const noexcept;
    [[nodiscard]] float GetPlayerMoveSpeedMul() const noexcept;
    [[nodiscard]] float GetPlayerPickupRangeMul() const noexcept;

    [[nodiscard]] float GetCoinMul() const noexcept;
    [[nodiscard]] float GetExpMul() const noexcept;
    [[nodiscard]] float GetMerchantPriceMul() const noexcept;
    [[nodiscard]] float GetItemDropChanceMul() const noexcept;

    [[nodiscard]] float GetPlayerRegenPerSec() const noexcept;
    [[nodiscard]] bool  IsHeartDropDisabled() const noexcept;
    // ---- 第十九轮新增：雷暴领域查询 ----
    // 返回 true 表示当前层至少有一个激活的修饰符开启了 playerAttackLightning
    [[nodiscard]] bool  IsPlayerAttackLightning() const noexcept;

    // ---- 应用到玩家属性（在 recomputePlayerStats 末尾调用）----
    // 直接修改 PlayerStats 的 damage/maxHp/moveSpeed/pickupRange 字段
    void ApplyToPlayerStats(float& damage, float& maxHp,
                            float& moveSpeed, float& pickupRange) const noexcept;

    // ---- 状态查询 ----
    [[nodiscard]] int GetActiveCount() const noexcept;
    [[nodiscard]] bool HasModifier(FloorModifierType type) const noexcept;
    [[nodiscard]] FloorModifierType GetSlot(int index) const noexcept;
    // 用于 UI 显示：返回当前激活的修饰符列表（最多 2 个，None 不返回）
    [[nodiscard]] std::array<FloorModifierType, kFloorModifierSlotCount> GetActiveModifiers() const noexcept;

    // 调试 / UI：拼接当前修饰符的简短描述（如 "嗜血狂暴 + 福星高照"）
    [[nodiscard]] std::string GetActiveSummary() const;

private:
    // 当前激活的修饰符列表（最多 kFloorModifierSlotCount 个，None 表示空槽）
    std::array<FloorModifierType, kFloorModifierSlotCount> active_{};
};

} // namespace cu
