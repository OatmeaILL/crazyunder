#pragma once

// ============================================================================
// Menus —— 各状态菜单系统（Phase 8）
// ----------------------------------------------------------------------------
// 职责：
//   提供游戏各状态下的菜单界面，使用代码绘制（sf::RectangleShape/sf::Text），
//   无外部 UI 库。所有菜单继承自 UIElement，由 UIManager 管理层级。
//
// 菜单类型：
//   1. MainMenu：主菜单（标题 + Start/Quit 按钮）
//   2. PauseMenu：暂停菜单（半透明遮罩 + Resume/Restart/Quit 按钮）
//   3. DeathScreen：死亡结算（YOU DIED + 统计信息 + Restart/Main Menu 按钮）
//   4. VictoryScreen：胜利结算（VICTORY + 统计信息 + Continue/Main Menu 按钮）
//   5. UpgradeChoiceMenu：升级选择（3 张卡片，鼠标悬停高亮，点击或按 1/2/3 选择）
//   6. InventoryMenu：背包界面（6 个装备槽位 + 词缀列表）
//
// 使用方式：
//   1. Initialize(font) 初始化（创建按钮等子元素）
//   2. 每帧 Update(dt) 更新（处理悬停等）
//   3. 每帧 Render(target) 绘制
//   4. 通过 GetButtonXXX() 获取按钮指针以注册回调
//   5. 升级菜单：SetOptions() 设置 3 个升级选项
//   6. 背包菜单：SetInventory() 设置装备数据
// ============================================================================

#include <SFML/Graphics.hpp>
#include <array>
#include <string>
#include <functional>
#include "ui/UIManager.h"
#include "gameplay/UpgradeSystem.h"
#include "gameplay/LootSystem.h"
#include "gameplay/InventorySystem.h"
#include "gameplay/MerchantSystem.h"
#include "gameplay/SkillSystem.h"
#include "gameplay/RelicSystem.h"
#include "core/SaveSystem.h"

namespace cu {

// ============================================================================
// MainMenu —— 主菜单
// ============================================================================
class MainMenu : public UIElement {
public:
    MainMenu();

    void Initialize(const sf::Font& font);

    // 获取按钮指针以注册回调
    Button* GetStartButton() { return startBtn_; }
    Button* GetLoadGameButton() { return loadGameBtn_; }
    Button* GetQuitButton() { return quitBtn_; }
    Button* GetSettingsButton() { return settingsBtn_; }
    Button* GetSoulWellButton() { return soulWellBtn_; } // 第二十四轮新增：灵魂之井入口

    void Update(float dt) override;
    void Render(sf::RenderTarget& target) const override;

private:
    const sf::Font* font_ = nullptr;
    Button* startBtn_ = nullptr;
    Button* loadGameBtn_ = nullptr;
    Button* quitBtn_ = nullptr;
    Button* settingsBtn_ = nullptr;
    Button* soulWellBtn_ = nullptr; // 第二十四轮新增：灵魂之井按钮

    // 简单粒子动画数据
    struct Particle {
        sf::Vector2f pos;
        sf::Vector2f vel;
        float life;
    };
    std::vector<Particle> particles_;
    float particleTimer_ = 0.f;

    void updateParticles(float dt);
};

// ============================================================================
// PauseMenu —— 暂停菜单
// ============================================================================
class PauseMenu : public UIElement {
public:
    PauseMenu();

    void Initialize(const sf::Font& font);

    Button* GetResumeButton() { return resumeBtn_; }
    Button* GetRestartButton() { return restartBtn_; }
    Button* GetSaveButton() { return saveBtn_; }
    Button* GetQuitToMenuButton() { return quitBtn_; }

    void Render(sf::RenderTarget& target) const override;

private:
    const sf::Font* font_ = nullptr;
    Button* resumeBtn_ = nullptr;
    Button* restartBtn_ = nullptr;
    Button* saveBtn_ = nullptr;
    Button* quitBtn_ = nullptr;
};

// ============================================================================
// DeathScreen —— 死亡结算
// ============================================================================
class DeathScreen : public UIElement {
public:
    DeathScreen();

    void Initialize(const sf::Font& font);

    Button* GetRestartButton() { return restartBtn_; }
    Button* GetMainMenuButton() { return menuBtn_; }

