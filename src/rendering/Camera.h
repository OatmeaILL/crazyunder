#pragma once

// ============================================================================
// Camera —— 2.5D 摄像机
// ----------------------------------------------------------------------------
// 职责：
//   1. 平滑跟随目标：用 lerp 插值移动，避免硬切带来的视觉突兀。
//   2. 屏幕震动：受击/爆炸时短暂偏移视野，增强打击感。
//   3. 缩放：像素风游戏常需放大像素，zoom > 1 = 放大（视野变小）。
//   4. 坐标转换：世界坐标 ↔ 屏幕坐标，用于鼠标拾取、UI 对齐等。
//
// 平滑跟随原理（帧率无关 lerp）：
//   简单 lerp `pos += (target - pos) * k` 在不同帧率下结果不同。
//   帧率无关版本：`pos = lerp(pos, target, 1 - exp(-speed * dt))`
//   等价于 `pos += (target - pos) * (1 - exp(-speed * dt))`
//   其中 speed 越大跟随越快。exp 保证无论 dt 多大，插值比例始终在 [0,1)。
//
// 屏幕震动原理：
//   在摄像机中心叠加一个随机方向的偏移，偏移幅度随时间衰减。
//   magnitude * (remaining/duration) * normalized_random_direction
//   衰减保证震动逐渐减弱直至停止。
// ============================================================================

#include <SFML/Graphics.hpp>

namespace cu {

class Camera {
public:
    Camera();

    // 设置视口大小（逻辑分辨率，固定 1280x720，用于坐标转换与 View 计算）
    void SetViewportSize(sf::Vector2f size);

    // 设置窗口物理像素尺寸（用于 ScreenToWorld 把窗口像素坐标转换为逻辑坐标）
    // 当窗口物理尺寸 ≠ 逻辑视口尺寸时，ScreenToWorld 需要先按比例转换
    void SetWindowPhysicalSize(sf::Vector2f size) noexcept { windowPhysicalSize_ = size; }
    [[nodiscard]] sf::Vector2f GetWindowPhysicalSize() const noexcept { return windowPhysicalSize_; }

    // 设置跟随目标（世界坐标）
    void SetTarget(sf::Vector2f target) noexcept { target_ = target; }

    // 直接设置位置（跳过 lerp，用于初始化/传送）
    void SetPosition(sf::Vector2f pos) noexcept;

    // 设置缩放（zoom > 1 = 放大，zoom < 1 = 缩小）
    void SetZoom(float zoom) noexcept { zoom_ = zoom; }
    [[nodiscard]] float GetZoom() const noexcept { return zoom_; }

    // 触发屏幕震动
    void Shake(float magnitude, float duration) noexcept;

    // 每帧更新：lerp 跟随 + 震动衰减
    void Update(float dt);

    // 获取当前中心位置（不含震动偏移）
    [[nodiscard]] sf::Vector2f GetPosition() const noexcept { return position_; }

    // 世界坐标 → 屏幕像素坐标
    [[nodiscard]] sf::Vector2f WorldToScreen(sf::Vector2f world) const;

    // 屏幕像素坐标 → 世界坐标
    [[nodiscard]] sf::Vector2f ScreenToWorld(sf::Vector2f screen) const;

    // 获取 SFML View（含震动偏移与缩放，供 RenderTarget::setView 使用）
    [[nodiscard]] const sf::View& GetView() const noexcept { return view_; }

private:
    sf::Vector2f viewportSize_;  // 逻辑视口尺寸（固定 1280x720）
    sf::Vector2f windowPhysicalSize_ = sf::Vector2f(1280.f, 720.f); // 窗口物理像素尺寸
    sf::Vector2f target_;        // 跟随目标（世界坐标）
    sf::Vector2f position_;      // 当前中心（世界坐标，lerp 后）
    float zoom_ = 1.f;           // 缩放系数

    // 震动参数
    float shakeMagnitude_ = 0.f;
    float shakeDuration_ = 0.f;
    float shakeTimer_ = 0.f;
    sf::Vector2f shakeOffset_;   // 当前震动偏移

    sf::View view_;              // 最终提交给 RenderTarget 的视图

    // 跟随速度（越大越快），用于帧率无关 lerp
    static constexpr float kFollowSpeed = 8.f;

    void updateView();
};

} // namespace cu
