#include "ui/DialogueBoxUI.h"
#include "utils/Logger.h"
#include <algorithm>
#include <cmath>

namespace cu {

DialogueBoxUI::DialogueBoxUI() {
    // 底部居中锚点
    anchor_ = UIAnchor::Center;
    // position 设为屏幕底部中间
    position_ = sf::Vector2f(640.f, 720.f - kPanelHeight * 0.5f - 10.f);
    size_ = sf::Vector2f(kPanelWidth, kPanelHeight);
    for (auto& b : choiceBounds_) b = sf::FloatRect();
}

void DialogueBoxUI::Initialize(const sf::Font& font) {
    font_ = &font;
    // 预计算面板位置（底部居中）
    panelX_ = (1280.f - kPanelWidth) * 0.5f;
    panelY_ = 720.f - kPanelHeight - 10.f;
}

void DialogueBoxUI::SetContent(const DialogueState& state) {
    speakerName_ = state.speakerName;
    displayedText_ = state.displayedText;
    fullText_ = state.fullText;
    showChoices_ = state.showChoices;
    showNextIndicator_ = state.showNextIndicator;

    choiceTexts_.clear();
    if (state.showChoices) {
        choiceTexts_ = state.choiceTexts;
    }
    choiceCount_ = static_cast<int>(choiceTexts_.size());
    if (choiceCount_ > kMaxChoices) choiceCount_ = kMaxChoices;
}

void DialogueBoxUI::Update(float dt) {
    UIElement::Update(dt);
    blinkTimer_ += dt;
    if (blinkTimer_ > 10.f) blinkTimer_ = 0.f;
}

void DialogueBoxUI::UpdateHover(sf::Vector2f mousePos) {
    hoveredChoice_ = -1;
    if (!showChoices_) return;
    for (int i = 0; i < choiceCount_; ++i) {
        if (choiceBounds_[i].contains(mousePos)) {
            hoveredChoice_ = i;
            return;
        }
    }
}

int DialogueBoxUI::HandleClick(sf::Vector2f mousePos) const {
    if (!showChoices_) return -1;
    for (int i = 0; i < choiceCount_; ++i) {
        if (choiceBounds_[i].contains(mousePos)) {
            return i;
        }
    }
    return -1;
}

void DialogueBoxUI::drawWrappedText(sf::RenderTarget& target, const std::string& text,
                                     sf::Vector2f pos, float maxWidth,
                                     unsigned int charSize, sf::Color color) const {
    if (!font_ || text.empty()) return;

    sf::Text measure;
    measure.setFont(*font_);
    measure.setCharacterSize(charSize);

    sf::Text drawText;
    drawText.setFont(*font_);
    drawText.setCharacterSize(charSize);
    drawText.setFillColor(color);

    float lineHeight = static_cast<float>(charSize) * 1.4f;
    std::string currentLine;
    float y = pos.y;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\n') {
            drawText.setString(utf8ToSfString(currentLine));
            drawText.setPosition(pos.x, y);
            target.draw(drawText);
            y += lineHeight;
            currentLine.clear();
            continue;
        }
        std::string trial = currentLine + c;
        measure.setString(utf8ToSfString(trial));
        float w = measure.getLocalBounds().width;
        if (w > maxWidth && !currentLine.empty()) {
            drawText.setString(utf8ToSfString(currentLine));
            drawText.setPosition(pos.x, y);
            target.draw(drawText);
            y += lineHeight;
            currentLine.clear();
            currentLine += c;
        } else {
            currentLine = trial;
        }
    }
    if (!currentLine.empty()) {
        drawText.setString(utf8ToSfString(currentLine));
        drawText.setPosition(pos.x, y);
        target.draw(drawText);
    }
}

