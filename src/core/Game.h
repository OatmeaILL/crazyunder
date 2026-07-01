#pragma once

// ============================================================================
// Game —— 游戏主类：状态机 + 主循环
// ----------------------------------------------------------------------------
// 主循环各阶段职责：
//   1. 事件处理（pollEvent）：输入、窗口关闭等。
//   2. 时间更新（Time::Update）：采集真实时间，按固定步长返回本帧更新次数。
//   3. 固定步长更新：按返回次数循环调用 Update(fixedDt)，保证逻辑确定性。
//   4. 插值渲染：用 alpha 在逻辑帧间插值，平滑显示。
//   5. window.display()：交换前后缓冲。
//
// 状态机：
//   GameState 枚举定义所有游戏状态。ChangeState 触发 OnExit(旧) / OnEnter(新)。
//   每个状态注册 onEnter/onExit/update/render 回调（std::function），
//   主循环按当前状态分发 update 与 render。
//
// Phase 8 扩展：
//   集成 UIManager、HUD、各菜单、AudioManager。
//   - Menu 状态：渲染 MainMenu，Start 按钮切到 Playing，Quit 关窗
//   - Playing 状态：渲染 HUD，升级触发时显示 UpgradeChoiceMenu，
//     按 I 键切换 InventoryMenu，AudioManager.PlayBGM("dungeon")
//   - Paused 状态：渲染 PauseMenu，Resume/Restart/Quit 按钮
//   - Dead 状态：渲染 DeathScreen，显示击杀数/等级
//   - Victory 状态：渲染 VictoryScreen
//   - ESC 键：Playing → Paused，Paused → Playing，Menu → 关窗
// ============================================================================

#include <SFML/Graphics.hpp>
#include <functional>
#include <unordered_map>
#include "core/Time.h"
#include "core/ResourceManager.h"
#include "core/Input.h"
#include "core/AudioManager.h"
#include "core/Settings.h"
#include "core/SaveSystem.h"
#include "ecs/Registry.h"
#include "rendering/Renderer.h"
#include "rendering/Camera.h"
#include "rendering/ParticleSystem.h"
#include "rendering/TextureAtlas.h"
#include "rendering/TileMap.h"
#include "gameplay/Animation.h"
#include "gameplay/Player.h"
#include "gameplay/FlowField.h"
#include "gameplay/EnemyAI.h"
#include "gameplay/EnemySpawner.h"
#include "gameplay/CombatSystem.h"
#include "gameplay/ProjectileSystem.h"
#include "gameplay/DungeonGenerator.h"
#include "gameplay/RoomSystem.h"
#include "gameplay/LootSystem.h"
#include "gameplay/InventorySystem.h"
#include "gameplay/UpgradeSystem.h"
#include "gameplay/ExpOrbSystem.h"
#include "gameplay/CoinSystem.h"
#include "gameplay/HeartSystem.h"
#include "gameplay/MerchantSystem.h"
#include "gameplay/UpgradeUI.h"
#include "gameplay/SkillSystem.h"
#include "gameplay/QuestSystem.h"
#include "gameplay/AchievementSystem.h"
#include "gameplay/RelicSystem.h"
#include "gameplay/ClassSystem.h"
#include "gameplay/FloorModifier.h"
#include "gameplay/SoulMemorySystem.h"
#include "gameplay/DialogueSystem.h"
#include "ui/QuestMenu.h"
#include "ui/AchievementMenu.h"
#include "ui/SoulWellMenu.h"
#include "ui/UIManager.h"
#include "ui/HUD.h"
#include "ui/Menus.h"
#include "ui/DialogueBoxUI.h"
#include "utils/UniformGrid.h"

namespace cu {

class Game {
public:
    enum class GameState {
        Menu,
        Playing,
        Paused,
        Dead,
        Victory
    };

    Game();
    ~Game() = default;

    // 启动主循环（阻塞直到窗口关闭）
    void Run();

    // 切换状态（触发 OnExit(旧) / OnEnter(新)）
    void ChangeState(GameState newState);

private:
    void handleEvents();
    void registerStates();

