#pragma once

// ============================================================================
// DialogueBoxUI —— 对话面板 UI 控件
// ----------------------------------------------------------------------------
// 布局：
//   底部居中面板（1024×180），Anchor 为 BottomCenter，基于 1280×720 逻辑分辨率。
//   子区域：
//     1. 说话者名称（左上角，金色，20px）
//     2. 对话内容（打字机效果，14px，自动换行）
//     3. 选项列表（底部横向排列，可选高亮）
//     4. "继续"指示器（右下角闪烁箭头，打字机完成后显示）
//
// 交互：
//   鼠标悬停选项 → 高亮该选项
//   鼠标点击选项 → 返回选中索引（由 Game 层调用 DialogueSystem::SelectChoice）
//   按交互键（E/Enter）→ 由 Game 层调用 DialogueSystem::Advance
//
// 使用方式：
//   1. 在 Game::initializeUI() 中调用 Initialize(font)
//   2. 每帧调用 SetContent(state) 同步对话状态
//   3. 每帧调用 Render(target) 绘制
//   4. HandleClick(mousePos) 检测选项点击
// ============================================================================

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <array>
#include "ui/UIManager.h"
#include "gameplay/DialogueSystem.h"

namespace cu {

class DialogueBoxUI : public UIElement {
public:
    DialogueBoxUI();

    void Initialize(const sf::Font& font);

    // 同步对话状态（每帧调用）
    void SetContent(const DialogueState& state);

    // 每帧更新（推进闪烁动画计时器）
    void Update(float dt) override;

    // 渲染
    void Render(sf::RenderTarget& target) const override;

    // 更新鼠标悬停状态
    void UpdateHover(sf::Vector2f mousePos);

    // 处理鼠标点击（返回选中的选项索引，-1=未命中选项）
    [[nodiscard]] int HandleClick(sf::Vector2f mousePos) const;

    // 是否有选项可见
    [[nodiscard]] bool HasChoices() const noexcept { return showChoices_; }

    // 面板高度（供外部计算偏移）
    static constexpr float kPanelWidth = 1024.f;
    static constexpr float kPanelHeight = 180.f;

private:
    const sf::Font* font_ = nullptr;

    // 内容数据
    std::string speakerName_;
    std::string displayedText_;
    std::string fullText_;
    bool showChoices_ = false;
    bool showNextIndicator_ = false;
    std::vector<std::string> choiceTexts_;

    // 面板位置（底部居中）
    float panelX_ = 0.f;
    float panelY_ = 0.f;

    // 选项边界（用于点击检测，最多 4 个选项）
    static constexpr int kMaxChoices = 4;
    mutable std::array<sf::FloatRect, kMaxChoices> choiceBounds_;
    int choiceCount_ = 0;

    // 悬停状态
    int hoveredChoice_ = -1;

    // 闪烁动画
    float blinkTimer_ = 0.f;

    // 自动换行辅助
    void drawWrappedText(sf::RenderTarget& target, const std::string& text,
                         sf::Vector2f pos, float maxWidth,
                         unsigned int charSize, sf::Color color) const;
};

} // namespace cu