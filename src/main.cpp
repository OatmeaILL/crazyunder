// ============================================================================
// CrazyUnder - 入口
// ----------------------------------------------------------------------------
// 创建 Game 实例并运行主循环。异常捕获后输出错误并返回 1。
// 命令行参数：
//   --test-objectpool : 运行对象池单元测试后退出
// ============================================================================

// 启用对象池单元测试编译（可通过 --test-objectpool 运行）
#define CU_OBJECTPOOL_ENABLE_TESTS

#include "core/Game.h"
#include "utils/Logger.h"
#include "utils/ObjectPool.h"

#include <string>
#include <exception>
#include <cstdio>
#include <clocale>

int main(int argc, char* argv[]) {
    system("chcp 65001");
    // 设置 UTF-8 locale，使 sf::String(const std::string&) 正确解析 UTF-8 中文
    // SFML 在 Windows 上使用 std::ctype<wchar_t> facet 转换字符
    // 必须设置 std::locale::global，否则 facet 使用 ASCII 编码导致中文显示为方框
    bool localeOk = false;
    try {
        std::locale::global(std::locale(".UTF-8"));
        localeOk = true;
    } catch (...) {
        try {
            std::locale::global(std::locale("C.UTF-8"));
            localeOk = true;
        } catch (...) {
            try {
                std::locale::global(std::locale("en_US.UTF-8"));
                localeOk = true;
            } catch (...) {
                localeOk = false;
            }
        }
    }
    // 同时设置 C locale（影响 setlocale）
    setlocale(LC_ALL, ".UTF-8");
    // 输出 locale 设置结果（用英文避免日志乱码干扰）
    printf("[LOCALE] UTF-8 locale set: %s, global locale name: %s\n",
           localeOk ? "YES" : "NO",
           std::locale().name().c_str());
    // 命令行参数：--test-objectpool 运行对象池单元测试
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test-objectpool") {
#ifdef CU_OBJECTPOOL_ENABLE_TESTS
            bool ok = cu::RunObjectPoolTests();
            return ok ? 0 : 1;
#else
            std::fprintf(stderr, "对象池测试未编译：请定义 CU_OBJECTPOOL_ENABLE_TESTS\n");
            return 1;
#endif
        }
    }

    try {
        cu::Game game;
        game.Run();
    } catch (const std::exception& e) {
        LOG_ERROR("致命异常: %s", e.what());
        return 1;
    } catch (...) {
        LOG_ERROR("未知致命异常");
        return 1;
    }
    return 0;
}
