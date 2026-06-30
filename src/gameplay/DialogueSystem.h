#pragma once

// ============================================================================
// DialogueSystem —— 对话引擎（解释执行层）
// ----------------------------------------------------------------------------
// 职责：
//   1. 管理对话树注册表，按 ID 查找对话树。
//   2. 管理当前对话状态：活跃节点、打字机进度、选项列表。
//   3. 执行节点遍历：Text → 下一节点 / Choice → 等待选择 / Action → 执行后跳转 /
//      Branch → 条件求值后跳转 / End → 关闭对话。
//   4. 动作分发：将 DialogueAction 映射到具体游戏操作（通过回调注入，避免循环依赖）。
//
// 使用方式：
//   1. 游戏启动时调用 RegisterDialogueTrees() 注册所有对话树。
//   2. 交互触发时调用 StartDialogue(treeId) 启动对话。
//   3. 每帧调用 Update(dt) 推进打字机效果。
//   4. 按交互键/点击选项时调用 Advance() / SelectChoice(index)。
//   5. 通过 SetActionHandler 注入动作回调（Game 层负责实现）。
// ============================================================================

#include <SFML/Graphics.hpp>
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>
#include "gameplay/DialogueTypes.h"

namespace cu {

// ---- 对话系统状态 ----
// 由 UI 层读取以渲染对话面板
struct DialogueState {
    bool active = false;              // 对话是否活跃
    std::string speakerName;          // 当前说话者（空 = 系统提示）
    std::string fullText;             // 当前节点完整文本
    std::string displayedText;        // 打字机效果当前已显示文本
    float typewriterProgress = 0.f;   // 打字机进度（0=刚开始, 1=完成）
    bool typewriterDone = false;      // 打字机是否完成
    bool showChoices = false;         // 是否显示选项列表
    std::vector<std::string> choiceTexts; // 选项文本
    bool showNextIndicator = false;   // 是否显示"点击继续"指示器
};

// ---- 动作处理器类型 ----
// 参数：action 类型 + 参数值，返回 true 表示动作执行成功
using DialogueActionHandler = std::function<bool(DialogueAction action, int param)>;

// ---- 分支条件求值器类型 ----
// 参数：条件类型 + 参数值，返回 true 表示条件成立
using DialogueConditionEvaluator = std::function<bool(BranchCondition cond, int param)>;

class DialogueSystem {
public:
    DialogueSystem() = default;

    // ---- 对话树注册 ----
    // 注册一个对话树，返回其 ID。通常在游戏初始化时批量调用。
    int RegisterTree(const DialogueTree& tree);
    // 按 ID 获取对话树（nullptr 若不存在）
    [[nodiscard]] const DialogueTree* GetTree(int treeId) const;

    // ---- 对话控制 ----
    // 启动指定对话树（从节点 1 开始）
    void StartDialogue(int treeId);
    // 关闭当前对话
    void EndDialogue();
    // 对话是否活跃
    [[nodiscard]] bool IsActive() const noexcept { return state_.active; }
    // 最后一次选择的选项目标节点 ID（-1=未选择或非 Choice 节点）
    [[nodiscard]] int GetLastChoiceNextNodeId() const noexcept { return lastChoiceNextNodeId_; }

    // ---- 对话推进 ----
    // 推进对话（按交互键时调用）：
    //   - 打字机未完成 → 立即完成打字机
    //   - Text 节点 + 打字机完成 → 跳转下一节点
    //   - 选项可见时 → 不处理（由 SelectChoice 处理）
    void Advance();
    // 选择选项（index 从 0 开始）
    void SelectChoice(int index);
    // 每帧更新（推进打字机效果）
    void Update(float dt);

    // ---- 获取当前状态 ----
    [[nodiscard]] const DialogueState& GetState() const noexcept { return state_; }

    // ---- 回调注入 ----
    // 设置动作处理器（Game 层注入，用于执行对话中的动作）
    void SetActionHandler(DialogueActionHandler handler) { actionHandler_ = std::move(handler); }
    // 设置条件求值器（Game 层注入，用于评估分支条件）
    void SetConditionEvaluator(DialogueConditionEvaluator eval) { conditionEval_ = std::move(eval); }

private:
    // 跳转到指定节点
    void goToNode(int nodeId);
    // 处理当前节点（Text/Choice/Action/Branch/End）
    void processCurrentNode();
    // 执行动作节点
    void executeAction(DialogueAction action, int param);
    // 评估分支条件
    [[nodiscard]] bool evaluateCondition(BranchCondition cond, int param) const;

    // 已注册的对话树
    std::vector<const DialogueTree*> trees_;
    // 当前对话树
    const DialogueTree* currentTree_ = nullptr;
    // 当前节点 ID
    int currentNodeId_ = -1;
    // 当前节点指针
    const DialogueNode* currentNode_ = nullptr;
    // 对话状态
    DialogueState state_;
    // 打字机速度（字符/秒）
    static constexpr float kTypewriterSpeed = 60.f;
    // 当前打字机索引（已显示字符数）
    int typewriterIndex_ = 0;
    // 打字机累计时间
    float typewriterAccum_ = 0.f;
    // 最后一次选择的选项目标节点 ID（-1=未选择）
    int lastChoiceNextNodeId_ = -1;

    // 回调
    DialogueActionHandler actionHandler_;
    DialogueConditionEvaluator conditionEval_;
};

} // namespace cu