    // ---- Playing 状态专用方法 ----
    // preserveProgress: true 时保留升级/装备/经验（用于下一层）
    void setupPlayingScene(bool preserveProgress = false);
    void updatePlaying(float dt); // 玩法更新：输入、移动、摄像机、粒子
    void renderPlaying(float alpha); // 渲染：批量绘制精灵 + 粒子 + 调试

    // ---- 第十七轮新增：地牢变异系统辅助 ----
    // 将当前 floorModifiers_ 的乘法系数推送到 EnemySpawner/LootSystem/MerchantSystem
    // 在 setupPlayingScene 中调用，确保所有子系统与本层 modifier 同步
    void applyFloorModifiersToSubsystems();
    // 渲染进入新层时的变异提示 Banner（4 秒淡入淡出）
    void renderFloorModifierBanner();
    // 渲染 HUD 持久指示器（显示当前激活的变异名）
    void renderFloorModifierHUD();

    // ---- Phase 7: 重新计算玩家属性 ----
    void recomputePlayerStats();

    // ---- 玩家贴图加载 ----
    // 尝试加载 assets/sprites/player.png，失败或尺寸不符则回退过程化生成
    // 返回图集中玩家 Sprite Sheet 的像素位置（左上角）
    PlayerSheetInfo loadPlayerSpriteSheet();

    // ---- Phase 8: UI 与音频集成 ----
    void initializeUI();        // 初始化所有 UI 元素
    void updateUI(float dt);     // 每帧更新 UI
    void renderUI();             // 渲染当前状态的 UI
    void handleUIInput();        // 处理 UI 相关输入（鼠标点击、I 键等）
    void handleUpgradeChoice();  // 处理升级选择逻辑
    void handleRelicChoice();    // 处理圣物选择逻辑（Boss 击败后 3 选 1）
    void renderRelicPanel();    // 渲染圣物查看面板（R 键切换显示已获得圣物）
    void showUpgradeChoice();    // 显示升级选择菜单
    void updateHUDData();        // 更新 HUD 数据
    void handleInteract();       // E 键交互：开关门 + 开宝箱
    void renderDoorHealthBars(); // 渲染门的血量条（屏幕空间）
    void renderBossHealthBar();  // 渲染 BOSS 血条（屏幕顶部）
    void renderChampionHealthBars(); // 渲染精英强化怪头上小血条（世界空间）
    void renderTutorial();       // 渲染按键教程覆盖层（首次进入 Playing 显示）
    void nextLevel();            // 进入下一层（重新生成地牢，保留玩家属性）
    void restartGame();          // 重新开始游戏（重置所有状态）
    void applySettings();        // 应用当前 settings_ 到 AudioManager 与窗口
    void handleSettingsMenuClick(int action); // 处理设置菜单按钮点击（1-8）
    void handleSoulWellMenuClick(int action); // 处理灵魂之井面板点击（1-6=购买强化, 7=返回）
    // 重置所有 UI 可见性标志（读档/新游戏/重新开始时调用）
    void resetAllUIFlags();

    // ---- 存档系统 ----
    // 从当前游戏状态构建存档数据
    [[nodiscard]] SaveData buildSaveData();
    // 将存档数据应用到游戏状态（重新生成地牢，恢复玩家属性/装备/技能）
    void applySaveData(const SaveData& data);
    // 显示存档/读档菜单（mode 决定是 Load 还是 SaveNew）
    void showSaveLoadMenu(SaveLoadMenu::Mode mode);
    // 处理存档菜单点击（action: 0=无 1-3=选槽 4=返回 5-7=删槽）
    void handleSaveLoadMenuClick(int action);
    // 显示职业选择菜单
    void showClassSelectMenu();
    // 处理职业选择菜单点击
    void handleClassSelectMenuClick(int action);
    // 保存当前进度到 currentSlot_（nextLevel 自动调用）
    void autoSaveCurrent();
    // 刷新存档菜单的槽位信息（每次显示前调用）
    void refreshSaveSlotInfo();

    // 调试面板辅助方法
    void teleportToRoom(RoomType type);  // 传送到指定类型房间
    void killAllEnemies();               // 秒杀所有敌人
    void addCoins(int amount);           // 增加金币
    void addExperience(int amount);      // 增加经验
    void clearScreen();                  // 清屏（移除所有敌人和弹幕）

    sf::RenderWindow window_;
    Time time_;
    ResourceManager& resources_;
    GameState state_;