    // 设置统计信息
    void SetStats(int kills, int level, float survivalTime);
    // 第二十四轮新增：设置本次死亡获得的灵魂碎片数（用于显示 meta progression 反馈）
    void SetShardsGained(int shards) { shardsGained_ = shards; }
    // 第三十轮新增：死亡回顾信息
    void SetDeathReview(const std::string& killerName, int combo, float dps) {
        killerName_ = killerName; comboAtDeath_ = combo; dps_ = dps;
    }

    void Render(sf::RenderTarget& target) const override;

private:
    const sf::Font* font_ = nullptr;
    Button* restartBtn_ = nullptr;
    Button* menuBtn_ = nullptr;
    int kills_ = 0;
    int level_ = 1;
    float survivalTime_ = 0.f;
    int shardsGained_ = 0; // 第二十四轮新增：本次死亡获得的灵魂碎片
    // 第三十轮新增：死亡回顾
    std::string killerName_ = "";
    int comboAtDeath_ = 0;
    float dps_ = 0.f;
};

// ============================================================================
// VictoryScreen —— 胜利结算
// ============================================================================
class VictoryScreen : public UIElement {
public:
    VictoryScreen();

    void Initialize(const sf::Font& font);

    Button* GetContinueButton() { return continueBtn_; }
    Button* GetMainMenuButton() { return menuBtn_; }

    // 设置统计信息
    void SetStats(int kills, int level, float survivalTime);

    void Render(sf::RenderTarget& target) const override;

private:
    const sf::Font* font_ = nullptr;
    Button* continueBtn_ = nullptr;
    Button* menuBtn_ = nullptr;

    int kills_ = 0;
    int level_ = 1;
    float survivalTime_ = 0.f;
};

// ============================================================================
// UpgradeChoiceMenu —— 升级选择菜单
// ============================================================================
class UpgradeChoiceMenu : public UIElement {
public:
    UpgradeChoiceMenu();

    void Initialize(const sf::Font& font);

    // 设置 3 个升级选项
    void SetOptions(const std::array<UpgradeOption, 3>& options);

    // 处理按键输入（1/2/3 选择）
    // 返回选中的索引（-1=未选择）
    int HandleKeyInput(int key);

    // 检查鼠标点击是否命中某张卡片
    // 返回命中的卡片索引（-1=未命中）
    int HandleMouseClick(sf::Vector2f mousePos) const;

    // 设置悬停的卡片索引（-1=无悬停）
    void SetHoveredCard(int index);

    // 获取选中的索引
    [[nodiscard]] int GetSelectedIndex() const noexcept { return selectedIndex_; }

    // 重置选择状态
    void ResetSelection() { selectedIndex_ = -1; hoveredCard_ = -1; }

    void Update(float dt) override;
    void Render(sf::RenderTarget& target) const override;

private:
    const sf::Font* font_ = nullptr;
    std::array<UpgradeOption, 3> options_;
    std::array<sf::FloatRect, 3> cardBounds_;
    int hoveredCard_ = -1;
    int selectedIndex_ = -1;

    // 卡片品质颜色
    sf::Color getQualityColor(const UpgradeOption& opt) const;
};

// ============================================================================
// RelicChoiceMenu —— 圣物选择菜单（第十五轮新增）
// ----------------------------------------------------------------------------
// Boss 击败后弹出 3 选 1 圣物选择界面，玩家选择一个圣物加入构筑。
// 复用 UpgradeChoiceMenu 的卡片布局模式（3 张卡片水平排列）。
//
// 交互：
//   鼠标悬停 → 边框高亮
//   鼠标点击卡片 或 按 1/2/3 键 → 选择对应圣物
//   选项数量可变（1-3 个），剩余位置显示"无可用"占位
// ============================================================================
class RelicChoiceMenu : public UIElement {
public:
    RelicChoiceMenu();

    void Initialize(const sf::Font& font);

    // 设置圣物选项（数量 0-3，None 占位被忽略）
    void SetOptions(const std::vector<RelicType>& relics);

    // 处理按键输入（1/2/3 选择），返回选中的索引（-1=未选择）
    int HandleKeyInput(int key);

    // 检查鼠标点击是否命中卡片，返回索引（-1=未命中）
    [[nodiscard]] int HandleMouseClick(sf::Vector2f mousePos) const;

    // 设置悬停卡片索引（-1=无悬停）
    void SetHoveredCard(int index) { hoveredCard_ = index; }

    [[nodiscard]] int GetSelectedIndex() const noexcept { return selectedIndex_; }
    void ResetSelection() { selectedIndex_ = -1; hoveredCard_ = -1; }

