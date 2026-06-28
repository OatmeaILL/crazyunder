# CrazyUnder 项目信息

> 本文档记录项目概述、系统描述与技术细节，供新会话继承上下文使用。

## 一、项目概述

**CrazyUnder** 是一款 2.5D 像素风 Roguelike 割草爽游。

- **技术栈**：C++17 + SFML 2.6+ + CMake
- **架构**：ECS（实体组件系统）+ 固定步长（1/30s）+ 插值渲染
- **性能目标**：500+ 敌人、1000+ 子弹/粒子同屏，100+ FPS
- **构建命令**：
  ```powershell
  & "E:\Programming\Visual Studio\IDE-\MSBuild\Current\Bin\MSBuild.exe" build\crazyunder.sln /p:Configuration=Release /m /verbosity:minimal
  ```
- **可执行文件**：`e:\Programming\xioamaipian\crazyunder\build\bin\Release\crazyunder.exe`
- **当前状态**：编译成功（2026-06-26），Release 版本无调试运行时依赖（msvcp140.dll 而非 msvcp140d.dll）

## 二、文件结构

```
e:\Programming\xioamaipian\crazyunder\
├── CMakeLists.txt                  # CMake 构建配置（含 /utf-8 编译选项）
├── PROJECT_STATUS.md               # 变更日志与版本记录
├── projectinfo.md                  # 项目信息（概述/系统描述/技术细节）
│
├── src/
│   ├── main.cpp                    # 程序入口（含 UTF-8 locale 设置）
│   │
│   ├── core/                       # 核心系统
│   │   ├── Game.h / Game.cpp       # 游戏主循环、状态机、场景搭建、教程覆盖层
│   │   ├── Input.h / Input.cpp     # 输入系统（IsKeyPressed 边沿触发）
│   │   ├── Time.h / Time.cpp       # 时间管理（固定步长 1/30s, MaxCatchUp=1）
│   │   ├── ResourceManager.h/.cpp  # 资源管理（字体优先 simhei.ttf）
│   │   └── AudioManager.h/.cpp     # 音频系统
│   │
│   ├── ecs/                        # ECS 框架
│   │   ├── Entity.h                # EntityId 定义
│   │   ├── Component.h             # 组件定义
│   │   └── Registry.h / Registry.cpp  # 注册表
│   │
│   ├── gameplay/                   # 游戏逻辑（23 个系统）
│   │   ├── Player.h / Player.cpp   # 玩家移动+碰撞
│   │   ├── PlayerCombat.h/.cpp     # 玩家战斗（普通攻击/闪避/AOE）
│   │   ├── EnemyAI.h / EnemyAI.cpp # 敌人 AI（流场+Boids+自动开门+射击+Boss机制+Champion）
│   │   ├── EnemySpawner.h/.cpp     # 敌人生成器（对象池+Champion概率生成）
│   │   ├── ProjectileSystem.h/.cpp # 弹幕系统（对象池+tile碰撞）
│   │   ├── CombatSystem.h/.cpp     # 战斗系统（OnHit/OnKill）
│   │   ├── CombatEffects.h/.cpp    # 战斗特效（飘字+粒子+UTF-8）
│   │   ├── DungeonGenerator.h/.cpp # 地牢生成（BSP树+门状态+障碍房+事件房+诅咒房）
│   │   ├── RoomSystem.h/.cpp       # 房间系统（事件房/诅咒房逻辑）
│   │   ├── FlowField.h/.cpp        # 流场寻路
│   │   ├── LootSystem.h/.cpp       # 战利品系统（品质/概率/中文名称/Champion掉落）
│   │   ├── InventorySystem.h/.cpp  # 背包系统（6装备槽+25格背包+右键菜单）
│   │   ├── MerchantSystem.h/.cpp   # 商人系统（买卖装备+技能+金币）
│   │   ├── UpgradeSystem.h/.cpp    # 升级系统（17种升级+技能点机制+AddLevels）
│   │   ├── UpgradeUI.h/.cpp        # 升级选择 UI
│   │   ├── ExpOrbSystem.h/.cpp     # 经验球系统（即时磁吸）
│   │   ├── CoinSystem.h/.cpp       # 金币系统（对象池+磁吸+OnCoinGained回调）
│   │   ├── SkillSystem.h/.cpp      # 技能系统（5个技能+装备槽+背包）
│   │   ├── HeartSystem.h/.cpp      # 爱心回血系统（对象池+磁吸+2%回血）
│   │   ├── QuestSystem.h/.cpp      # 任务系统（5个主线+依赖解锁+提交机制）
│   │   ├── AchievementSystem.h/.cpp# 成就系统（跨存档持久化+4类成就）
│   │   └── Animation.h/.cpp        # 动画系统
│   │
│   ├── rendering/                  # 渲染系统
│   │   ├── Renderer.h/.cpp         # 批量渲染器
│   │   ├── TileMap.h/.cpp          # 地图渲染（含门开关贴图）
│   │   ├── TextureAtlas.h/.cpp     # 纹理图集
│   │   ├── ParticleSystem.h/.cpp   # 粒子系统（对象池+4种预设特效）
│   │   ├── Camera.h/.cpp           # 摄像机
│   │   └── TextureGenerator        # 在 utils 中
│   │
│   ├── ui/                         # UI 系统（全中文，UTF-8 转换）
│   │   ├── HUD.h/.cpp              # HUD（含技能冷却倒计时+技能点提示+放大版技能图标+新版小地图）
│   │   ├── Menus.h/.cpp            # 菜单（主菜单/暂停/背包含右键菜单/商人/死亡/胜利/升级/调试/设置）
│   │   ├── QuestMenu.h/.cpp        # 任务面板 UI（Q键打开+卡片列表+领取按钮）
│   │   ├── AchievementMenu.h/.cpp  # 成就面板 UI（Tab键打开+分类网格+进度条）
│   │   └── UIManager.h/.cpp        # UI 管理器
│   │
│   └── utils/                      # 工具
│       ├── Logger.h                # 日志宏 + U8 宏 + utf8ToSfString
│       ├── UniformGrid.h/.cpp      # 均匀网格空间分区
│       ├── ObjectPool.h            # 对象池模板
│       └── TextureGenerator.h/.cpp # 过程化贴图生成（玩家/敌人/装备图标）
│
├── assets/
│   ├── sprites/player.png
│   ├── generated/
│   └── audio/
│
└── build/
    └── bin/Release/crazyunder.exe  # Release 版可执行文件
```