    // 状态回调集合
    struct StateCallbacks {
        std::function<void()> onEnter;
        std::function<void()> onExit;
        std::function<void(float)> update;
        std::function<void(float)> render;
    };
    std::unordered_map<GameState, StateCallbacks> states_;

    sf::Text hintText_;
    sf::Text fpsText_;
    sf::Text debugText_;   // F1 调试信息
    double fpsLogTimer_ = 0.0;

    // ---- Phase 2: ECS 与渲染系统 ----
    Registry registry_;
    Renderer renderer_;
    Camera camera_;
    ParticleSystem particles_;
    TextureAtlas atlas_;

    // ---- Phase 3: 输入与动画 ----
    Input input_;
    AnimationSystem animationSystem_;
    PlayerSheetInfo playerSheetInfo_; // 玩家贴图在图集中的位置
    sf::IntRect swordRect_;           // 剑士武器贴图在图集中的像素矩形

    EntityId playerId_ = kInvalidEntity; // 玩家实体 ID
    bool debugMode_ = false;             // F1 调试信息开关
    bool triggerExplosion_ = false;      // 空格触发爆炸（边沿触发标志）

    // ---- 第三十一轮新增：顿帧系统（Hit Stop）----
    // 暴击/击杀精英时短暂冻结游戏时间，增强打击感
    float hitStopTimer_ = 0.f;           // 顿帧剩余时间（>0 时跳过逻辑更新）
    static constexpr float kHitStopCrit = 0.04f;       // 暴击顿帧 40ms
    static constexpr float kHitStopEliteKill = 0.06f;   // 击杀精英顿帧 60ms
    static constexpr float kHitStopBossKill = 0.1f;     // 击杀 Boss 顿帧 100ms

    // ---- Phase 4: 敌人系统与流场 AI ----
    UniformGrid uniformGrid_;     // 空间网格（碰撞/邻近查询）
    FlowField flowField_;         // 流场寻路
    EnemySpawner enemySpawner_;   // 敌人生成器

    // 流场重算计时器（每 0.5s 重算一次，避免每帧重算）
    float flowFieldRecomputeTimer_ = 0.f;
    static constexpr float kFlowFieldRecomputeInterval = 0.5f;

    // 性能统计（调试用）
    float lastFlowFieldTimeMs_ = 0.f;  // 上一次流场重算耗时
    float lastAIUpdateTimeMs_ = 0.f;   // 上一次敌人 AI 更新耗时
    float lastCombatTimeMs_ = 0.f;     // 上一次敌人战斗更新耗时
    float lastProjectileTimeMs_ = 0.f; // 上一次弹幕更新耗时
    int currentWaveNumber_ = 0;        // 当前波次号

    // ---- Phase 5: 战斗与割草系统 ----
    CombatSystem combatSystem_;        // 战斗系统（伤害/状态/事件）
    ProjectileSystem projectileSystem_;// 弹幕系统（对象池管理）
    int totalKillCount_ = 0;           // 累计击杀数

    // ---- Phase 6: 地牢程序生成 ----
    DungeonGenerator dungeonGenerator_;  // 地牢生成器
    Dungeon dungeon_;                    // 当前地牢数据
    TileMap tileMap_;                    // Tile 地图渲染
    RoomSystem roomSystem_;              // 房间系统（门/清理检测）
    uint32_t dungeonSeed_ = 0;           // 当前地牢种子
    bool dungeonInitialized_ = false;    // 地牢是否已初始化

