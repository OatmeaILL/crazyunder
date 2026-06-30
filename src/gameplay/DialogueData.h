#pragma once

// ============================================================================
// DialogueData —— 对话数据文件（乞丐 / 神秘法师 / 商人）
// ----------------------------------------------------------------------------
// 本文件定义三个 NPC 的完整对话树。新增对话只需创建新文件，注册到 DialogueSystem。
// 注意：所有 const char* 为 UTF-8 编码字面量（MSVC /utf-8），UI 通过 utf8ToSfString() 转换。
// ============================================================================

#include "gameplay/DialogueTypes.h"

namespace cu {

// ============================================================================
// 1. 乞丐（Beggar）对话树 —— 8 节点
// ----------------------------------------------------------------------------
// 流程：开场白 → 金币检查 → 选择(施舍/拒绝) → 扣金币 → 给经验+回血 → 结束
// ============================================================================

static const DialogueOption kBeggarOptions[] = {
    { "给他 50 金币", 3 },
    { "转身离开",     4 },
    { nullptr,       -1 }
};

static const DialogueNode kBeggarNodes[] = {
    // ID 1: 开场白
    { 1, DialogueNodeType::Text, "流浪乞丐",
      "行行好吧，我已经三天没吃东西了...",
      2, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 2: 条件分支：金币>=50 走正常，否则走没钱路线
    { 2, DialogueNodeType::Branch, "流浪乞丐",
      "你能施舍一点金币给我吗？",
      -1, DialogueAction::None, 0,
      BranchCondition::HasGold, 50, 5, 6,
      nullptr, 0 },

    // ID 3: 施舍动作（扣 50 金币）
    { 3, DialogueNodeType::Action, nullptr,
      "你向乞丐递出了 50 金币。",
      7, DialogueAction::TakeGold, 50,
      BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 4: 拒绝 → 结束
    { 4, DialogueNodeType::Text, "流浪乞丐",
      "唉...愿主保佑你。",
      -1, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 5: 正常选择界面
    { 5, DialogueNodeType::Choice, "流浪乞丐",
      "你打算怎么做？",
      -1, DialogueAction::None, 0,
      BranchCondition::None, 0, -1, -1, kBeggarOptions, 2 },

    // ID 6: 没钱时的对话
    { 6, DialogueNodeType::Text, "流浪乞丐",
      "看你也不像有钱人的样子...算了。",
      -1, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 7: 给 100 经验
    { 7, DialogueNodeType::Action, nullptr,
      "乞丐感激地接过金币，为你祝福。",
      8, DialogueAction::GiveExp, 100,
      BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 8: 回复 20% HP
    { 8, DialogueNodeType::Action, nullptr,
      "你感到一股暖流涌入身体。",
      -1, DialogueAction::HealPlayer, 20,
      BranchCondition::None, 0, -1, -1, nullptr, 0 },
};

static const DialogueTree kBeggarDialogue{ kBeggarNodes, 8 };


// ============================================================================
// 2. 神秘法师（Mage）对话树 —— 7 节点
// ----------------------------------------------------------------------------
// 流程：开场白 → HP检查 → 选择(献祭/拒绝) → 扣HP → 给随机装备 → 结束
// 特殊：献祭 HP 使用 SacrificeHP 动作，给装备使用 GiveRandomItem 动作
// ============================================================================

static const DialogueOption kMageOptions[] = {
    { "献祭生命（-30% HP）", 4 },
    { "拒绝",               5 },
    { nullptr,             -1 }
};

static const DialogueNode kMageNodes[] = {
    // ID 1: 开场白
    { 1, DialogueNodeType::Text, "神秘法师",
      "旅行者...我能用一件宝物换取你的生命力。",
      2, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 2: 条件分支：HP 低于 30% 则拒绝
    { 2, DialogueNodeType::Branch, "神秘法师",
      "",
      -1, DialogueAction::None, 0,
      BranchCondition::HPBelow, 30, 6, 3,
      nullptr, 0 },

    // ID 3: 正常选择界面
    { 3, DialogueNodeType::Choice, "神秘法师",
      "你愿意献祭 30% 的生命力吗？",
      -1, DialogueAction::None, 0,
      BranchCondition::None, 0, -1, -1, kMageOptions, 2 },

    // ID 4: 献祭 HP（扣 30%）
    { 4, DialogueNodeType::Action, nullptr,
      "你感到生命力被抽走...",
      7, DialogueAction::SacrificeHP, 30,
      BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 5: 拒绝
    { 5, DialogueNodeType::Text, "神秘法师",
      "下次再来吧，门一直开着。",
      -1, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 6: HP 太低拒绝
    { 6, DialogueNodeType::Text, "神秘法师",
      "你的生命力太弱了，承受不住仪式的代价。",
      -1, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 7: 给随机装备（品质上限 3=暗金）
    { 7, DialogueNodeType::Action, nullptr,
      "神秘法师将一件宝物放在你面前。",
      -1, DialogueAction::GiveRandomItem, 2,
      BranchCondition::None, 0, -1, -1, nullptr, 0 },
};

static const DialogueTree kMageDialogue{ kMageNodes, 7 };


// ============================================================================
// 3. 商人（Merchant）对话树 —— 6 节点
// ----------------------------------------------------------------------------
// 流程：开场白 → 分支(贪婪之眼圣物检查) → 选择(交易/拒绝) → 结束
// 特殊：选择"交易"后结束对话，Game 层检测到商人对话结束则打开商人菜单
// ============================================================================

static const DialogueOption kMerchantOptions[] = {
    { "看看有什么好东西", 2 },
    { "没什么兴趣",       3 },
    { nullptr,            -1 }
};

static const DialogueNode kMerchantNpcNodes[] = {
    // ID 1: 开场白
    { 1, DialogueNodeType::Text, "神秘商人",
      "嘿嘿，旅行者，要不要看看我的珍藏？",
      4, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 2: 同意交易 → 对话结束，Game 层打开商人菜单
    { 2, DialogueNodeType::Text, "神秘商人",
      "好眼力！来看看吧。",
      -1, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 3: 拒绝
    { 3, DialogueNodeType::Text, "神秘商人",
      "下次再来吧，门一直开着。",
      -1, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 4: 条件分支：检查是否有贪婪之眼圣物（RelicType::GreedyEye = 6）
    { 4, DialogueNodeType::Branch, "神秘商人",
      "",
      5, DialogueAction::None, 0,
      BranchCondition::HasRelic, 6, 5, 6,
      nullptr, 0 },

    // ID 5: 有贪婪之眼 → 特殊对话 → 选择
    { 5, DialogueNodeType::Choice, "神秘商人",
      "啊，你身上有贪婪之眼的气息...我们是同类人。给你个折扣如何？",
      -1, DialogueAction::None, 0,
      BranchCondition::None, 0, -1, -1, kMerchantOptions, 2 },

    // ID 6: 无特殊圣物 → 普通对话 → 选择
    { 6, DialogueNodeType::Choice, "神秘商人",
      "怎么样，有兴趣吗？",
      -1, DialogueAction::None, 0,
      BranchCondition::None, 0, -1, -1, kMerchantOptions, 2 },
};

static const DialogueTree kMerchantDialogue{ kMerchantNpcNodes, 6 };


// ============================================================================
// 4. 教程对话树（首次游戏时显示）—— 5 节点
// ----------------------------------------------------------------------------
// 流程：欢迎 → 提示按键 → 选择(打开任务栏/知道了) → 打开任务栏 → 结束
// ============================================================================

static const DialogueOption kTutorialOptions[] = {
    { "打开任务栏", 3 },
    { "知道了",     4 },
    { nullptr,     -1 }
};

static const DialogueNode kTutorialNodes[] = {
    // ID 1: 欢迎
    { 1, DialogueNodeType::Text, "系统提示",
      "欢迎来到 CrazyUnder！这里有一些基本操作。",
      2, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 2: 操作提示
    { 2, DialogueNodeType::Text, "系统提示",
      "按 Q 键打开任务栏，查看当前任务。\n按 TAB 键查看成就，了解你的进度。",
      5, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 3: 打开任务栏（动作节点，执行 OpenQuestMenu）
    { 3, DialogueNodeType::Action, nullptr,
      "已打开任务栏，你可以在这里查看当前任务和进度。",
      -1, DialogueAction::OpenQuestMenu, 0,
      BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 4: 知道了（结束）
    { 4, DialogueNodeType::Text, "系统提示",
      "祝你好运，冒险者！",
      -1, DialogueAction::None, 0, BranchCondition::None, 0, -1, -1, nullptr, 0 },

    // ID 5: 选择界面
    { 5, DialogueNodeType::Choice, "系统提示",
      "你想现在查看任务栏吗？",
      -1, DialogueAction::None, 0,
      BranchCondition::None, 0, -1, -1, kTutorialOptions, 2 },
};

static const DialogueTree kTutorialDialogue{ kTutorialNodes, 5 };

} // namespace cu