    void Update(float dt) override;
    void Render(sf::RenderTarget& target) const override;

private:
    const sf::Font* font_ = nullptr;
    std::array<RelicType, 3> options_{};     // 卡片对应的圣物（None=无效占位）
    std::array<sf::FloatRect, 3> cardBounds_{};
    int hoveredCard_ = -1;
    int selectedIndex_ = -1;
};



// ============================================================================
// InventoryMenu —— 背包菜单（6 装备槽 + 25 格大背包）
// ----------------------------------------------------------------------------
// 布局：
//   顶部标题 "背包"
//   左侧：6 个装备槽（2 行 x 3 列）
//   中间：竖线分隔
//   右侧：25 格大背包（5 行 x 5 列）
//
// 交互：
//   鼠标悬停某格 → 边框闪烁高亮（被选中反馈）
//   左键点击装备槽 → 卸下装备到背包（若背包已满则提示）
//   左键点击背包格 → 装备到对应槽位（若槽位已有装备则交换）
// ============================================================================
class InventoryMenu : public UIElement {
public:
    InventoryMenu();

    void Initialize(const sf::Font& font);

    // 设置装备数据（含大背包）
    void SetInventory(const InventorySystem& inventory);

    // 更新（用于闪烁动画）
    void Update(float dt) override;

    // 处理鼠标悬停（更新 hoveredSlot_ / hoveredBackpack_）
    void UpdateHover(sf::Vector2f mousePos);

    // 处理鼠标左键点击
    // 返回值：{操作码, 索引}
    //   操作码：0=无操作, 1=卸下装备槽[索引], 2=装备背包[索引]
    [[nodiscard]] std::pair<int, int> HandleClick(sf::Vector2f mousePos) const;

    // 设置技能数据
    void SetSkillData(const PlayerComponent& pc);

    // 处理技能左键点击
    // 返回值：{操作码, 索引}
    //   操作码：0=无操作, 3=卸下技能槽[索引], 4=装备技能背包[索引]
    [[nodiscard]] std::pair<int, int> HandleSkillClick(sf::Vector2f mousePos) const;

    // ---- 右键上下文菜单 ----
    // 右键点击格子时打开子菜单，返回 true 表示打开了菜单
    bool HandleRightClick(sf::Vector2f mousePos);
    // 关闭上下文菜单
    void CloseContextMenu() { contextMenuVisible_ = false; }
    // 处理上下文菜单内的点击
    // 返回值：{action, targetInfo}
    //   action: 0=无点击, 1=装备/卸下, 2=丢弃
    //   targetInfo: {targetType, index} targetType: 1=装备槽 2=背包格 3=技能槽 4=技能背包
    [[nodiscard]] std::pair<int, std::pair<int, int>> HandleContextMenuClick(sf::Vector2f mousePos) const;
    // 上下文菜单是否可见
    [[nodiscard]] bool IsContextMenuVisible() const { return contextMenuVisible_; }

    void Render(sf::RenderTarget& target) const override;

private:
    const sf::Font* font_ = nullptr;
    std::array<EquipmentSlot, 6> slots_;
    std::array<std::optional<Item>, InventorySystem::kBackpackSize> backpack_;
    std::unordered_map<AffixType, float> totalAffixes_;

    // 装备槽位边界（用于鼠标交互）
    std::array<sf::FloatRect, 6> slotBounds_;
    // 大背包格边界（5 行 x 5 列）
    std::array<sf::FloatRect, InventorySystem::kBackpackSize> backpackBounds_;

    // 技能槽位边界（4个技能槽）
    std::array<sf::FloatRect, kSkillSlotCount> skillSlotBounds_;
    // 技能背包格边界（5格）
    std::array<sf::FloatRect, kSkillBackpackSize> skillBackpackBounds_;

    // 技能数据
    std::array<SkillInstance, kSkillSlotCount> skillSlots_;
    std::array<SkillType, kSkillBackpackSize> skillBackpack_;

    // 悬停状态（-1 = 无悬停）
    int hoveredSlot_ = -1;
    int hoveredBackpack_ = -1;
    // 技能悬停状态
    int hoveredSkillSlot_ = -1;
    int hoveredSkillBackpack_ = -1;
    // 闪烁计时器（用于边框闪烁动画）
    float blinkTimer_ = 0.f;

    // 鼠标位置（用于 tooltip 显示）
    sf::Vector2f mousePos_;

