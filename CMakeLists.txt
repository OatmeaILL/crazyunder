cmake_minimum_required(VERSION 3.16)
project(crazyunder CXX)

# ============================================================================
# CrazyUnder - 2.5D 像素风 Roguelike 割草爽游
# Phase 1：项目骨架与核心引擎层
# ============================================================================

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 统一输出目录，便于运行
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Windows 下默认使用 UTF-8 源码字符集（中文注释）
if(MSVC)
    add_compile_options(/utf-8 /W3)
endif()

# ----------------------------------------------------------------------------
# SFML 查找策略
#   方式一（推荐）：通过 vcpkg 安装 SFML，再 find_package
#       vcpkg install sfml:x64-windows
#       cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
#   方式二（回退）：find_package 失败时，使用 FetchContent 从 GitHub 下载 SFML 2.6.x 源码编译
# ----------------------------------------------------------------------------
set(FETCHCONTENT_QUIET OFF)

# 优先尝试 find_package（vcpkg / 系统安装）
find_package(SFML 2.6 COMPONENTS system window graphics audio QUIET)

if(NOT SFML_FOUND)
    message(STATUS "未找到预安装的 SFML，使用 FetchContent 从 GitHub 下载 SFML 2.6.x ...")
    include(FetchContent)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 2.6.x
        GIT_SHALLOW ON
    )
    # 关闭 SFML 自带示例与文档以加快编译
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SFML)
endif()

# ----------------------------------------------------------------------------
# 主可执行文件
# ----------------------------------------------------------------------------
add_executable(crazyunder
    src/main.cpp
    src/core/Game.cpp
    src/core/Game_Scene.cpp
    src/core/Game_Update.cpp
    src/core/Game_Render.cpp
    src/core/Game_Interaction.cpp
    src/core/Game_SaveLoad.cpp
    src/core/Game_Modifiers.cpp
    src/core/Game_Debug.cpp
    src/core/Time.cpp
    src/core/ResourceManager.cpp
    src/core/Input.cpp
    src/core/AudioManager.cpp
    src/core/SaveSystem.cpp
    src/ecs/Registry.cpp
    src/rendering/TextureAtlas.cpp
    src/rendering/Renderer.cpp
    src/rendering/Camera.cpp
    src/rendering/ParticleSystem.cpp
    src/gameplay/Animation.cpp
    src/gameplay/Player.cpp
    src/gameplay/FlowField.cpp
    src/gameplay/EnemyAI.cpp
    src/gameplay/EnemySpawner.cpp
    src/gameplay/CombatSystem.cpp
    src/gameplay/ProjectileSystem.cpp
    src/gameplay/PlayerCombat.cpp
    src/gameplay/CombatEffects.cpp
    src/gameplay/DungeonGenerator.cpp
    src/gameplay/RoomSystem.cpp
    src/gameplay/LootSystem.cpp
    src/gameplay/InventorySystem.cpp
    src/gameplay/UpgradeSystem.cpp
    src/gameplay/ExpOrbSystem.cpp
    src/gameplay/CoinSystem.cpp
    src/gameplay/HeartSystem.cpp
    src/gameplay/MerchantSystem.cpp
    src/gameplay/SkillSystem.cpp
    src/gameplay/UpgradeUI.cpp
    src/gameplay/QuestSystem.cpp
    src/gameplay/AchievementSystem.cpp
    src/gameplay/RelicSystem.cpp
    src/gameplay/FloorModifier.cpp
    src/gameplay/SoulMemorySystem.cpp
    src/gameplay/DialogueSystem.cpp
    src/rendering/TileMap.cpp
    src/ui/UIManager.cpp
    src/ui/HUD.cpp
    src/ui/Menus.cpp
    src/ui/QuestMenu.cpp
    src/ui/AchievementMenu.cpp
    src/ui/SoulWellMenu.cpp
    src/ui/DialogueBoxUI.cpp
    src/utils/TextureGenerator.cpp
    src/utils/UniformGrid.cpp
)

target_include_directories(crazyunder PRIVATE src)

# 兼容两种 SFML 目标命名：
#   - find_package（现代 CMake target）：SFML::Graphics 等
#   - FetchContent（SFML 子工程目标）：sfml-graphics 等
if(TARGET SFML::Graphics)
    target_link_libraries(crazyunder PRIVATE
        SFML::Graphics SFML::Window SFML::System SFML::Audio)
elseif(TARGET sfml-graphics)
    target_link_libraries(crazyunder PRIVATE
        sfml-graphics sfml-window sfml-system sfml-audio)
else()
    message(FATAL_ERROR "未找到 SFML 目标，请确认 SFML 已正确安装或可被 FetchContent 下载。")
endif()

# Windows：保持控制台子系统，便于查看 FPS / 日志输出
if(WIN32)
    set_target_properties(crazyunder PROPERTIES WIN32_EXECUTABLE OFF)
endif()

# ----------------------------------------------------------------------------
# 音频资源自动部署
#   将项目根目录下的 sound 文件夹复制到可执行文件输出目录
#   确保运行时能够正确加载音效文件
# ----------------------------------------------------------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/sound")
    add_custom_command(TARGET crazyunder POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/sound" "$<TARGET_FILE_DIR:crazyunder>/sound"
        COMMENT "复制 sound 文件夹到输出目录（音频资源）")
    message(STATUS "音频资源目录: ${CMAKE_SOURCE_DIR}/sound")
endif()

# ----------------------------------------------------------------------------
# Windows 运行时依赖自动部署
#   SFML Audio 模块动态依赖 openal32.dll，FetchContent 方式下该 DLL 位于
#   SFML 源码树的 extlibs/bin/x64/ 目录，但不会自动复制到输出目录，导致
#   运行时 STATUS_DLL_NOT_FOUND (0xC0000135) 错误。
#   此处在 POST_BUILD 阶段自动复制，使构建产物可直接运行。
#   兼容 FetchContent（${sfml_SOURCE_DIR}）与 vcpkg（$ENV{SFML_ROOT}）两种方式。
# ----------------------------------------------------------------------------
if(WIN32)
    set(OPENAL_DLL_CANDIDATES
        "${sfml_SOURCE_DIR}/extlibs/bin/x64/openal32.dll"
        "$ENV{SFML_ROOT}/bin/openal32.dll"
        "${SFML_DIR}/../bin/openal32.dll"
    )
    foreach(candidate ${OPENAL_DLL_CANDIDATES})
        if(EXISTS "${candidate}")
            add_custom_command(TARGET crazyunder POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${candidate}" "$<TARGET_FILE_DIR:crazyunder>"
                COMMENT "复制 openal32.dll 到输出目录（SFML Audio 运行时依赖）")
            message(STATUS "OpenAL32.dll 自动部署源: ${candidate}")
            break()
        endif()
    endforeach()
endif()
