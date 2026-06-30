#include "gameplay/DialogueSystem.h"
#include "utils/Logger.h"
#include <algorithm>
#include <cstring>

namespace cu {

// ============================================================================
// 对话树注册
// ============================================================================

int DialogueSystem::RegisterTree(const DialogueTree& tree) {
    int id = static_cast<int>(trees_.size());
    trees_.push_back(&tree);
    // 注意：调用者必须保证 DialogueTree 对象的生命周期长于 DialogueSystem。
    // 当前实现中，所有对话树均为 static const 全局变量（DialogueData.h），满足此要求。
    LOG_INFO("对话树已注册: id=%d, 节点数=%d", id, tree.nodeCount);
    return id;
}

const DialogueTree* DialogueSystem::GetTree(int treeId) const {
    if (treeId < 0 || treeId >= static_cast<int>(trees_.size())) return nullptr;
    return trees_[treeId];
}

// ============================================================================
// 对话控制
// ============================================================================

void DialogueSystem::StartDialogue(int treeId) {
    const DialogueTree* tree = GetTree(treeId);
    if (!tree) {
        LOG_WARN("对话树 id=%d 不存在", treeId);
        return;
    }
    currentTree_ = tree;
    state_.active = true;
    state_.showChoices = false;
    state_.showNextIndicator = false;
    state_.displayedText.clear();
    state_.fullText.clear();
    state_.typewriterDone = false;
    state_.typewriterProgress = 0.f;
    typewriterIndex_ = 0;
    typewriterAccum_ = 0.f;
    lastChoiceNextNodeId_ = -1;

    // 从节点 1 开始
    goToNode(1);
    LOG_INFO("对话启动: treeId=%d", treeId);
}

void DialogueSystem::EndDialogue() {
    state_.active = false;
    state_.showChoices = false;
    state_.showNextIndicator = false;
    currentTree_ = nullptr;
    currentNodeId_ = -1;
    currentNode_ = nullptr;
    LOG_INFO("对话结束");
}

// ============================================================================
// 对话推进
// ============================================================================

void DialogueSystem::Advance() {
    if (!state_.active) return;

    // 如果打字机未完成，立即完成
    if (!state_.typewriterDone) {
        state_.displayedText = state_.fullText;
        state_.typewriterDone = true;
        state_.typewriterProgress = 1.f;
        typewriterIndex_ = static_cast<int>(state_.fullText.size());
        // 完成打字机后，显示"继续"指示器（Text 和 Action 节点）
        if (currentNode_ && (currentNode_->type == DialogueNodeType::Text ||
                             currentNode_->type == DialogueNodeType::Action)) {
            state_.showNextIndicator = true;
        }
        return;
    }

    // 打字机已完成 + 按 E：Text 和 Action 节点需要玩家主动推进
    if (currentNode_ && (currentNode_->type == DialogueNodeType::Text ||
                         currentNode_->type == DialogueNodeType::Action)) {
        state_.showNextIndicator = false;
        if (currentNode_->nextNodeId < 0) {
            EndDialogue();
        } else {
            goToNode(currentNode_->nextNodeId);
        }
    }
    // Choice/Branch/Action/End 节点不由 Advance 处理：
    //   Choice → 由 SelectChoice() 处理
    //   Branch → goToNode 时自动跳转
    //   Action → goToNode 时自动执行并跳转
    //   End    → goToNode 时自动结束
}

void DialogueSystem::SelectChoice(int index) {
    if (!state_.active || !state_.showChoices || !currentNode_) return;
    if (currentNode_->type != DialogueNodeType::Choice) return;
    if (index < 0 || index >= currentNode_->optionCount) return;
    if (!currentNode_->options) return;

    const DialogueOption& opt = currentNode_->options[index];
    if (opt.text == nullptr) return; // 哨兵

    LOG_INFO("对话选择: [%d] %s -> nodeId=%d", index, opt.text, opt.nextNodeId);
    state_.showChoices = false;
    state_.choiceTexts.clear();
    lastChoiceNextNodeId_ = opt.nextNodeId;

    if (opt.nextNodeId < 0) {
        EndDialogue();
    } else {
        goToNode(opt.nextNodeId);
    }
}

void DialogueSystem::Update(float dt) {
    if (!state_.active || state_.typewriterDone) return;
    if (state_.fullText.empty()) {
        state_.typewriterDone = true;
        state_.typewriterProgress = 1.f;
        return;
    }

    // 打字机效果：按字符显示
    int totalChars = static_cast<int>(state_.fullText.size());
    typewriterAccum_ += dt * kTypewriterSpeed;
    int targetIndex = std::min(static_cast<int>(typewriterAccum_), totalChars);

    if (targetIndex > typewriterIndex_) {
        typewriterIndex_ = targetIndex;
        state_.displayedText = state_.fullText.substr(0, typewriterIndex_);
        state_.typewriterProgress = static_cast<float>(typewriterIndex_) / static_cast<float>(totalChars);
    }

    if (typewriterIndex_ >= totalChars) {
        state_.typewriterDone = true;
        state_.typewriterProgress = 1.f;
        // Text 和 Action 节点显示"继续"指示器
        if (currentNode_ && (currentNode_->type == DialogueNodeType::Text ||
                             currentNode_->type == DialogueNodeType::Action)) {
            state_.showNextIndicator = true;
        }
    }
}

// ============================================================================
// 内部方法
// ============================================================================

void DialogueSystem::goToNode(int nodeId) {
    if (!currentTree_) return;

    const DialogueNode* node = currentTree_->FindNode(nodeId);
    if (!node) {
        LOG_WARN("对话节点 %d 不存在，结束对话", nodeId);
        EndDialogue();
        return;
    }

    currentNodeId_ = nodeId;
    currentNode_ = node;
    processCurrentNode();
}

void DialogueSystem::processCurrentNode() {
    if (!currentNode_) return;

    switch (currentNode_->type) {
        case DialogueNodeType::Text: {
            // 设置文本内容，启动打字机
            state_.speakerName = currentNode_->speaker ? currentNode_->speaker : "";
            state_.fullText = currentNode_->text ? currentNode_->text : "";
            state_.displayedText.clear();
            state_.typewriterDone = false;
            state_.typewriterProgress = 0.f;
            state_.showChoices = false;
            state_.showNextIndicator = false;
            typewriterIndex_ = 0;
            typewriterAccum_ = 0.f;
            // 如果文本为空，直接完成
            if (state_.fullText.empty()) {
                state_.typewriterDone = true;
                state_.typewriterProgress = 1.f;
            }
            break;
        }

        case DialogueNodeType::Choice: {
            // 显示选择界面
            state_.speakerName = currentNode_->speaker ? currentNode_->speaker : "";
            state_.fullText = currentNode_->text ? currentNode_->text : "";
            state_.displayedText = state_.fullText;
            state_.typewriterDone = true;
            state_.typewriterProgress = 1.f;
            state_.showNextIndicator = false;
            state_.showChoices = true;

            state_.choiceTexts.clear();
            if (currentNode_->options) {
                for (int i = 0; i < currentNode_->optionCount; ++i) {
                    if (currentNode_->options[i].text == nullptr) break;
                    state_.choiceTexts.push_back(currentNode_->options[i].text);
                }
            }
            break;
        }

        case DialogueNodeType::Action: {
            // 先执行动作
            executeAction(currentNode_->action, currentNode_->actionParam);
            // 在对话框中显示结果文本，等玩家按 E 后再跳转
            if (currentNode_->text && currentNode_->text[0] != '\0') {
                state_.speakerName = "";
                state_.fullText = currentNode_->text;
                state_.displayedText.clear();
                state_.typewriterDone = false;
                state_.typewriterProgress = 0.f;
                state_.showChoices = false;
                state_.showNextIndicator = false;
                typewriterIndex_ = 0;
                typewriterAccum_ = 0.f;
            } else {
                // 无文本则直接跳转
                if (currentNode_->nextNodeId >= 0) {
                    goToNode(currentNode_->nextNodeId);
                } else {
                    EndDialogue();
                }
            }
            break;
        }

        case DialogueNodeType::Branch: {
            // 条件求值，选择分支
            bool result = evaluateCondition(currentNode_->branchCond, currentNode_->branchParam);
            int targetId = result ? currentNode_->branchTrueId : currentNode_->branchFalseId;
            LOG_INFO("对话分支: cond=%d param=%d result=%d -> nodeId=%d",
                     static_cast<int>(currentNode_->branchCond), currentNode_->branchParam,
                     result ? 1 : 0, targetId);
            if (targetId >= 0) {
                goToNode(targetId);
            } else {
                EndDialogue();
            }
            break;
        }

        case DialogueNodeType::End:
        default: {
            EndDialogue();
            break;
        }
    }
}

void DialogueSystem::executeAction(DialogueAction action, int param) {
    if (actionHandler_) {
        bool ok = actionHandler_(action, param);
        if (!ok) {
            LOG_WARN("对话动作执行失败: action=%d param=%d",
                     static_cast<int>(action), param);
        }
    }
}

bool DialogueSystem::evaluateCondition(BranchCondition cond, int param) const {
    if (conditionEval_) {
        return conditionEval_(cond, param);
    }
    return false;
}

} // namespace cu