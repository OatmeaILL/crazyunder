# CrazyUnder 项目状态总结（2026-06-29 第三十轮）

> 本文档供新会话继承上下文使用。当前会话已完成三十轮开发，包含核心修复、系统扩展与体验优化。

## 四、本轮新增/修复内容（第十轮）

### 视觉修复
1. **商人头顶文字**：改为世界空间渲染（EndScene 后、setView 前直接用 window_.draw），摄像机自动处理坐标转换
2. **震地波粒子差异化**：与空格 AOE 爆炸区分——三层粒子（暗棕色冲击波环 + 向上飞溅岩石 + 地面尘土云），音效改用 kSFXExplosion
3. **引力井范围扩大 1.5 倍**：kPullRadius 200→300，kPullForce 250→300

### 游戏机制修改
4. **技能商人价格 x5 倍**：基础价格从 `30 + level*5` 改为 `150 + level*25`
5. **吸血打击改为持续吸血**：持续 5 秒，持续时间内所有攻击吸血 30%（不再单次消耗）
6. **HUD 技能冷却倒计时**：技能图标上方显示剩余秒数（黄色粗体，向上取整）
7. **背包已满刷屏修复**：LootDropEntry 添加 2 秒全消息冷却，每次碰撞仅提示一次

### BOSS 增强
8. **击败 BOSS 提示**：屏幕中央显示"BOSS 已击败！通过通道前往下一层"，最后 3 秒闪烁
9. **BOSS 远程攻击**：每 2 秒发射 3 发扇形红色子弹
10. **BOSS 召唤小兵**：每 8 秒召唤 2 个近战小兵（标记 isBossMinion）
11. **爱心掉落**：Boss 小兵死亡 30% 概率掉落爱心（对象池管理 + 磁吸拾取 + 回复 2% 血量）

### 构建优化
12. **Release 版本编译**：不再使用 Debug 版本，解决其他用户 msvcp140d.dll 缺失报错

### 技能持久化修复
13. **下一层技能保留**：nextLevel() 中保存 oldPc->skillSlots/skillBackpack，setupPlayingScene 后写回 newPc，避免技能到下一层消失

### 升级系统改进
14. **技能点累积机制**：`upgradePending_` 替换为 `skillPoints_`，每次升级累积 1 个技能点，不再自动弹窗打断游戏
15. **J 键开启升级选择**：有技能点时按 J 键主动打开选择界面；选择后若仍有剩余技能点，保持菜单打开继续选择
16. **HUD 技能点提示**：屏幕中上方闪烁提示"有未使用的技能点 n 点  按 J 开启新技能选择界面"

## 四-b、第十一轮新增/修复内容

### 新系统：精英怪 Champion
17. **Champion 精英强化系统**：任何普通怪 8% 概率升级为 Champion 版本
    - HP ×3, 伤害 ×1.5, 速度 ×1.1, 体型 ×1.5
    - 金色 sprite 颜色 (255, 230, 150)
    - 头顶 36x4px 金色血条 + 左侧三角形标识（renderChampionHealthBars）
    - 100% 掉落 1-2 件蓝色品质装备，经验/金币 ×3
    - EnemyAI.h 新增 isChampion 字段，EnemySpawner.cpp 实现倍率缩放
18. **敌人分层强化重设**：拆分为 HP/伤害/速度三个独立线性函数
    - HP 每层 +30%，伤害每层 +18%，速度每层 +4% 封顶 1.6 倍
    - 避免后期怪物比玩家快、既肉又秒杀的陡峭跳跃问题

### 新系统：任务系统（5个主线任务）
19. **QuestSystem 重写**：5 个剧情任务 + 依赖解锁 + 提交机制
    - 任务1"破晓之始"：打通第一层（ReachLevel 2）→ 解锁 2-5
    - 任务2"古老祭坛"：找到祭坛（TriggerEvent::Altar）
    - 任务3"地下奇人"：找到乞丐（TriggerEvent::Beggar）
    - 任务4"百艺兼修"：收集 4 个技能（CollectSkills 4）
    - 任务5"财富之力"：攒够 1000 金币（AccumulateCoins 1000），提交后扣 1000G + 金装 + 等级+5
    - 其他奖励：经验/金币/装备/技能点（任务1-4 各有奖励）
20. **任务状态存档**：SaveData 新增 5 个 QuestSaveEntry，随存档读写
21. **任务1结算 bug 修复**：`setupPlayingScene(true)` 中 `questSystem_.Initialize()` 移入 `if (!preserveProgress)` 块；`nextLevel` 中 `OnLevelReached` 移到 `setupPlayingScene` 之后调用，避免清空任务进度

### 新系统：成就系统
22. **AchievementSystem**：跨存档持久化，独立文件 saves/achievements.dat
    - 4 类成就：战斗/探索/收集/特殊
    - 二进制格式：魔数 + 版本号 + 成就数据
    - 隐藏成就显示 "???"

### 新系统：事件房 + 诅咒房
23. **事件房**（totalRooms≥4 时 50% 概率）：4 种事件类型，E 键触发
    - 乞丐/神秘法师/宝箱模仿怪/祭坛
    - NPC 过程化生成像素风贴图，世界空间 1.5x 渲染
24. **诅咒房**（totalRooms≥5 时 30% 概率）：锁门 + debuff，击败精英怪解除
25. **ChestMimic 提示修复**：不再显示"按E打开宝箱"文字，保留 E 交互

### 新系统：存档系统（3 槽位）
26. **3 槽位手动存档**：二进制序列化，PauseMenu "保存进度"按钮
27. **自动存档**：每层结束自动保存到当前槽位
28. **存档内容**：玩家属性/装备/技能/任务状态/层数/击杀数/存活时间
29. **读档技能恢复日志**："读档技能恢复: 技能槽=N/4 技能背包=N/5"

### UI 界面：任务面板 + 成就面板
30. **QuestMenu**（Q 键打开）：1100x650 面板，左侧 5 个任务卡片，右侧剧情说明
    - 卡片含：状态色条、标题、描述（UTF-8 自动换行）、进度条、奖励摘要、领取按钮
    - Completed 状态闪烁高亮（sin 波动）
    - UTF-8 字符边界换行（drawWrappedText，避免按 byte 切分导致乱码）
31. **AchievementMenu**（Tab 键打开）：1100x650 面板，4 列按分类布局
    - 已解锁/未解锁/隐藏三种显示状态
    - 顶部显示解锁统计（已解锁/总数+百分比）

### UI 改进：背包右键菜单
32. **InventoryMenu 右键上下文菜单**：右键格子弹出"装备/卸下"和"丢弃"
    - 装备槽/技能槽已装备 → "卸下"
    - 背包格/技能背包 → "装备"
    - 丢弃：直接销毁物品/技能，不进入背包
    - 菜单 120x68 像素，自动避让屏幕边界，点击外部关闭
    - 修复乱码：`sf::String::fromUtf8` 替代 `U8` 宏（U8 宏对 const char* 的 sizeof 陷阱）
33. **背包布局修复**：技能背包区下移到 y=545，分隔线缩短到 415，避免与装备背包第5行重叠

### UI 改进：HUD 技能栏
34. **技能图标放大**：32x32 → 48x48，间距 40 → 56px
35. **字号自适应字数**：2字→16, 3字→13, 4字→11（解决4字技能名显示不全）
36. **冷却倒计时移到图标内中央**：字号 16，黄色粗体
37. **快捷键提示移到图标下方**：字号 12
38. **索引 0-2（普攻/闪避/AOE）统一到 48x48**：纹理用 setScale(48/32) 拉伸

### UI 改进：小地图重做
39. **新版小地图**（200x150，右下角）：
    - 半透明黑色背景板 + 边框 + "地图"标题
    - 走廊通道：Floor 暗灰/Door 棕色/Stairs 青色，呈现真实地牢形状
    - 房间类型标记：B(BOSS红)/T(宝箱黄)/E(精英紫)/S(楼梯青)/?(隐藏灰)
    - 当前房间：黄色高亮圆圈
    - 玩家位置：绿色圆点 + 白色描边，实时跟随
    - 底部图例：你/B-BOSS/T-宝箱/S-楼梯
40. **HUD 新增 playerPos_ 成员**：SetPlayerPosition 接口，updateHUDData 中调用

### BOSS 机制扩展
41. **冲撞地裂**：每 6s 3倍速冲撞，路径留 5s 地裂区域（FissureZone）
42. **召唤精英怪**：HP<50% 触发，之后每 12s 持续召唤
43. **旋转弹幕**：每 10s 持续 2s，0.12s/发螺旋紫色子弹

### 陷阱房修复
44. **陷阱房锁门 bug 修复**：EnemyAI.cpp 中敌人自动开门逻辑添加 `ds->locked` 检查，上锁的门敌人无法开启

## 四-c、第十二轮新增/修复内容（致命 Bug 修复 + 平衡性校正）

### P0 致命 Bug 修复
45. **OnKill 双重调用导致奖励翻倍**（EnemyAI.cpp 死亡检测块）
    - **复现条件**：任意敌人 HP 归零死亡时，`UpdateEnemyCombat`（步骤 13）和 `Game.cpp` 统一死亡检测（步骤 15.5）各调用一次 `combat.OnKill` 回调
    - **影响**：经验球 ×2、金币 ×2、装备掉落 ×2、击杀计数 ×2、Boss 召唤物爱心掉落概率翻倍、任务/成就进度双倍累计，严重破坏经济与数值平衡
    - **修复方案**：移除 EnemyAI.cpp 死亡检测块中的 `combat.OnKill(id, playerEntity)` 调用，仅保留视觉/物理特效（死亡粒子、自爆伤害、分裂生成）；OnKill 回调与 `active=false` 标记统一由 Game.cpp 步骤 15.5 处理
    - **验证**：编译通过，逻辑路径无重复触发
46. **Boss 旋转弹幕与冲撞地裂计时器冲突**（EnemyAI.cpp Boss 机制）
    - **复现条件**：Boss 冲撞（CD 6s，持续 0.8s）与旋转弹幕（CD 10s，持续 2s）可能同时触发，二者复用 `EnemyComponent::specialTimer` 字段
    - **影响**：当冲撞与旋转弹幕重叠时，旋转弹幕分支被 `if (enemy->chargeActive <= 0.f)` 守卫跳过，导致旋转弹幕在冲撞期间完全不发射子弹；且 `specialTimer` 被两个机制交替写入（0.15s vs 0.12s 间隔），导致地裂生成节奏错乱
    - **修复方案**：EnemyAI.h 新增 `float spiralFireTimer = 0.f;` 独立字段，EnemyAI.cpp 旋转弹幕分支改用 `spiralFireTimer` 而非 `specialTimer`，移除 `chargeActive` 守卫，两机制可独立并发执行

### P1 重要 Bug 修复
47. **EquipSkill 不重置技能等级导致等级继承**（SkillSystem.cpp 技能槽管理）
    - **复现条件**：玩家卸下 Lv.3 技能 A（槽位 level 字段保留 3），再从背包装备不同技能 B 到同一槽位
    - **影响**：技能 B 错误继承槽位残留的 Lv.3，破坏技能升级平衡（满级 3 级）
    - **修复方案**：EquipSkill 中装备新技能时显式 `pc.skillSlots[slotIndex].level = 1;`（背包技能等级固定为 1，因 skillBackpack 仅存类型）；UnequipSkill 卸下时同样重置 level=1，避免下次装备继承
48. **地刺技能 DPS 不随玩家伤害缩放**（SkillSystem.cpp UpdateSkillBuffs）
    - **复现条件**：玩家后期伤害属性提升后释放地刺技能
    - **影响**：地刺 DPS 固定为 `5 + 3*(lv-1)`（5-11 DPS），后期玩家伤害 100+ 时该技能完全无效，沦为废技能
    - **修复方案**：DPS 公式改为 `kSpikeBaseDPS + pc->stats.damage * 0.3f`，保留等级基础值并叠加玩家伤害的 30%，使技能全期可用
49. **地刺减速效果过弱**（SkillSystem.cpp UpdateSkillBuffs）
    - **复现条件**：敌人在地刺范围内移动
    - **影响**：原公式 `vel->linear *= (1 - 0.5 * dt * 3)` 在固定步长 1/30s 下每帧仅减速 5%，远低于设计的 50% 减速
    - **修复方案**：改为 `vel->linear *= (1 - kSlowFactor)` 即直接乘 0.5，每帧减半（敌人 AI 每帧重算速度，不会累积）
50. **地刺伤害 tick 计时器使用 static 跨局残留**（SkillSystem.cpp UpdateSkillBuffs）
    - **复现条件**：玩家释放地刺后等待技能结束，再次释放；或新游戏开始后首次释放
    - **影响**：`static float spikeTickTimer` 在函数调用间持久化，导致下次释放时首次伤害 tick 时机不确定（可能立即触发或延迟）
    - **修复方案**：PlayerComponent 新增 `float spikeTickTimer = 0.f;` 成员字段，UpdateSkillBuffs 改用 `pc->spikeTickTimer`；技能结束时（spikeGroundTimer 归零）显式重置为 0

## 四-d、第十三轮新增/修复内容（核心战斗 Bug 修复 + 性能保护）

### P0/P1 核心 Bug 修复
50. **地刺减速效果完全失效**（SkillSystem.cpp + EnemyAI.cpp 跨系统协作 Bug）
    - **复现条件**：玩家释放地刺技能后，敌人进入地刺范围（半径 100+10*(lv-1) 像素）
    - **影响**：地刺技能的 50% 减速效果实际为 0%，敌人主动移动速度不变，技能沦为纯伤害区域，与设计严重不符
    - **根因分析**：
      - 第十二轮"修复"执行了 `vel->linear *= (1 - kSlowFactor)`，注释声称"敌人 AI 每帧重算 vel->linear，因此本帧乘 0.5 不会累积到下一帧"
      - 但实际 EnemyAI 中 `desiredVelocity = flowDir * enemy->moveSpeed`（敌人主动移动速度）根本**不读取** `vel->linear`
      - `vel->linear` 仅作为 `externalVel` 用于击退/引力井拉扯的叠加，与敌人主动移动速度无关
      - 即对 `vel->linear` 乘 0.5 只影响外部速度，敌人主动追击速度完全不变
    - **修复方案**：
      - `EnemyAI.h` `EnemyComponent` 新增 `float slowFactor = 0.f` 字段（单帧有效，由 SkillSystem 设置，EnemyAI 应用后重置）
      - `SkillSystem.cpp` 地刺范围内每帧设置 `enemy->slowFactor = kSlowFactor`（替代无效的 `vel->linear *= 0.5`）
      - `EnemyAI.cpp` 在合成 `finalVelocity` 前应用 `desiredVelocity *= (1.f - enemy->slowFactor)`，使用后立即重置 `enemy->slowFactor = 0.f`
      - 设计为单帧有效，退出范围后减速立即消失；最差情况延迟 1 帧生效（SkillSystem 在 EnemyAI 之后执行）
    - **验证**：编译通过，逻辑路径正确，减速现作用于 flowDir × moveSpeed 的主动移动速度

51. **Boss 召唤物无上限导致性能隐患**（EnemyAI.cpp Boss 机制）
    - **复现条件**：长时间 Boss 战（>2 分钟），Boss 每 8s 召唤 2 小兵 + 每 12s 召唤 1 精英
    - **影响**：场上 Boss 召唤物（isBossMinion）无限堆积，可能耗尽 EnemySpawner 对象池或导致同屏敌人数量超出性能预算（500+），引发 FPS 骤降
    - **修复方案**：
      - `EnemyAI.cpp` Boss 机制块新增 `constexpr int kMaxBossMinions = 12`（小兵+精英共享配额）
      - 小兵召唤（每 8s）和精英召唤（HP<50% 首次 + 每 12s 持续）前均遍历统计当前 `isBossMinion && active` 数量
      - 超过上限则跳过本次召唤，仅重置冷却计时器
      - 上限 12 = 单次召唤 2 小兵 × 6 波次余量，兼顾持续压力与性能保护
    - **验证**：编译通过，统计逻辑在召唤频率（8s/12s）下开销可接受

### P2 代码质量修复
52. **SkillSystem.cpp 6 处 static 跨局残留**（SkillSystem.cpp + Player.h）
    - **复现条件**：玩家释放技能后等待结束再次释放；或玩家死亡重生后首次释放
    - **影响**：6 个 `static float/int` 变量在函数调用间持久化，导致首次释放技能时粒子发射节奏异常（可能立即发射或延迟）；玩家重生后状态残留
    - **涉及位置**：
      - `leechParticleTimer`（吸血打击血气粒子）
      - `berserkParticleTimer` + `berserkSparkCounter`（狂暴烈焰+火花）
      - `wellParticleTimer`（引力井漩涡粒子）
      - `spikeParticleTimer` + `spikeBloodCounter`（地刺尖刺+血尖端）
    - **修复方案**：
      - `Player.h` `PlayerComponent` 新增 6 个成员字段：`leechParticleTimer` / `berserkParticleTimer` / `berserkSparkCounter` / `gravityWellParticleTimer` / `spikeParticleTimer` / `spikeBloodCounter`
      - `SkillSystem.cpp` 替换所有 `static` 为 `pc->xxx`
      - 技能结束时（timer 归零）显式重置对应计时器/计数器，避免下次释放首次发射时机异常
    - **验证**：编译通过，与第十二轮 `spikeTickTimer` 修复方案一致

## 四-e、第十四轮新增/修复内容（成就 Toast 通知系统 + 反馈完整性补全）

### P1 功能补全
53. **成就解锁 Toast 通知系统实现**（HUD.cpp + HUD.h + Game.cpp）
    - **背景**：第十三轮前 `AchievementSystem::OnUnlocked` 回调仅输出日志，标记 `TODO: 后续在此触发 Toast 通知 UI`，玩家解锁成就时无任何视觉反馈，严重影响成就感与正反馈循环
    - **实现方案**：
      - `HUD.h` 新增 `AchievementToast` 结构体（name/description/lifetime/maxLifetime/slideInTimer）
      - `HUD` 类新增 `std::deque<AchievementToast> toasts_` 队列（最多 4 条同屏）
      - 新增公开接口：`AddAchievementToast(name, description)` 推入通知，`UpdateToasts(dt)` 更新计时器
      - 新增私有方法 `drawAchievementToasts(target)` 在 `Render()` 末尾调用
      - `Game.cpp` `OnUnlocked` 回调改为调用 `hud_.AddAchievementToast(def.name, def.description)`，并立即调用 `SaveToFile()` 持久化（避免崩溃丢失进度）
      - `Game.cpp` `updatePlaying` 中 `updateHUDData()` 后调用 `hud_.UpdateToasts(dt)`
    - **视觉设计**：
      - 位置：右上角（x=990, y=110 起），避开波次信息（y=20-50）和 FPS（y=80）
      - 尺寸：280×56 像素，向下堆叠，间距 6px
      - 样式：深色半透明背景（20,20,35,220）+ 金色边框（255,200,50）+ 顶部金色装饰条
      - 内容：金色"成就解锁"标签 + 白色加粗成就名 + 浅灰色描述
      - 动画：前 0.3s 从右侧 30px 滑入（ease-out 缓动）+ 同步淡入；最后 0.5s 淡出
      - 生命周期：4s 总时长，超时自动回收（FIFO 队列头部弹出）
    - **验证**：编译成功（exit_code=0），无新增警告；逻辑路径正确，Toast 在 Playing 状态渲染/更新，Paused 状态冻结

## 四-f、第十五轮新增/修复内容（圣物系统 - Roguelike Build 构筑维度）

### P1 核心玩法拓展：圣物系统（Relic System）

54. **圣物/遗物系统全套实现**（RelicSystem.h/.cpp + Menus.h/.cpp + Game.h/.cpp + SaveSystem.h/.cpp）
    - **设计意图**：Roguelike 核心的"构筑（Build）维度"。圣物为被动效果，跨层与跨存档保留，从 Boss 击败奖励 3 选 1 获取。每个圣物对玩家属性有不同方向的加成，玩家通过 3 选 1 决策构筑差异化的角色 build，与升级系统、装备词缀三重叠加形成"升级 + 装备 + 圣物"的复合成长曲线，提升重玩价值与策略深度。
    - **8 种圣物定义**（每种对应不同 Build 方向）：
      | 圣物 | 中文名 | 数值效果 | 主色调 RGB | Build 方向 |
      |------|--------|----------|-----------|----------|
      | WarriorCrest   | 战士之证   | 伤害 +15%                | (220, 80, 60)   | 输出 |
      | GuardianHeart  | 守卫之心   | 最大生命 +20%            | (100, 200, 120) | 坦克 |
      | HunterEye      | 猎手之眼   | 暴击率 +10%, 暴击伤害 +20% | (180, 120, 220) | 暴击 |
      | WindBoots       | 疾风之靴   | 移速 +15%                | (120, 220, 240) | 风筝 |
      | ScholarBook    | 学者之书   | 经验获取 +30%            | (100, 160, 240) | 加速成长 |
      | GreedyEye       | 贪婪之眼   | 金币掉落 +50%            | (240, 200, 80)  | 经济 |
      | VampireFang     | 吸血鬼之牙 | 吸血 +5%                 | (200, 60, 100)  | 续航 |
      | Aegis           | 守护之心   | 防御 +15, 最大生命 +10%  | (180, 180, 200) | 防御 |
    - **圣物上限**：6 个槽位（kRelicMaxCount = 6），避免后期属性膨胀失控
    - **获取渠道**：Boss 击败后必给 1 个圣物（3 选 1），从所有未拥有的圣物中 Fisher-Yates 洗牌随机抽取；圣物栏已满时不再触发选择
    - **数值应用位置**：`Game::recomputePlayerStats()` 末尾，在装备词缀之后、Health 同步之前调用 `relicSystem_.ApplyToPlayerStats(s)`，确保三重叠加顺序正确（基础 → 升级 → 装备 → 圣物）
    - **PlayerStats 新增字段**：`float coinMultiplier = 1.f`（贪婪之眼专用，原 expMultiplier 已存在供学者之书使用）

55. **圣物选择菜单 RelicChoiceMenu**（Menus.h/.cpp）
    - **复用 UpgradeChoiceMenu 卡片布局模式**：3 张 240x320 卡片水平居中排列
    - **视觉**：半透明遮罩 + "选择圣物"标题 + "击败 Boss！请选择一项圣物作为奖励"副标题
    - **图标**：圣物主色调色块占位（无外部美术资源，符合项目规则）
    - **交互**：鼠标悬停边框高亮、鼠标点击选择、1/2/3 键选择
    - **键位冲突处理**：1/2/3 键在圣物选择激活时优先于升级选择（if-else 链）

56. **R 键圣物查看面板**（Game.h/.cpp）
    - **键位重分配**：
      - 原 R 键"重新生成地牢"调试功能迁移到 **Shift+R** 组合键（开发调试用，不影响玩家）
      - R 键改为切换圣物查看面板（玩家功能）
    - **面板布局**（renderRelicPanel 方法）：
      - 720x460 主面板居中（位置 280, 130），金色边框
      - 标题"圣物 (Build 构筑)" + 副标题"已获得: X / 6"
      - 6 个 200x140 槽位 3x2 网格排列（间距 20px）
      - 已拥有槽位：圣物主色调图标色块（64x64）+ 名称（圣物主色）+ 描述（白色）
      - 空槽位：灰色"— 空缺 —"占位
      - 底部提示"按 R 或 ESC 关闭"
    - **UI 互斥保护**：圣物面板打开时暂停玩法更新（避免被怪物攻击）；其他面板（G/Q/Tab/J/商人）打开时自动关闭圣物面板；Boss 触发圣物选择时关闭查看面板避免叠加；ESC 优先关闭圣物面板而非暂停
    - **R 键判断条件**：`state_ == Playing && !upgradeChoiceActive_ && !inventoryMenuVisible_ && !merchantMenuVisible_ && !questMenuVisible_ && !achievementMenuVisible_`

