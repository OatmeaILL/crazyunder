# CrazyUnder 项目状态总结（2026-06-29 第三十三轮）
> [INDEX] 项目状态、第三十三轮详情、历史轮次索引、已知问题

---

## 四-u、第三十一轮新增/修复内容
> [INDEX] 帮助手册、死亡回顾、小地图标记、屏幕震动、阻碍房增强、物品叙事、动态事件、敌人 AI 改善、死亡回顾 bug 修复

### 设计意图
体验优化 + 敌人智能化。

### P1 功能
1. **帮助手册**（按 H 键）：Playing 状态可重复查看教程覆盖层
2. **死亡回顾系统**：击杀者中文名/连击中断/DPS
3. **小地图标记**：商人$图标、宝箱房?/T标记
4. **屏幕震动与顿帧**：暴击/击杀精英/BOSS/受伤触发
5. **阻碍房增强**：石柱反弹子弹（伤害衰减50%）
6. **物品叙事**：12圣物+5技能 lore 段
7. **动态事件**：事件房对话根据 build 变化
8. **敌人 AI 改善**：近战挤开、远程侧移、隐身伏击、自爆预判

### P0 修复
1. **死亡回顾数据预保存**：registry 清空前预存到成员变量
2. **死亡回顾 UI 重新布局**：左侧独立面板，分辨率适配
3. **死亡状态卡死**：ChangeState(Dead) 后立即 return

### 编译验证
- Release 版本编译成功（exit_code=0）

---

## 四-v、第三十二轮新增/修复内容（代码重构：Game.cpp 模块化拆分）

### 设计意图
`Game.cpp` 原为 4489 行单体文件，包含 51 个方法，严重臃肿难以维护。本轮在不改变任何功能的前提下，按职责将其拆分为 8 个模块文件，Game.h 无需任何修改。

### 拆分方案

| 文件 | 行数 | 职责 |
|------|------|------|
| `Game.cpp` | 638 | 核心：构造、Run、ChangeState、handleEvents、registerStates、initializeUI |
| `Game_Scene.cpp` | 783 | 场景：setupPlayingScene、loadPlayerSpriteSheet、nextLevel、restartGame、resetAllUIFlags |
| `Game_Update.cpp` | 682 | 更新：updatePlaying、updateHUDData、updateUI、updateFissureZones、recomputePlayerStats、updateDebugStats |
| `Game_Render.cpp` | 1076 | 渲染：renderPlaying、renderUI、renderDoorHealthBars、renderChampionHealthBars、renderBossHealthBar、renderTutorial、renderRelicPanel、renderFloorModifierBanner、renderFloorModifierHUD、renderEventHint、renderFissureZones |
| `Game_Interaction.cpp` | 904 | 交互：handleInteract、handleEventInteraction、handleUIInput、handleUpgradeChoice、handleRelicChoice、showUpgradeChoice |
| `Game_SaveLoad.cpp` | 252 | 存档：buildSaveData、applySaveData、showSaveLoadMenu、handleSaveLoadMenuClick、autoSaveCurrent、refreshSaveSlotInfo |
| `Game_Modifiers.cpp` | 65 | 变异：applyFloorModifiersToSubsystems、applyCurse、removeCurse |
| `Game_Debug.cpp` | 205 | 调试：teleportToRoom、killAllEnemies、addCoins、addExperience、clearScreen、handleSettingsMenuClick、handleSoulWellMenuClick、applySettings |

### 编译验证
- Release 版本增量编译成功（exit_code=0）
- 仅重新编译改动文件（Game.cpp + 7 个新模块）
- 无新增警告，仅历史遗留（C4244/C4819/C4996）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---

## 四-w、第三十三轮新增/修复内容（对话系统：NPC 接入 + 文本适配）

### 设计意图
将已存在的三个 NPC（事件房乞丐/神秘法师、商人）从硬编码 switch-case 改造为对话系统驱动，实现"数据逻辑彻底分离"：对话树定义在 .h 文件中，引擎解释执行。

### 新增对话树数据（`DialogueData.h`）
1. **流浪乞丐**（8 节点）：开场白 → Branch(金币≥50?) → Choice(施舍/拒绝) → Action(TakeGold 50) → Action(GiveExp 100) → Action(HealPlayer 20%) → End
2. **神秘法师**（7 节点）：开场白 → Branch(HP≥30%?) → Choice(献祭/拒绝) → Action(SacrificeHP 30%) → Action(GiveRandomItem) → End
3. **神秘商人**（6 节点）：开场白 → Branch(有贪婪之眼?) → Choice(交易/拒绝) → 对话结束后打开商人菜单