    // ---- 右键上下文菜单状态 ----
    bool contextMenuVisible_ = false;
    sf::Vector2f contextMenuPos_;           // 菜单左上角位置
    // 目标格子信息：{type, index} type: 1=装备槽 2=背包格 3=技能槽 4=技能背包
    int contextTargetType_ = 0;
    int contextTargetIndex_ = -1;
    // 菜单项边界（2项：装备/卸下, 丢弃）
    std::array<sf::FloatRect, 2> contextItemBounds_;
    // 菜单整体边界（用于点击外部关闭）
    sf::FloatRect contextMenuBounds_;
    // 根据目标类型返回菜单项文字
    [[nodiscard]] std::string getContextItemText(int itemIdx) const;

    // 装备图标纹理（6 种槽位）
    std::array<sf::Texture, 6> itemIcons_;
    // 槽位名 → 图标索引映射（Weapon=0...Amulet=5，与 ItemSlot 枚举一致）
    [[nodiscard]] const sf::Texture& getIconTexture(ItemSlot slot) const;

    // 品质颜色
    sf::Color getQualityColor(ItemQuality q) const;
    const char* getSlotName(ItemSlot s) const;
    const char* getAffixName(AffixType t) const;
    // 格式化词缀数值（百分比 ×100 显示）
    static std::string formatAffixValue(const Affix& affix);
    // 第二十三轮新增：套装加成简短描述（如 "+10%伤害"）
    static std::string formatSetBonusShort(SetBonusType type, float val);
};

// ============================================================================
// MerchantMenu —— 商人交易菜单（左侧购买 + 右侧出售）
// ----------------------------------------------------------------------------
// 布局：
//   左侧：商人售卖的 6 件物品（价格 + 品质边框）
//   右侧：玩家大背包 25 格物品（出售价格）
//   底部：玩家当前金币
//
// 交互：
//   左键点击左侧物品 → 购买
//   左键点击右侧物品 → 出售
//   按 G 或 ESC 关闭菜单
// ============================================================================
class MerchantMenu : public UIElement {
public:
    MerchantMenu();

    void Initialize(const sf::Font& font);

    // 设置商人库存数据（购买侧）
    void SetMerchantStock(const MerchantSystem& merchant);
    // 设置玩家背包数据（出售侧）
    // pc 可选：传入则同时刷新玩家技能背包数据（用于右侧技能背包显示）
    void SetBackpack(const InventorySystem& inventory, int playerCoins,
                     const PlayerComponent* pc = nullptr);

    // 更新鼠标悬停状态（用于技能 tooltip）
    void UpdateHover(sf::Vector2f mousePos);

    // 检测鼠标点击位置，返回操作类型与索引
    // 返回值：操作码（0=无操作，1=购买，2=出售，3=购买技能），index 为对应索引
    [[nodiscard]] std::pair<int, int> CheckClick(sf::Vector2f mousePos) const;

    void Render(sf::RenderTarget& target) const override;

private:
    const sf::Font* font_ = nullptr;
    int playerCoins_ = 0;

    // 商人库存（6 件）
    std::array<MerchantItem, MerchantSystem::kMerchantStockSize> stock_;
    mutable std::array<sf::FloatRect, MerchantSystem::kMerchantStockSize> stockBounds_;

    // 商人技能库存（2个）
    std::array<MerchantSkill, MerchantSystem::kMerchantSkillSize> skillStock_;
    mutable std::array<sf::FloatRect, MerchantSystem::kMerchantSkillSize> skillStockBounds_;

    // 玩家背包（25 格）
    std::array<std::optional<Item>, InventorySystem::kBackpackSize> backpack_;
    mutable std::array<sf::FloatRect, InventorySystem::kBackpackSize> backpackBounds_;

    // 玩家技能背包（5 格，仅显示）
    std::array<SkillType, kSkillBackpackSize> skillBackpack_;
    mutable std::array<sf::FloatRect, kSkillBackpackSize> skillBackpackBounds_;

    // 鼠标悬停状态（用于技能 tooltip，-1 = 无悬停）
    int hoveredSkillStock_ = -1;
    int hoveredSkillBackpack_ = -1;
    // 鼠标位置（用于 tooltip 显示）
    sf::Vector2f mousePos_;

    sf::Color getQualityColor(ItemQuality q) const;
    const char* getSlotName(ItemSlot s) const;
    const char* getAffixName(AffixType t) const;
    // 格式化词缀数值（百分比 ×100 显示）
    static std::string formatAffixValue(const Affix& affix);

