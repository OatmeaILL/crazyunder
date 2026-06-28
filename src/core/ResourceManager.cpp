#include "core/ResourceManager.h"
#include "utils/Logger.h"

namespace cu {

ResourceManager& ResourceManager::Instance() {
    static ResourceManager instance;
    return instance;
}

ResourceManager::ResourceManager() {
    createDefaultTexture();
    loadDefaultFont();
}

void ResourceManager::createDefaultTexture() {
    // 32x32 紫色方块，中间画 X 标记，用于标识“缺失资源”
    defaultTexture_.create(32, 32);
    sf::Image img;
    img.create(32, 32, sf::Color(128, 0, 128)); // 紫色底

    // 画 X：两条对角线（黑色）
    for (int i = 0; i < 32; ++i) {
        img.setPixel(i, i, sf::Color::Black);
        img.setPixel(31 - i, i, sf::Color::Black);
    }
    defaultTexture_.update(img);
}

void ResourceManager::loadDefaultFont() {
    // 尝试加载 Windows 系统字体作为默认字体，失败则留空（文本不渲染但不崩溃）
    // 优先使用 .ttf 格式（SFML 对 .ttc 支持有限，可能加载失败导致中文显示为方框）
    // 所有候选均支持中文字符
    const char* candidates[] = {
        "C:/Windows/Fonts/simhei.ttf",   // 黑体（.ttf，支持中文）
        "C:/Windows/Fonts/Deng.ttf",      // 等线（.ttf，支持中文）
        "C:/Windows/Fonts/simsunb.ttf",   // 宋体（.ttf，支持中文）
        "C:/Windows/Fonts/msyh.ttc",      // 微软雅黑（.ttc，备选）
        "C:/Windows/Fonts/simsun.ttc",    // 宋体（.ttc，备选）
        "C:/Windows/Fonts/arial.ttf",     // Arial（仅英文，最后备选）
    };
    for (const char* p : candidates) {
        if (defaultFont_.loadFromFile(p)) {
            LOG_INFO("默认字体加载成功: %s", p);
            return;
        }
    }
    LOG_WARN("未找到系统字体，文本将无法渲染。请放置字体到 assets/ 并通过 GetFont 加载。");
}

sf::Texture& ResourceManager::GetTexture(const std::string& path) {
    auto it = textures_.find(path);
    if (it != textures_.end()) return *it->second;

    auto tex = std::make_unique<sf::Texture>();
    if (!tex->loadFromFile(path)) {
        LOG_WARN("纹理加载失败: %s（使用占位纹理）", path.c_str());
        return defaultTexture_;
    }
    sf::Texture& ref = *tex;
    textures_[path] = std::move(tex);
    LOG_INFO("纹理已加载: %s", path.c_str());
    return ref;
}

sf::Font& ResourceManager::GetFont(const std::string& path) {
    auto it = fonts_.find(path);
    if (it != fonts_.end()) return *it->second;

    auto f = std::make_unique<sf::Font>();
    if (!f->loadFromFile(path)) {
        LOG_WARN("字体加载失败: %s（使用默认字体）", path.c_str());
        return defaultFont_;
    }
    sf::Font& ref = *f;
    fonts_[path] = std::move(f);
    return ref;
}

sf::SoundBuffer& ResourceManager::GetSoundBuffer(const std::string& path) {
    auto it = sounds_.find(path);
    if (it != sounds_.end()) return *it->second;

    auto sb = std::make_unique<sf::SoundBuffer>();
    if (!sb->loadFromFile(path)) {
        LOG_WARN("音频加载失败: %s（使用空缓冲）", path.c_str());
        return defaultSound_;
    }
    sf::SoundBuffer& ref = *sb;
    sounds_[path] = std::move(sb);
    return ref;
}

void ResourceManager::ReleaseUnused() {
    // 注意：调用者必须确保没有 sf::Sprite 等对象仍引用这些资源。
    // 建议在场景切换、所有渲染对象销毁后调用。
    textures_.clear();
    fonts_.clear();
    sounds_.clear();
    LOG_INFO("资源缓存已清空");
}

} // namespace cu
