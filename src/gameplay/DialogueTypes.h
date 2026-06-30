#pragma once

// ============================================================================
// DialogueTypes —— 对话系统核心类型定义（纯数据头文件驱动）
// ----------------------------------------------------------------------------
// 设计原则：
//   1. 所有类型为 POD 或 constexpr 兼容，零运行时解析开销。
//   2. 对话树完全由数据描述，引擎只负责"解释执行"。
//   3. 动作 ID 与引擎实现解耦，新增动作仅需在 DialogueSystem 中注册映射。
// ============================================================================

#include <cstdint>

namespace cu {

// ---- 对话节点类型 ----
enum class DialogueNodeType : uint8_t {
    Text,   // 纯文本节点（说话者 + 内容 + 下一节点 ID）
    Choice, // 选择节点（提示文本 + 选项列表）
    Action, // 动作节点（执行某操作后自动跳转下一节点）
    Branch, // 条件分支节点（根据条件跳转到不同节点）
    End     // 结束节点（关闭对话框）
};

// ---- 对话动作类型 ----
// 动作节点执行的操作。新增动作只需在此枚举中添加，并在 DialogueSystem 中注册实现。
enum class DialogueAction : uint8_t {
    None         = 0,  // 无操作
    GiveGold     = 1,  // 给予金币（参数：数量）
    TakeGold     = 2,  // 扣除金币（参数：数量）
    GiveExp      = 3,  // 给予经验（参数：数量）
    HealPlayer   = 4,  // 回复生命（参数：百分比，0-100）
    GiveItem     = 5,  // 给予装备（参数：品质等级 1-4）
    GiveSkill    = 6,  // 给予技能（参数：SkillType 枚举值）
    ApplyCurse   = 7,  // 施加诅咒
    RemoveCurse  = 8,  // 解除诅咒
    GrantLevels  = 9,  // 提升等级（参数：数量）
    MarkEventDone   = 10, // 标记事件已完成（参数：事件ID）
    SacrificeHP     = 11, // 献祭生命（参数：百分比 0-100，扣除当前HP的百分比）
    GiveRandomItem  = 12, // 给予随机品质装备（参数：品质上限 1-4）
    OpenQuestMenu   = 13, // 打开任务栏（参数：无）
};

// ---- 分支条件类型 ----
// Branch 节点使用的条件，引擎根据游戏状态求值后选择跳转分支。
enum class BranchCondition : uint8_t {
    None          = 0,  // 无条件（即走默认分支）
    HasGold       = 1,  // 金币 >= 参数值
    HasItem       = 2,  // 背包中有某品质装备
    HasRelic      = 3,  // 拥有某圣物（参数：RelicType 枚举值）
    HasSkill      = 4,  // 拥有某技能（参数：SkillType 枚举值）
    HPBelow       = 5,  // 生命值低于参数百分比（0-100）
    IsCursed      = 6,  // 是否处于诅咒状态
    QuestCompleted = 7, // 任务已完成（参数：任务ID）
    ComboAbove    = 8,  // 连击数 >= 参数值
};

// ---- 对话选项（用于 Choice 节点）----
// 一个选项包含显示文本与选中后跳转的目标节点 ID。
struct DialogueOption {
    const char* text;     // 选项显示文本（UTF-8）
    int nextNodeId;       // 选中后跳转的节点 ID（-1 = 结束对话）
};

// ---- 对话节点（核心数据结构）----
// 所有字段均为编译期常量兼容类型。整个对话树是 DialogueNode 的数组。
struct DialogueNode {
    int id;                          // 节点唯一 ID
    DialogueNodeType type;           // 节点类型
    const char* speaker;             // 说话者名称（nullptr = 无说话者，系统提示）
    const char* text;                // 文本内容（对话文本 / 选择提示 / 系统提示）
    int nextNodeId;                  // 下一节点 ID（Text/Action 节点用，-1=结束）
    DialogueAction action;           // 动作类型（仅 Action 节点有效）
    int actionParam;                 // 动作参数（语义因动作类型而异）
    BranchCondition branchCond;      // 分支条件（仅 Branch 节点有效）
    int branchParam;                 // 分支参数
    int branchTrueId;                // 条件为真时跳转节点 ID
    int branchFalseId;               // 条件为假时跳转节点 ID
    const DialogueOption* options;   // 选项列表（仅 Choice 节点有效，nullptr 结尾）
    int optionCount;                 // 选项数量
};

// ============================================================================
// 对话树（数据层）
// ============================================================================
// 一个对话树 = 节点数组 + 节点数量。由 DialogueData_*.h 数据文件定义。
// 每个 NPC/事件 引用一个 DialogueTree 实例。
// ============================================================================
struct DialogueTree {
    const DialogueNode* nodes;  // 节点数组指针
    int nodeCount;              // 节点数量
    // 按 ID 查找节点（内部使用，O(n)线性查找，节点数 < 20 时足够快）
    [[nodiscard]] const DialogueNode* FindNode(int id) const noexcept {
        for (int i = 0; i < nodeCount; ++i) {
            if (nodes[i].id == id) return &nodes[i];
        }
        return nullptr;
    }
};

} // namespace cu