void DialogueBoxUI::Render(sf::RenderTarget& target) const {
    if (!visible_ || !font_) return;

    // ---- 面板背景 ----
    sf::RectangleShape panelBg(sf::Vector2f(kPanelWidth, kPanelHeight));
    panelBg.setPosition(panelX_, panelY_);
    panelBg.setFillColor(sf::Color(15, 15, 25, 235));
    panelBg.setOutlineColor(sf::Color(180, 140, 80, 200));
    panelBg.setOutlineThickness(2.f);
    target.draw(panelBg);

    // ---- 顶部装饰线 ----
    sf::RectangleShape topLine(sf::Vector2f(kPanelWidth - 20.f, 2.f));
    topLine.setPosition(panelX_ + 10.f, panelY_ + 4.f);
    topLine.setFillColor(sf::Color(180, 140, 80, 120));
    target.draw(topLine);

    // ---- 说话者名称 ----
    if (!speakerName_.empty()) {
        sf::Text nameText;
        nameText.setFont(*font_);
        nameText.setString(utf8ToSfString(speakerName_));
        nameText.setCharacterSize(20);
        nameText.setFillColor(sf::Color(255, 200, 100));
        nameText.setStyle(sf::Text::Bold);
        nameText.setPosition(panelX_ + 20.f, panelY_ + 14.f);
        target.draw(nameText);

        // 名称下方分隔线
        sf::RectangleShape nameSep(sf::Vector2f(kPanelWidth - 40.f, 1.f));
        nameSep.setPosition(panelX_ + 20.f, panelY_ + 42.f);
        nameSep.setFillColor(sf::Color(180, 140, 80, 80));
        target.draw(nameSep);
    }

    // ---- 对话内容（打字机效果）----
    if (!displayedText_.empty()) {
        float textStartY = speakerName_.empty() ? panelY_ + 20.f : panelY_ + 52.f;
        drawWrappedText(target, displayedText_,
                        sf::Vector2f(panelX_ + 24.f, textStartY),
                        kPanelWidth - 48.f, 16, sf::Color(230, 230, 240));
    }

    // ---- "继续"闪烁指示器（打字机完成后显示）----
    if (showNextIndicator_ && !showChoices_) {
        float blink = 0.5f + 0.5f * std::sin(blinkTimer_ * 5.f);
        uint8_t alpha = static_cast<uint8_t>(120 + 135 * blink);

        sf::Text indicator;
        indicator.setFont(*font_);
        indicator.setString(utf8ToSfString("\xE2\x96\xBC")); // ▼
        indicator.setCharacterSize(18);
        indicator.setFillColor(sf::Color(255, 220, 100, alpha));
        indicator.setPosition(panelX_ + kPanelWidth - 40.f, panelY_ + kPanelHeight - 28.f);
        target.draw(indicator);

        // 提示文字
        sf::Text hint;
        hint.setFont(*font_);
        hint.setString(utf8ToSfString("按 E 继续"));
        hint.setCharacterSize(12);
        hint.setFillColor(sf::Color(200, 200, 200, alpha));
        hint.setPosition(panelX_ + kPanelWidth - 120.f, panelY_ + kPanelHeight - 24.f);
        target.draw(hint);
    }

    // ---- 选项列表（显示在对话面板上方，右对齐）----
    if (showChoices_ && choiceCount_ > 0) {
        float optW = 240.f;     // 选项按钮宽度
        float optH = 32.f;      // 选项按钮高度
        float optGap = 6.f;     // 选项间距
        float totalH = choiceCount_ * (optH + optGap) - optGap;
        // 起始 Y：面板上方留 10px 间距，选项整体从下往上排列
        float startY = panelY_ - 10.f - totalH;
        if (startY < 10.f) startY = 10.f; // 不超出屏幕上边界
        // X：右对齐，距面板右边缘 10px
        float optX = panelX_ + kPanelWidth - 10.f - optW;

        for (int i = 0; i < choiceCount_; ++i) {
            float optY = startY + i * (optH + optGap);
            sf::FloatRect bounds(optX, optY, optW, optH);
            choiceBounds_[i] = bounds;

            // 选项背景
            sf::RectangleShape optRect(sf::Vector2f(optW, optH));
            optRect.setPosition(optX, optY);

            if (i == hoveredChoice_) {
                optRect.setFillColor(sf::Color(80, 60, 30, 230));
                optRect.setOutlineColor(sf::Color(255, 200, 100));
                optRect.setOutlineThickness(2.f);
            } else {
                optRect.setFillColor(sf::Color(30, 30, 40, 210));
                optRect.setOutlineColor(sf::Color(120, 100, 70));
                optRect.setOutlineThickness(1.f);
            }
            target.draw(optRect);

            // 选项编号 + 文本
            std::string keyLabel = std::to_string(i + 1) + ". ";
            std::string fullLabel = keyLabel + choiceTexts_[i];

            sf::Text optText;
            optText.setFont(*font_);
            optText.setString(utf8ToSfString(fullLabel));
            optText.setCharacterSize(14);
            optText.setFillColor(i == hoveredChoice_ ? sf::Color(255, 220, 100) : sf::Color(220, 220, 220));
            optText.setStyle(i == hoveredChoice_ ? sf::Text::Bold : sf::Text::Regular);

            // 文本左对齐（留 10px 内边距）
            sf::FloatRect tb = optText.getLocalBounds();
            optText.setPosition(
                optX + 10.f,
                optY + (optH - tb.height) * 0.5f - tb.top);
            target.draw(optText);
        }
    }

    // 渲染子元素
    for (const auto& child : children_) {
        if (child) child->Render(target);
    }
}

} // namespace cu