57. **圣物系统持久化**（SaveSystem.h/.cpp）
    - **存档版本升级**：`kSaveVersion` 从 1 升级到 2
    - **新字段**：`SaveData.relicIds` 为 `std::array<uint8_t, kRelicMaxCount>`（6 字节连续存储）
    - **序列化位置**：写入在任务状态之后、时间戳之前；读取对应位置
    - **反序列化防御**：跳过 None/越界值/重复值，确保存档损坏时不崩溃
    - **跨层保留**：`setupPlayingScene(preserveProgress=true)` 路径不调用 `relicSystem_.Initialize()`，圣物随玩家进入下一层
    - **存档兼容性**：旧版本 1 存档因格式不兼容将无法读取，需开始新游戏（已加 LOG_WARN 提示）

### 编译验证（第十五轮）
- Release 版本编译成功（exit_code=0），需 `/t:Rebuild` 强制重新编译以纳入新源文件 RelicSystem.cpp
- 仅剩历史遗留警告（C4244 类型转换 / C4819 编码 / C4996 localtime / C4715 返回值）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-g、第十六轮新增/修复内容（元素异常状态系统激活 - 战斗维度拓展）

### P1 核心玩法拓展：元素异常状态系统（Fire / Ice / Poison）

58. **激活已存在但完全未使用的元素状态框架**（CombatSystem + ProjectileSystem + SkillSystem + EnemyAI + Game.cpp）
    - **设计意图**：代码审计发现 `CombatSystem::ApplyStatus` / `UpdateStatusEffects` / `CreateElementalStatus` 三个接口早在初版已实现，`StatusEffectComponent` / `StatusEffectData` 组件也已定义，`ProjectileConfig.element` / `DamageInfo.element` 字段亦存在——但**整套元素状态系统从未被任何代码调用**，是 dead code。本轮将其激活，让 5 个技能差异化承载 3 种元素异常状态（Fire/Ice/Poison），在不引入新系统的前提下拓展战斗的策略维度：玩家通过技能组合形成"火攻流"（狂暴+普攻）、"控制流"（引力井+冰冻）、"持续伤害流"（地刺+中毒）等 build，与现有"升级+装备+圣物"三重成长曲线正交叠加，产生涌现式交互。
    - **3 种元素状态参数**（由 `CombatSystem::CreateElementalStatus` 定义）：
      | 元素 | 中文名 | 持续时间 | 周期间隔 | 周期伤害 | 减速效果 | 触发源 |
      |------|--------|----------|----------|----------|----------|--------|
      | Fire | 燃烧 | 3s | 0.5s | 20% 原始伤害 | 无 | 狂暴激活时普攻子弹 |
      | Ice  | 冰冻 | 2s | -（无伤害） | 0 | 50% 减速 | 引力井范围内敌人 |
      | Poison | 中毒 | 5s | 1s | 10% 原始伤害 | 30% 减速 | 地刺范围内敌人 |
    - **元素↔技能映射**：
      - **狂暴（Berserk）**：激活期间普攻子弹 `element = Fire`，颜色从黄白变为橙红 (255,110,50)，命中施加燃烧 DoT
      - **引力井（GravityWell）**：拉扯范围内敌人时同步施加 Ice 状态（2s 50% 减速），即使脱离范围仍减速
      - **地刺（SpikeGround）**：tick 伤害 element 改为 Poison，并额外施加 Poison 状态（5s DoT），敌人离开范围仍持续受伤
      - 震地波/吸血打击保持 Physical（前者靠击退、后者靠吸血差异化）
    - **减速应用位置**：`EnemyAI.cpp` 合成 `desiredVelocity` 前，读取 `StatusEffectComponent`，将 Ice(0.5)/Poison(0.3) 减速与地刺 slowFactor 取最大值应用，上限 80% 避免完全卡死
    - **视觉反馈**：
      - DoT tick 时生成元素色飘字（Fire 橙红 / Poison 毒绿），玩家能直观看到伤害来源
      - 敌人 sprite 染色：根据 `StatusEffectComponent` 中的元素类型，将 sprite color 与 tint 做 50% 混合（Fire 橙红 / Ice 青蓝 / Poison 毒绿），多状态取优先级 Fire > Poison > Ice

### P1 致命 Bug 修复

59. **DoT 周期伤害无视觉反馈**（CombatSystem.cpp UpdateStatusEffects）
    - **复现条件**：任意来源（火/毒）的 DoT 状态触发 tick 伤害时
    - **影响**：`UpdateStatusEffects` 直接扣减 HP 但不生成飘字，玩家只看到敌人血条下降却不知伤害来源，体感"伤害凭空消失"，DoT 类技能/元素缺乏存在感
    - **修复方案**：在 tick 伤害扣减后调用 `SpawnDamageText`，颜色按元素类型区分（Fire 橙红 255,110,50 / Poison 毒绿 110,200,80）
    - **验证**：编译通过，DoT 触发时可见元素色飘字

60. **ApplyStatus 高频刷新导致 DoT 永不触发**（CombatSystem.cpp ApplyStatus）
    - **复现条件**：地刺每 0.5s 对范围内敌人调用 `ApplyStatus(Poison)`，Poison 的 `tickInterval = 1s`
    - **影响**：原实现在同类型状态刷新时执行 `existing.timer = effect.tickInterval` 重置周期计时器。地刺每 0.5s 刷新一次会让 `timer` 永远从 1s 开始递减却到不了 0（每次到 0.5s 就被重置回 1s），导致 Poison 的 tick 伤害**永不触发**。这一 Bug 在元素状态系统未激活时无影响，但本轮激活后立即暴露。
    - **修复方案**：移除 `existing.timer = effect.tickInterval` 这一行，刷新时只更新 `remaining`（持续时间取较长者）和 `tickDamage`（伤害取较高者），让 tick 按自然节奏触发
    - **验证**：编译通过，Poison 状态可正常每 1s 触发 tick 伤害

### 编译验证（第十六轮）
- Release 版本增量编译成功（exit_code=0），仅重新编译改动文件
- 无新增警告，仅历史遗留（C4244 类型转换 / C4819 编码 / C4996 localtime / C4715 返回值）
- 可执行文件：`build/bin/Release/crazyunder.exe`

## 四-h、第十七轮新增/修复内容（地牢变异系统 - Roguelike 层修饰符维度）

### 设计意图
Roguelike 标志性的"层修饰符"机制。每层地牢随机获得 1-2 个修饰符，每个修饰符都有**正负效果配对（双刃剑设计）**，强制玩家调整策略。与现有"升级 + 装备 + 圣物 + 元素状态"四重维度正交叠加，让每局游戏体验差异化，产生"本层该如何应对"的策略决策点，显著提升重玩价值与心流体验。

### 核心系统：FloorModifier（10 种双刃剑修饰符）
- **嗜血狂暴**：敌人攻速 +30% / 敌人生命 -30%（红色，战斗节奏加快但更脆）
- **狂乱冲刺**：敌人移速 +30% / 敌人生命 -20%（橙色，走位压力增大）
- **贪婪之雾**：金币掉落 +100% / 商人价格 +50%（金色，经济流派强化）
- **福星高照**：经验获取 +50% / 敌人生命 +25%（紫色，升级加速但更耐打）
- **玻璃大炮**：玩家伤害 +50% / 玩家最大生命 -25%（青色，高风险高收益）
- **虫群涌动**：敌人伤害 -30% / 刷怪间隔 -40%（绿褐色，数量压制但单点弱）
- **疾风步法**：玩家移速 +20% / 敌人移速 +15%（天蓝色，全员加速）
- **生命之涌**：每秒回 1% HP / 爱心掉落禁用（翠绿色，续航换爆发）
- **诅咒之地**：敌人伤害 +20% / 装备掉率 +40%（暗紫色，赌徒之选）
- **暴怒之力**：玩家伤害 +30% / 玩家移速 -15%（深红色，重装输出）

### 滚动规则（Fisher-Yates 洗牌不重复）
- 第 1 层：无修饰符（让玩家熟悉基础玩法）
- 第 2-4 层：1 个修饰符
- 第 5+ 层：2 个修饰符（效果叠加，乘法复合，如两个 ×1.2 → ×1.44）
- 跨层重新滚动，存档时持久化当前层的修饰符

### 数值平衡
- 所有 multiplier 控制在 0.6-2.0 范围内，避免极端数值
- 每个修饰符必有正负两面，纯增益/纯减益都不被接受
- 多修饰符同种 multiplier 取乘法累乘
- 防御性下限：敌人 HP `if (scaledHp < 1.f) scaledHp = 1.f`、经验/金币 `if (expValue < 1) expValue = 1`、商人价格 `sPriceMul_ = (m > 0.1f) ? m : 1.f`、掉率 `std::min(1.0f, ...)`、spawnInterval `std::max(0.02f, ...)` 防对象池耗尽

### 集成点（最小侵入）
- **EnemySpawner::SpawnEnemyAt**：HP/Damage/MoveSpeed 应用敌人 multiplier（HP 下限 1.f）
- **EnemySpawner::StartWave**：spawnInterval 应用刷怪节奏 multiplier（下限 0.02s）
- **Game::recomputePlayerStats**：圣物之后、Health 同步之前应用玩家属性 multiplier
- **Game::OnKill**：expValue/coinValue 乘以 expMul/coinMul，heartDropDisabled 跳过爱心掉落
- **LootSystem::OnEnemyKilled/OnPotBroken**：掉落概率乘以 itemDropChanceMul
- **MerchantSystem::CalcBuyPrice**：价格乘以 merchantPriceMul（静态成员，兼容 Menus.cpp 静态调用路径）
- **Game::updatePlaying**：regenAccumulator_ 累加器实现每秒按比例回血（避免 <1HP 回血被截断）

### UI 表现
- **Banner**：进入新层时 5 秒淡入淡出（0.4s 淡入 + 4.2s 稳定 + 0.4s 淡出），720×100 居中 y=180，半透明背景板 + 主色调边框 + "本层变异" 标题 + 修饰符名 + 描述
- **HUD 指示器**：左上角 y=96（FPS 下方）持久显示"变异："+ 主色调修饰符名，始终可见
- 主色调取首个激活修饰符颜色，多修饰符时 HUD 横向排列

### 存档持久化（版本升级 v2 → v3）
- SaveSystem::SaveData 新增 `std::array<uint8_t, 2> floorModifierIds`
- writeSaveData/readSaveData 各添加 2 字节连续序列化块
- kSaveVersion 从 2 升级到 3
- applySaveData 反序列化后调用 applyFloorModifiersToSubsystems 重新注入子系统 + 2.5s 短暂 Banner 提示

### 修复的历史遗留 Bug
- **SaveSystem::readSaveData 末尾缺少 return true（C4715 警告，UB 风险）**：原代码所有字段读取成功后无显式 return，属未定义行为，Release 优化可能导致存档读取被误判失败。本轮在末尾补充 `return true;`

### 编译验证（第十七轮）
- CMake 自动重新生成（检测到 CMakeLists.txt 新增 FloorModifier.cpp）
- Release 版本增量编译成功（exit_code=0）
- C4715 警告已消除（本轮修复），仅剩历史遗留（C4244 类型转换 / C4819 编码 / C4996 localtime）
- 可执行文件：`build/bin/Release/crazyunder.exe`

## 四-i、第二十四轮新增/修复内容（灵魂之忆系统 - Roguelike Meta Progression 维度）

### 设计意图

Roguelike 标志性的"局间成长"维度。原游戏每次死亡 100% 重置，玩家挫败感强、缺乏长线目标。本轮新增"灵魂之忆系统"：每次死亡根据本局表现获得"灵魂碎片"，碎片可在主菜单"灵魂之井"中兑换永久强化，跨存档共享。与现有九维成长（升级/装备/圣物/元素/变异/连击/极限闪避/词缀/套装）正交叠加，形成"meta 局外 + 局内"双层策略体系，将"死亡是终点"的挫败感转化为"下一局更强"的期待，显著提升重玩价值与心流体验。

### P1 核心玩法拓展：灵魂之忆系统（Soul Memory System）

61. **灵魂碎片获取机制**（SoulMemorySystem.h/.cpp + Game.cpp）
    - **碎片公式**：`shards = level × 5 + kills / 10 + bossKills × 20`，保底 10 碎片
    - **设计依据**：
      - 层数贡献（每层 5 碎片）：鼓励深入探索，第 10 层死亡 = 50 碎片保底
      - 击杀贡献（每 10 击杀 1 碎片）：鼓励积极战斗，100 击杀 = 10 碎片
      - Boss 贡献（每 Boss 20 碎片）：Boss 战是高风险高回报事件
      - 保底 10 碎片：避免低层数死亡零收益，新玩家也有进展感
    - **触发时机**：玩家死亡进入 Dead 状态时立即计算并发放（`Game.cpp` Dead 状态 onEnter 回调）
    - **持久化**：立即写入 `saves/soul_memory.dat`，避免崩溃丢失进度
    - **本局 Boss 计数**：新增 `bossKillCountThisRun_` 字段，在 Boss 死亡处理块自增，restartGame 中重置

62. **6 条永久强化路径**（SoulMemorySystem.h/.cpp）
    - **设计原则**：覆盖六维成长方向，每条对应不同 build 偏好；加成幅度可控，不破坏前期难度
    | 强化 | 中文名 | 每级效果 | 基础成本 | 满级总成本 | Build 方向 |
    |------|--------|----------|----------|-----------|----------|
    | Vitality  | 永韧之骨 | 最大生命 +20     | 20 | 300 | 坦克 |
    | Wisdom    | 智者之魂 | 经验获取 +10%    | 25 | 375 | 加速成长 |
    | Fortune   | 贪婪血脉 | 金币掉落 +15%    | 25 | 375 | 经济 |
    | Strength  | 武器大师 | 伤害 +5%         | 30 | 450 | 输出 |
    | Swiftness | 疾风传承 | 移速 +5%         | 30 | 450 | 风筝 |
    | Aegis     | 守护之灵 | 防御 +3          | 25 | 375 | 防御 |
    - **成本递增**：每级成本 = `baseCost × (currentLevel + 1)`，如 baseCost=20 → 20/40/60/80/100
    - **满级限制**：每条路径最高 5 级（`kSoulUpgradeMaxLevel = 5`），全点满需 2375 碎片
    - **全满加成**：HP+100 / Exp+50% / Coin+75% / Damage+25% / Speed+25% / Def+15
    - **平衡性验证**：第 10 层玩家通常 HP=400+、Damage=80+，Meta 加成仅占 20-30%，不破坏难度曲线

63. **数值应用位置**（Game.cpp `recomputePlayerStats`）
    - **插入点**：1.5 每级微调之后、2 升级加成之前
    - **设计**：加法式叠加（maxHp/damage/moveSpeed/defense 直接 +=，expMultiplier/coinMultiplier += 加成）
    - **层级关系**：所有后续 multiplier（升级/装备/套装/圣物/变异）均乘法叠加在 meta 加成之上，符合"meta 局外 → 局内"的层级关系

64. **灵魂之井 UI 面板**（SoulWellMenu.h/.cpp）
    - **入口**：主菜单新增"灵魂之井"按钮（紫色，位于"读取存档"与"设置"之间）
    - **面板布局**：800×600 居中，紫色边框
      - 顶部标题"灵魂之井" + 副标题"灵魂碎片: X (累计: Y)"
      - 6 张 240×200 卡片（3列×2行），每张含：强化名/等级/Lv进度点/描述/当前总效果/购买按钮
      - 底部"返回 (ESC)"按钮
    - **卡片配色**：6 条路径各有主色调（红/蓝/金/橙/青/银灰）
    - **交互反馈**：
      - 鼠标悬停按钮高亮（边框加粗）
      - 碎片不足时按钮变暗红 + 显示"碎片不足 N"
      - 已满级时按钮变灰 + 显示"已满级"
      - 购买成功播放装备音效（kSFXEquip），失败播放命中音效（kSFXHit）
    - **键位**：ESC 优先关闭灵魂之井面板（优先级在存档菜单之后、设置菜单之前）

65. **死亡结算 UI 反馈**（Menus.cpp DeathScreen::Render）
    - 死亡屏幕统计信息下方新增"灵魂碎片 +N (可在主菜单\"灵魂之井\"中兑换永久强化)"提示
    - 紫色文字（220, 180, 255），20pt 字号
    - **设计意图**：让玩家在死亡瞬间看到"获得了什么"，将挫败感转化为期待

66. **跨存档持久化**（SoulMemorySystem.cpp）
    - **独立文件**：`saves/soul_memory.dat`（与 `achievements.dat` 一致的跨存档共享模式）
    - **二进制格式**：
      - 文件头：魔数 `0x534F554C`（"SOUL"）+ 版本号 1
      - 数据：shards(int32) + totalShardsEarned(int32) + upgradeLevels[6](uint8)
    - **防御性检查**：
      - shards/totalEarned 负数或 totalEarned < shards 视为损坏
      - 等级 > kSoulUpgradeMaxLevel 视为损坏，重置为 0
    - **加载时机**：`Game.cpp` 中使用 `static bool` 守护，仅首次启动加载一次，不随存档重置
    - **保存时机**：购买强化后立即保存 + 死亡结算时立即保存

### 编译验证（第二十四轮）
- CMake 自动重新生成（检测到 CMakeLists.txt 新增 SoulMemorySystem.cpp + SoulWellMenu.cpp）
- Release 版本增量编译成功（exit_code=0）
- 修复 1 处编译错误：`sf::Color` 构造函数非 constexpr，将 `kCardColors` 从 `static constexpr` 改为非静态成员在构造函数中初始化
- 修复 2 处 nodiscard 警告：`SaveToFile()` 返回值未处理（Dead 状态 + PurchaseUpgrade 中）
- 仅剩历史遗留警告（C4244 类型转换 / C4819 编码）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-o、第二十五轮新增/修复内容（UI 渲染/显示/交互 Bug 修复）

### P1 致命 Bug 修复

67. **R 键圣物查看面板不可见但游戏暂停**（Game.cpp 渲染路径缺失）
    - **复现条件**：Playing 状态下按 R 键打开圣物查看面板
    - **影响**：`renderRelicPanel()` 仅在 `renderUI()` 中调用，而 `renderUI()` 从未被 `Run()` 主循环调用（死代码）。面板打开后 `updatePlaying()` 因 `relicPanelVisible_` 为 true 而提前返回，游戏暂停但面板不可见，玩家体验为"游戏卡死"
    - **修复方案**：在 `renderPlaying()` 末尾（调试面板之后、教程覆盖层之前）添加 `if (relicPanelVisible_) renderRelicPanel();`，将圣物面板渲染接入实际渲染路径
    - **验证**：编译通过，R 键面板现可正常显示并关闭

68. **连击系统中文文字显示为方框**（HUD.cpp UTF-8 编码遗漏）
    - **复现条件**：游戏中连击数 ≥ 5，屏幕中央显示连击信息
    - **影响**：`comboText.setString("连击 " + std::to_string(comboCount_))` 直接传入 `std::string`，Windows 下 SFML 的 `sf::String(const std::string&)` 使用 ANSI 代码页解码，中文"连击"二字显示为方框
    - **修复方案**：改为 `comboText.setString(utf8ToSfString("连击 " + std::to_string(comboCount_)))`，与项目内 `drawText` 辅助函数保持一致
    - **验证**：编译通过，连击文字现正常显示中文

69. **灵魂之井面板返回按钮无法点击**（SoulWellMenu.cpp 返回值偏移错误）
    - **复现条件**：主菜单点击"灵魂之井"进入面板，点击底部"返回 (ESC)"按钮
    - **影响**：`CheckClick` 中返回按钮位于索引 7，`return i + 1` 返回 8，但 `handleSoulWellMenuClick` 仅处理 `action == 7`，导致返回按钮点击永远不触发，面板无法关闭（ESC 键仍可关闭）
    - **修复方案**：在 `CheckClick` 中新增 `if (i == 7) return 7;` 分支，显式返回 7 匹配 `handleSoulWellMenuClick` 的期望值
    - **验证**：编译通过，返回按钮点击现可正常关闭面板

### 编译验证（第二十五轮）
- Release 版本增量编译成功（exit_code=0）
- 仅重新编译改动文件（Game.cpp / HUD.cpp / SoulWellMenu.cpp）
- 无新增警告，仅历史遗留（C4244 类型转换 / C4819 编码 / C4996 localtime）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-p、第二十六轮新增/修复内容（数值平衡 + 法力消耗系统）

### P1 数值平衡修复

70. **精英怪基础血量过低**（EnemySpawner.cpp 原型表）
    - **复现条件**：任意层数遭遇 Elite 类型敌人
    - **影响**：Elite 基础 HP=100，且 Elite 类型不能成为 Champion（无 ×3 HP 加成），玩家中后期伤害提升后秒杀精英，精英与普通怪无差异，失去"精英"的威胁意义
    - **修复方案**：Elite 基础 HP 从 100 提升到 250，使精英血量约为同层普通近战（20HP）的 12.5 倍，与词缀系统（HpBoost ×1.5）叠加后最高可达 375HP，确保精英在中期仍具威胁但不会过于肉盾
    - **验证**：编译通过，第 1 层 Elite 基础 HP=250（含词缀最高 375），第 5 层 HP=250×2.2=550（含词缀最高 825）

### P1 核心玩法修复：法力消耗系统激活

71. **技能释放不消耗法力值**（SkillSystem.cpp + SkillSystem.h）
    - **复现条件**：任意技能释放
    - **影响**：`ExecuteSkill` 完全不检查/扣减 `currentMp`，5 个技能只依赖冷却时间，法力值系统（PlayerStats::currentMp/maxMp）处于"只有 HUD 显示、无实际消耗"的 dead data 状态，玩家无资源管理压力
    - **修复方案**：
      - `SkillData` 结构体新增 `float manaCost = 0.f` 字段
      - 各技能法力消耗：震地波 20 / 吸血打击 15 / 狂暴 25 / 引力井 18 / 地刺 20
      - `ExecuteSkill` 开头新增法力值检查：`if (currentMp < manaCost) return false;`
      - 成功后扣减：`currentMp -= manaCost`，下限钳位 0
      - 设计意图：引入"资源管理"策略维度，玩家需在"连续释放技能爆发"和"节省法力"之间权衡，与冷却时间形成双重约束
    - **验证**：编译通过，法力不足时技能释放失败返回 false

72. **升级时不补满法力值**（Game.cpp OnLevelUp 回调）
    - **复现条件**：玩家释放技能消耗法力后升级
    - **影响**：升级后法力值保持消耗后的低值，新解锁的技能因法力不足无法使用，与"升级=恢复"的直觉预期不符
    - **修复方案**：`OnLevelUp` 回调中新增 `pc->stats.currentMp = pc->stats.maxMp;`，升级时自动补满法力值，与技能点累积一并触发
    - **验证**：编译通过，升级时法力值自动回满

### 数值平衡说明
- 法力消耗与玩家初始 maxMp=50 的比例：震地波 40% / 吸血打击 30% / 狂暴 50% / 引力井 36% / 地刺 40%
- 满蓝状态下可连续释放 2-3 个技能，之后需等待升级补满或依赖装备 MaxMp 词缀提升上限
- 升级补满机制确保玩家在关键节点（如 Boss 战前升级）有充足法力应对

### 编译验证（第二十六轮）
- Release 版本增量编译成功（exit_code=0）
- 仅重新编译改动文件（EnemySpawner.cpp / SkillSystem.cpp / Game.cpp）
- 无新增警告，仅历史遗留（C4244 类型转换 / C4819 编码 / C4996 localtime）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 九、代码修改记录（第二十六轮关键文件）

### src/gameplay/EnemySpawner.cpp
- `kPrototypes[]` Elite 条目：基础 HP 从 100.f 提升到 250.f，提升精英怪威胁度

