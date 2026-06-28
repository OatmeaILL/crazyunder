#include "core/Time.h"

namespace cu {

void Time::Reset() {
    lastTime_ = Clock::now();
    accumulator_ = 0.0;
    alpha_ = 0.0;
    realDt_ = 0.0;
    frameCount_ = 0;
    fpsTimer_ = 0.0;
    fps_ = 0;
    initialized_ = true;
}

int Time::Update() {
    if (!initialized_) {
        Reset();
    }

    TimePoint now = Clock::now();
    // 真实帧时间（秒）
    double frameTime = std::chrono::duration<double>(now - lastTime_).count();
    lastTime_ = now;

    // 防死亡螺旋第一重：clamp 单帧真实时间
    if (frameTime > MaxFrameTime) {
        frameTime = MaxFrameTime;
    }
    realDt_ = frameTime;

    // 累加真实时间
    accumulator_ += frameTime;

    // 消费累加器：每达到一个固定步长执行一次更新
    int steps = 0;
    while (accumulator_ >= FixedDeltaTime && steps < MaxCatchUp) {
        accumulator_ -= FixedDeltaTime;
        ++steps;
    }

    // 防死亡螺旋第二重：若达到 catch-up 上限仍有积压，丢弃剩余时间
    if (accumulator_ >= FixedDeltaTime) {
        accumulator_ = 0.0;
    }

    // 渲染插值因子：当前帧已累积时间 / 固定步长
    alpha_ = accumulator_ / FixedDeltaTime;

    // FPS 统计：每秒更新一次
    ++frameCount_;
    fpsTimer_ += frameTime;
    if (fpsTimer_ >= 1.0) {
        fps_ = static_cast<int>(static_cast<double>(frameCount_) / fpsTimer_);
        frameCount_ = 0;
        fpsTimer_ = 0.0;
    }

    return steps;
}

} // namespace cu
