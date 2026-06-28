#pragma once

// ============================================================================
// Input —— 输入管理器
// ----------------------------------------------------------------------------
// 职责：
//   封装 SFML 的事件驱动输入为查询接口，提供三类查询：
//     1. 状态查询（IsKeyDown / IsMouseDown）：当前帧按键是否按下。
//     2. 边沿检测（IsKeyPressed / IsKeyReleased）：本帧刚按下/刚释放。
//     3. 鼠标查询（位置、滚轮、按键）。
//
// 核心原理：双缓冲状态
//   维护 currentKeys_（本帧状态）与 previousKeys_（上一帧状态）。
//   - IsKeyDown  → currentKeys_[key]
//   - IsKeyPressed  → currentKeys_[key] && !previousKeys_[key]
//   - IsKeyReleased → !currentKeys_[key] && previousKeys_[key]
//
//   NewFrame() 在每帧开始时调用，将 current 拷贝到 previous，为新一帧做准备。
//   HandleEvent() 在事件循环中对每个事件更新 current 状态。
//
// 按键映射（Key Mapping）：
//   支持将逻辑动作（如 "MoveUp"）绑定到一个或多个物理按键。
//   例如 WASD 与方向键都可绑定到 "MoveUp/Down/Left/Right"。
//   查询时用 IsActionDown("MoveUp") 即可同时检测 W 和 Up。
//
// 与固定步长的关系：
//   NewFrame 每帧调用一次（非每固定步长），因此边沿检测在一帧内的所有
//   固定步长更新中都为 true。对于"按下一次触发一次"的逻辑（如攻击），
//   需在游戏逻辑中用消费标志或冷却时间避免重复触发。
// ============================================================================

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace cu {

class Camera; // 前向声明，避免头文件循环依赖

class Input {
public:
    Input();
    ~Input() = default;

    // 禁止拷贝（持有状态，不应被意外复制）
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    // ---- 帧生命周期 ----

    // 每帧开始调用：将当前状态拷贝到上一帧，清空边沿数据（滚轮增量）
    void NewFrame();

    // 处理 SFML 事件（在主循环 pollEvent 后调用）
    void HandleEvent(const sf::Event& event);

    // ---- 键盘查询 ----

    // 当前帧按键是否处于按下状态
    [[nodiscard]] bool IsKeyDown(sf::Keyboard::Key key) const;

    // 本帧刚按下（边沿触发：上一帧 up，本帧 down）
    [[nodiscard]] bool IsKeyPressed(sf::Keyboard::Key key) const;

    // 本帧刚释放（边沿触发：上一帧 down，本帧 up）
    [[nodiscard]] bool IsKeyReleased(sf::Keyboard::Key key) const;

    // ---- 鼠标查询 ----

    // 鼠标屏幕坐标（像素）
    [[nodiscard]] sf::Vector2i GetMousePosition() const noexcept { return mousePosition_; }

    // 鼠标世界坐标（通过摄像机逆变换）
    [[nodiscard]] sf::Vector2f GetMouseWorldPosition(const Camera& camera) const;

    // 鼠标按键状态
    [[nodiscard]] bool IsMouseDown(sf::Mouse::Button button) const;
    [[nodiscard]] bool IsMousePressed(sf::Mouse::Button button) const;
    [[nodiscard]] bool IsMouseReleased(sf::Mouse::Button button) const;

    // 滚轮增量（本帧滚动的格数，向上为正，向下为负）
    [[nodiscard]] float GetMouseWheelDelta() const noexcept { return mouseWheelDelta_; }

    // ---- 按键映射（动作绑定） ----

    // 添加按键映射：将物理按键绑定到逻辑动作
    // 同一动作可绑定多个按键（如 "MoveUp" 绑定 W 和 Up）
    void AddKeyMapping(const std::string& action, sf::Keyboard::Key key);

    // 移除某动作的所有按键映射
    void RemoveKeyMapping(const std::string& action);

    // 按动作查询（任一绑定按键满足即返回 true）
    [[nodiscard]] bool IsActionDown(const std::string& action) const;
    [[nodiscard]] bool IsActionPressed(const std::string& action) const;
    [[nodiscard]] bool IsActionReleased(const std::string& action) const;

    // ---- 便捷：注册默认 WASD + 方向键映射 ----
    void RegisterDefaultMappings();

private:
    // 键盘状态双缓冲
    // 使用固定大小数组，O(1) 查询，比 unordered_map 快
    // sf::Keyboard::KeyCount 是 SFML 定义的按键总数
    std::array<bool, sf::Keyboard::KeyCount> currentKeys_{};
    std::array<bool, sf::Keyboard::KeyCount> previousKeys_{};

    // 鼠标按键状态双缓冲
    std::array<bool, sf::Mouse::ButtonCount> currentMouseButtons_{};
    std::array<bool, sf::Mouse::ButtonCount> previousMouseButtons_{};

    sf::Vector2i mousePosition_{0, 0}; // 屏幕坐标
    float mouseWheelDelta_ = 0.f;      // 本帧滚轮增量

    // 按键映射：动作名 → 绑定的物理按键列表
    std::unordered_map<std::string, std::vector<sf::Keyboard::Key>> keyMappings_;

    // 窗口失焦时清空所有状态（避免按键卡住）
    void clearAllStates();
};

} // namespace cu