## 三、已完成的系统

### 核心架构
- ECS 注册表 + 组件系统（ComponentPool 稀疏集合，O(1) 增删查）
- 固定步长游戏循环（1/30s 逻辑更新 + 插值渲染，MaxCatchUp=1）
- 状态机：Menu / Playing / Paused / Dead / Victory
- Playing↔Paused 切换不清场不重建

### 中文显示系统
- **CMakeLists.txt**：`/utf-8` 编译选项
- **main.cpp**：`std::locale::global(std::locale(".UTF-8"))` 设置全局 UTF-8 locale
- **Logger.h**：`U8(str)` 宏 + `utf8ToSfString()` 函数
- **所有 setString 调用**：使用 `U8()` 宏或 `utf8ToSfString()` 转换
- **字体**：优先使用 simhei.ttf（黑体，.ttf 格式）
- **主标题**：使用英文 "CRAZYUNDER"（大字号中文渲染异常）

### 玩家操作
- **移动**：WASD
- **普通攻击**：鼠标左键（连发远程子弹）
- **闪避**：鼠标右键（带无敌帧，轴分离碰撞检测，跟随 WASD 方向）
- **AOE 爆炸**：空格键（清怪）
- **交互**：E 键（开门/宝箱/商人/楼梯/事件房 NPC）
- **技能**：数字键 1-4（震地波/吸血打击/狂暴/引力井/地刺）
- **背包**：G 键（打开/关闭，右键格子弹出装备/丢弃菜单）
- **技能升级**：J 键（有未使用技能点时打开升级选择界面）
- **任务面板**：Q 键（查看任务列表+领取奖励）
- **成就面板**：Tab 键（查看成就进度）
- **暂停**：P / ESC
- **调试面板**：F5 键