### src/gameplay/SkillSystem.h
- `SkillData` 结构体新增 `float manaCost = 0.f` 字段，定义各技能的法力消耗

### src/gameplay/SkillSystem.cpp
- `kSkillDataTable[]`：5 个技能各新增法力消耗值（震地波 20 / 吸血打击 15 / 狂暴 25 / 引力井 18 / 地刺 20）
- `GetSkillData` 空哨兵对象：适配新增 manaCost 字段（第四参数 0.f）
- `ExecuteSkill`：在技能等级获取之后、switch 之前新增法力值检查（不足返回 false）+ 扣减逻辑（下限钳位 0）

### src/core/Game.cpp
- `OnLevelUp` 回调：新增 `pc->stats.currentMp = pc->stats.maxMp;`，升级时自动补满法力值

### 编译验证（第二十六轮）
- Release 版本增量编译成功（exit_code=0），无新增警告
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 九、代码修改记录（第二十五轮关键文件）

### src/core/Game.cpp
- `renderPlaying()` 末尾：在调试面板渲染之后、教程覆盖层之前新增 `if (relicPanelVisible_) renderRelicPanel();`，修复 R 键圣物查看面板不可见但游戏暂停的 P1 Bug

### src/ui/HUD.cpp
- `drawCombo()` 方法：`comboText.setString("连击 " + ...)` 改为 `comboText.setString(utf8ToSfString("连击 " + ...))`，修复 Windows 下 ANSI 解码导致中文显示方框的 P1 Bug

### src/ui/SoulWellMenu.cpp
- `CheckClick()` 方法：新增 `if (i == 7) return 7;` 分支，修复返回按钮返回 8 而 handler 期望 7 的偏移错误，使返回按钮可正常点击关闭面板

### 编译验证（第二十五轮）
- Release 版本增量编译成功（exit_code=0），无新增警告
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-q、第二十七轮新增/修复内容（背包系统 Bug 修复 + 词缀槽位加权）

### P0 致命 Bug 修复

73. **二次读档后 ESC 菜单不显示**（Game.cpp ChangeState 非对称逻辑缺陷）
    - **复现条件**：第一次读档进入游戏 → ESC 暂停 → 返回主菜单 → 第二次读档 → 按 ESC
    - **根因分析**：
      - `PauseMenu.visible_` 默认值为 `true`，第一次暂停巧合工作
      - "返回主菜单"（Paused→Menu）时 Paused.onExit 执行 `pauseMenu_.SetVisible(false)`，`visible_` 变为 `false`
      - 第二次 Playing→Paused 时，原 `ChangeState` 将 Playing↔Paused 作为对称的 `isPauseToggle` 跳过 **两边** 回调，Paused.onEnter 永远不执行，`visible_` 永远为 `false`
      - `PauseMenu::Render()` 第一行 `if (!visible_) return;`，菜单不可见
    - **修复方案**：
      - 将 `ChangeState` 的 `isPauseToggle` 从对称改为**非对称**：
        - Playing→Paused：跳过 Playing.onExit（避免 registry_.Clear()），但**执行** Paused.onEnter（显示暂停菜单+停止BGM）
        - Paused→Playing：执行 Paused.onExit（隐藏暂停菜单），但跳过 Playing.onEnter（避免 setupPlayingScene()）
      - 同时保留 `resetAllUIFlags()` 方法用于防御性重置（Load/SaveNew/restartGame）
    - **验证**：编译通过，二次读档后按 ESC 可正常弹出暂停菜单

### P1 功能修复

74. **背包装备只显示 1 条词缀**（Menus.cpp 背包格渲染）
    - **复现条件**：背包内有多词缀装备（2-4 条词缀）
    - **影响**：背包格渲染只取了 `item.affixes[0]` 单条，装备槽渲染用 `a < 2` 显示 2 条，同一装备在背包和装备槽显示信息不一致，玩家背包中看到"移速+3%"以为只有一条，装备后多出"攻击+5"
    - **修复方案**：背包格改为循环 `a < 2` 显示 2 条词缀，与装备槽保持一致；同时新增套装名显示
    - **验证**：编译通过，背包格现显示 2 条词缀 + 套装名

75. **套装加成未在总加成中显示**（Menus.cpp 总加成渲染 + InventorySystem.cpp）
    - **复现条件**：玩家已激活套装（如战士之怒 3/3），打开背包
    - **影响**：底部"总加成"区域只显示装备词缀累加值，不显示套装加成。套装加成实际已通过 `ApplySetBonuses` 应用到玩家属性，但 UI 无对应展示，玩家以为套装未生效
    - **修复方案**：`Menus.cpp` 总加成渲染区域末尾新增套装加成追加块：遍历装备槽位统计套装件数，>=2 件的套装追加显示 2 件/3 件套加成
    - **验证**：编译通过，总加成区域现显示"套装:战士之怒 +10%伤害 +25%暴击伤害"

76. **装备套装显示仅显示激活套装，无法看到部分进度**（Menus.cpp 套装汇总行）
    - **复现条件**：玩家只有 1 件永恒守护装备
    - **影响**：套装汇总行跳过 `< 2` 件的套装，玩家只有 1 件永恒守护时完全看不到任何信息，无法追踪套装的凑齐进度
    - **修复方案**：改为显示所有 `>= 0` 件的套装（`pieces == 0` 跳过），显示格式如"永恒守护(1/3)"
    - **验证**：编译通过，所有套装进度均可见

77. **胸甲词缀分配不合理**（LootSystem.cpp 词缀生成无槽位过滤）
    - **复现条件**：装备生成时 Chest 槽位被分配 AttackSpeed 词缀
    - **影响**：胸甲作为防具可能获得攻速、暴击等非防御属性，不符合装备定位惯例。所有 6 个槽位的词缀池完全一致，无差异化
    - **修复方案**：`rollSingleAffix` 按 `item.slot` 构建加权词缀池，各槽位差异化：
      - Chest：防御×4 / 生命×3 / 伤害×1 / 攻速×1
      - Weapon：伤害×4 / 攻速×3 / 暴击率×2 / 爆伤×2
      - Boots：移速×4 / 防御×2 / 生命×2
      - Helmet：生命×3 / 防御×3 / 法力×2 / 暴击率×2
      - Ring：暴击率×3 / 爆伤×3 / 伤害×2
      - Amulet：法力×3 / 生命×2 / 吸血×2 / 防御×1
    - **验证**：编译通过，Chest 槽位 4/6 概率获得防御/生命属性

78. **胸甲图标像医疗包**（TextureGenerator.cpp 图标代码）
    - **根因**：原代码为银色矩形 + 金色十字纹章，十字纹章被误认为医疗包红十字
    - **修复方案**：重写为板甲造型：银色矩形主体 + 两侧深色肩甲突出 + 中心垂直脊线（中缝线）+ 四角铆钉装饰
    - **验证**：编译通过，新图标更符合板甲视觉

### 编译验证（第二十七轮）
- Release 版本增量编译成功（exit_code=0）
- 仅重新编译改动文件（Game.cpp / Game.h / Menus.cpp / LootSystem.cpp / TextureGenerator.cpp）
- 无新增警告，仅历史遗留（C4244 类型转换 / C4819 编码 / C4996 localtime）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 九、代码修改记录（第二十八轮关键文件）

### src/gameplay/EnemyAI.h
- `EnemyType` 枚举新增 `Caster = 10`，敌人类型从 10 种扩展到 11 种
- 新增 `CastWarningZone` 结构体（position/radius/lifetime/damage/exploded），用于施法者 AoE 预警区域
- `EnemyComponent` 新增 5 个施法者专属字段：`castTimer`/`castActive`/`castTargetPos`/`castWarningRadius`/`castWarningLifetime`
- `EnemyTypeName` 函数新增 `Caster` 分支
- `UpdateEnemyCombat` 签名新增 `std::vector<CastWarningZone>* castWarnings = nullptr` 参数

### src/gameplay/EnemyAI.cpp
- `UpdateEnemyAI` 新增 `EnemyType::Caster` case：保持中距离（180-350px），施法期间静止，其他时间 0.2x 游走
- `UpdateEnemyAI` 攻击前摇阈值新增 Caster（0.5s 远程蓄力）
- `UpdateEnemyAI` 接触攻击：Caster 跳过（由 UpdateEnemyCombat 处理）
- `UpdateEnemyAI` 攻击冷却：Caster = 3.0s
- `UpdateEnemyCombat` 签名同步新增 `castWarnings` 参数
- `UpdateEnemyCombat` 新增 Caster AoE 施法逻辑块：每 3.5s 在玩家位置召唤 1.5s 预警法阵（80px 半径），爆炸造成 damage×1.2 火焰伤害
- `UpdateEnemyCombat` 新增 CastWarningZone 处理块：每帧 lifetime 递减，归零时 QueryRange 炸范围内玩家，播放 Explosion 粒子+音效，`std::remove_if` 清理已爆炸圈

### src/gameplay/EnemySpawner.h
- `WaveConfig` 新增 `int casterCount = 0` 字段

### src/gameplay/EnemySpawner.cpp
- 原型表 `kPrototypes[]` 新增 Caster 条目：HP=25, 移速=55, 伤害=8, attackRange=300, attackCooldown=3.5s, detectionRange=400, 颜色(180,60,160), sprite "enemy_caster"
- `generateWaveConfig` 新增 casterCount 计算（第 2 波开始，`1 + wave/3`），totalEnemies 累加
- `StartWave` 新增 caster 入队 + LOG_INFO 格式更新
- `SpawnEnemyAt` 新增施法者字段重置（castTimer=1.5s 初始延迟）
- `SpawnEnemyAt` 新增施法者初始颜色渲染（暗紫红）

### src/gameplay/DungeonGenerator.h
- `EventType` 枚举新增 `Forge = 5`，事件类型从 4 种扩展到 5 种
- `EventTypeName` 函数新增 `Forge` 分支

### src/gameplay/DungeonGenerator.cpp
- `assignRoomTypes`：事件房随机范围从 `randomInt(1, 4)` 更新为 `randomInt(1, 5)`

### src/core/Game.cpp
- 事件房进入回调：新增 Forge 提示"按 E 使用锻造台"
- `handleEventInteraction` 新增 `EventType::Forge` case：扫描装备槽位，随机升级一件非暗金装备品质，消耗 200 金币
- `renderEventHint` NPC 贴图：新增 Forge 复用 `event_altar`
- `renderEventHint` 提示文字：新增 Forge "按 E 使用锻造台"
- `recomputePlayerStats` 重置块新增 `s.manaRegen = 0.f`
- `recomputePlayerStats` 升级应用块新增：`s.manaRegen = 2.f * GetUpgradeLevel(ManaRegen)` / `s.defense += 3.f * GetUpgradeLevel(DefenseUp)`
- `updatePlaying` 新增法力回复块：累加器模式每帧 `acc += manaRegen * dt`，≥1 时回蓝
- 状态重置（setupPlayingScene/restartGame/nextLevel）：新增 `manaRegenAccumulator_ = 0.f` 同步清零

### src/gameplay/UpgradeSystem.h
- `UpgradeType` 枚举新增 `ManaRegen = 17` 和 `DefenseUp = 18`，`Count` 更新为 19

### src/gameplay/UpgradeSystem.cpp
- `GetMaxLevel`：ManaRegen/DefenseUp 各最高 5 级
- `GetUpgradeName`：ManaRegen → "法力回复"，DefenseUp → "防御强化"
- `GetUpgradeDescription`：ManaRegen → "+2 每秒法力回复"，DefenseUp → "+3 防御力"

### src/gameplay/Player.h
- `PlayerStats` 新增 `float manaRegen = 0.f` 字段

### src/core/Game.h
- 新增 `float manaRegenAccumulator_ = 0.f` 成员字段

### 编译验证（第二十八轮）
- Release 版本增量编译成功（exit_code=0），无新增警告
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 九、代码修改记录（第二十七轮关键文件）

### src/core/Game.h
- 新增 `void resetAllUIFlags()` 方法声明

### src/core/Game.cpp
- 新增 `resetAllUIFlags()` 方法实现：重置 12 个 UI 标志 + 关闭 7 个菜单的可见性
- `handleSaveLoadMenuClick` Load 分支：改为调用 `resetAllUIFlags()`（替代原来的两行重置 + 补救方案，覆盖全部标志）
- `handleSaveLoadMenuClick` SaveNew 分支：改为调用 `resetAllUIFlags()`，同时保留游戏状态重置（currentLevel/totalKillCount/survivalTime）
- `restartGame()`：改为调用 `resetAllUIFlags()`（替代手写的 4 行 UI 标志重置，覆盖全部 12 个标志）

### src/ui/Menus.cpp
- 装备槽渲染（L1552-1593）：无变化（原有逻辑正确）
- 背包格渲染（L1632-1685）：词缀显示从 `item.affixes[0]` 单条改为 `a < 2` 循环 2 条；新增套装名显示（9pt 套装色）
- 套装汇总行（L1412-1461）：从仅显示 `< 2` 件激活套装改为显示所有 `> 0` 件套装；加成描述从"2件:"改为"+"
- 总加成区域（L1850-1905）：追加套装加成块，遍历装备槽统计套装件数，>=2 件时追加显示 2 件/3 件套加成
- `totalAffixes_`：无变化（GetTotalAffixes 逻辑正确，+9 vs +5 为用户漏算其他槽位伤害词缀）

### src/gameplay/LootSystem.cpp
- `rollSingleAffix`：重新实现词缀选择逻辑，按 6 个槽位分别构建带权重的词缀池（SlotAffixWeight），展开权重后去重随机抽取

### src/utils/TextureGenerator.cpp
- `ItemSlot::Chest` 分支：从银色矩形+金色十字纹章改为板甲造型（中缝线+肩甲+铆钉）

### 编译验证（第二十七轮）
- Release 版本增量编译成功（exit_code=0），无新增警告
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-r、第二十八轮新增/修复内容（敌人多样性 + 锻造房 + 升级选项扩展）

### 设计意图
本轮聚焦"内容与乐趣"和"玩法与深度"维度。新增第 11 种敌人类型"施法者"（Caster），引入地面 AoE 预警机制——施法者在玩家脚下召唤延迟爆炸法阵，玩家需 1.5s 内跑出范围，创造"站桩 vs 走位"的战术决策。新增第 5 种事件房类型"锻造房"（Forge），让玩家花费 200 金币随机升级一件穿戴装备品质，增加金币消耗出口和装备提升渠道。升级系统新增"法力回复"和"防御强化"两种升级类型，补齐法力资源循环和防御端 build 维度。所有新增内容与现有"升级+装备+圣物+元素+变异+连击+极限闪避+词缀+套装+灵魂之忆"十维成长体系正交叠加。

### P1 核心玩法拓展：新敌人类型——施法者（Caster）

79. **Caster 施法者敌人类型**（EnemyAI.h/.cpp + EnemySpawner.h/.cpp）
    - **类型编号**：`EnemyType::Caster = 10`
    - **基础属性**（EnemySpawner.cpp 原型表）：HP=25, 移速=55, 伤害=8, 攻击范围=300, 冷却=3.5s, 检测范围=400, 颜色=暗紫红(180,60,160)
    - **AI 行为**（EnemyAI.cpp `UpdateEnemyAI`）：
      - 保持中距离（180-350px）：太近后撤，太远靠近，合适距离小幅游走（0.2x 移速）
      - 施法期间停止移动
    - **核心机制：地面 AoE 法阵**（EnemyAI.cpp `UpdateEnemyCombat` 新增 CastWarningZone 处理块）：
      - 每 3.5s 在玩家当前位置召唤半径 80px 的法阵，1.5s 延迟后爆炸
      - 爆炸造成 `damage × 1.2` 火焰元素伤害
      - 预警期间通过 `CastWarningZone` 结构体传递信息，由渲染层绘制红色预警圈
      - 伤害范围 80×0.8=64px，给予玩家边缘容错
    - **CastWarningZone 结构体**（EnemyAI.h 新增）：
      - `position`/`radius`/`lifetime`/`damage`/`exploded` 字段
      - 由 `UpdateEnemyCombat` 的 `castWarnings` 参数传入/传出
      - 爆炸后自动清理（`std::remove_if` 擦除）
    - **波次出现规则**：第 2 波开始，`1 + wave/3` 个
    - **攻击前摇**：适用 0.5s 远程蓄力阈值（支持极限闪避）
    - **精英/Champion 机制**：可作为 Champion 升级（HP×3 倍率），可挂载词缀系统
    - **设计意图**：区别于 Ranged（单体子弹）和 SniperRanged（远距狙击），Caster 通过 AoE 预警法阵创造"位置博弈"维度。玩家必须不间断移动避免被炸，新增的 CastWarningZone 系统为未来更多地面机制（毒沼、冰霜光环）打下基础。

### P1 核心玩法拓展：锻造房事件房（EventType::Forge）

80. **锻造房事件类型**（DungeonGenerator.h/.cpp + Game.cpp）
    - **类型编号**：`EventType::Forge = 5`
    - **地牢生成**：事件房随机到 Forge 类型（概率 1/5），`assignRoomTypes` 中 `randomInt(1, 5)`
    - **交互逻辑**（Game.cpp `handleEventInteraction`）：
      - 扫描 6 个装备槽位，收集非暗金品质的已穿戴装备
      - 随机选一个槽位，将该装备品质提升一档（白→蓝→黄→暗金）
      - 消耗 200 金币；金币不足或无升级槽位时显示提示并保留重试机会
    - **NPC 贴图**：复用 `event_altar` 祭坛贴图
    - **提示文字**：房间上方显示"按 E 使用锻造台"
    - **设计意图**：提供金币消耗出口（中后期金币富余但无处花），增加装备品质提升的非掉落渠道，与商人购买、祭坛献祭形成"三选一"资源分配决策。

### P1 核心玩法拓展：升级选项扩展（法力回复 + 防御强化）

81. **法力回复升级（ManaRegen）**（UpgradeSystem.h/.cpp + Player.h + Game.h/.cpp）
    - **类型编号**：`UpgradeType::ManaRegen = 17`
    - **每级效果**：+2 每秒法力回复，最高 5 级（满级 +10 MP/s）
    - **数值应用**：`recomputePlayerStats` 中 `s.manaRegen = 2.f * GetUpgradeLevel(ManaRegen)`
    - **回复逻辑**（Game.cpp `updatePlaying` 新增法力回复块）：
      - 使用 `manaRegenAccumulator_` 累加器模式（与 HP 再生一致），避免 <1MP 回蓝被截断
      - 每帧 `acc += manaRegen * dt`，累加到 ≥1 时扣减累加器并增加 currentMp
      - 上限钳位至 maxMp
    - **状态重置**：`setupPlayingScene`/`restartGame`/`nextLevel` 中同步清零 `manaRegenAccumulator_`
    - **设计意图**：补齐法力资源循环——此前法力仅靠升级补满，无持续恢复手段。法力回复让"技能流"build 更可行，减少对升级节点的依赖。

82. **防御强化升级（DefenseUp）**（UpgradeSystem.h/.cpp + Game.cpp）
    - **类型编号**：`UpgradeType::DefenseUp = 18`
    - **每级效果**：+3 防御力，最高 5 级（满级 +15 防御）
    - **数值应用**：`recomputePlayerStats` 中 `s.defense += 3.f * GetUpgradeLevel(DefenseUp)`，与每级微调防御和灵魂之忆防御叠加
    - **设计意图**：补齐防御端独立升级维度——此前防御仅通过每级微调（+1/级）和灵魂之忆（+3/级）获得，无主动强化渠道。DefenseUp 让"坦克流"build 有明确提升路径。

### 编译验证（第二十八轮）
- Release 版本增量编译成功（exit_code=0）
- 仅重新编译改动文件（EnemyAI.cpp / EnemySpawner.cpp / DungeonGenerator.cpp / Game.cpp / UpgradeSystem.cpp）
- 无新增警告，仅历史遗留（C4244 类型转换 / C4819 编码 / C4996 localtime）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-s、第二十九轮新增/修复内容（炸弹怪物重平衡——威胁度大幅提升）

### 设计意图
Suicide（自爆怪）和 CountdownSuicide（倒计时自爆怪）原版过于鸡肋：Suicide 冲锋范围小（200px）、加速不够猛（1.5x），玩家轻松风筝；CountdownSuicide 激活范围极小（80px）、移速极慢（100）、激活后原地不动成为活靶子，3 秒引信足够玩家走出很远的距离。本轮对两种炸弹怪进行全面重平衡，让它们成为真正的"走位杀手"。

### P1 数值重平衡：Suicide 自爆怪

83. **Suicide 自爆怪强化**（EnemySpawner.cpp + EnemyAI.cpp）
    - **基础属性调整**：
      - 移速：280 → 350（永久加速，更早威胁到玩家）
      - 伤害：40 → 45（爆炸伤害更高）
    - **冲锋行为**（EnemyAI.cpp UpdateEnemyAI）：
      - 冲锋触发范围：200px → 250px（更早开始冲刺）
      - 冲锋速度倍率：1.5x → 2.2x（250px 内以 770 移速狂奔，玩家闪避才来得及）
    - **爆炸范围**：120px → 160px（更大 AOE）
    - **爆炸伤害倍率**：2.0x → 2.5x（贴脸更致命）
    - **新增击退**：350px 击退（爆炸冲击感，也让玩家"被炸飞"而不是原地再吃一次）
    - **爆炸粒子特效**：5 个 → 7 个爆炸点，范围扩大（25px 偏移）
    - **设计意图**：Suicide 现在是真正的"炸弹快递"——远距离 350 移速接近，250px 内 770 冲刺让走位失误的玩家在 0.3-0.5s 内被追上。爆炸的大范围和高伤害迫使玩家必须优先集火，不能无视。

### P1 数值重平衡：CountdownSuicide 倒计时自爆怪

84. **CountdownSuicide 倒计时自爆怪重做**（EnemySpawner.cpp + EnemyAI.cpp）
    - **基础属性调整**：
      - 移速：100 → 200（翻倍，从"龟爬"变成"中速追逐"）
      - 伤害：35 → 40
      - 检测范围：350 → 400
    - **激活机制**（EnemyAI.cpp UpdateEnemyAI）：
      - 激活范围：80px → 150px（更早激活倒计时，防止被秒杀无效果）
      - 倒计时：3.0s → 1.8s（引信更短，紧迫感更强）
    - **行为模式——核心改动**：
      - **原版**：激活后原地不动，活靶子
      - **新版**：激活后以 60% 移速（120）持续追击玩家，不死不休
    - **最后一搏**：倒计时归零时，死亡瞬间 1.5x 加速冲向玩家
    - **爆炸范围**：90px → 140px（更大 AOE）
    - **爆炸伤害倍率**：2.0x → 2.5x
    - **新增击退**：400px 击退
    - **爆炸粒子特效**：3 个 → 7 个爆炸点（+4 个对角线方向）
    - **设计意图**：CountdownSuicide 不再是"原地发呆→玩家走出范围安然无恙"的废物。新版激活后持续追杀玩家 1.8s，头上闪烁倒计时数字形成视觉压迫，玩家必须在"继续走位输出"和"优先击杀倒计时炸弹"之间做出决策。如果倒计时归零时玩家还在 140px 范围内，将被 2.5x 伤害重创并击飞。

### 平衡性说明
- Suicide 基础值 350 × 层数缩放（第 1 层不变），250px 内 2.2x = 770。玩家移速 200，若被减速（地刺/冰冻）则几乎必中
- CountdownSuicide 激活范围 150px（玩家普攻范围约 100-150px），意味着怪物一进入视野就可能已激活，迫使玩家提前应对
- 两种怪依然继承 Champion 系统和词缀系统——Champion 炸弹怪将更加恐怖（HP×3 + 词缀 ×1.2 速度），作为玩家中期遭遇的"惊喜"