### 核心修改
| 文件 | 修改内容 |
|------|---------|
| `DialogueTypes.h` | 新增 `SacrificeHP`(献祭HP) 和 `GiveRandomItem`(随机装备) 动作 |
| `Game.h` | 新增 `dialogueTreeId_Mage_`、`pendingEventRoomIdx_`(事件收盘)、`pendingMerchantOpen_`(开后菜单) |
| `Game.cpp (registerDialogueCallbacks)` | 注册三棵对话树，新增 SacrificeHP/GiveRandomItem 动作处理 |
| `Game_Interaction.cpp (handleInteract)` | 事件房：Beggar/Mage 走对话树（非对话事件走原逻辑）；商人：走对话树，对话结束再开菜单 |
| `Game_Update.cpp` | 对话非活跃时检查 `pendingEventRoomIdx_`(标记事件触发/关闭提示) 和 `pendingMerchantOpen_`(刷新并打开商人菜单) |

### 对话流程
```
事件房(乞丐/法师)按E → startDialogue → 打字机 → 选择/推进 → Action执行 → dialogueEnd → pending收盘
商人在范围按E → startDialogue → 选择(交易/拒绝) → 对话结束 → pending→打开商人菜单
```

### 非对话事件
- ChestMimic、Altar、Forge 仍使用原有的硬编码 `handleEventInteraction()` 逻辑，未改为对话系统。

### 编译验证
- Release 版本编译成功（0 错误，1 个 C4244 历史警告）
- 可执行文件：`build/bin/Release/crazyunder.exe`

---
> [INDEX] 第三十三轮~第十轮

| 轮次 | 核心内容 |
|------|---------|
| 第三十三轮 | 对话系统：DialogueTypes/DialogueData/DialogueSystem/DialogueBoxUI + NPCComponent |
| 第三十二轮 | Game.cpp 模块化拆分（1→8文件） |
| 第三十一轮 | 帮助手册、死亡回顾、小地图标记、屏幕震动、阻碍房增强、物品叙事、动态事件、敌人 AI 改善 |
| 第三十轮 | Renderer 性能优化、ECS View() 零分配优化 |
| 第二十九轮 | 炸弹怪物重平衡 |
| 第二十八轮 | Caster 施法者、锻造房事件、ManaRegen/DefenseUp 升级 |
| 第二十七轮 | 背包系统 Bug 修复 + 词缀槽位加权 |
| 第二十六轮 | 数值平衡 + 法力消耗系统 |
| 第二十五轮 | UI 渲染/显示/交互 Bug 修复 |
| 第二十四轮 | 灵魂之忆系统（Meta Progression） |
| 第二十三轮 | 装备套装系统（4套：Warrior/Sage/Wind/Guardian） |
| 第二十二轮 | 激活所有 QuestType、新增 5 支线任务、任务面板滚动 |
| 第二十一轮 | 激活 EnemyAffix 精英词缀系统 |
| 第二十轮 | 极限闪避反击系统、防御圣物 |
| 第十九轮 | 激活 Lightning 元素、连锁闪电修复、雷霆圣物、雷暴领域变异 |
| 第十八轮 | 连击系统（伤害加成：+20%/+35%/+50%/+75%） |
| 第十七轮 | 地牢变异系统（11种双刃剑变异） |
| 第十六轮 | 激活 Fire/Ice/Poison 元素状态 |
| 第十五轮 | 圣物系统（8种→12种） |
| 第十四轮 | 成就解锁 Toast 通知 |
| 第十三轮 | 地刺减速修复、Boss 召唤物上限、SkillSystem static 修复 |
| 第十二轮 | OnKill 双重调用修复、Boss 旋转弹幕修复 |
| 第十一轮 | Champion 精英强化系统、任务系统、成就系统、事件房/诅咒房 |
| 第十轮 | 商人头顶文字、技能价格、震地波粒子、爱心系统 |

---

## 五、已知问题
> [INDEX] P0运行稳定性、P1功能完善

### P0 - 运行稳定性
- BOSS 生成机制需验证

### P1 - 功能完善
- 平衡性调整：敌人 HP/伤害、玩家升级曲线
- 音效补充：部分技能/机制音效缺失
- 技能获取渠道：目前仅升级和商人购买
- [已解决] NPC 对话：已在事件房（乞丐/法师）和商人中集成对话系统
- [已解决] 对话数据：乞丐/法师/商人已完成完整对话树
