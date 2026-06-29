# CrazyUnder 项目信息
> [INDEX] 项目概述、系统描述与技术细节

## 一、项目概述
> [INDEX] 技术栈、架构、性能目标、构建命令
**CrazyUnder** 是一款 2.5D 像素风 Roguelike 割草爽游。
- **技术栈**：C++17 + SFML 2.6+ + CMake
- **架构**：ECS + 固定步长（1/30s）+ 插值渲染
- **性能目标**：500+ 敌人、1000+ 子弹/粒子同屏
- **构建命令**：
  ```powershell
  & "E:\Programming\Visual Studio\IDE-\MSBuild\Current\Bin\MSBuild.exe" build\crazyunder.sln /p:Configuration=Release /m /verbosity:minimal
  ```
- **可执行文件**：`e:\Programming\xioamaipian\crazyunder\build\bin\Release\crazyunder.exe`
- **当前状态**：编译成功（2026-06-29）

## 二、文件结构
> [INDEX] src/core、src/ecs、src/gameplay、src/rendering、src/ui、src/utils
```
e:\Programming\xioamaipian\crazyunder\
├── CMakeLists.txt
├── PROJECT_STATUS.md
├── projectinfo.md
├── src/
│   ├── main.cpp
│   ├── core/                       # Game（8文件模块化）、Input、Time、ResourceManager、AudioManager
│   ├── ecs/                        # Entity、Component、Registry
│   ├── gameplay/                   # Player、EnemyAI、CombatSystem、SkillSystem、QuestSystem 等 23 个系统
│   ├── rendering/                  # Renderer、TileMap、ParticleSystem、Camera
│   ├── ui/                         # HUD、Menus、QuestMenu、AchievementMenu
│   └── utils/                      # Logger、ObjectPool、TextureGenerator
└── build/bin/Release/crazyunder.exe
```

## 三、已完成的系统
> [INDEX] 核心架构、中文显示、玩家操作、敌人系统、BOSS系统、技能系统、装备背包、商人系统、升级系统、任务系统、成就系统、存档系统

### 核心架构
- ECS 注册表（ComponentPool 稀疏集合）
- 固定步长游戏循环（1/30s 逻辑更新 + 插值渲染）
- 状态机：Menu / Playing / Paused / Dead / Victory

### 中文显示系统
- CMakeLists.txt：`/utf-8` 编译选项
- Logger.h：`U8(str)` 宏 + `utf8ToSfString()` 函数

### 玩家操作
> [INDEX] 按键列表
- **移动**：WASD
- **攻击**：鼠标左键
- **闪避**：鼠标右键
- **AOE**：空格键
- **交互**：E
- **技能**：1-4
- **背包**：G
- **技能升级**：J
- **任务面板**：Q
- **成就面板**：Tab
- **暂停**：P / ESC
- **调试面板**：F5

### 敌人系统
> [INDEX] 11种敌人类型、Champion精英强化
- 基础类型：Melee、Ranged、Suicide、Elite、Boss
- 新增类型：StealthMelee、CountdownSuicide、Splitter、Shielded、SniperRanged、Caster
- Champion 精英强化：8%概率，HP×3/伤害×1.5/速度×1.1/体型×1.5
- 敌人分层强化：HP+30%/伤害+18%/速度+4%/层

### BOSS系统
> [INDEX] BOSS机制
- 远程攻击（每2s扇形3发）
- 召唤小兵（每8s召唤2个）
- 冲撞地裂（每6s，地裂区域）
- 召唤精英（HP<50%触发）
- 旋转弹幕（每10s，螺旋紫色子弹）
- 爱心掉落（30%概率）

### 技能系统
- 5个技能：震地波/吸血打击/狂暴/引力井/地刺
- 4装备槽（按键1-4）+ 5格技能背包
- 技能点机制（升级累积，J键主动开启）

### 装备与背包系统
- 6装备槽（Weapon/Helmet/Chest/Boots/Ring/Amulet）+ 25格大背包
- 右键菜单（装备/丢弃），左键快速穿卸
- 4种套装（Warrior/Sage/Wind/Guardian）
- 装备品质：普通/稀有/史诗/传说

### 商人系统
- 第一层必然出现，后续层50%概率
- E键交易，售卖6件装备+2个技能
- 价格倍率：普通1.0x/稀有4.0x/史诗7.0x/传说16.0x

### 升级系统
- 17种升级（12属性 + 5技能）
- 技能点机制（升级累积，J键主动开启）

### 任务系统
> [INDEX] 任务列表
- 5主线任务（破晓之始/古老祭坛/地下奇人/百艺兼修/财富之力）
- 5支线任务（KillTarget/CollectItem/SurviveTime/ClearRooms）
- Q键打开任务面板，依赖解锁机制

### 成就系统
- Tab键打开成就面板，4类（战斗/探索/收集/特殊）
- 跨存档持久化（achievements.dat）

### 圣物系统
> [INDEX] 圣物相关
- 12种圣物（R键查看面板）
- Boss3选1获取
- 存档持久化

### 地牢与房间
- BSP树生成随机地牢，9种房间类型
- 事件房（乞丐/法师/宝箱怪/祭坛/锻造台）
- 诅咒房（锁门+debuff，击败精英解除）
- 阻碍房（石柱反弹子弹+木墙可破坏）

### HUD与小地图
- 血条/蓝条/经验条 + 放大版技能图标（48x48）
- 右下角小地图（200x150）：房间标记B/T/E/S/?、玩家位置、图例
- 商人$图标、宝箱房?/T标记

### 存档系统
- 3槽位手动存档 + 每层自动存档
- 内容：玩家属性/装备/技能/任务状态/层数

## 六、关键技术细节
> [INDEX] 中文显示、技能持久化、Champion实现、圣物实现

### 中文显示
- `U8(str)` 宏 + `utf8ToSfString()` 函数

### 技能持久化
- `nextLevel()` 中手动保存恢复 `skillSlots/skillBackpack`

### Champion 精英怪实现
- EnemyComponent 新增 `isChampion`，EnemySpawner 应用倍率
- Game.cpp 渲染金色血条

### 圣物系统实现
- 三重成长叠加顺序：基础 → 升级 → 装备 → 圣物
- 存档二进制格式：6字节连续数组

## 七、新会话建议
> [INDEX] 验证本轮改动、验证圣物系统、验证元素状态、验证连击系统
- 运行 Release 版本，确认无运行时缺失报错
- 验证基础功能：任务系统、背包、小地图、事件房、Champion、Boss
- 验证圣物系统：Boss 后 3 选 1、R 键面板、存档保留
- 验证元素状态：Fire/Ice/Poison 触发、叠加、DoT 击杀
- 验证连击系统：5/10/25/50/100 连击、受伤重置、跨层重置