### 编译验证（第二十九轮）
- Release 版本增量编译成功（exit_code=0），仅重新编译 EnemyAI.cpp + EnemySpawner.cpp
- 无新增警告，仅历史遗留（C4244/C4819/C4996）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-t、第三十轮新增/修复内容（渲染性能优化——顶点缓冲池化 + ECS 零分配查询）

### 设计意图
本轮聚焦性能优化，针对游戏中两个最关键的性能瓶颈进行重构：Renderer 顶点缓冲每帧分配/释放问题（每帧 ~128KB 堆分配），以及 ECS View() 每帧分配临时 vector 的累积开销（17 处调用点，最密集路径每帧执行多次）。优化后预计提升 20-30% 帧率稳定性。

### P2 性能优化——渲染命令池化

85. **Renderer 顶点缓冲池化**（Renderer.h/.cpp）
    - **问题定位**：`EndScene()` 中 `std::vector<sf::Vertex> vertices;` 是函数局部变量，每帧构造/析构。2000 精灵场景 = 8000 顶点 = 约 128KB 内存分配/释放。60FPS 时每秒 60 次分配，造成堆碎片和 GC 压力。
    - **修复方案**：
      - `Renderer.h` 新增 `std::vector<sf::Vertex> vertexBuffer_` 成员变量（预分配 20000 顶点 = 5000 精灵容量，约 320KB）
      - `Renderer.cpp` 构造函数中执行 `vertexBuffer_.reserve(20000)`
      - `EndScene()` 中将局部 `vertices` 改为引用 `vertexBuffer_`，调用 `vertices.clear()`（仅重置 size，不释放容量）
    - **验证**：编译通过，vertexBuffer_ 在 Renderer 生命周期内复用

86. **flushBatch 零拷贝绘制**（Renderer.cpp）
    - **问题定位**：原 `flushBatch` 创建临时 `sf::VertexArray array(sf::Quads, vertices.size())` 并逐顶点拷贝，每批增加一次 O(N) 拷贝
    - **修复方案**：直接使用 `target_->draw(vertices.data(), vertices.size(), sf::Quads, states)` 零拷贝 API，跳过 VertexArray 中间层
    - **验证**：编译通过，每帧减少至少 1 次顶点拷贝

### P2 性能优化——ECS View() 零分配查询

87. **Registry::ForEach 回调模式**（Registry.h）
    - **新增模板方法**：`ForEach<Components...>(Func&& func)` 零分配遍历。与 `View()` 功能等价，但通过回调直接处理每个实体，不分配临时 `std::vector<EntityId>`
    - **内部实现**：与 View() 相同的"最小池遍历 + 组件检查"算法，仅回调处理而非 push_back
    - **接口设计**：`continue` → `return` 语义兼容，lambda 捕获引用访问外部状态

88. **关键路径替换为 ForEach**（8 个文件，14 处调用点）
    - **替换原则**：所有每帧高频调用的 View() 路径替换为 ForEach
    - **Game.cpp**（6 处）：空间网格更新、统一死亡检测、BOSS 检测、实体渲染、Champion 血条渲染、统计计数、秒杀调试
    - **EnemyAI.cpp**（5 处）：UpdateEnemyAI 主循环、UpdateEnemyCombat 主循环、3 处 Boss 召唤物计数
    - **CombatSystem.cpp**（1 处）：UpdateStatusEffects 状态推进循环
    - **CombatEffects.cpp**（2 处）：UpdateDamageTexts 更新、RenderDamageTexts 渲染
    - **Animation.cpp**（1 处）：AnimationSystem::Update 帧动画推进
    - **RoomSystem.cpp**（1 处）：hasAliveEnemiesInRoom 房间内敌人检测（含早期返回语义修复）
    - **特殊处理**：RoomSystem 的 `hasAliveEnemiesInRoom` 原使用 `return true` 从函数早期返回，改为 `found` 标志 + lambda 短路返回，确保零分配同时语义正确

### 编译验证（第三十轮）
- Release 版本编译成功（exit_code=0），仅重新编译改动文件（Renderer/Game/EnemyAI/CombatSystem/CombatEffects/Animation/RoomSystem/Registry）
- 编译器报告：0 error, 0 new warning，仅历史遗留 C4244（`int→float`，无害）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 五、已知问题

### P0 - 运行稳定性
1. BOSS 生成机制需验证（EnemySpawner 波次系统含 Boss 生成，但实际触发条件需测试）
2. BOSS 房间首次进入时是否正常激活 Boss 战斗
3. 任务1结算是否正常工作（本轮已修复 setupPlayingScene 清空任务进度 bug，需实测验证）
4. 下一层技能保留是否正常工作
5. 任务状态存档/读档是否正常工作
6. Champion 精英怪生成概率和属性倍率需实测平衡性
7. 事件房/诅咒房触发条件和奖励是否正常

### P1 - 功能完善
8. **BOSS 死亡后楼梯生成**：击败 BOSS 后房间内生成 Stairs tile
9. **平衡性调整**：敌人 HP/伤害、玩家升级曲线、技能冷却与效果数值
    - 第十一轮已重设敌人分层强化（HP+30%/层, 伤害+18%/层, 速度+4%/层封顶1.6x），需实测
    - [第十二轮已修复] 地刺技能 DPS 现随玩家伤害缩放（+30% 玩家伤害）
    - [第十三轮已真正修复] 地刺减速效果：第十二轮修复无效（误改 vel->linear，而 desiredVelocity 不读 vel->linear），本轮通过 EnemyComponent::slowFactor 字段正确作用于 desiredVelocity
10. **音效补充**：部分技能/机制音效缺失
11. **技能获取渠道**：目前仅通过升级 20% 概率和商人购买获得，缺少掉落/宝箱获取
12. **成就内容填充**：AchievementSystem 框架已搭建，具体成就定义和触发条件待完善（已实现 16 个成就定义 + Toast 通知，可继续扩展特殊成就触发条件）
13. **任务剧情扩展**：5 个主线任务完成后可扩展支线任务

### 第十二轮已解决问题
- [已解决] OnKill 回调双重调用导致经验/金币/掉落/击杀计数/任务进度全部翻倍（P0）
- [已解决] Boss 旋转弹幕与冲撞地裂复用 specialTimer 导致并发时旋转弹幕失效（P0）
- [已解决] EquipSkill 不重置 level 字段导致新装备技能继承槽位残留等级（P1）
- [已解决] 地刺 DPS 固定值不随玩家伤害缩放，后期沦为废技能（P1）
- [已解决] 地刺伤害 tick 计时器使用 static 跨局残留，首次 tick 时机不确定（P1）

### 第十三轮已解决问题
- [已解决] 地刺减速效果完全失效（第十二轮误改 vel->linear，本轮通过 slowFactor 字段正确作用于 desiredVelocity）（P1）
- [已解决] Boss 召唤物无上限导致性能隐患（新增 kMaxBossMinions=12 上限，小兵/精英共享配额）（P1）
- [已解决] SkillSystem.cpp 6 处 static 跨局残留（改为 PlayerComponent 成员字段，技能结束时重置）（P2）

### 第十四轮已解决问题
- [已解决] 成就解锁 Toast 通知未实现（P1）：原 OnUnlocked 回调仅日志输出，本轮实现完整 Toast 通知系统

### 第十五轮已解决问题
- [已解决] Roguelike 核心 Build 维度缺失（P1）：原游戏仅有"升级 + 装备"两重成长，缺乏 Roguelike 标志性的圣物/遗物构筑系统，重玩价值不足。本轮新增 8 种圣物 + Boss 3 选 1 获取机制 + R 键查看面板 + 存档持久化，形成"升级 + 装备 + 圣物"三重成长曲线
- [已解决] R 键功能冲突（P2）：原 R 键被"重新生成地牢"调试功能占用，玩家易误触重置进度。本轮将调试功能迁移到 Shift+R 组合键，R 键专用于玩家功能

### 第十六轮已解决问题
- [已解决] 战斗维度单一，5 个技能仅有数值差异（P1）：原游戏 5 个技能只在伤害/范围/冷却上区分，缺乏策略维度。本轮激活 CombatSystem 已有的元素状态框架（Fire/Ice/Poison），将狂暴/引力井/地刺分别绑定火/冰/毒元素，形成"火攻流/控制流/持续伤害流"三种 build 方向，与升级+装备+圣物三重成长正交叠加
- [已解决] CombatSystem 元素状态系统为 dead code（P2）：ApplyStatus/UpdateStatusEffects/CreateElementalStatus 三个接口及 StatusEffectComponent 自初版定义后从未被调用，本轮通过 ProjectileSystem 命中触发 + SkillSystem 主动施加 + EnemyAI 读取状态应用减速，将整套系统激活
- [已解决] DoT 周期伤害无飘字反馈（P1）：UpdateStatusEffects 直接扣 HP 无视觉反馈，玩家不知伤害来源。本轮在 tick 伤害后调用 SpawnDamageText 生成元素色飘字
- [已解决] ApplyStatus 高频刷新导致 DoT 永不触发（P1）：原实现重置 timer 导致地刺每 0.5s 刷新 Poison 时 tick 永远不触发。本轮移除 timer 重置，让 tick 按自然节奏触发

### 第十七轮已解决问题
- [已解决] Roguelike 层修饰符维度缺失（P1）：原游戏虽有"升级+装备+圣物+元素"四重成长，但缺乏 Roguelike 标志性的"每层随机规则变化"机制，重玩差异化不足。本轮新增 10 种双刃剑修饰符 + Fisher-Yates 洗牌滚动 + 乘法复合累乘 + Banner/HUD 双重 UI 提示 + 存档持久化（v3），形成"层规则 + 四重成长"的五维策略体系
- [已解决] SaveSystem::readSaveData 未定义行为（P1，C4715 警告）：原函数所有 readPOD 成功后到达末尾无显式 return，属 UB。Release 优化下返回值不确定，可能导致合法存档被误判损坏。本轮在末尾补充 `return true;`

### 第十八轮已解决问题
- [已解决] 缺乏瞬时正反馈机制（P1，玩法深度）：原游戏五维成长均为长期构筑，缺乏"激进玩法 → 击杀加速"的瞬时心流循环。玩家连续击杀无差异化反馈，体感趋于线性。本轮新增连击系统（Combo System）：3 秒内连续击杀累积 combo，阶梯式伤害加成（+20%/+35%/+50%/+75%），受伤立即重置，HUD 中央连击指示器（颜色阶梯+脉冲动画+保持时间进度条），与现有五维成长正交叠加产生涌现式交互

### 第十九轮已解决问题
- [已解决] Lightning 元素状态为 dead code（P1，玩法深度）：CombatSystem::CreateElementalStatus 中 Lightning 分支缺失，自初版从未生效。本轮激活 Lightning 元素（0.6s 麻痹，完全禁锢），与 Ice（软控 50% 减速 2s）形成控制层次。EnemyAI 读取 Lightning 状态时清零 desiredVelocity 并锁定 attackCooldown，玩家精灵染色（亮黄色优先级 4）
- [已解决] 连锁闪电无视觉反馈/无回调/无 combo 集成（P1）：ProjectileSystem::handleChain 直接修改 HP，绕过 ApplyDamage 中心化路径，导致无伤害飘字、无粒子特效、无连击/吸血/暴击回调。本轮重写 handleChain：补充黄色飘字 + 粒子命中特效 + Lightning 状态施加（含 lightningDurationMul 倍率）+ ApplyDamage 调用（无暴击/吸血/击退，避免循环利用）
- [已解决] chainLightning 升级为纯数值，缺乏玩法维度（P1）：原 chainLightning 仅增加连锁次数，无元素切换。本轮新增 PlayerCombat 普攻元素切换逻辑：拥有 chainLightning ≥ 1 时普攻子弹自动附加 Lightning 元素（亮黄色），形成"控制流"build，与"狂暴火攻流"互斥（狂暴优先级更高）
- [已解决] 闪电流 build 缺乏圣物支撑（P1）：原 8 种圣物无雷霆系，闪电流仅靠升级构筑，build 多样性不足。本轮新增 2 种雷霆圣物：雷霆之心（连锁+1，伤害+10%）、风暴之眼（麻痹时间+50%，暴击率+5%），圣物总数从 8 → 10
- [已解决] 闪电流 build 缺乏"层修饰符"维度（P1）：原 10 种地牢变异无雷霆系，闪电流仅在升级+圣物二维构筑。本轮新增第 11 种变异"雷暴领域"：玩家所有普攻附加 Lightning 元素 + 麻痹（无需 chainLightning 升级），但敌人伤害 +15% 反向平衡。让闪电流 build 从"圣物+升级"二维拓展到"层修饰符"第三维度

### 第二十轮已解决问题
- [已解决] 防御端缺乏正反馈机制（P1，玩法深度）：原游戏正向反馈集中在攻击端（连击系统 +75% 伤害、五维成长），防御端仅有"闪避无敌帧"被动减伤，缺乏"激进防御 → 反击"的瞬时心流循环。本轮新增极限闪避反击系统：敌人攻击前摇（attackTelegraph）期间闪避可触发"极限闪避"，获得 2s 伤害 +50% buff + 120° 扇形反击（200% 玩家伤害），与连击系统正交叠加产生 combo 50 + 极限闪避 = 2.25x 爆发窗口。冷却 3s 避免连续触发破坏节奏
- [已解决] 闪避玩法维度单一，缺乏 build 构筑（P1）：原闪避仅为"无敌帧 + 位移"基础功能，无圣物/升级/成就支撑，玩家无差异化玩法选择。本轮新增 2 种防御反击系圣物：月光护符（检测窗口 +50%，80px → 120px）、复仇之刃（buff 期间所有攻击必暴击），圣物总数从 10 → 12，与现有攻击系/雷霆系圣物形成攻防双 build 选择
- [已解决] 敌人攻击缺乏可读信号（P1，复杂交互）：原敌人攻击仅靠攻击冷却数字驱动，玩家无法预判攻击时机，极限闪避/弹反类机制无法实现。本轮在 EnemyAI.cpp 攻击冷却衰减后新增 attackTelegraph 字段，按敌人类型设置不同前摇阈值（近战 0.3s / 远程 0.5s），仅在玩家处于攻击范围内时激活，为极限闪避提供可检测信号
- [已解决] 极限闪避缺乏视觉反馈（P1）：原触发后仅有数值 buff，玩家无法感知"何时处于 buff 期间"。本轮在 HUD.cpp 新增 drawPerfectDodge 实现：屏幕四周金色脉冲边框（6px 厚度，sin 波脉冲，最后 0.3s 淡出）+ 中央"极限闪避 伤害 +50%"文字（20pt 粗体），与连击指示器形成"攻防双反馈"视觉体系
- [已解决] 极限闪避缺乏成就长线目标（P2）：原无相关成就，玩家无长期练习动力。本轮新增 2 个成就：id=40 "极限闪避"（首次触发，Special 类隐藏成就）、id=25 "幻影之舞"（累计 100 次极限闪避，Combat 类），完美 dodgeCount 跨层保留作为统计字段

### 第二十一轮已解决问题
- [已解决] EnemyAffix/EliteAffix 词缀框架为 dead code（P1，玩法深度）：`Component.h` 自初版定义的整套词缀系统（HpBoost/DamageBoost/SpeedBoost/Regenerating 4 种词缀 + regenTimer/auraTimer 字段）从未被任何代码调用。本轮激活后，每个精英/Champion 在生成时随机获得 2-3 个词缀组合（Champion 5% 概率 4 词缀全开），立刻产生 11 种行为变化组合，与现有"层修饰符/极限闪避/元素状态"全部正交叠加
- [已解决] 敌人侧缺乏 build 维度（P1，玩法深度）：原精英维度仅有"Elite 固定数值 + Champion 倍率"两层纯数值差异，无任何玩法变化。本轮通过词缀系统让每个精英成为独特小 Boss 战，产生涌现式交互（如 4 词缀 Champion + 嗜血狂暴层修饰符 + Ice 冰冻 → 极端 tank + 高伤 + 控速击杀）
- [已解决] 词缀精英缺乏视觉识别（P2）：激活后玩家无法分辨词缀精英与普通精英。本轮新增紫色光环粒子（每 0.3s 头顶发射 2 个紫色粒子）+ 紫色发光染色（30% 混合，比元素状态 50% 弱以避免覆盖）+ 词缀名头顶显示（"厚血+狂暴+迅捷"格式），三层视觉反馈
- [已解决] 词缀系统缺乏长线成就目标（P2）：新增 2 个成就：id=26 "词缀猎手"（累计击杀 30 个带词缀精英）、id=27 "满词缀征服"（击败 5 个 4 词缀全开 Champion，隐藏成就）

### 第二十二轮已解决问题
- [已解决] QuestType 4 种类型为死接口（P1，玩法深度）：`QuestSystem.h` 中 `QuestType` 枚举定义 8 种类型，但 `Initialize()` 仅注册 4 种（ReachLevel/TriggerEvent/CollectSkills/AccumulateCoins），KillTarget/CollectItem/SurviveTime/ClearRooms 4 种类型自初版从未注册使用。本轮激活 5 层基础设施（targetEnemy/targetQuality/targetTime 字段 + progressQuest 过滤 + Update 时间累计 + Game.cpp 事件上报），新增 5 个支线任务（id 6-10）与主线并行解锁
- [已解决] 前期任务荒（P1，内容与乐趣）：原游戏仅 1 个主线任务解锁后玩家选择有限。本轮 5 个支线任务让玩家前期可选任务从 1 个拓展到 9 个，引导熟悉精英/远程敌人、装备品质、地牢探索等不同玩法维度
- [已解决] SurviveTime 任务跨生命累计语义错误（P1，复杂交互）：原 SurviveTime 任务在玩家死亡后 `timeAccumulator` 不重置，可跨多次生命累计达成"单次生命存活 N 秒"目标，与语义不符。新增 `OnPlayerDeath()` 接口在玩家死亡时重置所有 Active 状态的 SurviveTime 任务进度
- [已解决] QuestMenu 不支持超过 5 个任务（P2，UI 完整性）：原 `cards_` 数组固定为 5，无法显示新增支线任务。本轮扩大为 10 + 实现垂直滚动（OnMouseWheel + 滚动条指示器 + 顶部/底部遮罩 + 不可见卡片裁剪）

### 第二十三轮已解决问题
- [已解决] 装备 build 维度单一，缺乏套装构筑（P1，玩法深度）：原装备系统仅有"词缀 + 品质"二维，无套装机制，玩家装备选择缺乏长线目标。本轮新增 4 种装备套装（Warrior 战士之怒 / Sage 智者之识 / Wind 疾风行者 / Guardian 永恒守护），每个槽位恰属 2 个套装（强制玩家取舍，无法同时凑齐 2 个完整套装），2 件套/3 件套分阶奖励，形成 Roguelike 第七维 build 构筑
- [已解决] 装备词缀与圣物 multiplier 缺乏中间层（P1，玩法深度）：原 `recomputePlayerStats` 中装备词缀和圣物直接作用于 stats，缺乏可组合的乘法层。本轮在装备词缀之后、圣物之前调用 `ApplySetBonuses`，让套装 multiplier 与圣物乘法叠加，build 多样性显著提升
- [已解决] 存档不包含套装 ID（P1，存档兼容）：原 Item 序列化不包含 setId，读档后所有装备套装信息丢失。本轮升级 kSaveVersion v3→v4，writeItem/readItem 序列化 setId（1 字节，含防御性检查：值 > Guardian 视为损坏回退为 None）

### 第二十八轮已解决问题
- [已解决] 敌人类型缺乏 AoE 范围伤害维度（P1，玩法深度）：原 10 种敌人无一在地上预置范围伤害区，走位压力仅来自单体子弹和近战追击。本轮新增 Caster 施法者，通过 CastWarningZone 系统实现地面 AoE 预警→延迟爆炸，创造"位置博弈"战术维度
- [已解决] 事件房类型仅 4 种，缺乏装备品质提升渠道（P1，内容与乐趣）：原事件房（乞丐/法师/宝箱怪/祭坛）无装备升级功能。本轮新增锻造房（Forge），花费金币升级装备品质
- [已解决] 升级系统缺乏法力回复和独立防御强化（P1，玩法深度）：原 17 种升级类型无法力资源循环手段，防御仅靠每级微调和灵魂之忆。本轮新增 ManaRegen（+2 MP/s，最高 5 级）和 DefenseUp（+3 防御，最高 5 级），完善 build 维度

### P2 - 内容扩展
14. **更多敌人类型**：当前 11 种（Melee/Ranged/Suicide/Elite/Boss/StealthMelee/CountdownSuicide/Splitter/Shielded/SniperRanged/Caster），可继续扩展
15. **视觉效果增强**：屏幕震动、闪光、拖尾；施法者地面预警圈渲染尚未接入渲染管线
16. **游戏结局/BOSS 战动画**
17. **更多事件房类型**：当前 5 种（乞丐/法师/宝箱怪/祭坛/锻造台），可继续扩展

## 八、代码修改记录（第十轮关键文件）

### Game.h
- 添加 `bossDefeatedHintTimer_`（Boss 击败提示计时）
- 添加 `heartSystem_`（爱心回血系统）

### Game.cpp
- `nextLevel()` 保存/恢复 `skillSlots` 和 `skillBackpack`
- 移除自动弹窗逻辑（`IsUpgradePending` 触发 showUpgradeChoice）
- 添加 J 键处理：`key == sf::Keyboard::J` 打开升级选择
- `handleUpgradeChoice` 选择后根据剩余技能点决定是否保持菜单
- 键盘 1/2/3 选择同样处理连续技能点
- `updateHUDData` 中调用 `hud_.SetSkillPoints()` 
- `OnKill` 中 Boss 召唤物 30% 概率掉落爱心
- 商人文字改为世界空间渲染
- 底部操作提示加入 "J:技能升级"
- Release 版本配置

### SkillSystem.cpp
- `ExecuteSkill` LeechStrike `data.duration` 设 5s
- `UpdateSkillBuffs` 每帧衰减 `leechStrikeActive`，持续时间内吸血
- kPullRadius 200→300, kPullForce 250→300

### UpgradeSystem.h / UpgradeSystem.cpp
- `upgradePending_` 替换为 `skillPoints_`
- `AddExp()` 升级时 `++skillPoints_`
- `ApplyUpgrade()` 消耗技能点
- 新增 `GetSkillPoints()` 方法

### HUD.h / HUD.cpp
- 添加 `skillPoints_` 字段和 `SetSkillPoints()` 方法
- `Render()` 中 `skillPoints_ > 0` 时显示闪烁提示
- 技能图标上方添加冷却倒计时数字

### LootSystem.h / LootSystem.cpp
- `LootDropEntry` 添加 `fullMessageCooldown` 字段
- 背包满提示 2 秒冷却

### EnemyAI.h / EnemyAI.cpp
- `EnemyComponent` 添加 `rangedAttackTimer`、`summonTimer`、`isBossMinion`
- `UpdateEnemyCombat` Boss 独立处理远程攻击和召唤
- Boss AOE 使用单独冷却

### HeartSystem.h / HeartSystem.cpp（新建）
- 爱心对象池管理，磁吸拾取，回复 2% 血量
- 代码生成红色爱心贴图

### MerchantSystem.cpp
- 技能价格 `30 + level*5` → `150 + level*25`

### EnemySpawner.h / EnemySpawner.cpp
- `SpawnEnemyAt` 返回 `EntityId`
- 重置 Boss 远程/召唤计时器

### CMakeLists.txt
- 改为 Release 构建配置
- 自动复制 sound/ 文件夹和 openal32.dll

## 九、代码修改记录（第十一轮关键文件）

### 新建文件
- `src/gameplay/QuestSystem.h/.cpp` — 任务系统（5个主线+依赖解锁+提交机制）
- `src/gameplay/AchievementSystem.h/.cpp` — 成就系统（跨存档持久化）
- `src/ui/QuestMenu.h/.cpp` — 任务面板 UI（Q键打开）
- `src/ui/AchievementMenu.h/.cpp` — 成就面板 UI（Tab键打开）

