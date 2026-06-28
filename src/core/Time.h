#pragma once

// ============================================================================
// Time —— 高精度固定步长时间管理
// ----------------------------------------------------------------------------
// 为什么用固定步长（Fixed Timestep）？
//   物理与游戏逻辑要求确定性：相同输入应产生相同结果。可变步长会让积分
//   误差随帧率波动，导致碰撞穿透、AI 抖动等问题。固定步长（1/120s）保证
//   每次逻辑更新推进相同时间，逻辑稳定可复现。
//
// 累加器模式（Accumulator）：
//   真实帧时间不均匀（受系统调度、垂直同步影响）。我们用累加器收集真实
//   时间，每当累加器 >= FixedDeltaTime 就消费一次固定更新，从而把不均匀
//   的真实时间“量化”为均匀的固定步长。
//
// 渲染插值因子 alpha：
//   逻辑更新是离散的，但渲染应连续。alpha = accumulator / FixedDeltaTime
//   表示“当前帧距离上一次逻辑更新已推进的比例”。渲染时用 alpha 在上一帧
//   与当前帧状态间线性插值，使画面平滑，消除卡顿感。
//
// 防死亡螺旋（Death Spiral）：
//   若某帧耗时过长，累加器会堆积大量待更新，导致下一帧要执行更多更新，
//   进而更慢，形成恶性循环。两重防护：
//     1. 单帧真实时间上限 clamp（MaxFrameTime），避免单次巨大跳跃。
//     2. 单帧固定更新次数上限（MaxCatchUp=5），超出则丢弃积压时间。
// ============================================================================

#include <chrono>

namespace cu {

class Time {
public:
    // 固定步长：1/30 秒，逻辑更新频率
    // 注：原 1/120s 每帧需 2 次更新，1/60s 在省电模式仍需 2 次。
    //     改为 1/30s 后每帧仅 1 次更新，性能大幅提升，逻辑仍稳定。
    static constexpr double FixedDeltaTime = 1.0 / 30.0;

    // 单帧 catch-up 上限：限制为 1，避免低帧率时多次更新导致恶性循环
    static constexpr int MaxCatchUp = 1;

    // 单帧真实时间上限（秒）：超过则 clamp，避免累加器爆炸
    static constexpr double MaxFrameTime = 0.25;

    Time() = default;

    // 重置计时基准（窗口启动/恢复时调用）
    void Reset();

    // 采集真实时间，消费累加器，返回本帧应执行的固定更新次数。
    // 调用者应按返回值循环调用 Update(fixedDt)。
    [[nodiscard]] int Update();

    // 渲染插值因子 [0,1)
    [[nodiscard]] double GetAlpha() const noexcept { return alpha_; }

    // 当前 FPS（每秒更新一次）
    [[nodiscard]] int GetFPS() const noexcept { return fps_; }

    // 上一帧真实帧时间（秒），用于监控/调试
    [[nodiscard]] double GetDeltaTime() const noexcept { return realDt_; }

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint lastTime_{};
    double accumulator_ = 0.0; // 累加的真实时间
    double alpha_ = 0.0;       // 渲染插值因子
    double realDt_ = 0.0;      // 上一帧真实帧时间

    // FPS 统计
    int fps_ = 0;
    int frameCount_ = 0;
    double fpsTimer_ = 0.0;

    bool initialized_ = false;
};

} // namespace cu
