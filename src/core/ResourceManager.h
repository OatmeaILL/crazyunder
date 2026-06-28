#pragma once

// ============================================================================
// ResourceManager —— 单例资源管理器
// ----------------------------------------------------------------------------
// 职责：
//   - 模板化加载并缓存纹理/字体/音频缓冲，按路径唯一存储。
//   - 重复加载返回缓存，避免重复 IO 与显存占用。
//   - 加载失败时返回默认占位资源（纹理为 32x32 紫色方块带 X 标记），
//     并输出 WARN 日志，保证游戏不会因缺资源崩溃。
//
// 线程安全：
//   Phase 1 为单线程，简化实现不加锁。后续若引入异步加载可加 mutex。
// ============================================================================

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <unordered_map>
#include <memory>
#include <string>

namespace cu {

class ResourceManager {
public:
    static ResourceManager& Instance();

    // 获取纹理（失败返回默认占位纹理）
    sf::Texture& GetTexture(const std::string& path);
    // 获取字体（失败返回默认字体）
    sf::Font& GetFont(const std::string& path);
    // 获取音频缓冲（失败返回默认空缓冲）
    sf::SoundBuffer& GetSoundBuffer(const std::string& path);

    // 清理缓存（仅在场景切换、确认无外部引用时调用）
    void ReleaseUnused();

    // 默认占位资源访问
    sf::Texture& GetDefaultTexture() { return defaultTexture_; }
    sf::Font& GetDefaultFont() { return defaultFont_; }

private:
    ResourceManager();
    ~ResourceManager() = default;
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    void createDefaultTexture();
    void loadDefaultFont();

    sf::Texture defaultTexture_;
    sf::Font defaultFont_;
    sf::SoundBuffer defaultSound_;

    std::unordered_map<std::string, std::unique_ptr<sf::Texture>> textures_;
    std::unordered_map<std::string, std::unique_ptr<sf::Font>> fonts_;
    std::unordered_map<std::string, std::unique_ptr<sf::SoundBuffer>> sounds_;
};

} // namespace cu
