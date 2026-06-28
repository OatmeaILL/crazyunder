#pragma once

// ============================================================================
// QuestMenu  任务面板 UI
// ----------------------------------------------------------------------------
// 布局：
//   左侧：任务列表面板（最多 10 个任务卡片，支持垂直滚动）
//     - 标题 + 状态标识（已解锁/已完成/已领取）
//     - 描述（自动换行）
//     - 进度条（当前/目标）
//     - 奖励摘要（图标+文字）
//     - "领取奖励"按钮（仅 Completed 状态显示）
//   右侧：剧情说明面板（任务背景故事）
//
// 交互：
//   按 Q 键打开/关闭面板
//   鼠标左键点击"领取奖励"按钮 → 领取对应任务奖励
//   鼠标滚轮 → 垂直滚动任务列表（任务超过 5 个时启用）
//   ESC 键关闭面板（与背包一致优先级）
// ============================================================================

#include <SFML/Graphics.hpp>
#include <array>
#include "ui/UIManager.h"
#include "gameplay/QuestSystem.h"

namespace cu {

class QuestMenu : public UIElement {
public:
    QuestMenu();

    void Initialize(const sf::Font& font);

    // 设置任务数据（每帧或打开时调用）
    void SetQuestData(const QuestSystem& questSystem);

    // 每帧更新（累加闪烁计时器）
    void Update(float dt) override;

    // 更新悬停状态（用于按钮高亮）
    void UpdateHover(sf::Vector2f mousePos);

    // 处理鼠标点击
    // 返回值：{操作码, 任务ID}
    //   操作码：0=无操作, 1=领取奖励
    //   任务ID：被点击的任务 ID（1-10）
    [[nodiscard]] std::pair<int, int> CheckClick(sf::Vector2f mousePos) const;

    // 第二十二轮新增：处理鼠标滚轮滚动（垂直滚动任务列表）
    // delta: 正值=向上滚，负值=向下滚
    void OnMouseWheel(float delta);

    void Render(sf::RenderTarget& target) const override;

private:
    const sf::Font* font_ = nullptr;

    // 任务卡片数据（第二十二轮：5 → 10，支持支线任务）
    struct QuestCardData {
        int id = 0;
        QuestState state = QuestState::Locked;
        std::string title;
        std::string description;
        int currentProgress = 0;
        int targetProgress = 1;
        QuestReward reward;
        // "领取奖励"按钮边界（仅 Completed 状态有效）
        sf::FloatRect claimBtnBounds;
    };
    std::array<QuestCardData, 10> cards_;

    // 当前悬停的领取按钮索引（-1=无悬停）
    int hoveredClaimBtn_ = -1;
    // 闪烁计时器（用于 Completed 卡片边框闪烁）
    float blinkTimer_ = 0.f;

    // ---- 第二十二轮新增：垂直滚动支持 ----
    // 当前滚动偏移（像素，向下滚为正）
    float scrollOffset_ = 0.f;
    // 最大可滚动距离（由 SetQuestData 根据任务数量计算）
    float maxScrollOffset_ = 0.f;
    // 单次滚轮滚动步长（像素）
    static constexpr float kScrollStep = 60.f;

    // ---- 渲染辅助 ----
    // 状态对应颜色
    [[nodiscard]] sf::Color getStateColor(QuestState state) const;
    // 状态对应文字
    [[nodiscard]] const char* getStateText(QuestState state) const;
    // 奖励文字摘要
    [[nodiscard]] std::string formatReward(const QuestReward& reward) const;
    // 自动换行文本
    void drawWrappedText(sf::RenderTarget& target, const std::string& text,
                         sf::Vector2f pos, float maxWidth, float lineSpacing,
                         unsigned int charSize, sf::Color color) const;
};

} // namespace cu
