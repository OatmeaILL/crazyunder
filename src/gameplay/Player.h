#pragma once

// ============================================================================
// Player —— 玩家实体创建与更新
// ----------------------------------------------------------------------------
// 职责：
//   1. CreatePlayer：工厂函数，创建玩家实体并挂载所有必要组件。
//   2. UpdatePlayer：每固定步长调用，处理输入、移动、朝向、动画状态切换。
//
// 玩家组件组成：
//   Transform    —— 位置/旋转/缩放
//   Sprite       —— 渲染贴图（sourceRect 由动画系统更新）
//   Velocity     —— 速度（用于其他系统引用，如碰撞）
//   Collider     —— 碰撞体（圆形，radius=16）
//   Health       —— 生命值（100/100）
//   Tag          —— 标签（Player）
//   AnimationComponent —— 动画状态与帧数据
//   PlayerComponent —— 玩家专属属性（stats、朝向、动画状态）
//
// 移动逻辑：
//   读取 WASD/方向键输入，构建 8 方向移动向量。
//   对角线移动时向量长度 > 1，需归一化避免对角线快移（否则比直线快 41%）。
//   最终位移 = 归一化方向 × moveSpeed × dt。
//
// 朝向逻辑：
//   鼠标位置 → 世界坐标 → 计算玩家到鼠标的角度 → 映射到 4 方向枚举。
//   4 方向划分（以玩家为中心，鼠标相对位置）：
//     角度 ∈ [-45°, 45°)   → Right
//     角度 ∈ [45°, 135°)   → Down
//     角度 ∈ [135°, 225°)  → Left
//     角度 ∈ [225°, 315°)  → Up
// ============================================================================

#include <SFML/Graphics.hpp>
#include "ecs/Entity.h"
#include "ecs/Registry.h"
#include "gameplay/Animation.h"
#include "gameplay/DungeonGenerator.h"
#include "gameplay/SkillSystem.h"

namespace cu {

class Input;
class Camera;

// ---- 玩家属性结构体 ----
// Phase 7 扩展：增加暴击/吸血/拾取范围/经验加成等字段，
// 供 InventorySystem 词缀聚合与 UpgradeSystem 升级效果应用。
struct PlayerStats {
    float moveSpeed = 200.f;     // 移动速度（像素/秒）
    float attackSpeed = 2.0f;    // 攻击速度（次/秒）
    float damage = 10.f;         // 伤害值
    float maxHp = 100.f;         // 最大生命值
    float currentHp = 100.f;     // 当前生命值
    float maxMp = 50.f;          // 最大魔法值
    float currentMp = 50.f;      // 当前魔法值
    int level = 1;               // 等级
    float exp = 0.f;             // 当前经验值
    float expToNext = 100.f;     // 升级所需经验
    int coins = 0;               // 金币数量

    // ---- Phase 7: 战利品/升级扩展属性 ----
    float critChance = 0.15f;    // 暴击率（0~1）
    float critDamage = 1.5f;     // 暴击伤害倍率
    float lifesteal = 0.f;       // 吸血比例（0~1）
    float pickupRange = 80.f;    // 拾取磁吸范围（像素）
    float expMultiplier = 1.f;   // 经验获取倍率
    float coinMultiplier = 1.f;  // 金币掉落倍率（圣物"贪婪之眼"等使用）

    // ---- Phase 7: 升级效果字段（由 UpgradeSystem 应用） ----
    int projectileBonusSplit = 0;     // 子弹分裂加成
    int projectileBonusPierce = 0;    // 子弹穿透加成
    int chainLightning = 0;           // 连锁闪电次数
    float aoeCooldownReduce = 0.f;    // AOE 冷却减少（秒）
    float dodgeCooldownReduce = 0.f;  // 闪避冷却减少（秒）
    float defense = 0.f;              // 防御力（减伤）
    float manaRegen = 0.f;            // 每秒法力回复

    // ---- 第十九轮新增：闪电流 build 专属字段 ----
    // 圣物"风暴之眼"通过此字段延长 Lightning 麻痹时间
    // 默认 1.0 = 基础 0.6s，圣物加成后可达 0.9s（+50%）
    // 应用位置：ProjectileSystem::handleHit / handleChain 中 ApplyStatus 时乘以此倍率
    float lightningDurationMul = 1.f; // Lightning 麻痹持续时间倍率

    // ---- 第十九轮新增：雷暴领域层修饰符标志 ----
    // 由 Game::recomputePlayerStats 根据 floorModifiers_.IsPlayerAttackLightning() 设置
    // true = 当前层为雷暴领域，玩家所有普攻子弹自动附加 Lightning 元素
    // 设计意图：让"闪电流 build"拓展到"层修饰符"维度，
    // 即使未抽到 chainLightning 升级/雷霆圣物，进入雷暴领域层也可临时体验闪电流玩法
    bool  floorLightningActive = false;