    // ---- Phase 7: 战利品与局内成长系统 ----
    LootSystem lootSystem_;             // 战利品掉落系统
    InventorySystem inventorySystem_;   // 背包与装备系统
    UpgradeSystem upgradeSystem_;       // 升级系统
    ExpOrbSystem expOrbSystem_;         // 经验球系统
    CoinSystem coinSystem_;             // 金币系统
    HeartSystem heartSystem_;           // 爱心掉落系统（Boss 召唤物掉落）
    MerchantSystem merchantSystem_;    // 商人系统
    UpgradeUI upgradeUI_;               // 升级选择 UI（数据层）
    QuestSystem questSystem_;           // 任务系统（框架）
    AchievementSystem achievementSystem_; // 成就系统（框架，跨存档共享）
    RelicSystem relicSystem_;             // 圣物系统（第十五轮新增，跨层保留的被动 build）
    FloorModifierSystem floorModifiers_;  // 地牢变异系统（第十七轮新增，每层随机的双刃剑修饰符）
    SoulMemorySystem soulMemory_;         // 灵魂之忆系统（第二十四轮新增，跨局永久成长 meta progression）
    // 第二十轮新增：极限闪避成就检测字段
    // PlayerCombat 触发极限闪避时增加 perfectDodgeCount，Game 每帧检测变化并上报成就
    // 不修改 PlayerCombat 接口，通过比对 lastPerfectDodgeCount_ 与 pc->perfectDodgeCount 实现
    int lastPerfectDodgeCount_ = 0;
    bool upgradeChoiceActive_ = false;  // 升级选择模式激活标志
    std::array<UpgradeOption, 3> currentUpgradeOptions_; // 当前升级选项
    // ---- 圣物选择菜单（Boss 击败后 3 选 1）----
    bool relicChoiceActive_ = false;       // 圣物选择菜单是否激活
    std::vector<RelicType> currentRelicOptions_; // 当前圣物选项（1-3 个）

    // ---- Phase 8: UI 与音频系统 ----
    UIManager uiManager_;               // UI 层级管理器
    HUD hud_;                            // 游戏内 HUD
    MainMenu mainMenu_;                  // 主菜单
    PauseMenu pauseMenu_;                // 暂停菜单
    DeathScreen deathScreen_;            // 死亡结算
    VictoryScreen victoryScreen_;        // 胜利结算
    UpgradeChoiceMenu upgradeMenu_;      // 升级选择菜单
    RelicChoiceMenu relicMenu_;            // 圣物选择菜单（Boss 击败后 3 选 1）
    InventoryMenu inventoryMenu_;        // 背包菜单
    MerchantMenu merchantMenu_;          // 商人交易菜单
    DebugPanel debugPanel_;              // F5 调试面板
    SettingsMenu settingsMenu_;           // 设置菜单（音量/分辨率）
    Settings settings_;                   // 游戏设置（持久化到 settings.ini）
    QuestMenu questMenu_;                 // 任务面板（按 Q 打开）
    AchievementMenu achievementMenu_;     // 成就面板（按 Tab 打开）
    SoulWellMenu soulWellMenu_;           // 灵魂之井面板（主菜单入口，永久强化购买）
    bool inventoryMenuVisible_ = false;   // 背包菜单是否可见
    bool merchantMenuVisible_ = false;    // 商人菜单是否可见
    bool debugPanelVisible_ = false;      // 调试面板是否可见
    bool settingsMenuVisible_ = false;    // 设置菜单是否可见
    bool questMenuVisible_ = false;       // 任务菜单是否可见
    bool achievementMenuVisible_ = false; // 成就菜单是否可见
    bool relicPanelVisible_ = false;      // 圣物查看面板是否可见（R 键切换）
    bool soulWellMenuVisible_ = false;    // 灵魂之井面板是否可见（主菜单入口）
    ClassSelectMenu classSelectMenu_;     // 职业选择菜单（新游戏时弹出）
    bool classSelectMenuVisible_ = false; // 职业选择菜单是否可见
    PlayerClass selectedClass_ = PlayerClass::Mage; // 当前选择的职业
    SaveLoadMenu saveLoadMenu_;           // 存档/读档槽位选择菜单
    SaveSystem saveSystem_;               // 存档系统
    bool saveLoadMenuVisible_ = false;    // 存档菜单是否可见
    int currentSlot_ = 1;                 // 当前游戏使用的存档槽位（1-3）
    bool godMode_ = false;                // 无敌模式
    bool tutorialVisible_ = false;        // 按键教程是否可见（首次进入 Playing 显示）
    bool tutorialShown_ = false;          // 本次会话是否已显示过教程（避免重复弹出）
    float survivalTime_ = 0.f;           // 存活时间（用于死亡/胜利结算）
    int lastShardsGained_ = 0;           // 上次死亡获得的灵魂碎片数（用于死亡结算显示）
    int bossKillCountThisRun_ = 0;       // 本局 Boss 击杀数（用于死亡碎片计算）