### 按键教程系统
- 首次从主菜单点击"开始游戏"时显示操作指南覆盖层
- 半透明遮罩 + 居中卡片 + 8 项按键说明
- 任意键或鼠标点击关闭教程
- 教程显示期间游戏逻辑暂停

### 门系统
- **DoorState 结构**：`{bool open=false, float hp=30, float maxHp=30}`
- **门默认关闭**（阻挡子弹和玩家）
- **怪物自动开门**：敌人遇到关闭的门时自动开门通过（不攻击破坏）
- **玩家E键开关门**：handleInteract 方法（在 handleEvents 中调用）
- **子弹攻击门**：关闭的门阻挡子弹并扣 HP
- **门开关贴图区分**：CreateDoorTile（关闭）+ CreateDoorOpenTile（打开）
- **门血量条**：renderDoorHealthBars

### 战斗反馈系统（颜色规范）
- **受伤**：红色伤害数字
- **回复血量**：绿色数字 "+X"
- **获得经验**：亮绿色 "+EXP X"
- **暴击**：黄色数字（放大字号）
- **物品拾取**：品质颜色飘字 "品质级 物品名"
  - 传说级：亮金色 (255, 180, 30)
  - 史诗级：紫色 (180, 80, 255)
  - 稀有级：蓝色 (80, 140, 255)
  - 普通级：浅灰白 (220, 220, 220)
- **掉落物粒子特效**：物品掉落/经验球生成时触发品质色粒子爆裂

### 敌人系统（10种 + Champion 精英强化）
- **基础类型**：Melee、Ranged、Suicide、Elite、Boss
- **新增类型**：
  - **StealthMelee**：间歇性隐身近战
  - **CountdownSuicide**：靠近后头顶倒计时自爆
  - **Splitter**：死亡分裂成 2 个小怪
  - **Shielded**：带盾怪（正面减伤 50%）
  - **SniperRanged**：狙击远程（超远距离高伤害，靠近时快速撤退）
- **Champion 精英强化系统**：任何普通怪 8% 概率升级为 Champion 版本
  - HP ×3, 伤害 ×1.5, 速度 ×1.1, 体型 ×1.5
  - Sprite 颜色调为金色 (255, 230, 150)
  - 头顶 36x4px 金色血条 + 左侧三角形标识（世界空间渲染）
  - 100% 掉落 1-2 件蓝色品质装备，经验/金币 ×3
- **敌人分层强化**（分维度线性缩放）
  - HP：每层 +30%（1.0 + (level-1)×0.30）
  - 伤害：每层 +18%（1.0 + (level-1)×0.18）
  - 速度：每层 +4% 且封顶 1.6 倍（1.0 + (level-1)×0.04，第16层后不再增加）
- **Suicide 修复**：移速 120→200，攻击范围 20→30，200px 内触发 1.5x 冲锋
- **AI**：流场+Boids 寻路 + 自动开门（上锁的门无法开启）

### BOSS 系统
- **检测**：updatePlaying 查找存活的 BOSS
- **血条**：renderBossHealthBar 在屏幕顶部中央渲染半透明血条
- **回血机制**：玩家离开 BOSS 房间 5 秒后，BOSS 每秒回 2% 最大生命
- **远程攻击**：每 2s 朝玩家发射 3 发扇形红色子弹
- **召唤小兵**：每 8s 召唤 2 个近战小兵（标记 isBossMinion）
- **AOE 技能**：每 3s 范围伤害
- **冲撞地裂**：每 6s 3倍速冲撞，路径留 5s 地裂区域（FissureZone）
- **召唤精英怪**：HP<50% 触发，之后每 12s 持续召唤
- **旋转弹幕**：每 10s 持续 2s，0.12s/发螺旋紫色子弹
- **爱心掉落**：Boss 召唤物死亡 30% 概率掉落爱心（回复 2% 血量）
- **击败提示**：击败后屏幕中央显示"BOSS 已击败！通过通道前往下一层"（最后 3 秒闪烁）

### 技能系统（5个）
- **SkillType 枚举**：GroundSlam / LeechStrike / Berserk / GravityWell / SpikeGround
- **4 个技能装备槽**（按键 1-4）+ **5 格技能背包**
- **技能商人售卖**：MerchantSystem 生成 2 个随机技能库存
- **技能点机制**：升级累积技能点，按 J 键主动开启选择界面
- **各技能独特粒子特效**：差异化视觉呈现

