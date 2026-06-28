#pragma once

// ============================================================================
// HUD —— 游戏内抬头显示（Phase 8）
// ----------------------------------------------------------------------------
// 职责：
//   在屏幕空间绘制游戏内 HUD，包括：
//     1. 左下：血条（红色，渐变）+ 蓝条（蓝色）+ 经验条（黄色，底部全宽）
//     2. 左上：等级 Lv.X + 当前 HP/MP 数值
//     3. 右上：波次信息 Wave X + 敌人数
//     4. 右下：小地图（地牢房间缩略图，当前房间高亮）
//     5. 技能图标（左下角上方）：普攻/闪避/AOE 三个图标 + 冷却覆盖
//     6. FPS（右上角小字）
//
// 渲染：
//   所有 HUD 元素在屏幕空间绘制，不参与 Y-Sort。
//   使用 sf::RectangleShape、sf::Text、sf::Sprite 代码绘制，无外部 UI 库。
//   技能图标与小地图背景使用 TextureGenerator 过程化生成的纹理。
//
// 使用方式：
//   1. Initialize(font) 初始化（加载字体与纹理）
//   2. 每帧 Update(stats, wave, enemyCount, fps, dungeon) 更新数据
//   3. 每帧 Render(target) 绘制到屏幕
// ============================================================================

#include <SFML/Graphics.hpp>
#include <deque>
#include <string>
#include "gameplay/Player.h"
#include "gameplay/SkillSystem.h"

namespace cu {

struct Dungeon; // 前向声明
class RoomSystem;

// ---- 成就 Toast 通知条目 ----
// 成就解锁时由 Game 层推入 HUD，显示在屏幕右上角，自动淡出。
struct AchievementToast {
    std::string name;           // 成就名称（中文）
    std::string description;    // 成就描述（中文）
    float lifetime = 0.f;       // 剩余显示时间（秒）
    float maxLifetime = 4.0f;   // 总显示时长（用于淡入淡出计算）
    float slideInTimer = 0.f;   // 滑入动画计时器（0~0.3s）
};

class HUD {
public:
    HUD();
    ~HUD() = default;

    // 初始化：加载字体与过程化纹理
    void Initialize(const sf::Font& font);

    // 更新 HUD 数据
    // stats: 玩家属性（HP/MP/EXP/Level）
    // wave: 当前波次号
    // enemyCount: 当前敌人数
    // fps: 当前帧率
    // dungeon: 地牢数据（用于小地图）
    // currentRoomIndex: 当前房间索引（-1=走廊）
    void Update(const PlayerStats& stats, int wave, int enemyCount, int fps,
                const Dungeon& dungeon, int currentRoomIndex);

    // 渲染 HUD 到目标
    void Render(sf::RenderTarget& target) const;

    // 设置技能冷却（0~1，1=完全冷却中，0=可用）
    void SetSkillCooldown(int skillIndex, float progress);

    // 设置技能槽冷却数据（来自技能系统，4个槽位）
    void SetSkillSlotData(const std::array<SkillInstance, kSkillSlotCount>& slots);

    // 设置未使用的技能点数（用于显示提示）
    void SetSkillPoints(int points) { skillPoints_ = points; }

    // 设置玩家世界位置（用于小地图玩家标记）
    void SetPlayerPosition(sf::Vector2f pos) { playerPos_ = pos; }

    // 第三十轮新增：设置商人世界位置（用于小地图 $ 标记）
    void SetMerchantPosition(sf::Vector2f pos, bool active) {
        merchantPos_ = pos; merchantActive_ = active;
    }

    // ---- 成就 Toast 通知接口 ----
    // 推入一条成就解锁通知（最多保留 4 条，超出则丢弃最旧）
    void AddAchievementToast(const std::string& name, const std::string& description);
    // 每帧更新 Toast 计时器与动画（dt 为固定步长）
    void UpdateToasts(float dt);

    // ---- 第十八轮新增：连击系统接口 ----
    // 由 Game 层每帧调用，传入当前 combo 数、剩余保持时间、对应伤害乘数
    // HUD 内部根据 comboCount > 0 决定是否渲染中央连击指示器
    void SetComboData(int comboCount, float comboTimer, float damageMul) {
        // 检测 combo 增长：新值大于旧值时触发脉冲动画（数字放大回弹）
        if (comboCount > comboCount_) {
            comboPulseTimer_ = 0.3f; // 0.3s 衰减期
        }
        comboCount_ = comboCount;
        comboTimer_ = comboTimer;
        comboDamageMul_ = damageMul;
    }

    // ---- 第二十轮新增：极限闪避系统接口 ----
    // 由 Game 层每帧调用，传入 buff 剩余时间与冷却剩余时间
    // HUD 根据 buffTimer > 0 渲染屏幕金色光环边框（标识 buff 激活）
    void SetPerfectDodgeData(float buffTimer, float cooldown) {
        perfectDodgeBuffTimer_ = buffTimer;
        perfectDodgeCooldown_ = cooldown;
    }