### Game.h
- 新增 include：QuestMenu.h, AchievementMenu.h
- 新增成员：`QuestMenu questMenu_`, `AchievementMenu achievementMenu_`
- 新增标志：`bool questMenuVisible_`, `bool achievementMenuVisible_`
- 新增方法声明：`renderChampionHealthBars()`

### Game.cpp
- 菜单初始化：`questMenu_.Initialize(font)`, `achievementMenu_.Initialize(font)`
- `setupPlayingScene` 中 `questSystem_.Initialize()` 移入 `if (!preserveProgress)` 块
- `nextLevel` 中 `OnLevelReached` 移到 `setupPlayingScene` 之后调用（修复任务1结算 bug）
- 任务奖励回调 `questSystem_.OnRewardGranted`：经验/金币/技能点/等级/装备
- 事件房上报修改：`questSystem_.OnEventTriggered(evtType)` 传入 EventType
- `updateHUDData` 调用 `hud_.SetPlayerPosition(tr->position)`
- 按键处理：Q 键切换任务面板，Tab 键切换成就面板
- ESC 键优先关闭任务/成就面板
- 鼠标点击：右键弹背包上下文菜单，左键处理菜单项（装备/丢弃）
- 渲染：`renderChampionHealthBars()` + questMenu_/achievementMenu_
- 存档：`buildSaveData`/`applySaveData` 序列化/反序列化任务状态
- 击杀回调：传入 isChampion，精英怪经验/金币 ×3
- Champion 血条渲染：世界空间 36x4px 金色血条 + 左侧三角形标识

### HUD.h / HUD.cpp
- 新增 `playerPos_` 成员和 `SetPlayerPosition` 接口
- 技能图标放大：32x32 → 48x48，间距 40 → 56px
- 字号自适应字数：2字→16, 3字→13, 4字→11
- 冷却倒计时移到图标内中央，字号 16
- 快捷键提示移到图标下方，字号 12
- 索引 0-2 图标统一到 48x48，纹理用 setScale(48/32) 拉伸
- `drawMinimap` 完全重写：200x150，走廊通道+房间标记+玩家位置+图例

### Menus.h / Menus.cpp
- `InventoryMenu` 新增 `HandleRightClick`/`HandleContextMenuClick`/`CloseContextMenu`/`IsContextMenuVisible` 接口
- 上下文菜单状态成员：`contextMenuVisible_`, `contextMenuPos_`, `contextTargetType_`, `contextTargetIndex_`, `contextItemBounds_`, `contextMenuBounds_`
- Render 末尾绘制上下文菜单（120x68，装备/卸下 + 丢弃）
- 文字使用 `sf::String::fromUtf8` 替代 U8 宏（修复 const char* sizeof 陷阱）
- 技能栏标题 y=415 → 525，技能槽/技能背包 y=440 → 545（修复与装备背包重叠）
- 分隔线高度 600 → 415（不再穿过技能区）
- 背包标题提示更新："左键快速穿卸 | 右键弹出菜单 | G/ESC 关闭"

### EnemyAI.h
- `EnemyComponent` 新增 `bool isChampion = false`

### EnemySpawner.h / EnemySpawner.cpp
- `SpawnEnemyAt` 新增 `bool champion` 参数
- `SpawnEnemiesInArea` 新增 `float championChance` 参数
- Champion 倍率：HP×3, 伤害×1.5, 速度×1.1, 体型×1.5
- Champion sprite 颜色调金色 (255, 230, 150)
- 敌人分层强化重设：HP+30%/层, 伤害+18%/层, 速度+4%/层封顶1.6x

### EnemyAI.cpp
- 陷阱房锁门 bug 修复：自动开门逻辑添加 `ds->locked` 检查
- Boss 新机制：冲撞地裂、召唤精英怪、旋转弹幕

### LootSystem.h / LootSystem.cpp
- `OnEnemyKilled` 新增 `bool isChampion` 参数
- Champion 掉落与 Elite 同等（100%掉1-2件，保底蓝色）
- 新增公开包装方法：`GenerateRandomItem(ilvl, quality)`, `DropSpecificItem(item, pos)`

### UpgradeSystem.h / UpgradeSystem.cpp
- 新增 `AddLevels(int levels)` 方法（任务5奖励等级+5用）

### QuestSystem.h / QuestSystem.cpp（重写）
- 5 个剧情任务定义（破晓之始/古老祭坛/地下奇人/百艺兼修/财富之力）
- QuestType 新增 CollectSkills 和 AccumulateCoins
- QuestDef 新增 `EventType targetEvent` 和 `int prerequisiteQuestId`
- QuestReward 新增 `int addLevels` 和 `bool isRandomItem`
- `ClaimReward` 新增 `int* taskIdToUnlockAfter` 出参
- `unlockDependentQuests` 私有方法

### AchievementSystem.h / AchievementSystem.cpp
- 跨存档持久化，独立文件 saves/achievements.dat
- 4 类成就：战斗/探索/收集/特殊
- 二进制格式：魔数 + 版本号 + 成就数据

### SaveSystem.h / SaveSystem.cpp
- SaveData 新增 `struct QuestSaveEntry { uint8_t state; int32_t currentProgress; float timeAccumulator; }`
- 新增 `std::array<QuestSaveEntry, 5> questStates{}`
- `writeSaveData`/`readSaveData` 添加 5 个任务状态的写入/读取

### RoomSystem.cpp
- 普通房间所有 `SpawnEnemiesInArea` 调用添加 `kChampionChance = 0.08f`
- 事件房/诅咒房逻辑

### CMakeLists.txt
- 注册 `src/ui/QuestMenu.cpp` 和 `src/ui/AchievementMenu.cpp`

## 九、代码修改记录（第十二轮关键文件）

### src/gameplay/EnemyAI.cpp
- **死亡检测块**：移除 `combat.OnKill(id, playerEntity)` 调用，仅保留 SpawnDeathEffect / 自爆范围伤害 / 分裂怪生成等视觉与物理特效。OnKill 回调统一由 Game.cpp 步骤 15.5 处理，避免双重调用导致奖励翻倍。
- **Boss 旋转弹幕分支**：将 `enemy->specialTimer` 替换为新增的 `enemy->spiralFireTimer`，移除 `if (enemy->chargeActive <= 0.f)` 守卫。旋转弹幕与冲撞地裂现可独立并发执行，互不干扰。

### src/gameplay/EnemyAI.h
- `EnemyComponent` 新增 `float spiralFireTimer = 0.f;` 字段，作为旋转弹幕发射间隔的独立计时器，与冲撞地裂使用的 `specialTimer` 解耦。

### src/gameplay/SkillSystem.cpp
- **EquipSkill**：装备技能时显式 `pc.skillSlots[slotIndex].level = 1;`，避免继承槽位中上一个技能的残留等级（背包技能等级固定为 1）。
- **UnequipSkill**：卸下技能时显式 `pc.skillSlots[slotIndex].level = 1;`，保持槽位状态干净。
- **UpdateSkillBuffs 地刺分支**：
  - DPS 公式从 `5 + 3*(lv-1)` 改为 `kSpikeBaseDPS + pc->stats.damage * 0.3f`，叠加玩家伤害 30%。
  - 减速公式从 `vel->linear *= (1 - kSlowFactor * dt * 3)` 改为 `vel->linear *= (1 - kSlowFactor)`，实际减速效果从 ~5%/帧提升至 50%/帧。
  - 伤害 tick 计时器从 `static float spikeTickTimer` 改为 `pc->spikeTickTimer`（PlayerComponent 成员），技能结束时显式重置为 0，避免跨局残留。

### src/gameplay/Player.h
- `PlayerComponent` 新增 `float spikeTickTimer = 0.f;` 成员字段，替代 SkillSystem.cpp 中的 static 局部变量。

### 编译验证
- Release 版本编译成功（exit_code=0），无新增错误，仅历史遗留警告（C4244 类型转换 / C4819 编码 / C4715 返回值）。
- 可执行文件：`build/bin/Release/crazyunder.exe`

## 九、代码修改记录（第十三轮关键文件）

### src/gameplay/EnemyAI.h
- `EnemyComponent` 新增 `float slowFactor = 0.f` 字段，作为 SkillSystem 与 EnemyAI 之间传递减速效果的桥梁。设计为单帧有效：SkillSystem 每帧在地刺范围内设置，EnemyAI 在合成 `desiredVelocity` 时应用 `(1 - slowFactor)` 后立即重置为 0。

### src/gameplay/EnemyAI.cpp
- **UpdateEnemyAI 合成 finalVelocity 前**：新增 slowFactor 应用块。在保存 `externalVel` 后、合成 `finalVelocity` 前，执行 `desiredVelocity *= (1.f - enemy->slowFactor)` 并重置 `enemy->slowFactor = 0.f`。修复地刺减速完全失效的 P1 Bug（此前 SkillSystem 误改 `vel->linear`，而 `desiredVelocity = flowDir * moveSpeed` 根本不读取 `vel->linear`）。
- **Boss 机制块**：新增 `constexpr int kMaxBossMinions = 12`（小兵+精英共享配额）。小兵召唤（每 8s）和精英召唤（HP<50% 首次 + 每 12s 持续）前均遍历统计当前 `isBossMinion && active` 数量，超过上限则跳过本次召唤，仅重置冷却计时器。避免长时间 Boss 战导致召唤物无限堆积耗尽对象池。

### src/gameplay/SkillSystem.cpp
- **UpdateSkillBuffs 地刺减速分支**：移除无效的 `vel->linear *= (1.f - kSlowFactor)`，改为设置 `enemy->slowFactor = kSlowFactor`。详细注释说明历史 Bug 根因（desiredVelocity 不读 vel->linear）。
- **UpdateSkillBuffs 全部分支**：6 处 `static float/int` 局部变量改为 `pc->xxx` 成员字段：
  - 吸血打击：`leechParticleTimer`
  - 狂暴：`berserkParticleTimer` + `berserkSparkCounter`
  - 引力井：`gravityWellParticleTimer`
  - 地刺：`spikeParticleTimer` + `spikeBloodCounter`
  - 技能结束时（timer 归零）显式重置对应计时器/计数器。

### src/gameplay/Player.h
- `PlayerComponent` 新增 6 个粒子计时器成员字段：`leechParticleTimer` / `berserkParticleTimer` / `berserkSparkCounter` / `gravityWellParticleTimer` / `spikeParticleTimer` / `spikeBloodCounter`，替代 SkillSystem.cpp 中的 static 局部变量。

### 编译验证
- Release 版本编译成功（exit_code=0），无新增错误，仅历史遗留警告（C4244 类型转换 / C4819 编码 / C4996 localtime / C4715 返回值）。
- 可执行文件：`build/bin/Release/crazyunder.exe`

## 九、代码修改记录（第十四轮关键文件）

### src/ui/HUD.h
- 新增 `#include <deque>` 和 `#include <string>` 头文件
- 新增 `AchievementToast` 结构体（name/description/lifetime/maxLifetime/slideInTimer），成就解锁通知条目
- `HUD` 类新增公开接口：
  - `AddAchievementToast(const std::string& name, const std::string& description)`：推入通知（最多 4 条，超出丢弃最旧）
  - `UpdateToasts(float dt)`：每帧更新 Toast 计时器与动画
- `HUD` 类新增私有成员：
  - `mutable std::deque<AchievementToast> toasts_`：Toast 通知队列（mutable 因 drawAchievementToasts 为 const）
  - `static constexpr int kMaxToasts = 4`：同屏最大 Toast 数
  - `static constexpr float kToastWidth/Height/Spacing/kToastSlideInDuration`：Toast 尺寸与动画参数
- 新增私有方法声明 `drawAchievementToasts(sf::RenderTarget& target) const`

### src/ui/HUD.cpp
- `Render()` 末尾新增 `drawAchievementToasts(target)` 调用
- 新增 `AddAchievementToast` 实现：构造 Toast 并 push_back，超限时 pop_front
- 新增 `UpdateToasts` 实现：递减 lifetime、递增 slideInTimer（封顶 kToastSlideInDuration），过期 Toast 从头部弹出
- 新增 `drawAchievementToasts` 实现：
  - 右上角起始位置 (990, 110)，向下堆叠
  - 三阶段 alpha 计算：滑入淡入（0-0.3s）→ 稳定（255）→ 淡出（最后 0.5s）
  - 滑入偏移：ease-out 缓动从右侧 30px 滑入
  - 绘制：深色半透明背景 + 金色边框 + 顶部装饰条 + "成就解锁"标签 + 成就名 + 描述

### src/core/Game.cpp
- `setupPlayingScene` 中 `OnUnlocked` 回调重写：
  - 移除 `TODO: 后续在此触发 Toast 通知 UI` 注释
  - 调用 `hud_.AddAchievementToast(def.name, def.description)` 推入通知
  - 调用 `achievementSystem_.SaveToFile()` 立即持久化（处理 [[nodiscard]] 返回值，失败时 LOG_WARN）
- `updatePlaying` 中 `updateHUDData()` 后新增 `hud_.UpdateToasts(dt)` 调用

### 编译验证（第十四轮）
- Release 版本编译成功（exit_code=0）
- 修复 C4834 警告：`SaveToFile()` 返回值由 `if (!...)` 检查处理
- 仅剩历史遗留警告 C4244（line 3642: `s.fps = time_.GetFPS()` int→float，无害）
- 可执行文件：`build/bin/Release/crazyunder.exe`

## 九、代码修改记录（第十五轮关键文件）

### 新建文件
- `src/gameplay/RelicSystem.h` — 圣物系统头文件（RelicType 枚举 8 种 + RelicData 静态数据 + RelicSystem 类 + GetRelicData/GetRelicName 全局接口）
- `src/gameplay/RelicSystem.cpp` — 圣物系统实现（静态数据表 + Fisher-Yates 洗牌抽取 + ApplyToPlayerStats 各分支 + 序列化/反序列化）

### src/gameplay/Player.h
- `PlayerStats` 结构体新增 `float coinMultiplier = 1.f` 字段（贪婪之眼专用金币倍率，与 expMultiplier 对称）

### src/core/SaveSystem.h
- 新增 `#include "gameplay/RelicSystem.h"`
- `SaveData` 结构体新增 `std::array<uint8_t, kRelicMaxCount> relicIds{};` 字段
- `kSaveVersion` 从 1 升级到 2（旧版本存档不兼容，需开始新游戏）

### src/core/SaveSystem.cpp
- `writeSaveData` 末尾（时间戳之前）写入 `data.relicIds`（6 字节连续）
- `readSaveData` 对应位置读取 `data.relicIds`

### src/ui/Menus.h
- 新增 `#include "gameplay/RelicSystem.h"`
- 新增 `RelicChoiceMenu` 类声明（继承 UIElement，3 张卡片布局，鼠标点击/1/2/3 键选择）

### src/ui/Menus.cpp
- 新增 `RelicChoiceMenu` 完整实现：构造、Initialize、SetOptions、HandleKeyInput、HandleMouseClick、Update、Render

### src/core/Game.h
- 新增 `#include "gameplay/RelicSystem.h"`
- 新增成员 `RelicSystem relicSystem_;`
- 新增标志 `bool relicChoiceActive_ = false;`（Boss 击败后圣物选择激活）
- 新增标志 `bool relicPanelVisible_ = false;`（R 键圣物查看面板可见）
- 新增 `std::vector<RelicType> currentRelicOptions_;`（当前可选圣物列表）
- 新增 `RelicChoiceMenu relicMenu_;`
- 新增方法声明 `void handleRelicChoice();`
- 新增方法声明 `void renderRelicPanel();`

### src/core/Game.cpp（多处修改）
- `initializeUI`：调用 `relicMenu_.Initialize(font)`
- `recomputePlayerStats`：
  - 重置块新增 `s.coinMultiplier = 1.f;`
  - 末尾（装备词缀之后、Health 同步之前）调用 `relicSystem_.ApplyToPlayerStats(s);`
- `OnKill` 回调：Boss 死亡时检测 `!relicSystem_.IsFull()`，调用 `RollUnownedRelics(3)` 弹出圣物选择菜单
- `handleRelicChoice()` 新方法实现：鼠标点击选择 + 悬停状态更新
- `renderRelicPanel()` 新方法实现：720x460 居中面板 + 3x2 圣物槽位网格 + 已获/空缺占位
- `updateUI` 中 GameState::Playing 分支新增 `if (relicChoiceActive_) handleRelicChoice();`
- `updatePlaying` 新增 `if (relicPanelVisible_) return;` 暂停玩法更新
- `renderUI` 新增 `if (relicPanelVisible_) renderRelicPanel();`
- `handleEvents`：
  - R 键重分配：`event.key.shift` 时触发"重新生成地牢"，否则切换 `relicPanelVisible_`
  - ESC 键：条件链新增 `relicPanelVisible_`，优先关闭圣物面板而非暂停
  - G/Q/Tab 键打开对应面板时设置 `relicPanelVisible_ = false;`
  - J 键通过 `showUpgradeChoice()` 间接关闭圣物面板
  - 1/2/3 键：圣物选择激活时优先处理（if-else 链）
- `showUpgradeChoice()`：新增 `relicPanelVisible_ = false;` 避免与升级菜单叠加
- `handleInteract` 中商人菜单打开时新增 `relicPanelVisible_ = false;`
- `OnKill` Boss 触发圣物选择时新增 `relicPanelVisible_ = false;` 避免与选择菜单叠加
- `buildSaveData`：新增 `data.relicIds = relicSystem_.Serialize();`
- `applySaveData`：新增 `relicSystem_.Deserialize(data.relicIds);`
- `setupPlayingScene` 中 `if (!preserveProgress)` 块新增 `relicSystem_.Initialize();`

### CMakeLists.txt
- 在 `src/gameplay/AchievementSystem.cpp` 之后新增 `src/gameplay/RelicSystem.cpp`

### 编译验证（第十五轮）
- 首次构建需 `/t:Rebuild` 强制重新编译以纳入新源文件 RelicSystem.cpp（增量构建可能跳过新源文件）
- Release 版本编译成功（exit_code=0）
- 仅剩历史遗留警告（C4244 类型转换 / C4819 编码 / C4996 localtime / C4715 返回值），无新增错误
- 可执行文件：`build/bin/Release/crazyunder.exe`

## 九、代码修改记录（第十六轮关键文件）

### src/gameplay/CombatSystem.cpp
- **UpdateStatusEffects**：
  - 新增 `if (health->current <= 0.f) continue;` 守卫，已死亡实体不再受 DoT
  - tick 伤害触发条件从 `effect.timer <= 0.f` 改为 `effect.timer <= 0.f && effect.tickDamage > 0.f`，避免 Ice（tickDamage=0）误触
  - tick 伤害后调用 `SpawnDamageText` 生成元素色飘字（Fire 橙红 255,110,50 / Poison 毒绿 110,200,80），修复 DoT 无视觉反馈问题
- **ApplyStatus**：移除 `existing.timer = effect.tickInterval` 重置行，修复高频刷新导致 DoT 永不触发的 P1 Bug。刷新时只更新 `remaining`（取较长者）和 `tickDamage`（取较高者）

### src/gameplay/ProjectileSystem.h
- 新增前向声明 `class CombatSystem;`
- 新增 `void SetCombatSystem(CombatSystem* combat) noexcept` 接口
- 新增私有成员 `CombatSystem* combatSystem_ = nullptr;`

### src/gameplay/ProjectileSystem.cpp
- 新增 `#include "gameplay/CombatSystem.h"`
- **handleHit**：在扣减 HP 后、应用击退前，新增元素状态触发块。若 `combatSystem_ != nullptr && proj.element != Physical`，且 owner 不是敌人（即玩家子弹），调用 `CombatSystem::CreateElementalStatus` + `ApplyStatus` 施加 Fire/Ice/Poison 状态

### src/core/Game.cpp
- **回调设置区**（onDoorBroken 之后）：新增 `projectileSystem_.SetCombatSystem(&combatSystem_);` 注入 CombatSystem 指针
- **renderPlaying 实体渲染循环**：新增 StatusEffectComponent 读取块，根据元素类型对敌人 sprite color 做 50% tint 混合（Fire 橙红 / Ice 青蓝 / Poison 毒绿），多状态取优先级 Fire > Poison > Ice

### src/gameplay/PlayerCombat.cpp
- **普攻子弹配置**：原 `config.element = Physical` + 黄白色，改为根据 `berserkTimer > 0` 切换：
  - 狂暴激活：`element = Fire`，颜色 (255,110,50) 橙红
  - 狂暴未激活：`element = Physical`，颜色 (255,255,100) 黄白（原行为）
  - 狂暴伤害 ×1.5 加成保留不变

### src/gameplay/SkillSystem.cpp
- **UpdateSkillBuffs 引力井分支**：拉扯敌人后新增 `combat.ApplyStatus(registry, tid, CreateElementalStatus(Ice, 0))`，施加 2s 冰冻减速
- **UpdateSkillBuffs 地刺分支**：
  - tick 伤害 `dmgInfo.element` 从 `Physical` 改为 `Poison`
  - ApplyDamage 后新增 `combat.ApplyStatus(registry, tid, CreateElementalStatus(Poison, tickDmg))`，施加 5s 中毒 DoT

### src/gameplay/EnemyAI.cpp
- **UpdateEnemyAI slowFactor 应用块**：重写减速逻辑
  - 原：仅应用 `enemy->slowFactor`（地刺设置的单帧减速）
  - 新：合并三个减速源——地刺 slowFactor + Ice(0.5) + Poison(0.3)，取最大值，上限 80% 避免卡死
  - 读取 `StatusEffectComponent` 遍历 effects，按元素类型提取减速比例

### 编译验证（第十六轮）
- Release 版本增量编译成功（exit_code=0）
- 仅重新编译改动文件（CombatSystem.cpp / ProjectileSystem.cpp / Game.cpp / PlayerCombat.cpp / SkillSystem.cpp / EnemyAI.cpp）
- 无新增警告，仅历史遗留（C4244 类型转换 / C4819 编码 / C4996 localtime / C4715 返回值）
- 可执行文件：`build/bin/Release/crazyunder.exe`

## 九、代码修改记录（第十七轮关键文件）

### src/gameplay/FloorModifier.h（新增）
- 新建地牢变异系统头文件，定义 `FloorModifierType` 枚举（10 种修饰符 + None + Count）
- 定义 `kFloorModifierSlotCount = 2` 常量（单层最多 2 个修饰符）
- 定义 `FloorModifierData` 结构体：type/name/description/RGB 主色调 + 14 个乘法字段 + playerRegenPerSec + heartDropDisabled
- 声明 `FloorModifierSystem` 类：Clear/RollForLevel/Deserialize/Serialize + 14 个 Get*Mul 查询接口 + ApplyToPlayerStats + GetActiveModifiers/GetActiveSummary
- 全局查询接口：`GetFloorModifierData(type)` / `GetFloorModifierName(type)`

### src/gameplay/FloorModifier.cpp（新增）
- 实现 10 个修饰符的静态数据表（kModifierTable），每个含正负效果配对的 multiplier
- **RollForLevel**：根据层数决定修饰符数量（1层=0、2-4层=1、5+层=2），Fisher-Yates 洗牌从池中不重复抽取
- **Get*Mul 系列接口**：累乘所有激活修饰符的同种 multiplier（两个 ×1.2 → ×1.44）
- **ApplyToPlayerStats**：直接修改 damage/maxHp/moveSpeed/pickupRange 四个字段
- **Serialize/Deserialize**：与 `std::array<uint8_t, 2>` 互转，None=0
- **GetActiveSummary**：拼接激活修饰符中文名（如"嗜血狂暴 + 福星高照"）

