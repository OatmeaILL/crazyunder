#include "ui/UIManager.h"
#include "core/Input.h"
#include "core/AudioManager.h"
#include "utils/Logger.h"
#include <algorithm>

namespace cu {

// ============================================================================
// UIElement 实现
// ============================================================================

void UIElement::Update(float dt) {
    if (!visible_) return;
    // 更新子元素
    for (auto& child : children_) {
        if (child) child->Update(dt);
    }
}

void UIElement::Render(sf::RenderTarget& target) const {
    if (!visible_) return;
    // 渲染自身（派生类重写 Render 时应调用基类或自行绘制）
    // 基类不绘制任何内容，仅渲染子元素
    for (const auto& child : children_) {
        if (child) child->Render(target);
    }
}

sf::FloatRect UIElement::GetBounds() const noexcept {
    // 根据锚点计算实际边界
    switch (anchor_) {
        case UIAnchor::TopLeft:
            return sf::FloatRect(position_.x, position_.y, size_.x, size_.y);
        case UIAnchor::Center:
            return sf::FloatRect(
                position_.x - size_.x * 0.5f,
                position_.y - size_.y * 0.5f,
                size_.x, size_.y);
        case UIAnchor::BottomRight:
            return sf::FloatRect(
                position_.x - size_.x,
                position_.y - size_.y,
                size_.x, size_.y);
    }
    return sf::FloatRect();
}

bool UIElement::Contains(sf::Vector2f point) const noexcept {
    return GetBounds().contains(point);
}

// ============================================================================
// Button 实现
// ============================================================================

Button::Button() {
    characterSize_ = 24u;
}

void Button::SetText(const std::string& text) {
    text_ = text;
}

void Button::SetFont(const sf::Font& font) {
    font_ = &font;
}

bool Button::OnClick(sf::Vector2f mousePos) {
    (void)mousePos;
    if (!enabled_) return false;
    pressed_ = true;
    // 统一播放按钮点击音效（kSFXPickup 实际加载的是 menu_click.mp3）
    AudioManager::Instance().PlaySFX(AudioManager::kSFXPickup);
    if (onClick_) {
        onClick_();
    }
    return true; // 消费事件
}

void Button::OnHover(sf::Vector2f mousePos) {
    (void)mousePos;
    // 悬停状态由 OnMouseEnter/OnMouseLeave 管理
}

void Button::Render(sf::RenderTarget& target) const {
    if (!visible_) return;

    sf::FloatRect bounds = GetBounds();

    // 绘制背景（圆角矩形简化为普通矩形 + 边框）
    sf::RectangleShape bg(sf::Vector2f(bounds.width, bounds.height));
    bg.setPosition(bounds.left, bounds.top);

    // 根据状态选择颜色
    sf::Color color = bgColor_;
    if (!enabled_) {
        color = sf::Color(bgColor_.r / 2, bgColor_.g / 2, bgColor_.b / 2, bgColor_.a);
    } else if (pressed_) {
        color = pressedColor_;
    } else if (hovered_) {
        color = hoverColor_;
    }
    bg.setFillColor(color);
    bg.setOutlineThickness(2.f);
    bg.setOutlineColor(sf::Color(20, 20, 30));
    target.draw(bg);

    // 绘制文本
    if (font_ && !text_.empty()) {
        sf::Text text;
        text.setFont(*font_);
        text.setString(utf8ToSfString(text_));
        text.setCharacterSize(characterSize_);
        text.setFillColor(textColor_);

        // 居中对齐文本
        sf::FloatRect textBounds = text.getLocalBounds();
        text.setPosition(
            bounds.left + (bounds.width - textBounds.width) * 0.5f,
            bounds.top + (bounds.height - textBounds.height) * 0.5f - textBounds.top);
        target.draw(text);
    }

    // 渲染子元素
    for (const auto& child : children_) {
        if (child) child->Render(target);
    }
}

// ============================================================================
// Panel 实现
// ============================================================================

Panel::Panel() {
    bgColor_ = sf::Color(30, 30, 40, 200);
    borderColor_ = sf::Color(100, 100, 120);
    borderWidth_ = 2.f;
}

void Panel::Render(sf::RenderTarget& target) const {
    if (!visible_) return;

    sf::FloatRect bounds = GetBounds();

    sf::RectangleShape bg(sf::Vector2f(bounds.width, bounds.height));
    bg.setPosition(bounds.left, bounds.top);
    bg.setFillColor(bgColor_);
    if (borderWidth_ > 0.f) {
        bg.setOutlineThickness(borderWidth_);
        bg.setOutlineColor(borderColor_);
    }
    target.draw(bg);

    // 渲染子元素
    for (const auto& child : children_) {
        if (child) child->Render(target);
    }
}

// ============================================================================
// Text 实现
// ============================================================================

Text::Text() {
    text_.setFillColor(sf::Color::White);
}

void Text::SetFont(const sf::Font& font) {
    text_.setFont(font);
}

void Text::SetString(const std::string& str) {
    text_.setString(utf8ToSfString(str));
}

void Text::SetCharacterSize(unsigned int size) {
    text_.setCharacterSize(size);
}

void Text::SetFillColor(sf::Color color) {
    text_.setFillColor(color);
}

void Text::SetOutlineColor(sf::Color color) {
    text_.setOutlineColor(color);
}

void Text::SetOutlineThickness(float thickness) {
    text_.setOutlineThickness(thickness);
}

void Text::SetStyle(sf::Uint32 style) {
    text_.setStyle(style);
}

void Text::Render(sf::RenderTarget& target) const {
    if (!visible_) return;

    // 根据对齐方式调整文本位置
    sf::Text text = text_; // 复制一份以调整位置
    sf::FloatRect textBounds = text.getLocalBounds();
    sf::FloatRect bounds = GetBounds();

    switch (align_) {
        case Align::Left:
            text.setPosition(bounds.left, bounds.top);
            break;
        case Align::Center:
            text.setPosition(
                bounds.left + (bounds.width - textBounds.width) * 0.5f,
                bounds.top + (bounds.height - textBounds.height) * 0.5f - textBounds.top);
            break;
        case Align::Right:
            text.setPosition(
                bounds.left + bounds.width - textBounds.width,
                bounds.top + (bounds.height - textBounds.height) * 0.5f - textBounds.top);
            break;
    }
    target.draw(text);

    // 渲染子元素
    for (const auto& child : children_) {
        if (child) child->Render(target);
    }
}

// ============================================================================
// ProgressBar 实现
// ============================================================================

ProgressBar::ProgressBar() {
    value_ = 0.f;
    maxValue_ = 100.f;
    bgColor_ = sf::Color(40, 40, 40, 200);
    fillColor_ = sf::Color(0, 200, 0);
    gradientFill_ = false;
}

void ProgressBar::SetValue(float value) noexcept {
    value_ = std::max(0.f, std::min(value, maxValue_));
}

float ProgressBar::GetProgress() const noexcept {
    if (maxValue_ <= 0.f) return 0.f;
    return value_ / maxValue_;
}

void ProgressBar::Render(sf::RenderTarget& target) const {
    if (!visible_) return;

    sf::FloatRect bounds = GetBounds();

    // 绘制背景
    sf::RectangleShape bg(sf::Vector2f(bounds.width, bounds.height));
    bg.setPosition(bounds.left, bounds.top);
    bg.setFillColor(bgColor_);
    bg.setOutlineThickness(1.f);
    bg.setOutlineColor(sf::Color(20, 20, 20));
    target.draw(bg);

    // 绘制填充
    float progress = GetProgress();
    if (progress > 0.f) {
        sf::Color fillColor = fillColor_;
        // 渐变填充：进度低于 30% 时变红
        if (gradientFill_ && progress < 0.3f) {
            // 从红色到 fillColor_ 的渐变
            float t = progress / 0.3f; // 0~1
            fillColor.r = static_cast<uint8_t>(200 + (fillColor_.r - 200) * t);
            fillColor.g = static_cast<uint8_t>(0 + (fillColor_.g - 0) * t);
            fillColor.b = static_cast<uint8_t>(0 + (fillColor_.b - 0) * t);
        }

        sf::RectangleShape fillRect(sf::Vector2f(bounds.width * progress, bounds.height));
        fillRect.setPosition(bounds.left, bounds.top);
        fillRect.setFillColor(fillColor);
        target.draw(fillRect);
    }

    // 渲染子元素
    for (const auto& child : children_) {
        if (child) child->Render(target);
    }
}

// ============================================================================
// UIManager 实现
// ============================================================================

UIManager::UIManager() {
    // 初始化所有层级
}

UIElement* UIManager::AddElement(std::unique_ptr<UIElement> element, UILayer layer) {
    if (!element) return nullptr;
    UIElement* ptr = element.get();
    auto& vec = layers_[static_cast<size_t>(layer)];
    vec.push_back(std::move(element));
    return ptr;
}

void UIManager::ClearLayer(UILayer layer) noexcept {
    auto& vec = layers_[static_cast<size_t>(layer)];
    vec.clear();
}

void UIManager::ClearAll() noexcept {
    for (auto& layer : layers_) {
        layer.clear();
    }
}

void UIManager::Update(float dt) {
    for (auto& layer : layers_) {
        for (auto& elem : layer) {
            if (elem) elem->Update(dt);
        }
    }
}

void UIManager::Render(sf::RenderTarget& target) const {
    // 按层级顺序渲染（从低到高，高层覆盖低层）
    for (const auto& layer : layers_) {
        for (const auto& elem : layer) {
            if (elem) elem->Render(target);
        }
    }
}

bool UIManager::HandleMouseEvent(sf::Vector2f mousePos, bool mousePressed) {
    // 从最高层到最低层查找命中的元素
    for (int layerIdx = static_cast<int>(UILayer::Count) - 1; layerIdx >= 0; --layerIdx) {
        auto& layer = layers_[layerIdx];
        // 从后向前遍历（后添加的元素在顶层）
        for (int i = static_cast<int>(layer.size()) - 1; i >= 0; --i) {
            UIElement* elem = layer[i].get();
            if (!elem || !elem->IsVisible()) continue;

            // 递归查找子元素中命中的最顶层元素
            UIElement* hit = pickChild(elem, mousePos);
            if (hit) {
                return dispatchToElement(hit, mousePos, mousePressed);
            }
            // 检查元素自身是否命中
            if (elem->Contains(mousePos)) {
                return dispatchToElement(elem, mousePos, mousePressed);
            }
        }
    }
    return false;
}

UIElement* UIManager::pickElement(sf::Vector2f mousePos) const {
    for (int layerIdx = static_cast<int>(UILayer::Count) - 1; layerIdx >= 0; --layerIdx) {
        const auto& layer = layers_[layerIdx];
        for (int i = static_cast<int>(layer.size()) - 1; i >= 0; --i) {
            UIElement* elem = layer[i].get();
            if (!elem || !elem->IsVisible()) continue;
            UIElement* hit = pickChild(elem, mousePos);
            if (hit) return hit;
            if (elem->Contains(mousePos)) return elem;
        }
    }
    return nullptr;
}

UIElement* UIManager::pickChild(UIElement* parent, sf::Vector2f mousePos) const {
    if (!parent || !parent->IsVisible()) return nullptr;

    // 从后向前遍历子元素（后添加的在顶层）
    const auto& children = parent->GetChildren();
    for (int i = static_cast<int>(children.size()) - 1; i >= 0; --i) {
        UIElement* child = children[i].get();
        if (!child || !child->IsVisible()) continue;
        // 递归查找子元素的子元素
        UIElement* hit = pickChild(child, mousePos);
        if (hit) return hit;
        if (child->Contains(mousePos)) return child;
    }
    return nullptr;
}

bool UIManager::dispatchToElement(UIElement* element, sf::Vector2f mousePos, bool mousePressed) {
    if (!element) return false;

    // 触发悬停回调
    element->OnHover(mousePos);

    // 如果鼠标按下，触发点击回调
    if (mousePressed) {
        return element->OnClick(mousePos);
    }

    return true; // 悬停也算消费事件
}

} // namespace cu