    // ---- 第二十轮新增：极限闪避系统字段 ----
    // 设计意图：奖励精准操作，建立"敌人攻击前摇 → 闪避反击 → 伤害爆发"的攻防正反馈循环，
    // 与连击系统（攻击端正反馈）形成"攻防双反馈"，产生"激进技术流"涌现交互。
    //   - dodgeWindowMul：极限闪避窗口倍率（1.0=0.3s，圣物"月光护符"加成后 1.5）
    //   - perfectDodgeGuaranteedCrit：复仇之刃圣物标志，true 时下次攻击强制暴击
    // 应用位置：PlayerCombat 闪避检测 / CombatSystem::ApplyDamage 暴击判定
    float dodgeWindowMul = 1.f;            // 极限闪避窗口倍率（圣物月光护符加成）
    bool  perfectDodgeGuaranteedCrit = false; // 极限闪避后下次攻击必暴击（复仇之刃圣物）
};

// ---- 玩家专属组件 ----
// 存储玩家属性与状态，挂载到玩家实体上。
struct PlayerComponent {
    PlayerStats stats;
    FacingDirection facing = FacingDirection::Down;  // 当前朝向
    PlayerAnimState animState = PlayerAnimState::Idle; // 当前动画状态
    float attackTimer = 0.f;   // 攻击动画剩余时间
    float hurtTimer = 0.f;     // 受击动画剩余时间
    bool wasMoving = false;    // 上一帧是否在移动（用于动画状态切换）

    // ---- Phase 5: 战斗状态字段 ----
    float attackCooldown = 0.f;      // 普攻冷却剩余时间（秒）
    float dodgeCooldown = 0.f;       // 闪避冷却剩余时间（秒）
    float dodgeInvincibility = 0.f;  // 闪避无敌帧剩余时间（秒）
    float dodgeDashTimer = 0.f;      // 冲刺持续剩余时间（秒）
    sf::Vector2f dodgeDirection{0.f, 0.f}; // 冲刺方向（归一化）
    float aoeCooldown = 0.f;         // AOE 技能冷却剩余时间（秒）
    int killCount = 0;               // 累计击杀数
    float footstepTimer = 0.f;       // 脚步声计时器
    float dodgeSoundTimer = 0.f;    // 闪避音效计时器
    int dodgeSoundCount = 0;        // 闪避音效剩余播放次数
    bool dodgeButtonHeld = false;   // 闪避按键是否已按下（防重复触发）

    // ---- 技能系统字段 ----
    std::array<SkillInstance, kSkillSlotCount> skillSlots{};           // 4个技能槽（按键1-4），默认空
    std::array<SkillType, kSkillBackpackSize> skillBackpack{};        // 技能背包（5格），默认空
    float berserkTimer = 0.f;           // 狂暴剩余时间
    float leechStrikeActive = 0.f;      // 吸血打击激活标志（>0=激活）
    int   leechStrikeLevel = 1;         // 吸血打击技能等级（影响吸血比例 30%+10%*(lv-1)）
    float gravityWellTimer = 0.f;       // 引力井剩余时间
    sf::Vector2f gravityWellPos{0.f, 0.f}; // 引力井位置
    float spikeGroundTimer = 0.f;       // 地刺剩余时间
    sf::Vector2f spikeGroundPos{0.f, 0.f}; // 地刺位置
    float spikeTickTimer = 0.f;         // 地刺伤害 tick 计时器（每 0.5s 造成一次伤害）

    // ---- 技能粒子发射计时器（替代 SkillSystem.cpp 中的 static 局部变量）----
    // 此前使用 static float/int 跨函数调用持久化，存在以下问题：
    //   1. 玩家死亡重生后状态残留，首次释放技能时粒子节奏异常；
    //   2. 关卡切换时残留；
    //   3. 未来引入多玩家或观战模式会共享状态。
    // 改为 PlayerComponent 成员字段，随玩家实体生命周期管理。
    float leechParticleTimer = 0.f;       // 吸血打击持续血气粒子计时器
    float berserkParticleTimer = 0.f;     // 狂暴持续烈焰粒子计时器
    int   berserkSparkCounter = 0;        // 狂暴金色火花间隔计数器
    float gravityWellParticleTimer = 0.f; // 引力井持续漩涡粒子计时器
    float spikeParticleTimer = 0.f;       // 地刺持续尖刺粒子计时器
    int   spikeBloodCounter = 0;          // 地刺血红色尖端间隔计数器

