#include "core/Input.h"
#include "rendering/Camera.h"

namespace cu {

// SFML 2.6 中 KeyCount 与 ButtonCount 为枚举值，需转换为 size_t 用作数组大小
// 部分编译器对 enum 索引数组有警告，这里显式转换
static constexpr std::size_t kKeyCount = static_cast<std::size_t>(sf::Keyboard::KeyCount);
static constexpr std::size_t kButtonCount = static_cast<std::size_t>(sf::Mouse::ButtonCount);

Input::Input() {
    currentKeys_.fill(false);
    previousKeys_.fill(false);
    currentMouseButtons_.fill(false);
    previousMouseButtons_.fill(false);
}

void Input::NewFrame() {
    // 将本帧状态拷贝到上一帧，为下一帧的边沿检测做准备
    previousKeys_ = currentKeys_;
    previousMouseButtons_ = currentMouseButtons_;
    // 滚轮增量是每帧重置的（事件驱动，一帧内可能多次滚动）
    mouseWheelDelta_ = 0.f;
}

void Input::HandleEvent(const sf::Event& event) {
    switch (event.type) {
        // ---- 键盘事件 ----
        case sf::Event::KeyPressed: {
            auto key = event.key.code;
            if (key >= 0 && key < sf::Keyboard::KeyCount) {
                // 注意：OS 按键重复会持续发送 KeyPressed 事件。
                // 这里只设置 currentKeys_ 为 true，边沿检测由 NewFrame 的双缓冲
                // 机制保证（previous 为 false 时才算"刚按下"）。
                currentKeys_[static_cast<std::size_t>(key)] = true;
            }
            break;
        }
        case sf::Event::KeyReleased: {
            auto key = event.key.code;
            if (key >= 0 && key < sf::Keyboard::KeyCount) {
                currentKeys_[static_cast<std::size_t>(key)] = false;
            }
            break;
        }

        // ---- 鼠标按键事件 ----
        case sf::Event::MouseButtonPressed: {
            auto btn = event.mouseButton.button;
            if (btn >= 0 && btn < sf::Mouse::ButtonCount) {
                currentMouseButtons_[static_cast<std::size_t>(btn)] = true;
            }
            break;
        }
        case sf::Event::MouseButtonReleased: {
            auto btn = event.mouseButton.button;
            if (btn >= 0 && btn < sf::Mouse::ButtonCount) {
                currentMouseButtons_[static_cast<std::size_t>(btn)] = false;
            }
            break;
        }

        // ---- 鼠标移动 ----
        case sf::Event::MouseMoved:
            mousePosition_ = sf::Vector2i(event.mouseMove.x, event.mouseMove.y);
            break;

        // ---- 鼠标滚轮 ----
        // SFML 2.6 中 MouseWheelScrolled 包含 delta（浮点，支持触摸板平滑滚动）
        case sf::Event::MouseWheelScrolled:
            mouseWheelDelta_ += event.mouseWheelScroll.delta;
            break;

        // ---- 窗口失焦：清空所有状态，避免按键卡住 ----
        case sf::Event::LostFocus:
            clearAllStates();
            break;

        default:
            break;
    }
}

// ============================================================================
// 键盘查询
// ============================================================================

bool Input::IsKeyDown(sf::Keyboard::Key key) const {
    if (key < 0 || key >= sf::Keyboard::KeyCount) return false;
    return currentKeys_[static_cast<std::size_t>(key)];
}

bool Input::IsKeyPressed(sf::Keyboard::Key key) const {
    if (key < 0 || key >= sf::Keyboard::KeyCount) return false;
    auto idx = static_cast<std::size_t>(key);
    // 本帧 down 且上一帧 up = 刚按下
    return currentKeys_[idx] && !previousKeys_[idx];
}

bool Input::IsKeyReleased(sf::Keyboard::Key key) const {
    if (key < 0 || key >= sf::Keyboard::KeyCount) return false;
    auto idx = static_cast<std::size_t>(key);
    // 本帧 up 且上一帧 down = 刚释放
    return !currentKeys_[idx] && previousKeys_[idx];
}

// ============================================================================
// 鼠标查询
// ============================================================================

sf::Vector2f Input::GetMouseWorldPosition(const Camera& camera) const {
    // 屏幕坐标 → 世界坐标，由 Camera 的逆变换完成
    return camera.ScreenToWorld(sf::Vector2f(mousePosition_));
}

bool Input::IsMouseDown(sf::Mouse::Button button) const {
    if (button < 0 || button >= sf::Mouse::ButtonCount) return false;
    return currentMouseButtons_[static_cast<std::size_t>(button)];
}

bool Input::IsMousePressed(sf::Mouse::Button button) const {
    if (button < 0 || button >= sf::Mouse::ButtonCount) return false;
    auto idx = static_cast<std::size_t>(button);
    return currentMouseButtons_[idx] && !previousMouseButtons_[idx];
}

bool Input::IsMouseReleased(sf::Mouse::Button button) const {
    if (button < 0 || button >= sf::Mouse::ButtonCount) return false;
    auto idx = static_cast<std::size_t>(button);
    return !currentMouseButtons_[idx] && previousMouseButtons_[idx];
}

// ============================================================================
// 按键映射
// ============================================================================

void Input::AddKeyMapping(const std::string& action, sf::Keyboard::Key key) {
    auto& keys = keyMappings_[action];
    // 避免重复添加同一按键
    for (auto k : keys) {
        if (k == key) return;
    }
    keys.push_back(key);
}

void Input::RemoveKeyMapping(const std::string& action) {
    keyMappings_.erase(action);
}

bool Input::IsActionDown(const std::string& action) const {
    auto it = keyMappings_.find(action);
    if (it == keyMappings_.end()) return false;
    for (auto key : it->second) {
        if (IsKeyDown(key)) return true;
    }
    return false;
}

bool Input::IsActionPressed(const std::string& action) const {
    auto it = keyMappings_.find(action);
    if (it == keyMappings_.end()) return false;
    for (auto key : it->second) {
        if (IsKeyPressed(key)) return true;
    }
    return false;
}

bool Input::IsActionReleased(const std::string& action) const {
    auto it = keyMappings_.find(action);
    if (it == keyMappings_.end()) return false;
    for (auto key : it->second) {
        if (IsKeyReleased(key)) return true;
    }
    return false;
}

void Input::RegisterDefaultMappings() {
    // 移动：WASD + 方向键
    AddKeyMapping("MoveUp",    sf::Keyboard::W);
    AddKeyMapping("MoveUp",    sf::Keyboard::Up);
    AddKeyMapping("MoveDown",  sf::Keyboard::S);
    AddKeyMapping("MoveDown",  sf::Keyboard::Down);
    AddKeyMapping("MoveLeft",  sf::Keyboard::A);
    AddKeyMapping("MoveLeft",  sf::Keyboard::Left);
    AddKeyMapping("MoveRight", sf::Keyboard::D);
    AddKeyMapping("MoveRight", sf::Keyboard::Right);

    // 功能键
    AddKeyMapping("Attack",   sf::Keyboard::Space);
    AddKeyMapping("Pause",    sf::Keyboard::P);
    AddKeyMapping("Debug",    sf::Keyboard::F1);
}

void Input::clearAllStates() {
    currentKeys_.fill(false);
    currentMouseButtons_.fill(false);
    mouseWheelDelta_ = 0.f;
}

} // namespace cu