### 爱心回血系统（HeartSystem）
- **爱心生成**：Boss 召唤物死亡 30% 概率掉落
- **爱心贴图**：代码生成红色发光爱心（核心+光晕）
- **拾取机制**：对象池管理，与金币系统一致的 0.15s 散射后磁吸
- **回血效果**：拾取后回复玩家 2% 最大生命值，触发绿色飘字

### 装备与背包系统
- **6 装备槽**：Weapon / Helmet / Chest / Boots / Ring / Amulet
- **25 格大背包**：G 键打开
- **自动装备**：拾取时若对应槽位为空则自动装备，否则入包
- **左键快速穿卸**：左键点击装备槽卸下，左键点击背包格装备（旧装备交换）
- **右键上下文菜单**：右键格子弹出"装备/卸下"和"丢弃"两个选项
  - 装备槽/技能槽已装备 → "卸下"
  - 背包格/技能背包 → "装备"
  - 丢弃：直接销毁物品/技能，不进入背包/技能背包
  - 菜单 120x68 像素，自动避让屏幕边界，点击外部关闭
- **悬停闪烁**：鼠标悬停装备格时黄色边框闪烁
- **装备图标**：24x24 像素图标（剑/盔/甲/靴/戒/坠），在背包/商人界面显示
- **百分比词缀修复**：value 是小数（如 0.05），显示时 ×100
- **布局修复**：装备背包与技能背包分离，技能区下移到 y=545 避免重叠

### 商人系统
- **出现规则**：第一层必然出现，后续层 50% 概率出现在起始房间
- **位置**：玩家右侧 48 像素
- **初始金币**：第一层玩家初始 50 金币
- **互动**：E 键打开商人菜单
- **商品**：6 件装备 + 2 个随机技能，每次刷新品质随层数提升
- **买卖**：左键购买/出售
- **价格倍率**：普通 1.0x / 稀有 4.0x / 史诗 7.0x / 传说 16.0x
- **技能价格**：基础价格 150 + level×25（x5倍）
- **头顶文字**：世界空间渲染"神秘商人"，靠近显示"按 E 交易"

### 金币系统
- 怪物/罐子/宝箱掉落金币（对象池 CoinSystem）
- 拾取半径 30px，0.15s 散射后磁吸
- 玩家属性含 coins 字段，HUD 显示

### 升级系统（17种升级）
- **升级公式**：expToNext = 100 × level × 1.5
- **技能点机制**：每次升级累积 1 个技能点，按 J 键主动开启选择界面（不再自动弹窗打断游戏）
- **属性升级（12种）**：伤害/攻速/移速/生命/暴击率/暴击伤害/吸血/子弹分裂/子弹穿透/连锁闪电/AOE冷却/闪避冷却
- **技能升级（5种）**：震地波/吸血打击/狂暴/引力井/地刺（maxLevel=1，仅一次）
- **随机抽取**：从未满级升级中随机抽 3 个选项，20% 概率替换最后一个为技能升级

### 下一层入口
- **Stairs tile**：E 键进入下一层
- **属性保留**：等级、经验、属性、装备、金币、**技能槽、技能背包**全部保留
- **BOSS 限制**：BOSS 存活时无法进入下一层

### HUD（抬头显示）
- 血条/蓝条/经验条
- **放大版技能图标**（48x48，间距 56px）
  - 字号自适应字数：2字→16, 3字→13, 4字→11
  - 冷却倒计时数字位于图标内中央（字号 16，黄色粗体）
  - 快捷键提示位于图标下方（1-4 / LMB / RMB / SPC）
  - 技能等级 Lv2/Lv3 显示在图标右下角
- **技能点提示**（`skillPoints_ > 0` 时显示闪烁文字"有未使用的技能点 n 点  按 J 开启新技能选择界面"）
- 等级/波次/敌人数量/FPS
- **新版小地图**（200x150，右下角）
  - 半透明黑色背景板 + 边框 + "地图"标题
  - 走廊通道：Floor 暗灰、Door 棕色、Stairs 青色，呈现真实地牢形状
  - 房间类型标记：B(BOSS红)/T(宝箱黄)/E(精英紫)/S(楼梯青)/?(隐藏灰)
  - 当前房间：黄色高亮圆圈
  - 玩家位置：绿色圆点 + 白色描边，实时跟随
  - 底部图例：你/B-BOSS/T-宝箱/S-楼梯