    // 装备图标纹理（6 种槽位）
    std::array<sf::Texture, 6> itemIcons_;
    [[nodiscard]] const sf::Texture& getIconTexture(ItemSlot slot) const;
};

// ============================================================================
// DebugPanel —— F5 调试面板
// ----------------------------------------------------------------------------
// 功能：
//   - 传送到指定房间类型（出生房、宝箱房、陷阱房、阻碍房、Boss房、楼梯房、事件房、诅咒房）
//   - 作弊功能（无敌、秒杀、加金币、加经验、加技能点、满血、下一层等）
//   - 实时状态信息显示（FPS、层数、敌人/弹幕/粒子数、Boss状态、玩家属性等）
//
// 交互：
//   - F5 键打开/关闭面板
//   - 点击按钮执行对应功能
// ============================================================================
class DebugPanel : public UIElement {
public:
    DebugPanel();

    void Initialize(const sf::Font& font);

    // 更新（用于按钮悬停效果）
    void Update(float dt) override;

    void Render(sf::RenderTarget& target) const override;

    // 检测鼠标点击，返回操作类型
    // 0=无操作
    // 1=传送出生房, 2=传送宝箱房, 3=传送陷阱房, 4=传送阻碍房
    // 5=无敌, 6=秒杀, 7=加金币, 8=加经验, 9=清屏
    // 10=传送Boss房, 11=传送楼梯房, 12=下一层, 13=加技能点
    // 14=清除诅咒, 15=满血, 16=重置技能冷却, 17=传送事件房, 18=传送诅咒房
    [[nodiscard]] int CheckClick(sf::Vector2f mousePos) const;

    // ---- 实时状态数据（由 Game 每帧填充）----
    struct DebugStats {
        float fps = 0.f;
        int dungeonLevel = 1;
        int currentRoomIndex = -1;
        const char* currentRoomType = "Unknown";
        int totalRooms = 0;
        int clearedRooms = 0;
        int enemyCount = 0;
        int projectileCount = 0;
        int particleCount = 0;
        int fissureCount = 0;

        // 玩家状态
        float playerX = 0.f;
        float playerY = 0.f;
        float playerHp = 0.f;
        float playerMaxHp = 0.f;
        int playerLevel = 1;
        int playerExp = 0;
        int playerExpToNext = 0;
        int playerCoins = 0;
        int playerSkillPoints = 0;
        bool playerCursed = false;

        // Boss 状态
        bool bossActive = false;
        float bossHpPercent = 0.f;

        // 性能
        float aiTimeMs = 0.f;
        float combatTimeMs = 0.f;
        float projectileTimeMs = 0.f;

        // 存档
        int currentSlot = 1;
    };

    void SetStats(const DebugStats& stats) { stats_ = stats; }
    void SetGodMode(bool v) { godMode_ = v; }

private:
    const sf::Font* font_ = nullptr;

    // ---- 原有按钮（1-9）----
    Button* teleportSpawnBtn_ = nullptr;
    Button* teleportTreasureBtn_ = nullptr;
    Button* teleportTrapBtn_ = nullptr;
    Button* teleportObstacleBtn_ = nullptr;
    Button* godModeBtn_ = nullptr;
    Button* killAllBtn_ = nullptr;
    Button* addCoinsBtn_ = nullptr;
    Button* addExpBtn_ = nullptr;
    Button* clearScreenBtn_ = nullptr;

    // ---- 新增按钮（10-18）----
    Button* teleportBossBtn_ = nullptr;
    Button* teleportStairsBtn_ = nullptr;
    Button* nextLevelBtn_ = nullptr;
    Button* addSkillPointBtn_ = nullptr;
    Button* removeCurseBtn_ = nullptr;
    Button* fullHealBtn_ = nullptr;
    Button* resetCooldownBtn_ = nullptr;
    Button* teleportEventBtn_ = nullptr;
    Button* teleportCursedBtn_ = nullptr;

    // 按钮边界（用于点击检测，索引 1-18 对应操作码，0 未使用）
    mutable std::array<sf::FloatRect, 19> buttonBounds_;