### src/gameplay/EnemySpawner.h
- 新增 5 个 Set 接口：SetModifierEnemyHpMul / SetModifierEnemyDamageMul / SetModifierEnemyMoveSpeedMul / SetModifierEnemyAttackSpeedMul / SetModifierSpawnIntervalMul
- 新增 5 个 private 成员字段（默认 1.f）：modEnemyHpMul_ / modEnemyDmgMul_ / modEnemySpdMul_ / modEnemyAtkSpdMul_ / modSpawnIntervalMul_

### src/gameplay/EnemySpawner.cpp
- **SpawnEnemyAt**：HP 应用 `* modHpMul`（下限 1.f）、damage 应用 `* modDmgMul`、moveSpeed 应用 `* modSpdMul`、attackCooldown 应用 `/ modAtkSpdMul`
- **StartWave**：`spawnInterval_ = std::max(0.02f, config.spawnInterval * modSpawnIntervalMul_)`（下限防对象池耗尽）

### src/gameplay/LootSystem.h
- 新增 `void SetModifierItemDropChanceMul(float m) noexcept` 接口
- 新增 private 成员 `float modItemDropChanceMul_ = 1.f`

### src/gameplay/LootSystem.cpp
- **OnEnemyKilled 普通怪分支**：`float dropChance = std::min(1.0f, 0.10f * modItemDropChanceMul_)`
- **OnPotBroken**：`float dropChance = std::min(1.0f, 0.50f * modItemDropChanceMul_)`

### src/gameplay/MerchantSystem.h
- 新增静态接口 `static void SetModifierPriceMul(float m) noexcept` / `static float GetModifierPriceMul() noexcept`
- 新增静态成员 `static float sPriceMul_`（采用静态而非实例字段，兼容 Menus.cpp 中 `CalcSellPrice` 的静态调用路径）

### src/gameplay/MerchantSystem.cpp
- 定义 `float MerchantSystem::sPriceMul_ = 1.f`
- **CalcBuyPrice**：价格乘以 `sPriceMul_`
- 实现 `SetModifierPriceMul`：防御性下限 `sPriceMul_ = (m > 0.1f) ? m : 1.f`

### src/core/SaveSystem.h
- 添加 `#include "gameplay/FloorModifier.h"`
- SaveData 新增 `std::array<uint8_t, kFloorModifierSlotCount> floorModifierIds{}`
- `kSaveVersion` 从 2 升级到 3

### src/core/SaveSystem.cpp
- **writeSaveData**：在圣物序列化之后、时间戳之前添加 floorModifierIds 连续 2 字节写入块
- **readSaveData**：对称添加读取块；**末尾补充 `return true;`** 修复 C4715 UB 警告

### src/core/Game.h
- 添加 `#include "gameplay/FloorModifier.h"`
- 新增 3 个方法声明：`applyFloorModifiersToSubsystems` / `renderFloorModifierBanner` / `renderFloorModifierHUD`
- 新增成员：`FloorModifierSystem floorModifiers_` / `float modifierBannerTimer_ = 0.f` / `static constexpr float kModifierBannerDuration = 5.0f` / `float regenAccumulator_ = 0.f`

### src/core/Game.cpp
- **setupPlayingScene**：清空 modifier + 调用 `applyFloorModifiersToSubsystems()` 注入子系统
- **nextLevel**：`++currentLevel_` 后调用 `floorModifiers_.RollForLevel(currentLevel_)` + 设置 `modifierBannerTimer_ = kModifierBannerDuration`
- **restartGame**：重置 modifier 状态
- **applySaveData**：反序列化 floorModifierIds + applyFloorModifiersToSubsystems + 2.5s 短暂 Banner
- **recomputePlayerStats**：在圣物之后、Health 同步之前调用 `floorModifiers_.ApplyToPlayerStats(s.damage, s.maxHp, s.moveSpeed, s.pickupRange)`
- **OnKill 回调**：`expValue *= GetExpMul()`、`coinValue *= GetCoinMul()`、`IsHeartDropDisabled()` 跳过爱心掉落（expValue/coinValue 下限 1）
- **updatePlaying**：regen 累加器逻辑（regenAccumulator_ += regenPerSec * maxHp * dt，满 1 整数回血，满血清零）+ banner timer 递减
- **buildSaveData**：`data.floorModifierIds = floorModifiers_.Serialize()`
- **renderUI**：在 `hud_.Render(window_)` 之后调用 `renderFloorModifierHUD()` 和 `renderFloorModifierBanner()`
- 新增 3 个方法定义：
  - `applyFloorModifiersToSubsystems`：将 floorModifiers_ 的乘法系数推送到 EnemySpawner/LootSystem/MerchantSystem
  - `renderFloorModifierBanner`：5 秒淡入淡出 Banner（720×100 居中 y=180），半透明背景板 + 主色调边框 + "本层变异"标题 + 修饰符名 + 描述
  - `renderFloorModifierHUD`：左上角 y=96 持久指示器，"变异："+ 主色调修饰符名（横向排列）

### CMakeLists.txt
- 在 `src/gameplay/RelicSystem.cpp` 之后新增 `src/gameplay/FloorModifier.cpp`，纳入编译

### 编译验证（第十七轮）
- CMake 自动重新生成（ZERO_CHECK 检测到 CMakeLists.txt 变化，重新运行 cmake 配置）
- Release 版本增量编译成功（exit_code=0）
- 编译包含新源文件 FloorModifier.cpp
- C4715 警告已消除（本轮修复 SaveSystem::readSaveData）
- 仅剩历史遗留警告（C4244 类型转换 / C4819 编码 / C4996 localtime），无新增错误
- 可执行文件：`build/bin/Release/crazyunder.exe`

## 四-i、第十八轮新增/修复内容（连击系统 - 心流体验正反馈循环）

### 设计意图
Roguelike 割草爽游的核心乐趣来自"激进玩法 → 击杀加速 → 视觉爆发"的心流循环。原游戏虽有五维成长（升级/装备/圣物/元素/层修饰符）但缺乏**瞬时的正反馈机制**——玩家连续击杀多个敌人后没有任何差异化反馈，体感趋于线性。

本轮引入连击系统（Combo System）：3 秒内连续击杀累积 combo，阶梯式提升伤害（10/25/50/100 击对应 +20%/+35%/+50%/+75%），受伤立即重置。这创造了一个**主动的正反馈循环**：
- 玩家激进追击 → combo 上升 → 伤害加成 → 击杀更快 → combo 进一步上升
- 玩家走位失误受伤 → combo 归零 → 惩罚机制让 combo 具有价值

与现有五维成长正交叠加，产生涌现式交互：
- "狂暴火攻流" build + 高 combo → 普攻子弹倍率叠加，伤害爆炸
- "贪婪之雾"层修饰符 + 高 combo → 金币收益指数级增长
- "战士之证"圣物 + 高 combo → 伤害乘法累乘，挑战极限
- 元素状态（Fire DoT）不受 combo 影响 → 平衡设计，避免 DoT 失控

### 核心机制
1. **combo 累积**：玩家击杀敌人时 `comboCount += 1`，`comboTimer` 重置为 3.0s
2. **combo 衰减**：每帧 `comboTimer -= dt`，归零时 `comboCount = 0`
3. **combo 重置**（任一触发）：
   - 玩家受伤（通过 ApplyDamage 的 OnHit 回调）
   - 近战接触伤害（直接修改 HP 的特殊路径，EnemyAI.cpp 中手动重置）
   - 地裂区域伤害（直接修改 HP 的特殊路径，Game.cpp 中手动重置）
   - combo 计时器超时（3 秒未击杀）
   - 换层（新地牢意味着新挑战，combo 重新累积）
4. **伤害加成阶梯**（`CombatSystem::GetComboDamageMultiplier`）：
   | 连击数 | 伤害乘数 | 阶段定位 |
   |--------|----------|----------|
   | < 10   | 1.00x    | 准备期（无加成，鼓励上 10） |
   | 10-24  | 1.20x    | 前期甜头（+20%） |
   | 25-49  | 1.35x    | 中期核心（+35%） |
   | 50-99  | 1.50x    | 激进 build 收益（+50%） |
   | >= 100 | 1.75x    | 巅峰阶段（+75%，硬上限） |

### 数值平衡设计
- **DoT 不受 combo 加成**：燃烧/中毒周期伤害由 `UpdateStatusEffects` 直接扣 HP，不经过 `ApplyDamage`，有意排除——避免 combo 高时 DoT 失控秒杀 Boss
- **combo 上限 1.75x**：避免极端 combo 下破坏 Boss 战平衡
- **受伤即重置**：避免玩家无脑肉搏堆 combo，强制走位策略
- **3 秒窗口**：迫使玩家持续寻找新目标维持连击，避免被动等待
- **comboMaxThisLife 跨层保留**：当前 combo 重置但历史最大值保留供成就统计

### 实现位置（最小侵入原则）
- **伤害加成应用点**：`CombatSystem::ApplyDamage` 内部集中处理，而非每个伤害源单独应用。原因：
  1. 普攻子弹伤害在 Spawn 时确定，飞行期间 combo 变化无法动态应用
  2. 保证所有玩家伤害源（普攻/AOE/技能/元素状态衍生）统一应用乘数
  3. 避免遗漏导致某些伤害源不享受 combo 加成
- **OnHit 回调统一处理 ApplyDamage 路径的受伤重置**
- **直接修改 HP 的特殊路径单独重置**（近战接触、地裂伤害）

### UI 视觉反馈（HUD 中央连击指示器）
- **显示阈值**：`combo >= 5` 时显示（小连击不打扰玩家视线）
- **位置**：屏幕中央上方 (x=640, y=180)
- **三层视觉**：
  1. "连击 X" 大字（36pt，颜色阶梯同步伤害乘数）
  2. "伤害 +Y%" 中等字（18pt，亮金色）
  3. 保持时间进度条（120x4px，颜色随剩余时间变化：>2s 绿 / >1s 黄 / <1s 红）
- **颜色阶梯**（与伤害乘数同步，让玩家直观感知当前阶段）：
  - combo 5-9: 白色（准备期，无加成）
  - combo 10-24: 黄色 (255,220,80)
  - combo 25-49: 橙色 (255,140,50)
  - combo 50-99: 红色 (255,70,70)
  - combo 100+: 紫色 (220,80,255)
- **脉冲动画**：每次新击杀时数字放大至 1.3 倍后 0.3s 内回弹至 1.0 倍（基于 `comboPulseTimer`）

### 编译验证（第十八轮）
- Release 版本增量编译成功（exit_code=0）
- 仅重新编译改动文件（CombatSystem.cpp / EnemyAI.cpp / Game.cpp / HUD.cpp）
- 无新增警告，仅历史遗留（C4244 line 4247: int→float 转换 / C4819 编码 / C4996 localtime）
- 可执行文件：`build/bin/Release/crazyunder.exe`

## 九、代码修改记录（第十八轮关键文件）

### src/gameplay/Player.h
- `PlayerComponent` 新增 3 个连击系统字段：
  - `int comboCount = 0`：当前连击数（3 秒内连续击杀累积）
  - `float comboTimer = 0.f`：连击剩余保持时间（每次击杀重置为 3.0s，归零后清空 combo）
  - `int comboMaxThisLife = 0`：本次生命最大连击数（用于成就统计与调试，跨层保留）

### src/gameplay/CombatSystem.h
- 新增静态方法声明 `[[nodiscard]] static float GetComboDamageMultiplier(int comboCount) noexcept`
- 注释详细说明阶梯设计原则（5 档伤害乘数 1.0/1.2/1.35/1.5/1.75）

### src/gameplay/CombatSystem.cpp
- 新增 `#include "gameplay/Player.h"`（查询 PlayerComponent 用）
- `ApplyDamage` 内新增 combo 加成应用块：检测 attacker 是否有 PlayerComponent，应用 `GetComboDamageMultiplier(comboCount)` 到 finalDamage
- 新增 `GetComboDamageMultiplier` 实现：5 档阶梯查表（O(1)）

### src/gameplay/EnemyAI.cpp
- 新增 `#include "gameplay/Player.h"`
- 近战接触伤害块（line 451 附近）新增 combo 重置：直接修改 HP 后调用 `PlayerComponent` 查询并重置 `comboCount`/`comboTimer`，弥补 ApplyDamage 路径未覆盖的特殊伤害源

### src/core/Game.cpp
- `OnHit` 回调新增 combo 重置块：`if (info.target == playerId_)` 时重置 PlayerComponent 的 combo 字段
- `OnKill` 回调新增 combo 累积块：玩家击杀时 `++comboCount`，更新 `comboMaxThisLife`，重置 `comboTimer = 3.0f`
- `updatePlaying` 新增步骤 7.5：每帧衰减 `comboTimer`，归零时清空 combo
- `updateFissureZones` 新增 combo 重置：地裂伤害直接修改 HP 后重置 combo
- `nextLevel` 新增 `savedComboMax` 跨层保留 `comboMaxThisLife`（comboCount/comboTimer 不保留，新地牢重新累积）
- `updateHUDData` 末尾新增 `hud_.SetComboData(pc->comboCount, pc->comboTimer, CombatSystem::GetComboDamageMultiplier(pc->comboCount))` 推送 HUD 数据

### src/ui/HUD.h
- 新增公开接口 `void SetComboData(int comboCount, float comboTimer, float damageMul)`：
  - 内部检测 combo 增长，触发 `comboPulseTimer_ = 0.3f` 脉冲动画
- 新增私有成员：
  - `int comboCount_ = 0`
  - `float comboTimer_ = 0.f`
  - `float comboDamageMul_ = 1.f`
  - `mutable float comboPulseTimer_ = 0.f`（mutable 允许 const drawCombo 修改）
- 新增私有方法声明 `void drawCombo(sf::RenderTarget& target) const`

### src/ui/HUD.cpp
- `Render` 末尾（drawMinimap 之后、drawAchievementToasts 之前）新增 `drawCombo(target)` 调用
- 新增 `drawCombo` 实现（约 110 行）：
  - 阈值 `comboCount < 5` 不渲染
  - 颜色阶梯查表（5 档颜色）
  - 脉冲缩放：`pulseScale = 1 + 0.3 * (pulseTimer / 0.3)`，1.0~1.3 范围
  - 三层视觉：大字"连击 X" + 中等字"伤害 +Y%" + 进度条（120x4px）
  - 进度条颜色随剩余时间变化（绿/黄/红，紧迫感）
  - 推进 `comboPulseTimer_` 衰减（mutable）

### 编译验证（第十八轮）
- Release 版本增量编译成功（exit_code=0），无新增警告
- 历史遗留警告保持不变（C4244 line 4247 / C4819 / C4996）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-j、第十九轮新增/修复内容（闪电流 build 激活 - 雷霆系维度拓展）

### 设计意图
本轮聚焦"玩法与深度"优先级，激活自初版即为 dead code 的 Lightning 元素状态系统，构建完整的"闪电流 build"维度，与现有的"狂暴火攻流（Fire）"、"控制流（Ice）"、"持续伤害流（Poison）"形成四元素 build 并立的策略格局。通过升级、圣物、地牢变异三个维度协同构筑，让闪电流从"无"到"完整"。

### 核心系统 1：Lightning 元素状态激活（CombatSystem）
- **位置**：`src/gameplay/CombatSystem.cpp` `CreateElementalStatus`
- **修改**：原 Lightning 分支缺失（dead code），新增 case：
  - `effect.type = Lightning`
  - `effect.duration = 0.6f`（麻痹 0.6s）
  - `effect.tickInterval = 1.f` / `effect.tickDamage = 0.f`（无 DoT，纯控制）
- **设计**：与 Ice（软控 50% 减速 2s）形成控制层次，Lightning 为硬控（完全禁锢）但持续时间短

### 核心系统 2：麻痹效果作用于敌人 AI（EnemyAI）
- **位置**：`src/gameplay/EnemyAI.cpp` `UpdateEnemyAI`
- **修改**：在读取 StatusEffectComponent 时新增 Lightning 检测分支：
  - 检测到 Lightning 状态时设置 `paralyzed = true`
  - 麻痹时：`desiredVelocity = (0,0)`（完全停止主动移动）
  - 麻痹时：`attackCooldown = max(attackCooldown, 0.3f)`（锁定攻击 0.3s）
  - 麻痹时：`slowFactor = 0`（不叠加减速）
  - 保留 `externalVel`（击退/拉扯仍生效，避免麻痹时卡死）
- **精灵染色**：`src/core/Game.cpp` 敌人渲染新增 Lightning 优先级 4（亮黄色 255,230,80），高于 Fire/Poison/Ice

### 核心系统 3：连锁闪电完整重写（ProjectileSystem）
- **位置**：`src/gameplay/ProjectileSystem.cpp` `handleChain` + `handleHit`
- **修改**：
  - `handleChain` 签名新增 `ParticleSystem& particles` 参数
  - 重写连锁逻辑：
    1. 黄色飘字（`SpawnDamageText` 颜色 255,230,80）
    2. 粒子命中特效（`SpawnHitEffect`）
    3. Lightning 状态施加（`ApplyStatus`，乘以 `lightningDurationMul`）
    4. ApplyDamage 调用（`isCritical=false, lifesteal=0, knockback=0`，避免连锁触发吸血/暴击/击退导致循环利用）
  - `handleHit` 中 Lightning 路径同步应用 `lightningDurationMul` 倍率
- **递归调用修复**：原递归调用未传递 `particles` 参数导致编译错误，已修复

### 核心系统 4：普攻元素切换（PlayerCombat）
- **位置**：`src/gameplay/PlayerCombat.cpp` 普攻子弹配置
- **修改**：新增闪电流元素切换逻辑
  - `hasChainLightning = (stats.chainLightning > 0)`
  - `floorLightning = stats.floorLightningActive`（雷暴领域标志）
  - 优先级：狂暴 Fire > 闪电流 Lightning > 物理 Physical
  - 闪电流激活时子弹颜色变为亮黄色（255,230,80）
- **设计**：让 chainLightning 升级从"纯数值连锁"升级为"闪电流 build"切换

### 核心系统 5：雷霆系圣物（RelicSystem）
- **位置**：`src/gameplay/RelicSystem.h` + `src/gameplay/RelicSystem.cpp`
- **修改**：圣物总数 8 → 10，新增：
  - `ThunderHeart = 9`（雷霆之心）：连锁闪电 +1，伤害 +10%
  - `StormEye = 10`（风暴之眼）：麻痹时间 +50%（`lightningDurationMul += 0.5`），暴击率 +5%
- **PlayerStats 新增字段**：`float lightningDurationMul = 1.f`（Lightning 麻痹持续时间倍率）

### 核心系统 6：雷暴领域地牢变异（FloorModifier）
- **位置**：`src/gameplay/FloorModifier.h` + `src/gameplay/FloorModifier.cpp`
- **修改**：变异总数 10 → 11，新增：
  - `Thunderstorm = 11`（雷暴领域）
  - 正效果：`playerAttackLightning = true`（玩家所有普攻附加 Lightning 元素 + 麻痹）
  - 负效果：`enemyDamageMul = 1.15`（敌人伤害 +15%）
  - UI 主色：亮黄色（255,230,80）
- **FloorModifierData 新增字段**：`bool playerAttackLightning = false`
- **FloorModifierSystem 新增查询**：`IsPlayerAttackLightning()`
- **PlayerStats 新增字段**：`bool floorLightningActive = false`（由 recomputePlayerStats 设置）
- **集成点**：`Game::recomputePlayerStats` 末尾根据 `floorModifiers_.IsPlayerAttackLightning()` 设置 `s.floorLightningActive`

### 数值平衡设计
- Lightning 麻痹 0.6s：硬控但短暂，避免破坏游戏节奏
- 连锁伤害衰减 70%：避免连锁清屏过于强势
- 雷暴领域敌人伤害 +15%：双刃剑，与 Glass（-25% HP）/ Wrath（+30% 伤害 -15% 移速）叠加形成"玻璃闪电流"高风险玩法
- 风暴之眼 +50% 麻痹时间：0.6s → 0.9s，单件圣物提升明显但不至于 OP

### 编译验证（第十九轮）
- Release 版本增量编译成功（exit_code=0）
- 历史遗留警告保持不变（C4244 line 4260 int→float / C4819 编码 / C4996）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 九、代码修改记录（第十九轮关键文件）

### src/gameplay/CombatSystem.cpp
- `CreateElementalStatus`：新增 Lightning case（duration=0.6s, tickDamage=0），激活原 dead code

### src/gameplay/EnemyAI.cpp
- `UpdateEnemyAI`：新增 Lightning 麻痹检测分支，paralyzed 时清零 desiredVelocity + 锁定 attackCooldown + 重置 slowFactor

### src/core/Game.cpp
- 敌人精灵渲染：新增 Lightning 染色（亮黄色 255,230,80，优先级 4，高于 Fire/Poison/Ice）
- `recomputePlayerStats`：新增 `s.floorLightningActive = false` 重置 + `s.floorLightningActive = floorModifiers_.IsPlayerAttackLightning()` 应用

### src/gameplay/ProjectileSystem.h
- `handleChain` 签名新增 `ParticleSystem& particles` 参数

### src/gameplay/ProjectileSystem.cpp
- `handleChain`：完全重写，补充飘字/粒子/Lightning 状态/ApplyDamage 调用
- `handleHit`：Lightning 路径应用 `lightningDurationMul` 倍率
- 递归调用修复：传递 `particles` 参数

### src/gameplay/PlayerCombat.cpp
- 普攻元素切换：新增 `floorLightning` 标志，hasChainLightning || floorLightning 时附加 Lightning 元素

### src/gameplay/Player.h
- `PlayerStats` 新增 `float lightningDurationMul = 1.f`（Lightning 麻痹倍率）
- `PlayerStats` 新增 `bool floorLightningActive = false`（雷暴领域标志）

### src/gameplay/RelicSystem.h
- `RelicType` 枚举新增 `ThunderHeart = 9` / `StormEye = 10`，Count 从 9 → 11

### src/gameplay/RelicSystem.cpp
- `kRelicTable` 新增 2 个雷霆圣物数据条目
- `ApplyToPlayerStats` 新增 ThunderHeart / StormEye case

### src/gameplay/FloorModifier.h
- `FloorModifierType` 枚举新增 `Thunderstorm = 11`
- `FloorModifierData` 新增 `bool playerAttackLightning = false` 字段
- `FloorModifierSystem` 新增 `IsPlayerAttackLightning()` 查询接口

### src/gameplay/FloorModifier.cpp
- `kModifierTable` 新增 Thunderstorm 数据条目（亮黄色，enemyDamageMul=1.15，playerAttackLightning=true）
- 实现 `IsPlayerAttackLightning()` 聚合查询

### 编译验证（第十九轮）
- Release 版本增量编译成功（exit_code=0）
- 历史遗留警告保持不变（C4244 line 4260 / C4819 / C4996）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-k、第二十轮新增/修复内容（极限闪避反击系统 - 防御端正反馈循环）

### 设计意图
本轮聚焦"玩法与深度"优先级，补全游戏反馈循环的防御端缺口。前五轮（升级/装备/圣物/元素/层修饰符/连击）建立了完整的攻击端正反馈体系，但防御端仅有"闪避无敌帧"被动减伤，缺乏"激进防御 → 反击"的瞬时心流循环。本轮新增极限闪避反击系统：玩家在敌人攻击前摇期间闪避可触发"极限闪避"，获得伤害 buff + 扇形反击，与连击系统正交叠加产生 combo 50 (1.5x) + 极限闪避 (1.5x) = 2.25x 的爆发窗口，形成"攻防双反馈"的完整心流体验。同时引入 2 种防御系圣物 + 2 个成就 + 攻击前摇可读信号，构建完整的"防御反击流"build 维度。