    // ---- 第十八轮新增：连击系统字段 ----
    // 设计意图：奖励激进玩法，建立"连续击杀 → 伤害提升 → 击杀加速"的正反馈循环，
    // 产生心流体验，与现有"升级+装备+圣物+元素+层修饰符"五维成长正交叠加。
    //   - comboCount：当前连击数（3 秒内连续击杀累积）
    //   - comboTimer：连击剩余保持时间（每次击杀重置为 3.0s，归零后清空 combo）
    //   - comboMaxThisLife：本次生命最大连击数（用于成就统计与调试）
    // 受伤时 combo 立即重置为 0（避免无脑肉搏），换层/死亡时重置。
    int   comboCount = 0;
    float comboTimer = 0.f;
    int   comboMaxThisLife = 0;

    // ---- 第二十轮新增：极限闪避系统字段 ----
    // 设计意图：与连击系统（攻击端正反馈）形成"攻防双反馈"。
    //   - perfectDodgeCooldown：极限闪避冷却剩余（3s，避免连续触发破坏平衡）
    //   - perfectDodgeBuffTimer：极限闪避伤害加成剩余（2s，+50% 伤害）
    //   - perfectDodgeCount：累计极限闪避次数（成就统计，跨层保留）
    //   - perfectDodgeMaxThisLife：本次生命最大累计值（调试用）
    // 触发条件：闪避时附近 80px 内有敌人且敌人 attackTelegraph > 0（攻击前摇中）
    // 触发效果：buff 计时器 = 2s，cooldown = 3s，反击扇形伤害，屏幕震动+飘字
    // 受伤不重置 perfectDodge（与 combo 不同，buff 是防御反击奖励，不应被攻击惩罚）
    float perfectDodgeCooldown = 0.f;   // 极限闪避冷却剩余（秒）
    float perfectDodgeBuffTimer = 0.f;  // 极限闪避伤害加成剩余（秒，>0 时伤害 ×1.5）
    int   perfectDodgeCount = 0;        // 累计极限闪避次数（跨层保留，成就统计）
    int   perfectDodgeMaxThisLife = 0;  // 本次生命最大累计（调试用）

    // ---- 诅咒房系统字段 ----
    bool cursed = false;                // 是否处于诅咒状态（移速 -30%、攻速 -20%）
    int  cursedRoomIndex = -1;          // 触发诅咒的房间索引（清理后解除）

    // ---- 事件房交互字段 ----
    bool eventPromptActive = false;     // 是否显示事件交互提示
    int  eventRoomIndex = -1;           // 当前可交互的事件房索引

    // ---- 死亡回顾系统 ----
    EntityId lastAttackerEntity = kInvalidEntity; // 最后攻击玩家的敌人实体 ID
    float totalDamageDealt = 0.f;       // 本局总伤害输出（用于 DPS 计算）
};

// ---- 玩家 Sprite Sheet 信息 ----
// 记录玩家贴图在图集中的位置，供动画系统计算帧坐标。
struct PlayerSheetInfo {
    int atlasX = 0;            // Sprite Sheet 在图集中的左上角 X（像素）
    int atlasY = 0;            // Sprite Sheet 在图集中的左上角 Y（像素）
    int frameSize = 32;        // 每帧尺寸
    int framesPerRow = 4;      // 每行帧数
};

// ============================================================================
// CreatePlayer —— 创建玩家实体
// ----------------------------------------------------------------------------
// registry: ECS 注册表
// position: 出生位置（世界坐标）
// sheetInfo: 玩家贴图在图集中的位置信息
// 返回玩家实体 ID
// ============================================================================
[[nodiscard]] EntityId CreatePlayer(Registry& registry,
                                    sf::Vector2f position,
                                    const PlayerSheetInfo& sheetInfo);

// ============================================================================
// UpdatePlayer —— 更新玩家逻辑（每固定步长调用）
// ----------------------------------------------------------------------------
// registry: ECS 注册表
// playerId: 玩家实体 ID
// input: 输入管理器
// camera: 摄像机（用于鼠标世界坐标转换）
// dt: 固定步长时间（秒）
// sheetInfo: 玩家贴图信息（用于动画状态切换时重新设置帧）
// ============================================================================
void UpdatePlayer(Registry& registry, EntityId playerId,
                  const Input& input, const Camera& camera, float dt,
                  const PlayerSheetInfo& sheetInfo,
                  const Dungeon* dungeon = nullptr);

// ============================================================================
// 辅助函数
// ============================================================================

// 根据鼠标世界坐标与玩家位置计算朝向
[[nodiscard]] FacingDirection ComputeFacing(sf::Vector2f playerPos,
                                            sf::Vector2f mouseWorldPos) noexcept;

// 获取玩家朝向角度（度，0=右，逆时针）
[[nodiscard]] float GetPlayerFacingAngle(sf::Vector2f playerPos,
                                         sf::Vector2f mouseWorldPos) noexcept;

} // namespace cu