    // 技能槽总数（3 个实际技能 + 4 个占位技能槽）
    static constexpr int kHudSkillCount = 7;

private:
    const sf::Font* font_ = nullptr;

    // ---- 进度条数据 ----
    float hpValue_ = 100.f;
    float hpMax_ = 100.f;
    float mpValue_ = 50.f;
    float mpMax_ = 50.f;
    float expValue_ = 0.f;
    float expMax_ = 100.f;
    int level_ = 1;
    int coins_ = 0;

    // ---- 状态信息 ----
    int wave_ = 0;
    int enemyCount_ = 0;
    int fps_ = 0;
    int skillPoints_ = 0;  // 未使用的技能点数（>0 时显示提示）

    // ---- 第十八轮新增：连击系统数据（由 Game 层 SetComboData 推入）----
    int   comboCount_ = 0;       // 当前连击数
    float comboTimer_ = 0.f;     // 连击剩余保持时间（秒）
    float comboDamageMul_ = 1.f; // 当前连击对应的伤害乘数（用于显示，如 +20% / +35%）
    // 连击淡入动画计时器：每次击杀重置为 0.3s，期间数字放大并淡入
    mutable float comboPulseTimer_ = 0.f;

    // ---- 第二十轮新增：极限闪避系统数据（由 Game 层 SetPerfectDodgeData 推入）----
    float perfectDodgeBuffTimer_ = 0.f;  // 极限闪避伤害 buff 剩余（>0 时渲染金色光环）
    float perfectDodgeCooldown_ = 0.f;   // 极限闪避冷却剩余（用于调试/未来 UI 扩展）

    // ---- 小地图数据 ----
    const Dungeon* dungeon_ = nullptr;
    int currentRoomIndex_ = -1;
    sf::Vector2f playerPos_{0.f, 0.f};  // 玩家世界坐标（用于小地图）
    sf::Vector2f merchantPos_{0.f, 0.f}; // 商人世界坐标（用于小地图 $ 标记）
    bool merchantActive_ = false;         // 商人是否活跃

    // ---- 技能冷却（0=可用，1=完全冷却）----
    // 0=普攻(LMB), 1=闪避(RMB), 2=AOE(SPC), 3-6=占位技能槽(1/2/3/4)
    float skillCooldown_[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

    // ---- 技能系统技能槽数据（4个）----
    std::array<SkillInstance, kSkillSlotCount> skillSlots_;

    // ---- 过程化纹理 ----
    // 0=普攻(剑), 1=闪避(盾), 2=AOE(爆炸), 3-6=占位(锁)
    sf::Texture skillIcons_[7];
    sf::Texture minimapBg_;

    // ---- 文本对象（复用避免每帧创建）----
    mutable sf::Text textCache_;

    // ---- 成就 Toast 通知队列（右上角，最多 4 条同时显示）----
    // 使用 deque 支持头部弹出（过期通知）。新通知从底部追加，向上堆叠。
    // 每条通知生命周期 4s：前 0.3s 滑入，中间稳定显示，最后 0.5s 淡出。
    mutable std::deque<AchievementToast> toasts_;
    static constexpr int kMaxToasts = 4;          // 同屏最大 Toast 数
    static constexpr float kToastWidth = 280.f;    // Toast 宽度
    static constexpr float kToastHeight = 56.f;    // Toast 高度
    static constexpr float kToastSpacing = 6.f;    // Toast 间距
    static constexpr float kToastSlideInDuration = 0.3f;  // 滑入动画时长

    // ---- 绘制辅助 ----
    void drawProgressBar(sf::RenderTarget& target, sf::Vector2f pos, sf::Vector2f size,
                         float progress, sf::Color fillColor,
                         bool gradient = false) const;
    void drawSkillIcon(sf::RenderTarget& target, sf::Vector2f pos, int skillIndex) const;
    void drawMinimap(sf::RenderTarget& target, sf::Vector2f pos) const;
    void drawText(sf::RenderTarget& target, const std::string& str,
                  sf::Vector2f pos, unsigned int size, sf::Color color,
                  sf::Text::Style style = sf::Text::Regular) const;
    void drawAchievementToasts(sf::RenderTarget& target) const;
    // ---- 第十八轮新增：连击系统 HUD 渲染 ----
    // 屏幕中央上方显示"连击 X"+"伤害 +Y%"+保持时间进度条
    // 仅在 comboCount_ >= 5 时显示（小连击不打扰玩家视线）
    void drawCombo(sf::RenderTarget& target) const;
    // ---- 第二十轮新增：极限闪避 buff 光环渲染 ----
    // 屏幕四周渲染金色脉冲边框，标识极限闪避 buff 激活（伤害 +50%）
    // 仅在 perfectDodgeBuffTimer_ > 0 时显示
    void drawPerfectDodge(sf::RenderTarget& target) const;
};

} // namespace cu