### 调试面板
- **F5 键开关**：半透明面板 + 作弊按钮
- **传送功能**：出生房/宝箱房/陷阱房/阻碍房
- **作弊功能**：无敌/加金币/加经验/清屏/击杀全部敌人

### 地牢与房间
- BSP 树生成随机地牢（房间+走廊）
- 房间类型（9种）：普通、精英、BOSS、起始、宝箱、楼梯、陷阱、障碍、隐藏
- 波次系统：随波数增加敌人种类和数量
- 阻碍房：30% 不可破坏石柱 + 70% 可破坏木墙，路径连通性检查
- **事件房**（totalRooms≥4 时 50% 概率生成）：4 种事件类型，E 键触发
  - 乞丐：施舍 50 金币换随机蓝色装备
  - 神秘法师：花费 100 金币换 1 个随机技能
  - 宝箱模仿怪：触发战斗，击败掉落高品质装备
  - 祭坛：献祭 20% 当前血量换永久属性提升
  - NPC 过程化生成像素风贴图（灰褐破衣乞丐/深紫长袍法师/石质三层台祭坛）
  - 渲染：世界空间 1.5x 缩放，与玩家等大
- **诅咒房**（totalRooms≥5 时 30% 概率生成）
  - 进入后锁门，施加移速/攻速 debuff
  - 需击败精英怪+小兵解除诅咒
  - 解除后掉落史诗品质装备

### 任务系统（5个主线任务）
- **任务面板**：Q 键打开，1100x650 面板，左侧 5 个任务卡片，右侧剧情说明
- **任务依赖解锁**：通过 prerequisiteQuestId 字段实现，ClaimReward 时解锁依赖任务
- **任务类型**：ReachLevel / TriggerEvent / CollectSkills / AccumulateCoins
- **5 个主线任务**：
  | ID | 名称 | 类型 | 目标 | 奖励 |
  |----|------|------|------|------|
  | 1 | 破晓之始 | ReachLevel | 到达第2层 | 200exp + 100G + 蓝装 |
  | 2 | 古老祭坛 | TriggerEvent | 触发祭坛 | 300exp + 150G + 史诗装 |
  | 3 | 地下奇人 | TriggerEvent | 触发乞丐 | 300exp + 150G + 史诗装 |
  | 4 | 百艺兼修 | CollectSkills | 收集4技能 | 500exp + 200G + 1技能点 + 暗金装 |
  | 5 | 财富之力 | AccumulateCoins | 攒1000G | -1000G + 金装 + 等级+5 |
- **任务1完成后解锁任务2-5**
- **任务5金币提交机制**：reward.coins = -1000 表示扣除，达标后进入 Completed
- **任务状态存档**：5 个 QuestSaveEntry（state + currentProgress + timeAccumulator）

### 成就系统（跨存档持久化）
- **成就面板**：Tab 键打开，1100x650 面板，4 列按分类（战斗/探索/收集/特殊）布局
- **持久化文件**：saves/achievements.dat（独立于存档，所有存档共享）
- **二进制格式**：魔数 + 版本号 + 成就数据
- **显示规则**：
  - 已解锁：完整显示名称+描述+解锁时间
  - 未解锁非隐藏：灰色显示+进度条
  - 隐藏成就：显示 "???"

### 存档系统（3 槽位 + 任务状态）
- **3 个手动存档槽**：二进制序列化
- **自动存档**：每层结束自动保存到当前槽位
- **存档内容**：玩家属性/装备/技能/任务状态/层数/击杀数/存活时间
- **任务状态序列化**：QuestSystem::Serialize/Deserialize 接口
- **读档日志**：输出"读档技能恢复: 技能槽=N/4 技能背包=N/5"

### 粒子系统
- 对象池管理（5000 个槽位，零堆分配）
- 4 种预设特效：Explosion（橙红爆炸）、HitSpark（黄白火花）、LevelUpBeam（蓝白光柱）、LootGlow（金色发光）
- 通用 Emit 接口：EmitConfig 支持位置/速度/颜色/大小/生命期/径向发射等参数

