#pragma once

// ============================================================================
// Logger —— 轻量日志工具（仅头文件）
// ----------------------------------------------------------------------------
// 特性：
//   - 宏 LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR(fmt, ...)
//   - printf 风格格式串，输出到 std::cerr，带时间戳与级别标签
//   - 定义编译宏 LOG_DISABLE_DEBUG 可关闭 DEBUG 级别日志
//   - 不依赖第三方库
// ============================================================================

#include <cstdio>
#include <ctime>
#include <cstdarg>

namespace cu {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

namespace detail {

// 级别对应的字符串标签（固定 5 字符宽度，便于对齐）
inline const char* level_str(LogLevel l) {
    switch (l) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?    ";
}

// 安全地获取本地时间（Windows 使用 localtime_s）
inline std::tm safe_localtime(std::time_t t) {
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    tmv = *std::localtime(&t);
#endif
    return tmv;
}

// 实际日志实现：写时间戳 + 级别 + 用户消息到 stderr
inline void log_impl(LogLevel level, const char* fmt, ...) {
    std::time_t t = std::time(nullptr);
    std::tm tmv = safe_localtime(t);
    char timebuf[24];
    std::strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tmv);

    std::fprintf(stderr, "[%s] [%s] ", timebuf, level_str(level));

    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);

    std::fprintf(stderr, "\n");
    std::fflush(stderr);
}

} // namespace detail
} // namespace cu

// ----------------------------------------------------------------------------
// 对外日志宏
// ----------------------------------------------------------------------------
#ifndef LOG_DISABLE_DEBUG
    #define LOG_DEBUG(fmt, ...) ::cu::detail::log_impl(::cu::LogLevel::Debug, (fmt), ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) do { } while (0)
#endif

#define LOG_INFO(fmt, ...)  ::cu::detail::log_impl(::cu::LogLevel::Info,  (fmt), ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  ::cu::detail::log_impl(::cu::LogLevel::Warn,  (fmt), ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) ::cu::detail::log_impl(::cu::LogLevel::Error, (fmt), ##__VA_ARGS__)

// ============================================================================
// U8 宏 —— 将 UTF-8 字符串字面量转换为 sf::String
// ----------------------------------------------------------------------------
// SFML 的 sf::String(const std::string&) 使用 ANSI 解码，一次只处理一个字节，
// 无法正确处理多字节 UTF-8 中文。使用 fromUtf8 正确转换。
// 用法：text.setString(U8("中文"));
// ============================================================================
#include <SFML/System/String.hpp>
#include <string>
#define U8(str) sf::String::fromUtf8((str), (str) + sizeof(str) - 1)
namespace cu {
inline sf::String utf8ToSfString(const std::string& s) {
    return sf::String::fromUtf8(s.begin(), s.end());
}
}
