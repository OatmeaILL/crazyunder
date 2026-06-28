#pragma once

// ============================================================================
// UIManager —— UI 框架：层级管理与基础控件（Phase 8）
// ----------------------------------------------------------------------------
// 职责：
//   1. UIElement 基类：所有 UI 控件的基类，提供位置/尺寸/锚点/可见性/启用状态
//      与子元素树形结构。派生类重写 Update/Render 实现具体控件。
//   2. Button：按钮控件，支持文本、背景色、悬停/按下状态、点击回调。
//   3. Panel：面板控件，带背景色与边框，可包含子元素。
//   4. Text：文本控件，封装 sf::Text，支持字体/大小/颜色/对齐。
//   5. ProgressBar：进度条控件，支持当前值/最大值/渐变填充（低血量变红）。
//   6. UIManager：层级管理器，按 5 个层级（Background/HUD/Menu/Popup/Cursor）
//      管理所有 UIElement，负责 Update/Render/鼠标事件分发。
//
// 层级设计：
//   Background（背景层）< HUD（抬头显示）< Menu（菜单）< Popup（弹窗）< Cursor（光标）
//   渲染顺序从低到高，高层覆盖低层。
//   鼠标事件从高到低分发，第一个命中的元素消费事件。
//
// 锚点（Anchor）：
//   TopLeft：左上角对齐（position 即左上角坐标）
//   Center：中心对齐（position 即中心坐标）
//   BottomRight：右下角对齐（position 即右下角坐标）
//   锚点影响命中测试与渲染位置的计算。
// ============================================================================

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include <array>
#include <functional>
#include <string>

namespace cu {

class Input; // 前向声明

// ---- UI 层级枚举 ----
enum class UILayer : uint8_t {
    Background = 0, // 背景层（如菜单背景）
    HUD        = 1, // 抬头显示层（血条/小地图等）
    Menu       = 2, // 菜单层（主菜单/暂停菜单等）
    Popup      = 3, // 弹窗层（升级选择/背包等）
    Cursor     = 4, // 光标层（最顶层）
    Count      = 5  // 层级总数（哨兵）
};

// ---- 锚点枚举 ----
enum class UIAnchor : uint8_t {
    TopLeft     = 0, // 左上角对齐
    Center      = 1, // 中心对齐
    BottomRight = 2  // 右下角对齐
};

// ============================================================================
// UIElement —— UI 控件基类
// ============================================================================
class UIElement {
public:
    UIElement() = default;
    virtual ~UIElement() = default;

    // ---- 位置与尺寸 ----
    void SetPosition(sf::Vector2f pos) noexcept { position_ = pos; }
    [[nodiscard]] sf::Vector2f GetPosition() const noexcept { return position_; }

    void SetSize(sf::Vector2f size) noexcept { size_ = size; }
    [[nodiscard]] sf::Vector2f GetSize() const noexcept { return size_; }

    void SetAnchor(UIAnchor anchor) noexcept { anchor_ = anchor; }
    [[nodiscard]] UIAnchor GetAnchor() const noexcept { return anchor_; }

    // ---- 状态 ----
    void SetVisible(bool visible) noexcept { visible_ = visible; }
    [[nodiscard]] bool IsVisible() const noexcept { return visible_; }

    void SetEnabled(bool enabled) noexcept { enabled_ = enabled; }
    [[nodiscard]] bool IsEnabled() const noexcept { return enabled_; }

    // ---- 子元素管理 ----
    void AddChild(std::unique_ptr<UIElement> child) {
        if (child) children_.push_back(std::move(child));
    }

    template <typename T, typename... Args>
    T* CreateChild(Args&&... args) {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = child.get();
        children_.push_back(std::move(child));
        return ptr;
    }

    // 获取子元素列表（只读）
    [[nodiscard]] const std::vector<std::unique_ptr<UIElement>>& GetChildren() const noexcept {
        return children_;
    }