## 六、关键技术细节

### 中文显示
- SFML `sf::String(const std::string&)` 在 Windows 使用 ANSI 解码导致乱码
- 解决方案：`U8(str)` 宏 + `utf8ToSfString()` 使用 `sf::String::fromUtf8`

### 装备图标生成
- `TextureGenerator::CreateItemIcon(ItemSlot slot)` 生成 24x24 像素图标
- 6 个图标注册到图集：`icon_weapon/helmet/chest/boots/ring/amulet`
- `InventoryMenu`/`MerchantMenu` 的 `Initialize()` 中创建 `sf::Texture` 对象池

### 技能系统持久化
- `PlayerComponent.skillSlots` / `skillBackpack` 随玩家实体销毁而丢失
- `nextLevel()` 中手动保存并恢复这两个字段
- `UpgradeSystem` 的 `upgradeLevels_` 记录技能是否已获取，避免重复获取

### 技能点机制
- `UpgradeSystem::skillPoints_` 累积未使用的技能点
- `AddExp()` 升级时 `++skillPoints_`
- `ApplyUpgrade()` 消耗 1 个技能点
- `IsUpgradePending()` 改为 `skillPoints_ > 0`
- 选择后若仍有剩余技能点，重新滚动选项保持菜单打开

### 商人头顶文字世界空间渲染
- `Game::renderPlaying()` 中：`EndScene()` 后、`setView(defaultView)` 前
- 直接在 `window_.draw(merchantNameText)`，使用 `setOrigin` 居中
- 摄像机视图自然处理坐标转换，不再使用 `mapCoordsToPixel`

### Boss 机制实现位置
- **远程射击**：`EnemyAI.cpp` `UpdateEnemyCombat()` 中 Boss 独立处理，每 2s 发射 3 发
- **召唤小兵**：`EnemyAI.cpp` `UpdateEnemyCombat()` 中调用 `spawner->SpawnEnemyAt()`
- **AOE 范围伤害**：单独冷却（不受攻击冷却限制），每 3s 对周围敌人造成 15 伤害
- **爱心掉落**：`Game.cpp` `OnKill` 回调中检测 `isBossMinion`，30% 概率调用 `heartSystem_.Spawn()`

### HeartSystem 实现
- 独立对象池管理（与 CoinSystem 一致的对象池模式）
- `HeartData` 结构：position, velocity, size, active, lifetime, magnetTimer
- `Spawn(position)` / `Update(dt)` / `Render(Renderer&)` 接口
- 磁吸拾取：0.15s 散射后磁吸，回复 2% 最大生命
- 贴图：代码生成红色发光核心 + 粉色光晕

### Champion 精英怪实现
- `EnemyAI.h` `EnemyComponent` 新增 `bool isChampion = false`
- `EnemySpawner.h` `SpawnEnemyAt` 新增 `bool champion` 参数，`SpawnEnemiesInArea` 新增 `float championChance`
- `EnemySpawner.cpp` 中应用倍率：HP×3, 伤害×1.5, 速度×1.1, 体型×1.5，sprite 颜色调金色
- `LootSystem.cpp` `OnEnemyKilled` 新增 `bool isChampion` 参数，Champion 掉落与 Elite 同等
- `Game.cpp` `renderChampionHealthBars()` 世界空间渲染 36x4px 金色血条

### 任务系统实现要点
- **依赖解锁**：`QuestDef.prerequisiteQuestId` 字段，ClaimReward 时调用 `unlockDependentQuests`
- **事件上报**：`OnEventTriggered(EventType)` 区分乞丐/祭坛/法师/宝箱
- **金币提交**：`reward.coins = -1000` 表示扣除，`OnRewardGranted` 回调中 `pc->stats.coins += reward.coins` 处理正负
- **等级奖励**：`UpgradeSystem::AddLevels(int)` 直接增加 level_ 和 skillPoints_，不触发 OnLevelUp 回调
- **装备奖励**：`LootSystem::GenerateRandomItem(ilvl, quality)` + `InventorySystem::PickupItem`
- **技能点奖励**：通过 `AddExp(reward.skillPoints * 200)` 间接获得（每技能点约 200 经验）

