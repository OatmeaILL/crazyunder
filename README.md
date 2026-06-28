# CrazyUnder

2.5D 像素风 Roguelike 割草爽游。C++17 + SFML 2.6 + CMake。

## 目录结构

```
crazyunder/
├── CMakeLists.txt
├── src/
│   ├── core/        # 核心引擎：Time / ResourceManager / Game
│   ├── ecs/         # 实体组件系统（Phase 2）
│   ├── gameplay/    # 玩法系统（Phase 4+）
│   ├── rendering/   # 渲染（Phase 2+）
│   ├── ui/          # 界面（Phase 8）
│   ├── utils/       # 工具：Logger / ObjectPool
│   └── main.cpp
└── assets/
    ├── sprites/     # 美术贴图
    ├── generated/   # 过程化生成资源
    └── audio/       # 音频
```

## 依赖

- **C++17** 编译器（MSVC 19.x 优先，兼容 MinGW-w64）
- **CMake 3.16+**
- **SFML 2.6+**（system / window / graphics / audio 模块）

## 安装 SFML（推荐 vcpkg）

```bash
# 1. 克隆 vcpkg（若已有可跳过）
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.bat

# 2. 安装 SFML（x64-windows，动态库）
vcpkg install sfml:x64-windows
```

国内网络受限时，可设置 vcpkg 镜像或使用代理后重试。也可手动从
[SFML 官网](https://www.sfml-dev.org/download.php) 下载预编译包并解压，
然后通过 `SFML_DIR` 指向其 `lib/cmake/SFML` 目录。

## 构建

### 方式一：vcpkg（推荐）

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg 根目录>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
```

### 方式二：FetchContent 自动下载（无需预装 SFML）

若未通过 vcpkg 安装，CMakeLists.txt 会在 `find_package` 失败时自动使用
`FetchContent` 从 GitHub 下载 SFML 2.6.x 源码并编译。

```bash
cmake -B build -S .
cmake --build build --config Debug
```

> 注意：FetchContent 首次配置需要联网（git），且会编译 SFML 全部源码，耗时较长。
> Windows 上 SFML 自带 `extlibs` 依赖（freetype 等），通常可离线编译。

### 方式三：手动指定 SFML 路径

```bash
cmake -B build -S . -DSFML_DIR="C:/SFML-2.6.1/lib/cmake/SFML"
cmake --build build --config Debug
```

## 运行

```bash
# 可执行文件位于 build/bin/
./build/bin/crazyunder.exe        # Debug
./build/bin/crazyunder.exe --test-objectpool   # 运行对象池单元测试
```

启动后将显示 1280x720 窗口，控制台每秒输出 FPS，按 ESC 退出。

## Phase 1 已实现

- 固定步长时间管理（1/120s，累加器 + 插值 alpha + 防死亡螺旋）
- 单例资源管理器（纹理/字体/音频缓存，加载失败回退占位资源）
- 状态机主循环（Menu / Playing / Paused / Dead / Victory）
- 模板对象池（O(1) acquire/release，紧凑数组遍历）
- 轻量日志工具（LOG_DEBUG/INFO/WARN/ERROR）