    // ---- 更新与渲染 ----
    // dt: 帧间隔时间（秒）
    virtual void Update(float dt);

    // target: 渲染目标（窗口或纹理）
    virtual void Render(sf::RenderTarget& target) const;

    // ---- 鼠标交互 ----
    // 鼠标进入元素区域（仅边沿触发）
    virtual void OnMouseEnter() {}
    // 鼠标离开元素区域（仅边沿触发）
    virtual void OnMouseLeave() {}
    // 鼠标在元素上点击（左键按下）
    // 返回 true 表示消费事件，阻止向低层元素传播
    virtual bool OnClick(sf::Vector2f mousePos) { (void)mousePos; return false; }
    // 鼠标悬停在元素上（每帧调用）
    virtual void OnHover(sf::Vector2f mousePos) { (void)mousePos; }

    // ---- 命中测试 ----
    // 检查鼠标坐标是否在元素区域内（考虑锚点）
    [[nodiscard]] virtual bool Contains(sf::Vector2f point) const noexcept;

    // 获取元素的实际边界矩形（考虑锚点转换后的左上角与尺寸）
    [[nodiscard]] sf::FloatRect GetBounds() const noexcept;

protected:
    sf::Vector2f position_{0.f, 0.f}; // 位置（语义取决于锚点）
    sf::Vector2f size_{0.f, 0.f};     // 尺寸（宽高）
    UIAnchor anchor_ = UIAnchor::TopLeft;
    bool visible_ = true;
    bool enabled_ = true;
    bool hovered_ = false; // 当前是否被悬停

    std::vector<std::unique_ptr<UIElement>> children_; // 子元素列表
};

// ============================================================================
// Button —— 按钮控件
// ============================================================================
class Button : public UIElement {
public:
    Button();

    // 设置按钮文本
    void SetText(const std::string& text);
    [[nodiscard]] const std::string& GetText() const noexcept { return text_; }

    // 设置字体
    void SetFont(const sf::Font& font);

    // 设置颜色
    void SetBackgroundColor(sf::Color color) noexcept { bgColor_ = color; }
    void SetHoverColor(sf::Color color) noexcept { hoverColor_ = color; }
    void SetPressedColor(sf::Color color) noexcept { pressedColor_ = color; }
    void SetTextColor(sf::Color color) noexcept { textColor_ = color; }

    // 设置点击回调
    void SetOnClick(std::function<void()> callback) { onClick_ = std::move(callback); }

    // 重写交互
    void OnMouseEnter() override { hovered_ = true; }
    void OnMouseLeave() override { hovered_ = false; pressed_ = false; }
    bool OnClick(sf::Vector2f mousePos) override;
    void OnHover(sf::Vector2f mousePos) override;

    // 重写渲染
    void Render(sf::RenderTarget& target) const override;

private:
    std::string text_;
    const sf::Font* font_ = nullptr;
    unsigned int characterSize_ = 24u;

    sf::Color bgColor_{60, 80, 120};       // 默认背景色
    sf::Color hoverColor_{90, 120, 180};   // 悬停色
    sf::Color pressedColor_{40, 60, 100};  // 按下色
    sf::Color textColor_{sf::Color::White};

    bool pressed_ = false;
    std::function<void()> onClick_;
};

// ============================================================================
// Panel —— 面板控件
// ============================================================================
class Panel : public UIElement {
public:
    Panel();

    void SetBackgroundColor(sf::Color color) noexcept { bgColor_ = color; }
    void SetBorderColor(sf::Color color) noexcept { borderColor_ = color; }
    void SetBorderWidth(float width) noexcept { borderWidth_ = width; }

    void Render(sf::RenderTarget& target) const override;

private:
    sf::Color bgColor_{30, 30, 40, 200};     // 半透明深色背景
    sf::Color borderColor_{100, 100, 120};    // 边框色
    float borderWidth_ = 2.f;                  // 边框宽度
};

// ============================================================================
// Text —— 文本控件
// ============================================================================
class Text : public UIElement {
public:
    Text();

