#include "rendering/Camera.h"
#include <cmath>
#include <cstdlib>

namespace cu {

Camera::Camera()
    : viewportSize_(1280.f, 720.f)
    , target_(0.f, 0.f)
    , position_(0.f, 0.f) {
    updateView();
}

void Camera::SetViewportSize(sf::Vector2f size) {
    viewportSize_ = size;
    updateView();
}

void Camera::SetPosition(sf::Vector2f pos) noexcept {
    position_ = pos;
    target_ = pos;
    updateView();
}

void Camera::Shake(float magnitude, float duration) noexcept {
    // 新震动若更强则覆盖当前震动
    if (magnitude > shakeMagnitude_ * (shakeTimer_ / shakeDuration_)) {
        shakeMagnitude_ = magnitude;
        shakeDuration_ = duration;
        shakeTimer_ = duration;
    }
}

void Camera::Update(float dt) {
    // 帧率无关 lerp：1 - exp(-speed * dt)
    float lerpFactor = 1.f - std::exp(-kFollowSpeed * dt);
    position_ += (target_ - position_) * lerpFactor;

    // 震动衰减
    if (shakeTimer_ > 0.f) {
        shakeTimer_ -= dt;
        if (shakeTimer_ <= 0.f) {
            shakeTimer_ = 0.f;
            shakeMagnitude_ = 0.f;
            shakeOffset_ = sf::Vector2f(0.f, 0.f);
        } else {
            // 随机方向 * 当前幅度（线性衰减）
            float ratio = shakeTimer_ / shakeDuration_;
            float angle = static_cast<float>(std::rand()) / RAND_MAX * 6.2831853f;
            float mag = shakeMagnitude_ * ratio;
            shakeOffset_ = sf::Vector2f(std::cos(angle) * mag, std::sin(angle) * mag);
        }
    }

    updateView();
}

sf::Vector2f Camera::WorldToScreen(sf::Vector2f world) const {
    // 世界坐标减去视图中心 = 相对偏移，再按缩放映射到屏幕像素
    sf::Vector2f viewSize = viewportSize_ / zoom_;
    sf::Vector2f relative = world - (position_ + shakeOffset_);
    return sf::Vector2f(
        relative.x + viewSize.x * 0.5f,
        relative.y + viewSize.y * 0.5f
    );
}

sf::Vector2f Camera::ScreenToWorld(sf::Vector2f screen) const {
    // 先把窗口物理像素坐标转换为逻辑坐标（viewportSize_，通常 1280x720）
    // 当窗口物理尺寸 ≠ 逻辑视口尺寸时（如 1920x1080 窗口显示 1280x720 逻辑分辨率），
    // SFML View 会自动等比缩放，鼠标坐标也需相应转换。
    sf::Vector2f logical;
    if (windowPhysicalSize_.x > 0.f && windowPhysicalSize_.y > 0.f) {
        logical.x = screen.x * (viewportSize_.x / windowPhysicalSize_.x);
        logical.y = screen.y * (viewportSize_.y / windowPhysicalSize_.y);
    } else {
        logical = screen;
    }
    sf::Vector2f viewSize = viewportSize_ / zoom_;
    sf::Vector2f relative = logical - viewSize * 0.5f;
    return relative + (position_ + shakeOffset_);
}

void Camera::updateView() {
    // 视图大小 = 视口大小 / 缩放（zoom > 1 → 视图更小 → 放大）
    sf::Vector2f viewSize = viewportSize_ / zoom_;
    view_.setSize(viewSize);
    // 中心 = 当前位置 + 震动偏移
    view_.setCenter(position_ + shakeOffset_);
    // 默认视口占满整个窗口
    view_.setViewport(sf::FloatRect(0.f, 0.f, 1.f, 1.f));
}

} // namespace cu