    // ---- 死亡回顾系统 ----
    std::string lastKillerName_ = "";     // 最后击杀玩家的敌人名称
    float totalDamageDealt_ = 0.f;       // 本局总伤害输出（用于 DPS 计算）
    int comboAtDeath_ = 0;               // 死亡时的连击数

    // ---- 下一层与 BOSS 系统 ----
    int currentLevel_ = 1;               // 当前层数
    EntityId bossEntityId_ = kInvalidEntity; // BOSS 实体 ID
    bool bossActive_ = false;            // BOSS 是否存活
    bool bossRoomEntered_ = false;       // 玩家是否曾进入 BOSS 房间
    float bossNoDamageTimer_ = 0.f;      // 玩家离开 BOSS 房间后的计时器
    float bossDefeatedHintTimer_ = 0.f; // BOSS 击败后提示计时器
    static constexpr float kBossRegenDelay = 5.f;   // 离开 BOSS 房间 5 秒后开始回血
    static constexpr float kBossRegenRate = 0.02f;   // 每秒回血 2% 最大生命

    // ---- 第十七轮新增：地牢变异系统 Banner 与再生累加器 ----
    // bannerTimer_ > 0 时显示"本层变异：XXX"提示，倒计时归零后消失
    // 总时长 5 秒：前 0.4s 淡入，中间 4.2s 稳定，最后 0.4s 淡出
    float modifierBannerTimer_ = 0.f;
    static constexpr float kModifierBannerDuration = 5.0f;
    // 再生累加器：变异系统每秒回血比例通常 < 1HP，需累加到整数才生效
    float regenAccumulator_ = 0.f;
    float manaRegenAccumulator_ = 0.f;  // 法力回复累加器（避免 <1MP 回蓝被截断）

    // ---- Boss 冲撞地裂区域管理 ----
    std::vector<FissureZone> fissureZones_; // 活跃的地裂区域列表

    // ---- 事件房交互状态 ----
    // 当前可交互的事件房索引（-1=无），由 RoomSystem::OnEventRoomEnter 设置
    int activeEventRoomIdx_ = -1;
    EventType activeEventType_ = EventType::None;
    // 事件交互对话框是否可见（按 E 触发后显示，玩家选择后关闭）
    bool eventDialogVisible_ = false;
    // 事件房交互提示文字（"按 E 与乞丐对话" 等）
    sf::Text eventHintText_;

    // ---- 诅咒房相关方法 ----
    void applyCurse(int roomIndex);      // 施加诅咒
    void removeCurse();                  // 解除诅咒

    // ---- 事件房相关方法 ----
    void handleEventInteraction();       // 处理 E 键事件交互
    void renderEventHint();              // 渲染事件房交互提示
    void renderEventDialog();            // 渲染事件对话框
    void executeEventChoice(bool accept);// 执行事件选择（accept=true 接受/确认）

    // ---- 第三十三轮新增：对话系统 ----
    DialogueSystem dialogueSystem_;          // 对话引擎
    DialogueBoxUI dialogueBoxUI_;            // 对话面板 UI
    // 对话系统注册的树 ID
    int dialogueTreeId_Beggar_ = -1;        // 乞丐对话树 ID
    int dialogueTreeId_Mage_ = -1;          // 神秘法师对话树 ID
    int dialogueTreeId_MerchantNpc_ = -1;   // 商人对话树 ID
    int dialogueTreeId_Tutorial_ = -1;      // 教程对话树 ID
    // 对话结束后待处理的操作
    int pendingEventRoomIdx_ = -1;          // 对话结束后要标记触发的事件房索引
    bool pendingMerchantOpen_ = false;      // 对话结束后要打开商人菜单
    bool pendingTutorialQuestOpen_ = false; // 对话结束后要打开任务栏
    // 对话系统回调注册
    void registerDialogueCallbacks();        // 注册动作处理器与条件求值器
    void renderDialogueBox();               // 渲染对话面板

    // ---- 地裂区域更新与渲染 ----
    void updateFissureZones(float dt);   // 更新地裂区域（计时、伤害玩家）
    void renderFissureZones();           // 渲染地裂区域（视觉提示）

    // ---- 调试面板状态更新 ----
    void updateDebugStats();             // 填充 DebugPanel 实时状态数据
};

} // namespace cu