    // 实时状态数据
    DebugStats stats_;
    bool godMode_ = false;
};

// ============================================================================
// SettingsMenu —— 设置菜单（音量、分辨率）
// ----------------------------------------------------------------------------
// 提供音量调节（+/- 按钮）和分辨率选择，应用后保存到 settings.ini
// 操作返回值：
//   0=无操作 1=BGM- 2=BGM+ 3=SFX- 4=SFX+
//   5=分辨率上一个 6=分辨率下一个 7=应用 8=返回
// ============================================================================
class SettingsMenu : public UIElement {
public:
    SettingsMenu();

    void Initialize(const sf::Font& font);

    // 设置当前显示值（由 Game 同步）
    void SetBGMVolume(float v) { bgmVolume_ = v; }
    void SetSFXVolume(float v) { sfxVolume_ = v; }
    void SetResolution(int w, int h) { resW_ = w; resH_ = h; }

    [[nodiscard]] float GetBGMVolume() const noexcept { return bgmVolume_; }
    [[nodiscard]] float GetSFXVolume() const noexcept { return sfxVolume_; }
    [[nodiscard]] int GetResW() const noexcept { return resW_; }
    [[nodiscard]] int GetResH() const noexcept { return resH_; }

    void Render(sf::RenderTarget& target) const override;

    // 检测鼠标点击，返回操作类型（0-8）
    [[nodiscard]] int CheckClick(sf::Vector2f mousePos) const;

    // 预设分辨率列表
    struct Resolution { int w; int h; const char* label; };
    static constexpr int kResolutionCount = 4;
    static const Resolution kResolutions[kResolutionCount];

    // 获取当前分辨率索引
    [[nodiscard]] int GetCurrentResolutionIndex() const;

private:
    const sf::Font* font_ = nullptr;
    float bgmVolume_ = 50.f;
    float sfxVolume_ = 70.f;
    int resW_ = 1280;
    int resH_ = 720;

    // 按钮边界（用于点击检测，索引 1-8 对应 8 个按钮，0 未使用）
    mutable std::array<sf::FloatRect, 9> buttonBounds_;
};

// ============================================================================
// SaveLoadMenu —— 存档/读档槽位选择菜单
// ----------------------------------------------------------------------------
// 布局：
//   顶部标题（根据 Mode 显示"读取存档"或"保存到槽位"/"新游戏"）
//   中部 3 个槽位卡片横排（显示层数/击杀/时长/金币/玩家等级/时间戳）
//   每张卡片底部有"删除"按钮（仅对已有存档显示）
//   底部"返回"按钮
//
// 模式：
//   Load   —— 读取存档（仅有存档的槽位可点击载入）
//   SaveNew —— 保存到槽位（新游戏选槽位 或 暂停时手动保存，所有槽位可点）
//
// 操作返回值（CheckClick）：
//   0=无操作 1=选槽1 2=选槽2 3=选槽3
//   4=返回 5=删槽1 6=删槽2 7=删槽3
// ============================================================================
class SaveLoadMenu : public UIElement {
public:
    enum class Mode { Load, SaveNew };

    SaveLoadMenu();

    void Initialize(const sf::Font& font);

    void SetMode(Mode mode) { mode_ = mode; }
    [[nodiscard]] Mode GetMode() const noexcept { return mode_; }

    // 设置 3 个槽位的概要信息（由 Game 调用 SaveSystem.GetAllSlotInfo 后传入）
    void SetSlotInfo(const std::array<SaveSlotInfo, SaveSystem::kSlotCount>& info);

    void Update(float dt) override;
    void Render(sf::RenderTarget& target) const override;

    // 检测鼠标点击，返回操作码（0-7，见上方注释）
    [[nodiscard]] int CheckClick(sf::Vector2f mousePos) const;

private:
    const sf::Font* font_ = nullptr;
    Mode mode_ = Mode::Load;
    std::array<SaveSlotInfo, SaveSystem::kSlotCount> slotInfo_{};

    // 3 个槽位卡片边界 + 3 个删除按钮边界 + 1 个返回按钮边界
    mutable std::array<sf::FloatRect, SaveSystem::kSlotCount> cardBounds_;
    mutable std::array<sf::FloatRect, SaveSystem::kSlotCount> deleteBtnBounds_;
    mutable sf::FloatRect backBtnBounds_;
    float blinkTimer_ = 0.f; // 用于空槽位边框闪烁

    // 格式化存活时间 "MM:SS"
    [[nodiscard]] static std::string formatTime(float seconds) ;
    // 格式化时间戳 "YYYY-MM-DD HH:MM"
    [[nodiscard]] static std::string formatTimestamp(int64_t unixSec);
};

} // namespace cu