### U8 宏的 sizeof 陷阱（本轮修复）
- `U8(str)` 宏定义为 `sf::String::fromUtf8((str), (str) + sizeof(str) - 1)`
- 对字符串字面量：`sizeof` 返回字节数（含 `\0`），减 1 正确
- **对 `const char*` 变量**：`sizeof` 返回指针大小（8 字节），减 1 后只取前 7 字节，导致中文乱码
- **修复方案**：改用 `sf::String::fromUtf8(str.begin(), str.end())` 直接迭代 std::string

### UTF-8 自动换行实现
- `drawWrappedText` 逐个 UTF-8 字符累积，判断首字节前缀确定字符长度（1-4 字节）
- 用 `sf::Text::getLocalBounds().width` 实际测量宽度，超宽时在字符边界换行
- 避免按 byte 切分导致中文乱码（UTF-8 中文占 3 字节）

### 成就系统持久化
- 独立文件 `saves/achievements.dat`，跨存档共享
- 二进制格式：魔数 + 版本号 + 成就数据
- `static bool` 确保仅初始化一次
- `LoadFromFile()` 返回 `[[nodiscard]]` bool，调用处需检查

### 新版小地图坐标转换
- `worldToMap` lambda：地牢世界坐标 → 小地图坐标
- `scale = min(mapW/dungeonPixelW, mapH/dungeonPixelH)` 保持比例
- `offsetX/Y` 居中偏移
- HUD 新增 `playerPos_` 成员，`SetPlayerPosition` 接口，`updateHUDData` 中调用

### 圣物系统实现要点（第十五轮新增）
- **三重成长叠加顺序**：`recomputePlayerStats()` 中先应用基础属性 → 升级系统等级加成 → 装备词缀 → 圣物加成（`relicSystem_.ApplyToPlayerStats(s)`），最后同步 HP 上限到当前 HP
- **圣物获取算法**：`RollUnownedRelics(count)` 从所有未拥有圣物中筛选后 Fisher-Yates 洗牌取前 N 个，保证不重复且随机
- **存档二进制格式**：6 字节连续数组（`std::array<uint8_t, 6>`），每字节对应一个圣物槽位的 `RelicType` 枚举值（None=0）
- **反序列化防御**：`Deserialize` 跳过 None/越界(>=Count)/重复值，确保存档损坏时不崩溃且不产生无效状态
- **圣物选择菜单键位优先级**：1/2/3 键在 `relicChoiceActive_` 为 true 时优先处理圣物选择，否则才处理升级选择（if-else 链）
- **R 键与 Shift+R 共存**：通过 `event.key.shift` 标志区分，Shift+R 触发"重新生成地牢"调试功能，纯 R 键切换圣物查看面板
- **UI 互斥**：圣物查看面板打开时其他面板（G/Q/Tab/J/商人）打开会主动关闭圣物面板；Boss 触发圣物选择时关闭查看面板避免叠加；ESC 优先关闭圣物面板而非暂停

## 七、新会话建议

### 优先级 1：验证本轮改动
1. 运行 Release 版本，确认无运行时缺失报错
2. 进入游戏，打通第一层，确认任务1"破晓之始"能结算并解锁任务2-5
3. 按 Q 键打开任务面板，确认 5 个任务卡片显示正确，领取按钮工作
4. 按 Tab 键打开成就面板，确认分类布局和显示状态
5. 按 G 键打开背包，右键格子弹出"装备/卸下"+"丢弃"菜单，文字无乱码
6. 背包中装备背包第5行与技能背包不重叠，标题"技能背包"不与装备格重叠
7. HUD 左下角技能图标 48x48，4字技能名（如"吸血打击"）完整显示
8. 右下角小地图可读：走廊形状、房间标记(B/T/E/S/?)、玩家位置、图例
9. 进入事件房，E 键触发事件，NPC 贴图正常显示
10. 挑战 Champion 精英怪，确认金色血条和掉落
11. 挑战 Boss，确认冲撞地裂、召唤精英、旋转弹幕新机制