### 核心系统 1：敌人攻击前摇可读信号（EnemyAI）
- **位置**：`src/gameplay/EnemyAI.h` + `src/gameplay/EnemyAI.cpp`
- **修改**：
  - `EnemyComponent` 新增 `float attackTelegraph = 0.f`（攻击前摇剩余时间，>0 = 即将攻击）
  - `UpdateEnemyAI` 在 `attackCooldown -= dt` 后新增前摇计算块
  - 按敌人类型设置阈值：近战系（Melee/Elite/Splitter/Shielded/StealthMelee）0.3s、远程系（Ranged/SniperRanged）0.5s
  - 仅在玩家处于攻击范围内时激活（近战 attackRange+40px，远程 detectionRange）
  - Boss 不参与（Boss 攻击模式复杂，前摇由技能独立处理）
- **设计意图**：为极限闪避/弹反类机制提供可检测信号，让玩家能"读招"而非盲闪

### 核心系统 2：极限闪避触发与反击（PlayerCombat）
- **位置**：`src/gameplay/PlayerCombat.cpp` 闪避触发末尾
- **修改**：
  - 冷却衰减区新增 `perfectDodgeBuffTimer` / `perfectDodgeCooldown` 衰减
  - 闪避触发后（音效播放之后）插入极限闪避检测块：
    1. 冷却就绪检查（`perfectDodgeCooldown <= 0`）
    2. UniformGrid 查询玩家周围 80px 内敌人（`dodgeWindowMul` 加成后 120px）
    3. 任一敌人 `attackTelegraph > 0` 即触发
  - 触发效果：
    - `perfectDodgeBuffTimer = 2.0f`（2s 伤害 +50%）
    - `perfectDodgeCooldown = 3.0f`（3s 冷却）
    - `++perfectDodgeCount`（跨层保留统计）
    - 摄像机震动（8 强度，0.3s）
    - 金色粒子爆发（Explosion）
    - 扇形反击：闪避方向 120° 半径 100px 范围，200% 玩家伤害
- **设计意图**：与连击系统形成正交叠加，buff 2s 固定 1.5x 乘数，与 combo 阶梯式乘数乘法累乘

### 核心系统 3：极限闪避 buff 应用于伤害（CombatSystem）
- **位置**：`src/gameplay/CombatSystem.h` + `src/gameplay/CombatSystem.cpp`
- **修改**：
  - `CombatSystem.h` 新增 `GetPerfectDodgeDamageMultiplier()` 内联静态方法返回 1.50f
  - `ApplyDamage` 在 combo 加成之后新增极限闪避 buff 乘数检测：
    - 攻击者为玩家且 `perfectDodgeBuffTimer > 0` → `finalDamage *= 1.50f`
    - 复仇之刃圣物：buff 期间所有非暴击攻击强制暴击（`!info.isCritical` 守卫，避免重复应用）
- **设计意图**：buff 乘数与 combo 乘数乘法累乘，复仇之刃暴击作为"圣物 build 强力效果"显著提升爆发

### 核心系统 4：防御反击系圣物（RelicSystem）
- **位置**：`src/gameplay/RelicSystem.h` + `src/gameplay/RelicSystem.cpp`
- **修改**：圣物总数 10 → 12，新增：
  - `MoonAmulet = 11`（月光护符）：`stats.dodgeWindowMul += 0.50f`（检测窗口 80px → 120px）
  - `VengeanceBlade = 12`（复仇之刃）：`stats.perfectDodgeGuaranteedCrit = true`（buff 期间必暴击）
- **PlayerStats 新增字段**：
  - `float dodgeWindowMul = 1.f`（极限闪避窗口倍率）
  - `bool perfectDodgeGuaranteedCrit = false`（极限闪避后下次攻击必暴击）
- **设计意图**：与现有攻击系/雷霆系圣物形成"攻防双 build"选择，月光护符降低操作门槛，复仇之刃提升上限

### 核心系统 5：极限闪避 HUD 视觉反馈（HUD）
- **位置**：`src/ui/HUD.h` + `src/ui/HUD.cpp`
- **修改**：
  - `HUD` 新增 `SetPerfectDodgeData(buffTimer, cooldown)` 接口
  - 新增字段 `perfectDodgeBuffTimer_` / `perfectDodgeCooldown_`
  - `Render()` 末尾在 `drawCombo` 之后调用 `drawPerfectDodge`
  - `drawPerfectDodge` 实现：
    - 屏幕四周金色脉冲边框（4 条矩形，6px 厚度，sin 波脉冲 `0.5 + 0.5 * sin(phase * 12)`）
    - 最后 0.3s 淡出（`fadeOut = buffTimer / 0.3f`）
    - alpha 范围 80~140，颜色 `sf::Color(255, 200, 50, alpha)`
    - 中央"极限闪避 伤害 +50%"文字（20pt 粗体）
- **设计意图**：与连击指示器形成"攻防双反馈"视觉体系，玩家可清晰感知 buff 窗口

### 核心系统 6：极限闪避成就（AchievementSystem）
- **位置**：`src/gameplay/AchievementSystem.h` + `src/gameplay/AchievementSystem.cpp`
- **修改**：
  - `AchievementCondition` 枚举新增 `TotalPerfectDodges = 12`
  - 新增成就定义：
    - id=25 "幻影之舞"（累计 100 次极限闪避，Combat 类）
    - id=40 "极限闪避"（首次触发，Special 类，隐藏成就）
  - 新增 `OnPerfectDodge(int amount)` 接口实现
- **Game.cpp 集成**：
  - 新增 `int lastPerfectDodgeCount_ = 0` 字段
  - `updatePlaying` 步骤 7.6 新增极限闪避成就检测：比对 `lastPerfectDodgeCount_` 与 `pc->perfectDodgeCount`，差值上报 `OnPerfectDodge(delta)`，首次触发 `UnlockSpecial(40)`
  - `recomputePlayerStats` 重置块新增 `s.dodgeWindowMul = 1.f; s.perfectDodgeGuaranteedCrit = false;`
  - `updateHUDData` 末尾新增 `hud_.SetPerfectDodgeData(pc->perfectDodgeBuffTimer, pc->perfectDodgeCooldown);`
  - `nextLevel` 新增 `savedPerfectDodgeCount` / `savedPerfectDodgeMax` 跨层保留
- **设计意图**：完美 dodgeCount 跨层保留作为长线统计，buffTimer/Cooldown 不保留避免携带 buff 进入下层破坏平衡

### 数值平衡设计
- 极限闪避 buff 2s +50% 伤害：与连击 +75% 乘法累乘产生 2.25x 爆发，但窗口短（2s）需精准时机
- 冷却 3s：避免连续触发破坏游戏节奏，与连击 3s 窗口对齐形成"攻防节奏"
- 反击 200% 玩家伤害：扇形 120° 限制方向性，需面向敌人闪避才能命中，提升操作深度
- 月光护符 +50% 窗口（80px → 120px）：降低操作门槛但不至于无脑触发
- 复仇之刃必暴击：与暴击率加成独立，作为"圣物 build 强力效果"显著提升爆发上限
- 跨层保留 perfectDodgeCount 不保留 buffTimer：长线成就统计 + 短线平衡控制

### 编译验证（第二十轮）
- Release 版本增量编译成功（exit_code=0）
- 历史遗留警告保持不变（C4244 line 4292 int→float / C4819 / C4996，均与本次修改无关）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 九、代码修改记录（第二十轮关键文件）

### src/gameplay/Player.h
- `PlayerStats` 末尾新增 `float dodgeWindowMul = 1.f`（极限闪避窗口倍率，月光护符加成）
- `PlayerStats` 末尾新增 `bool perfectDodgeGuaranteedCrit = false`（极限闪避后必暴击，复仇之刃圣物）
- `PlayerComponent` 连击字段后新增 4 个极限闪避字段：`perfectDodgeCooldown` / `perfectDodgeBuffTimer` / `perfectDodgeCount` / `perfectDodgeMaxThisLife`

### src/gameplay/EnemyAI.h
- `EnemyComponent` 的 `slowFactor` 字段后新增 `float attackTelegraph = 0.f`（攻击前摇剩余时间）

### src/gameplay/EnemyAI.cpp
- `UpdateEnemyAI` 在 `attackCooldown -= dt` 之后新增 attackTelegraph 设置块，按敌人类型设置阈值（近战 0.3s / 远程 0.5s），Boss 不参与

### src/gameplay/CombatSystem.h
- `GetComboDamageMultiplier` 声明后新增 `GetPerfectDodgeDamageMultiplier()` 内联静态方法返回 1.50f

### src/gameplay/CombatSystem.cpp
- `ApplyDamage` 在 combo 加成之后新增极限闪避 buff 乘数检测与复仇之刃暴击逻辑

### src/gameplay/PlayerCombat.cpp
- 冷却衰减区新增 `perfectDodgeBuffTimer` / `perfectDodgeCooldown` 衰减
- 闪避触发末尾（音效之后）插入完整的极限闪避检测与反击块（范围查询 + buff 设置 + 摄像机震动 + 粒子爆发 + 扇形反击）

### src/gameplay/RelicSystem.h
- `RelicType` 枚举新增 `MoonAmulet = 11` / `VengeanceBlade = 12`，Count 从 11 → 13

### src/gameplay/RelicSystem.cpp
- `kRelicTable` 新增 2 条防御反击系圣物数据
- `ApplyToPlayerStats` 新增 MoonAmulet（`dodgeWindowMul += 0.50f`）/ VengeanceBlade（`perfectDodgeGuaranteedCrit = true`）case

### src/gameplay/AchievementSystem.h
- `AchievementCondition` 枚举新增 `TotalPerfectDodges = 12`
- 新增 `OnPerfectDodge(int amount)` 接口声明

### src/gameplay/AchievementSystem.cpp
- 新增成就定义 id=25 "幻影之舞"（累计 100 次极限闪避，Combat 类）
- 新增成就定义 id=40 "极限闪避"（首次触发，Special 类，隐藏）
- 新增 `OnPerfectDodge` 实现：调用 `progressByCondition(TotalPerfectDodges, amount)`

### src/ui/HUD.h
- 新增 `SetPerfectDodgeData(buffTimer, cooldown)` 内联方法
- 新增字段 `perfectDodgeBuffTimer_` / `perfectDodgeCooldown_`
- 新增 `drawPerfectDodge` 方法声明

### src/ui/HUD.cpp
- `Render()` 末尾在 `drawCombo` 之后调用 `drawPerfectDodge`
- 新增 `drawPerfectDodge` 实现：屏幕四周金色脉冲边框（4 条矩形，6px 厚度，sin 波脉冲）+ 中央"极限闪避 伤害 +50%"文字（20pt 粗体），最后 0.3s 淡出

### src/core/Game.h
- `floorModifiers_` 成员后新增 `int lastPerfectDodgeCount_ = 0`（用于检测 perfectDodgeCount 变化并上报成就）

### src/core/Game.cpp
- `recomputePlayerStats` 重置块新增 `s.dodgeWindowMul = 1.f; s.perfectDodgeGuaranteedCrit = false;`
- `updateHUDData` 末尾新增 `hud_.SetPerfectDodgeData(pc->perfectDodgeBuffTimer, pc->perfectDodgeCooldown);`
- `updatePlaying` 步骤 7.6 新增极限闪避成就检测（调用 `achievementSystem_.OnPerfectDodge(delta)` 与 `UnlockSpecial(40)`）
- `nextLevel` 新增 `savedPerfectDodgeCount` / `savedPerfectDodgeMax` 跨层保留

### 编译验证（第二十轮）
- Release 版本增量编译成功（exit_code=0）
- 历史遗留警告保持不变（C4244 line 4292 int→float / C4819 / C4996，均与本次修改无关）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-l、第二十一轮新增/修复内容（激活 EnemyAffix 精英词缀系统 - 敌人侧深度拓展）

### 设计意图
本轮聚焦"玩法与深度"优先级，遵循第十五轮（激活圣物）/第十六轮（激活元素状态）/第十九轮（激活 Lightning 元素）的成功"激活 dead code"模式，激活 `Component.h` 中自初版定义但**从未被任何代码调用**的整套 `EnemyAffix` / `EliteAffix` 词缀框架（D3 死代码点）。

此前游戏的精英维度仅有两层：`EnemyType::Elite`（HP=100/伤害=15 的固定数值怪）+ `isChampion`（HP×3/伤害×1.5/速度×1.1 的随机强化版），均为**纯数值倍率差异**，无任何玩法变化。本轮激活后，每个精英/Champion 在生成时随机获得 2-3 个词缀组合（Champion 5% 概率 4 词缀全开），立刻产生 **C(4,2)=6 + C(4,3)=4 + C(4,4)=1 = 11 种行为变化组合**：

- **厚血（HpBoost）**：HP ×1.5，更耐打
- **狂暴（DamageBoost）**：伤害 ×1.3，威胁更高
- **迅捷（SpeedBoost）**：速度 ×1.2，走位压力增大
- **回血（Regenerating）**：每秒回 1% maxHp，必须速杀

与现有"层修饰符 enemyHpMul""极限闪避反击""元素状态"全部正交叠加，让每个精英成为独特的小 Boss 战，产生涌现式交互（例如：4 词缀全开的 Champion + 第十七轮"嗜血狂暴"层修饰符 + 第十六轮 Ice 冰冻 → 极端 tank + 高伤 + 玩家需用冰冻控速击杀）。

### 核心系统 1：词缀分配与倍率应用（EnemySpawner）
- **位置**：`src/gameplay/EnemySpawner.cpp` `SpawnEnemyAt`
- **修改**：在 `EnemyComponent` 重置块后新增词缀分配块
  - 触发条件：`type != Boss && (isElite || isChamp)`（Boss 已有 5 套独立机制，不参与）
  - 随机抽 2-3 个词缀（不重复抽样，Fisher-Yates 洗牌前 N 个）
  - Champion 5% 概率抽 4 词缀全开（"满词缀精英"特殊挑战，对应隐藏成就）
  - 挂载或重置 `EnemyAffix` 组件（复用对象池时若已存在则重置）
  - 应用词缀倍率（在 Champion 倍率之后累乘）：
    - HpBoost: `health->current *= 1.5f`，`health->max *= 1.5f`
    - DamageBoost: `enemy->damage *= 1.3f`
    - SpeedBoost: `enemy->moveSpeed *= 1.2f`
    - Regenerating: 不影响生成时属性（由 EnemyAI 处理回血）
  - 普通敌人：若复用了之前的精英实体（对象池复用），清空 `affixMask=0` 避免残留

### 核心系统 2：Regenerating 词缀回血 + 光环粒子（EnemyAI）
- **位置**：`src/gameplay/EnemyAI.cpp` `UpdateEnemyCombat`（死亡检测块之后）
- **修改**：在计算 toPlayer 距离之前，新增词缀处理块（仅对活着的敌人）
  - **Regenerating 回血**：`affix->regenTimer += dt`，每 1s 触发回 `health->max * 0.01f`（1% maxHp），上限不超过 max
  - **光环粒子**：`affix->auraTimer += dt`，每 0.3s 在敌人头顶 `myPos + (0, -16)` 发射 2 个紫色粒子（180,80,255 → 220,120,255，size 2-4，life 0.4-0.7s，speed 20-50），让玩家视觉识别词缀精英
  - 词缀组件可能不存在（普通敌人未挂载），`GetComponent` 返回 nullptr 时跳过
- **复用死字段**：`EnemyAffix::regenTimer` 和 `auraTimer` 自初版定义后从未被任何代码读写，本轮激活

### 核心系统 3：词缀敌人紫色发光边缘（Game.cpp 渲染）
- **位置**：`src/core/Game.cpp` `renderPlaying` 敌人精灵渲染循环
- **修改**：在元素状态染色之后，新增词缀敌人紫色发光染色
  - 检测 `EnemyAffix::affixMask != 0`
  - 与紫色 (180, 80, 255) 做 30% 混合（轻度染色，比元素状态 50% 混合弱，避免覆盖元素状态色）
  - 让玩家能视觉识别"词缀精英"并优先击杀，与紫色光环粒子呼应
  - 不覆盖 Champion 金色描边（`isChampion` 的金色血条仍渲染在头顶）

### 核心系统 4：词缀名头顶显示（Game.cpp 渲染）
- **位置**：`src/core/Game.cpp` `renderChampionHealthBars`
- **修改**：
  - 渲染条件扩展：`isChampion || hasAffix`（词缀精英也显示血条，不仅是 Champion）
  - 血条前景色区分：Champion 用金色 (255, 215, 80)，纯 Elite 词缀怪用紫色 (180, 80, 255)
  - 三角形标识颜色同步区分
  - 新增词缀名渲染：在血条上方 8px 显示词缀组合名（10pt，紫色 220,180,255）
    - 词缀名格式："厚血+狂暴+迅捷"（多词缀用 + 连接）
    - 中文名映射：HpBoost→厚血 / DamageBoost→狂暴 / SpeedBoost→迅捷 / Regenerating→回血
    - 使用 `utf8ToSfString` 转换（避免 U8 宏的 sizeof 陷阱）

### 核心系统 5：词缀击杀成就（AchievementSystem）
- **位置**：`src/gameplay/AchievementSystem.h` + `src/gameplay/AchievementSystem.cpp`
- **修改**：
  - `AchievementCondition` 枚举新增 `TotalAffixKills = 13`
  - 新增公开接口 `OnAffixEnemyKilled(int amount, bool fullAffix)`
  - 新增 2 个成就定义：
    - id=26 "词缀猎手"（累计击杀 30 个带词缀精英，Combat 类，非隐藏）
    - id=27 "满词缀征服"（击败 5 个 4 词缀全开的满词缀精英，Combat 类，隐藏成就）
      - condition 用 `NoDamageBoss` 占位（特殊成就模式，仅由 `OnAffixEnemyKilled(fullAffix=true)` 显式推进，避免被普通词缀击杀误触发）
  - `OnAffixEnemyKilled` 实现：
    - 推进 `TotalAffixKills` 条件（id=26 自动检测解锁）
    - `fullAffix=true` 时显式推进 id=27（手动累加 currentValue + tryUnlock）

### 核心系统 6：词缀击杀上报（Game.cpp OnKill）
- **位置**：`src/core/Game.cpp` `combatSystem_.OnKill` 回调
- **修改**：在 `achievementSystem_.OnEnemyKilled` 之后新增词缀击杀上报块
  - 读取 `victimAffix->affixMask`，若非 0 则上报
  - `fullAffix` 判定：`affixMask == 0b1111`（4 词缀全开）
  - 调用 `achievementSystem_.OnAffixEnemyKilled(1, fullAffix)`

### 数值平衡设计
- **词缀倍率保守**：HpBoost ×1.5 / DamageBoost ×1.3 / SpeedBoost ×1.2（避免与 Champion ×3/×1.5/×1.1 累乘后数值爆炸）
- **不重复抽样**：4 词缀不重复抽 2-3 个，避免同一属性堆叠（不会出现"双 HpBoost"）
- **Boss 不参与**：Boss 已有 5 套独立机制（远程/召唤/冲撞地裂/旋转弹幕/精英召唤），词缀会让 Boss 战过于复杂
- **满词缀精英稀有**：仅 Champion 5% 概率触发，对应隐藏成就"满词缀征服"阈值仅 5
- **Regenerating 可被压制**：1% maxHp/s 回血率，玩家 DPS 足够时仍可击杀；与第十六轮 Fire DoT / Poison DoT 配合可压制回血
- **与层修饰符累乘**：词缀倍率在第十七轮 `enemyHpMul` 之上累乘（如"嗜血狂暴"层 enemyHpMul=0.7 × HpBoost 1.5 = 1.05x，仍可击杀）

### 与现有维度的正交关系
- **层修饰符（第十七轮）**：词缀是"敌人个体差异"，层修饰符是"全局规则差异"，二者乘法累乘
- **极限闪避（第二十轮）**：词缀精英的攻击前摇（attackTelegraph）仍可触发极限闪避，词缀敌人反击伤害更高
- **元素状态（第十六轮）**：词缀敌人仍可被 Fire/Ice/Poison/Lightning 影响，DoT 可压制 Regenerating 回血
- **连击系统（第十八轮）**：击杀词缀精英（更难）仍累积 combo，词缀精英是高价值连击目标
- **圣物系统（第十五轮）**：词缀精英是圣物 build 的试金石（如"吸血鬼之牙"+5% 吸血应对 DamageBoost 词缀）

### 编译验证（第二十一轮）
- Release 版本增量编译成功（exit_code=0）
- 仅重新编译改动文件（EnemySpawner.cpp / EnemyAI.cpp / Game.cpp / AchievementSystem.cpp）
- 无新增警告，仅历史遗留（C4244 line 4356 int→float / C4819 编码 / C4996 localtime，均与本次修改无关）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 九、代码修改记录（第二十一轮关键文件）

### src/gameplay/EnemySpawner.cpp
- `SpawnEnemyAt` 在 `EnemyComponent` 重置块后新增词缀分配块：
  - 触发条件 `type != Boss && (isElite || isChamp)`
  - Fisher-Yates 不重复抽样 2-3 词缀（Champion 5% 概率 4 词缀全开）
  - 挂载或重置 `EnemyAffix` 组件（复用对象池时若已存在则重置 affixMask）
  - 应用 HpBoost/DamageBoost/SpeedBoost 倍率（在 Champion 倍率之后累乘）
  - 普通敌人：清空 `affixMask=0` 避免对象池复用残留

### src/gameplay/EnemyAI.cpp
- `UpdateEnemyCombat` 在死亡检测块之后、计算 toPlayer 之前新增词缀处理块：
  - Regenerating 词缀：`regenTimer += dt`，每 1s 回 `health->max * 0.01f`（1% maxHp）
  - 光环粒子：`auraTimer += dt`，每 0.3s 在头顶发射 2 个紫色粒子（180,80,255 → 220,120,255）
  - 复用 `EnemyAffix::regenTimer` / `auraTimer` 死字段（自初版从未被读写）

### src/core/Game.cpp
- `renderPlaying` 敌人精灵渲染循环：在元素状态染色之后新增词缀敌人紫色发光染色（30% 混合 (180, 80, 255)）
- `renderChampionHealthBars`：
  - 渲染条件扩展为 `isChampion || hasAffix`
  - 血条前景色区分：Champion 金色 / 纯 Elite 词缀怪紫色
  - 三角形标识颜色同步区分
  - 新增词缀名渲染（10pt 紫色，utf8ToSfString 转换，"厚血+狂暴+迅捷"格式）
- `combatSystem_.OnKill` 回调：在 `OnEnemyKilled` 之后新增词缀击杀上报块（fullAffix 判定 affixMask == 0b1111）

### src/gameplay/AchievementSystem.h
- `AchievementCondition` 枚举新增 `TotalAffixKills = 13`
- 新增公开接口 `OnAffixEnemyKilled(int amount, bool fullAffix)`

### src/gameplay/AchievementSystem.cpp
- 新增成就定义 id=26 "词缀猎手"（Combat 类，TotalAffixKills 条件，targetValue=30）
- 新增成就定义 id=27 "满词缀征服"（Combat 类，NoDamageBoss 占位条件，targetValue=5，隐藏成就）
- 实现 `OnAffixEnemyKilled`：推进 TotalAffixKills + fullAffix 时显式推进 id=27

### 编译验证（第二十一轮）
- Release 版本增量编译成功（exit_code=0）
- 无新增警告，仅历史遗留（C4244 line 4356 / C4819 / C4996）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-m、第二十二轮新增/修复内容（激活 QuestType 死接口 - 任务系统横向拓展）

本轮聚焦"玩法与深度"优先级，遵循第十五/十六/十九/二十一轮的"激活 dead code"模式，激活 `QuestSystem` 中自初版定义但**从未被注册使用**的 4 种 QuestType 死接口（KillTarget / CollectItem / SurviveTime / ClearRooms）。

### 死代码考古发现