    void SetFont(const sf::Font& font);
    void SetString(const std::string& str);
    void SetCharacterSize(unsigned int size);
    void SetFillColor(sf::Color color);
    void SetOutlineColor(sf::Color color);
    void SetOutlineThickness(float thickness);
    void SetStyle(sf::Uint32 style);

    // 对齐方式（影响文本相对 position 的偏移）
    enum class Align { Left, Center, Right };
    void SetAlign(Align align) noexcept { align_ = align; }

    [[nodiscard]] const sf::Text& GetText() const noexcept { return text_; }

    void Render(sf::RenderTarget& target) const override;

private:
    sf::Text text_;
    Align align_ = Align::Left;
};

// ============================================================================
// ProgressBar —— 进度条控件
// ============================================================================
class ProgressBar : public UIElement {
public:
    ProgressBar();

    // 设置值
    void SetValue(float value) noexcept;
    void SetMaxValue(float max) noexcept { maxValue_ = max; }
    [[nodiscard]] float GetValue() const noexcept { return value_; }
    [[nodiscard]] float GetMaxValue() const noexcept { return maxValue_; }
    [[nodiscard]] float GetProgress() const noexcept; // 0~1

    // 设置颜色
    void SetBackgroundColor(sf::Color color) noexcept { bgColor_ = color; }
    void SetFillColor(sf::Color color) noexcept { fillColor_ = color; }
    // 启用渐变填充（低值时变红）
    void SetGradientFill(bool enable) noexcept { gradientFill_ = enable; }

    void Render(sf::RenderTarget& target) const override;

private:
    float value_ = 0.f;
    float maxValue_ = 100.f;
    sf::Color bgColor_{40, 40, 40, 200};
    sf::Color fillColor_{0, 200, 0}; // 默认绿色
    bool gradientFill_ = false;       // 渐变填充开关
};

// ============================================================================
// UIManager —— UI 层级管理器
// ============================================================================
class UIManager {
public:
    UIManager();

    // 添加元素到指定层级
    // 返回指向元素的原始指针（调用者可保存用于后续操作，元素所有权归 UIManager）
    UIElement* AddElement(std::unique_ptr<UIElement> element, UILayer layer);

    // 模板化添加：创建元素并添加到指定层级
    template <typename T, typename... Args>
    T* CreateElement(UILayer layer, Args&&... args) {
        auto elem = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = elem.get();
        AddElement(std::move(elem), layer);
        return ptr;
    }

    // 清空指定层级
    void ClearLayer(UILayer layer) noexcept;

    // 清空所有层级
    void ClearAll() noexcept;

    // 每帧更新：更新所有可见元素
    void Update(float dt);

    // 渲染：按层级顺序渲染所有可见元素
    void Render(sf::RenderTarget& target) const;

    // 鼠标事件分发：从最高层到最低层，第一个命中的元素消费事件
    // mousePos: 鼠标屏幕坐标
    // mousePressed: 鼠标左键是否在本帧刚按下（边沿触发）
    // 返回 true 表示有 UI 元素消费了事件
    bool HandleMouseEvent(sf::Vector2f mousePos, bool mousePressed);

private:
    // 5 个层级的元素列表
    std::array<std::vector<std::unique_ptr<UIElement>>, static_cast<size_t>(UILayer::Count)> layers_;

    // 在指定层级从上到下查找命中的元素
    // 返回命中的元素指针，未命中返回 nullptr
    UIElement* pickElement(sf::Vector2f mousePos) const;

    // 递归处理元素的悬停/点击状态
    // 返回 true 表示事件被消费
    bool dispatchToElement(UIElement* element, sf::Vector2f mousePos, bool mousePressed);

    // 递归查找子元素中命中的最顶层元素
    UIElement* pickChild(UIElement* parent, sf::Vector2f mousePos) const;
};

} // namespace cu