### 优先级 1：验证第十五轮圣物系统改动
12. 击败 Boss 后弹出圣物 3 选 1 菜单，鼠标悬停边框高亮、点击或按 1/2/3 选择正常
13. 选择圣物后属性立即生效（如选"战士之证"后伤害提升 15%，可在调试面板 F5 验证）
14. 按 R 键打开圣物查看面板，确认 3x2 槽位布局、已获圣物显示图标/名称/描述、空槽显示"— 空缺 —"
15. 按 R 或 ESC 关闭圣物面板；圣物面板打开时玩法暂停不被攻击
16. 打开其他面板（G/Q/Tab/J/商人）时圣物面板自动关闭，无 UI 叠加
17. 进入下一层后圣物保留（不打 Boss 不重置）；存档后读档圣物保留
18. 按 Shift+R 仍可触发"重新生成地牢"调试功能，纯 R 键不会误触重置
19. 圣物满 6 个后击败 Boss 不再弹出选择菜单（避免空选项）

### 优先级 1：验证第十六轮元素异常状态系统改动
20. 装备狂暴技能（按键 3），激活狂暴后普攻子弹颜色从黄白变为橙红，命中敌人后敌人持续受到燃烧伤害（橙红色飘字每 0.5s 出现一次，持续 3s）
21. 装备引力井技能（按键 4），释放后范围内敌人不仅被拉近，还会被冰冻（敌人 sprite 染青蓝色，移动速度明显变慢 50%，持续 2s）
22. 装备地刺技能（按键 5），释放后范围内敌人 sprite 染毒绿色，每 1s 出现毒绿色飘字（持续 5s），敌人移速降低 30%；敌人离开地刺范围后中毒仍持续到 5s 结束
23. 多状态叠加验证：先用地刺（Poison）再切狂暴（Fire）打同一敌人，敌人 sprite 颜色按 Fire > Poison > Ice 优先级显示橙红（毒绿被覆盖）
24. 减速上限验证：地刺 + Poison 同时作用时，敌人移速最多降到 20%（80% 减速上限），不会完全卡死
25. DoT 击杀验证：用狂暴火攻把敌人打到低血量后停止攻击，让燃烧 DoT 完成击杀，确认仍正常掉落经验球/金币/装备（OnKill 回调通过 Game.cpp 步骤 15.5 死亡检测触发）
26. 飘字不刷屏验证：多个敌人同时燃烧时，飘字数量在合理范围（每敌每 0.5s 一个，0.8s 后自动消失），不会刷屏卡顿

### 优先级 1：验证第十八轮连击系统改动
27. 连续击杀 5 个敌人后，屏幕中央上方（y=180）出现"连击 5"白色大字，下方显示"继续连击解锁加成"灰色提示，下方进度条（120x4px）显示 3 秒倒计时
28. 继续击杀累积至 10 连击，"连击 10"颜色变为黄色，下方显示"伤害 +20%"亮金色字；每次新击杀时数字明显放大后回弹（脉冲动画）
29. 累积至 25 连击，颜色变为橙色，"伤害 +35%"；50 连击变红色"+50%"；100 连击变紫色"+75%"（最高加成）
30. 停止击杀 3 秒后，连击数字消失（comboTimer 归零，comboCount 清空）；重新击杀时从 1 开始累积
31. 受伤重置验证：累积一定 combo 后被敌人击中受伤，连击立即归零消失（红色飘字+受伤音效同时触发）；近战接触伤害、自爆伤害、地裂区域伤害均触发重置
32. 跨层重置验证：进入下一层后当前 combo 清零，但 comboMaxThisLife 跨层保留（可通过 F5 调试面板查看；未来成就系统可读取此字段）
33. 加成生效验证：高 combo（如 50 连击 +50%）下，普攻子弹/AOE/技能伤害明显提升（建议对比 1 连击和 50 连击打同种敌人的伤害飘字差距）
34. DoT 不受加成验证：combo 高时观察 Fire/Poison 周期伤害飘字数值，应与 combo 无关（保持原 20%/10% 原始伤害比例），避免 DoT 失控
35. 圣物叠加验证：拥有"战士之证 +15%"圣物 + 50 连击 +50%，总伤害应为 base × 1.15 × 1.5 = 1.725x（乘法累乘）