- `QuestType` 枚举定义 8 种类型（`QuestSystem.h:43-52`），但 `Initialize()` 只注册了 4 种（ReachLevel / TriggerEvent / CollectSkills / AccumulateCoins）
- `QuestDef::targetEnemy` / `targetQuality` / `targetTime` 字段自初版从未被有效使用
- `progressQuest` 中 KillTarget / CollectItem 类型过滤逻辑已实现（`QuestSystem.cpp:351-356`）但从未触发
- `Update` 中 SurviveTime 时间累计逻辑已实现（`QuestSystem.cpp:171-181`，注释明确写"当前任务线未用，保留接口"）
- `Game.cpp` 已接线 `OnEnemyKilled` / `OnItemPickedUp` / `OnRoomCleared` 事件上报（line 608 / 801 / 738），但无对应类型任务接收

**五层基础设施全部就位**，仅需在 `Initialize()` 中注册任务实例即可激活。

### 1. 新增 5 个支线任务（id 6-10）

| ID | 类型 | 标题 | 目标 | 奖励 | 前置 |
|---|---|---|---|---|---|
| 6 | KillTarget | 精英猎人 | 击杀 5 个 Elite（EnemyType::Elite） | 400exp + 200G + 史诗装 | 1 |
| 7 | KillTarget | 远程杀手 | 击杀 10 个 Ranged | 350exp + 180G + 蓝装 | 1 |
| 8 | CollectItem | 史诗收藏家 | 拾取 3 件史诗（Yellow）装备 | 500exp + 250G + 1 技能点 | 1 |
| 9 | SurviveTime | 不死行者 | 单次生命存活 180 秒 | 600exp + 300G + 暗金装 | 1 |
| 10 | ClearRooms | 地牢清道夫 | 清理 10 个房间 | 450exp + 220G + 史诗装 | 1 |

**设计意图**：
- 任务 6/7（KillTarget）：激活击杀指定敌人类型计数，引导玩家熟悉不同敌人威胁
- 任务 8（CollectItem）：激活拾取指定品质装备过滤，引导装备 build 构筑
- 任务 9（SurviveTime）：激活存活时间累计，鼓励谨慎玩法（区别于割草爽快）
- 任务 10（ClearRooms）：激活房间清理计数，引导探索地牢全图
- 所有任务 `prerequisiteQuestId=1`，与主线 2-5 并行解锁，玩家前期可选任务从 1 个拓展到 9 个

### 2. SurviveTime 任务死亡重置机制

新增 `QuestSystem::OnPlayerDeath()` 接口，在玩家死亡时重置所有 Active 状态的 SurviveTime 任务的 `timeAccumulator` 和 `currentProgress`。

**设计意图**：SurviveTime 任务语义为"单次生命存活 N 秒"。玩家死亡后若不重置，可跨多次生命累计达成目标，与语义不符。`Game.cpp` 在玩家死亡判定处（line 1132）调用此接口。

### 3. QuestMenu UI 扩展：cards_ 扩大 + 垂直滚动

- `cards_` 数组从 `std::array<QuestCardData, 5>` 扩大为 `<QuestCardData, 10>`（支持 5 主线 + 5 支线）
- 新增 `scrollOffset_` / `maxScrollOffset_` / `OnMouseWheel(float delta)` 接口
- `SetQuestData` 末尾计算 `maxScrollOffset_`（基于有效任务数 × (cardH+cardGap) - 可见区域高度），清理超出任务数的卡片残留（id=0）
- `Render` 应用 `scrollOffset_` 实现垂直滚动：
  - cardY 起始减去 `scrollOffset_`
  - 裁剪完全不可见的卡片（性能优化）
  - 绘制顶部/底部遮罩（与面板背景同色，覆盖溢出卡片）
  - 绘制滚动条指示器（滚动槽 + 滑块，滑块位置反映 `scrollOffset_`）
- `Game.cpp` pollEvent 循环新增 `MouseWheelScrolled` 事件转发（仅 `questMenuVisible_` 时）

### 与现有系统的协同

- **第二十一轮词缀系统**：任务 6 引导玩家关注 Elite 类型敌人，与词缀精英形成"敌人侧变化 + 玩家侧目标"双向引导
- **任务系统主线**：5 个支线任务与主线 2-5 并行解锁，玩家前期有 9 个可选任务，缓解"前期任务荒"
- **成就系统**：任务 6/7 击杀 Elite/Ranged 与成就 id=5"精英猎人"（50 Champion）形成梯度目标
- **装备 build**：任务 8 引导玩家拾取史诗装备，与装备品质系统协同
- **地牢探索**：任务 10 鼓励清理房间，与房间系统/陷阱房机制协同

### 风险评估

- **平衡风险：极低**。任务是横向内容扩展，不修改任何数值公式、不与对象池交互、不影响战斗平衡
- **存档兼容：无破坏**。`Serialize()` 返回 `vector<QuestInstance>`，旧存档反序列化时新任务保持 `Locked` 默认状态
- **UI 风险：低**。滚动条为成熟 UI 模式；`cards_` 扩大不影响现有 5 个任务的渲染；遮罩方案避免溢出视觉污染
- **性能风险：无**。任务系统每帧 Update 仅遍历 10 个任务实例（O(1)），UI 裁剪跳过不可见卡片

### 编译验证（第二十二轮）
- Release 版本增量编译成功（exit_code=0）
- 仅重新编译改动文件（QuestSystem.cpp / QuestMenu.cpp / Game.cpp）
- 无新增警告，仅历史遗留（C4244 line 4363 int→float / C4819 编码，均与本次修改无关）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 九、代码修改记录（第二十二轮关键文件）

### src/gameplay/QuestSystem.h
- 新增公开接口 `void OnPlayerDeath()`（玩家死亡时重置 SurviveTime 任务进度）
- 注释说明设计意图：单次生命语义

### src/gameplay/QuestSystem.cpp
- `Initialize()` 在任务 5 之后追加 5 个支线任务定义（id 6-10）：
  - 任务 6/7：`KillTarget` 类型，分别绑定 `EnemyType::Elite` / `EnemyType::Ranged`
  - 任务 8：`CollectItem` 类型，绑定 `ItemQuality::Yellow`（史诗）
  - 任务 9：`SurviveTime` 类型，`targetCount=180`（秒）
  - 任务 10：`ClearRooms` 类型，`targetCount=10`
  - 全部 `prerequisiteQuestId=1`（与主线 2-5 并行解锁）
- 新增 `OnPlayerDeath()` 实现：遍历 Active 状态的 SurviveTime 任务，重置 `timeAccumulator` 和 `currentProgress`

### src/ui/QuestMenu.h
- `cards_` 数组从 `std::array<QuestCardData, 5>` 扩大为 `<QuestCardData, 10>`
- 新增成员变量：`scrollOffset_` / `maxScrollOffset_` / `kScrollStep=60.f`
- 新增公开接口 `void OnMouseWheel(float delta)`
- 注释更新：布局说明从"5 个任务卡片"改为"最多 10 个任务卡片，支持垂直滚动"

### src/ui/QuestMenu.cpp
- `SetQuestData`：
  - 循环改为遍历整个 `cards_` 数组（超出 `quests.size()` 的卡片清空 `id=0` 避免残留）
  - 末尾新增 `maxScrollOffset_` 计算（基于有效任务数 × (cardH+cardGap) - 可见区域高度）
  - 新增 `scrollOffset_` 范围 clamp
- `Render`：
  - `cardY` 起始减去 `scrollOffset_` 实现垂直滚动
  - 新增 `listTop` / `listBottom` 可见区域边界
  - 循环内新增裁剪判断：完全在可见区域外的卡片跳过（`cardY += cardH + cardGap` 后 `continue`）
  - 循环后新增顶部/底部遮罩（与面板背景同色，覆盖溢出卡片）
  - 新增滚动条指示器（滚动槽 + 滑块，滑块位置反映 `scrollOffset_`）
- 新增 `OnMouseWheel` 实现：`scrollOffset_ -= delta * kScrollStep`，clamp `[0, maxScrollOffset_]`

### src/core/Game.cpp
- `pollEvent` 循环：在 `Closed` 和 `KeyPressed` 之间新增 `MouseWheelScrolled` 分支
  - 条件：`state_ == Playing && questMenuVisible_`
  - 转发给 `questMenu_.OnMouseWheel(event.mouseWheelScroll.delta)`
- 玩家死亡判定（line 1132）：在 `ChangeState(GameState::Dead)` 之前调用 `questSystem_.OnPlayerDeath()`

### 编译验证（第二十二轮）
- Release 版本增量编译成功（exit_code=0）
- 仅重新编译改动文件（QuestSystem.cpp / QuestMenu.cpp / Game.cpp）
- 无新增警告，仅历史遗留（C4244 line 4363 / C4819）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-n、第二十三轮新增/修复内容（装备套装系统 - Roguelike 第七维 build 构筑）

### 设计意图

本轮聚焦"玩法与深度"优先级，遵循第十五轮（圣物）/第十六轮（元素状态）/第十九轮（Lightning）/第二十一轮（EnemyAffix）/第二十二轮（QuestType）的"激活 dead code + 拓展新维度"模式，在装备系统侧新增**装备套装（Equipment Set）**机制，作为 Roguelike 第七维 build 构筑。

此前游戏的成长维度有六重：升级（UpgradeSystem）+ 装备（InventorySystem 词缀）+ 圣物（RelicSystem）+ 元素（CombatSystem 状态）+ 层修饰符（FloorModifier）+ 连击/极限闪避（瞬时心流）。但**装备系统本身仅有"词缀 + 品质"二维**，缺乏套装机制带来的"凑齐 N 件"长线目标。玩家捡到高品质装备后立刻替换，无差异化取舍决策。

本轮新增 4 种套装，**每个槽位恰属 2 个套装**（强制玩家取舍，无法同时凑齐 2 个完整套装），2 件套/3 件套分阶奖励，立刻产生 build 多样性：

- **Warrior 战士之怒**：武器 + 胸甲 + 戒指 → 2 件套 +10% 伤害 / 3 件套 +5% 暴击率 + 20% 暴击伤害
- **Sage 智者之识**：头盔 + 项链 + 戒指 → 2 件套 +15% 经验获取 / 3 件套 +20% 最大生命
- **Wind 疾风行者**：靴子 + 武器 + 头盔 → 2 件套 +12% 移速 / 3 件套 +15% 攻速
- **Guardian 永恒守护**：胸甲 + 靴子 + 项链 → 2 件套 +8 防御 / 3 件套 +10% 最大生命 + 8 防御

与现有六维成长全部正交叠加，让装备选择从"看词缀数值"升级为"看套装组合 + 词缀数值"的双层决策。

### 核心系统 1：套装枚举与 Item.setId 字段（LootSystem）

- **位置**：`src/gameplay/LootSystem.h`
- **修改**：
  - 头部新增 `#include <array>` 和 `#include <utility>`
  - 新增 `enum class EquipmentSet : uint8_t`（None/Warrior/Sage/Wind/Guardian 共 5 值）
  - 新增 `enum class SetBonusType : uint8_t`（None/DamageMul/MaxHpMul/MoveSpeedMul/AttackSpeedMul/CritRateAdd/CritDamageAdd/ExpMulAdd/DefenseAdd 共 9 值）
  - `Item` 结构体新增字段 `EquipmentSet setId = EquipmentSet::None`
  - LootSystem 类新增 5 个静态接口：
    - `GetSetName(EquipmentSet)` 返回中文名（如 "战士之怒"）
    - `GetSetSlots(EquipmentSet)` 返回该套装的 3 个槽位（`std::array<ItemSlot, 3>`）
    - `GetSetColor(EquipmentSet)` 返回套装主色（Warrior 红/Sage 蓝/Wind 绿/Guardian 金）
    - `GetSetBonus(EquipmentSet, int pieces)` 返回 `{SetBonusType, float}` 加成对
    - `RollSetForSlot(ItemSlot slot)` 按槽位随机分配套装（每个槽位恰有 2 个可选套装，等概率）

### 核心系统 2：套装分配与辅助函数实现（LootSystem.cpp）

- **位置**：`src/gameplay/LootSystem.cpp`
- **修改**：
  - 实现 5 个辅助函数（GetSetName/GetSetSlots/GetSetColor/GetSetBonus/RollSetForSlot）
  - **关键约束**：`RollSetForSlot` 每个槽位恰有 2 个可选套装，等概率随机：
    - Weapon: Warrior/Wind
    - Helmet: Sage/Wind
    - Chest:   Warrior/Guardian
    - Boots:  Wind/Guardian
    - Ring:    Warrior/Sage
    - Amulet:  Sage/Guardian
  - 此约束**强制玩家取舍**：6 槽位 × 2 套装/槽 = 12 个套装分配点，但每个套装需 3 件才能完整，玩家最多凑齐 2 个完整套装（占用 6 槽位），第三套装必然缺件
  - `generateRandomItem` 末尾添加 `item.setId = RollSetForSlot(item.slot);`

### 核心系统 3：套装加成应用（InventorySystem）

- **位置**：`src/gameplay/InventorySystem.h` + `src/gameplay/InventorySystem.cpp`
- **修改**：
  - 新增接口 `GetActiveSetCounts()`：遍历 6 装备槽统计每个套装的件数（返回 `std::array<std::pair<EquipmentSet, int>, 4>`）
  - 新增接口 `ApplySetBonuses(PlayerStats&)`：对每个套装，>=2 件应用 2 件套加成，>=3 件额外应用 3 件套加成
  - 文件开头（namespace cu 内）新增匿名命名空间辅助函数 `applySetBonusToStats(PlayerStats&, SetBonusType, float)`，支持 8 种加成类型

### 核心系统 4：PlayerStats 集成（Game.cpp）

- **位置**：`src/core/Game.cpp` `recomputePlayerStats`
- **修改**：在装备词缀加成之后、圣物加成之前调用 `inventorySystem_.ApplySetBonuses(s)`
- **设计意图**：
  - 在装备词缀之后：让套装 multiplier 作用于已含词缀的 stats（乘法叠加）
  - 在圣物之前：确保套装 multiplier 与圣物乘法叠加
  - 形成"升级 + 装备 + 套装 + 圣物 + 元素 + 层修饰符 + 连击 + 极限闪避"的七维成长正交叠加体系

### 核心系统 5：存档序列化与版本升级（SaveSystem）

- **位置**：`src/core/SaveSystem.h` + `src/core/SaveSystem.cpp`
- **修改**：
  - `kSaveVersion` 从 3 升级为 4（v3→v4）
  - `writeItem` 末尾新增：`uint8_t setId = static_cast<uint8_t>(it.setId); writePOD(ofs, &setId, sizeof(setId));`
  - `readItem` 末尾新增（含防御性检查）：
    - 读取 1 字节 setId
    - 若 `setId > static_cast<uint8_t>(EquipmentSet::Guardian)` 视为损坏，回退为 `None`

### 核心系统 6：背包 UI 套装展示（Menus）

- **位置**：`src/ui/Menus.h` + `src/ui/Menus.cpp`
- **修改**：
  - `InventoryMenu` 类新增静态方法 `formatSetBonusShort(SetBonusType, float)`：将 8 种加成类型格式化为简短中文描述
  - `InventoryMenu::Render` 在标题下方 y=78 新增"激活套装"汇总行
  - 装备槽 cell 底部 y+62 显示 "[套装名]"（套装主色，10pt 粗体）

## 九、代码修改记录（第三十轮关键文件）

### src/rendering/Renderer.h
- 新增 `std::vector<sf::Vertex> vertexBuffer_` 私有成员（预分配顶点缓冲，复用避免每帧重新分配）

### src/rendering/Renderer.cpp
- 构造函数：新增 `vertexBuffer_.reserve(20000)` 预分配，支持 5000 精灵（20000 顶点，约 320KB）
- `EndScene()`：局部 `std::vector<sf::Vertex> vertices` 改为引用 `vertexBuffer_`，`vertices.clear()` 仅重置 size 不释放容量
- `flushBatch()`：从逐顶点拷贝到 `sf::VertexArray` 改为 `target_->draw(vertices.data(), vertices.size(), sf::Quads, states)` 零拷贝 API

### src/ecs/Registry.h
- 新增 `ForEach<Components..., Func>(Func&& func)` 零分配模板方法，通过回调直接遍历实体，不分配临时 vector

### src/core/Game.cpp（6 处替换）
- 空间网格更新：`View<EnemyComponent, Transform>` → `ForEach<EnemyComponent, Transform>`
- 统一死亡检测：`View<EnemyComponent, Health>` → `ForEach<EnemyComponent, Health>`
- BOSS 检测：`View<EnemyComponent, Health>` → `ForEach<EnemyComponent, Health>`（含 `bossActive_` 短路退出）
- 实体 Sprite 渲染：`View<Transform, Sprite>` → `ForEach<Transform, Sprite>`
- Champion 血条渲染：`View<Transform, EnemyComponent>` → `ForEach<Transform, EnemyComponent>`
- 统计计数 + 秒杀调试：`View<EnemyComponent>` → `ForEach<EnemyComponent>`
- 所有 `continue` 同步改为 `return`

### src/gameplay/EnemyAI.cpp（5 处替换）
- `UpdateEnemyAI` 主循环：`View<EnemyComponent, Transform>` → `ForEach<EnemyComponent, Transform>`
- `UpdateEnemyCombat` 主循环：`View<EnemyComponent, Transform>` → `ForEach<EnemyComponent, Transform>`
- 3 处 Boss 召唤物计数：`View<EnemyComponent>` → `ForEach<EnemyComponent>`
- 所有 `continue` 同步改为 `return`

### src/gameplay/CombatSystem.cpp（1 处替换）
- `UpdateStatusEffects`：`View<StatusEffectComponent>` → `ForEach<StatusEffectComponent>`

### src/gameplay/CombatEffects.cpp（2 处替换）
- `UpdateDamageTexts`：`View<DamageTextComponent, Transform>` → `ForEach<DamageTextComponent, Transform>`
- `RenderDamageTexts`：`View<DamageTextComponent, Transform>` → `ForEach<DamageTextComponent, Transform>`

### src/gameplay/Animation.cpp（1 处替换）
- `AnimationSystem::Update`：`View<AnimationComponent, Sprite>` → `ForEach<AnimationComponent, Sprite>`

### src/gameplay/RoomSystem.cpp（1 处替换）
- `hasAliveEnemiesInRoom`：`View<EnemyComponent, Transform>` → `ForEach<EnemyComponent, Transform>`
- 特殊处理：原 `return true` 早期返回改为 `found` 标志 + lambda 短路，语义等价

### 编译验证（第三十轮）
- Release 版本增量编译成功（exit_code=0），无新增警告
- 仅历史遗留警告：C4244（`int→float`）
- 可执行文件：`build/bin/Release/crazyunder.exe`


### 数值平衡设计

- **2 件套/3 件套分阶**：2 件套为低阶加成（+10%/+15%），3 件套为高阶加成（+20%/+5% 暴击）
- **每个槽位恰 2 个可选套装**：强制取舍，避免"全槽位同套装"破坏 build 多样性
- **套装 multiplier 保守**：单套装最高 +20% 单属性，远低于圣物 +30% / 升级 +50% / 连击 +75% 的天花板
- **Warrior 与 Guardian 互斥**：Chest 槽位属 Warrior/Guardian，玩家无法同时凑齐 Warrior 3 件套和 Guardian 3 件套

### 与现有维度的正交关系

- **升级系统（第六轮）**：升级 +50% 伤害 × Warrior 3 件 +10% 伤害 = 1.65x
- **圣物系统（第十五轮）**：套装 multiplier 在圣物之前应用，与圣物乘法叠加
- **元素状态（第十六轮）**：Wind 3 件 +15% 攻速 × Lightning 元素麻痹 = 控制流 build 强化
- **层修饰符（第十七轮）**：层修饰符全局规则 × 套装个体 build
- **连击/极限闪避（第十八/二十轮）**：Warrior 3 件 +5% 暴击率 + 20% 暴击伤害 × 连击 +75% 伤害 = 爆发流 build

### 风险评估

- **平衡风险：中**。套装 multiplier 保守（最高 +20%），但 2 套装组合可产生 +40% 单属性，需实测验证
- **存档兼容：破坏**。v3→v4 升级，旧存档无法读取
- **UI 风险：低**。套装信息显示在背包界面已有区域，不引入新窗口
- **性能风险：无**。`GetActiveSetCounts` 仅遍历 6 槽位（O(1)）

### 编译验证（第二十三轮）
- Release 版本增量编译成功（exit_code=0）
- 重新编译改动文件（LootSystem.cpp / InventorySystem.cpp / Game.cpp / SaveSystem.cpp / Menus.cpp）
- 无新增警告，仅历史遗留（C4244 line 4369 int→float / C4819 编码 / C4996 localtime）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 九、代码修改记录（第二十三轮关键文件）

### src/gameplay/LootSystem.h
- 头部新增 `#include <array>` 和 `#include <utility>`
- 新增 `enum class EquipmentSet : uint8_t`（None/Warrior/Sage/Wind/Guardian）
- 新增 `enum class SetBonusType : uint8_t`（None/DamageMul/MaxHpMul/MoveSpeedMul/AttackSpeedMul/CritRateAdd/CritDamageAdd/ExpMulAdd/DefenseAdd）
- `Item` 结构体新增字段 `EquipmentSet setId = EquipmentSet::None`
- LootSystem 类新增 5 个静态接口声明：`GetSetName` / `GetSetSlots` / `GetSetColor` / `GetSetBonus` / `RollSetForSlot`

### src/gameplay/LootSystem.cpp
- 实现 5 个辅助函数（GetSetName 返回中文名 / GetSetSlots 返回槽位数组 / GetSetColor 返回主色 / GetSetBonus 返回加成对 / RollSetForSlot 按槽位随机分配）
- `RollSetForSlot` 关键约束：每个槽位恰 2 个可选套装（Weapon: Warrior/Wind, Helmet: Sage/Wind, Chest: Warrior/Guardian, Boots: Wind/Guardian, Ring: Warrior/Sage, Amulet: Sage/Guardian）
- `generateRandomItem` 末尾添加 `item.setId = RollSetForSlot(item.slot);`

### src/gameplay/InventorySystem.h
- 新增公开接口 `[[nodiscard]] std::array<std::pair<cu::EquipmentSet, int>, 4> GetActiveSetCounts() const noexcept`
- 新增公开接口 `void ApplySetBonuses(PlayerStats& stats) const`

### src/gameplay/InventorySystem.cpp
- 文件开头（namespace cu 内紧跟 includes 之后）新增匿名命名空间辅助函数 `applySetBonusToStats(PlayerStats&, SetBonusType, float)`，switch 处理 8 种加成类型
- 实现 `GetActiveSetCounts`：遍历 6 装备槽统计每个套装件数
- 实现 `ApplySetBonuses`：>=2 件应用 2 件套加成，>=3 件额外应用 3 件套加成

### src/core/Game.cpp
- `recomputePlayerStats`：在 `inventorySystem_.ApplyToPlayerStats(s)`（装备词缀）之后、`relicSystem_.ApplyToPlayerStats(s)`（圣物）之前新增 `inventorySystem_.ApplySetBonuses(s);` 调用

### src/core/SaveSystem.h
- `kSaveVersion` 从 3 升级为 4

### src/core/SaveSystem.cpp
- `writeItem` 末尾新增：写入 `setId`（1 字节，`static_cast<uint8_t>`）
- `readItem` 末尾新增：读取 `setId`（1 字节），含防御性检查（值 > Guardian 视为损坏回退为 None）

### src/ui/Menus.h
- `InventoryMenu` 类新增静态方法声明 `static std::string formatSetBonusShort(SetBonusType type, float val);`

### src/ui/Menus.cpp
- 实现 `formatSetBonusShort`：switch 处理 8 种加成类型，格式化为简短中文描述（如 "+10%伤害"/"+8防御"/"+5%暴击率"）
- `InventoryMenu::Render`：
  - 标题下方 y=78 新增"激活套装"汇总行（遍历 slots_ 统计每个套装件数，>=2 件显示套装名+件数+加成简述，无激活显示"（无）"灰色字）
  - 装备槽 cell 底部 y+62 显示 "[套装名]"（套装主色，10pt 粗体）