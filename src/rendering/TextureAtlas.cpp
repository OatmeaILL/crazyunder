#include "rendering/TextureAtlas.h"
#include "utils/Logger.h"
#include <fstream>
#include <sstream>

namespace cu {

bool TextureAtlas::AddImage(const std::string& name, const std::string& path) {
    if (nameIndex_.find(name) != nameIndex_.end()) {
        LOG_WARN("图集图片名称重复: %s", name.c_str());
        return false;
    }

    AtlasEntry entry;
    entry.name = name;
    if (!entry.source.loadFromFile(path)) {
        LOG_WARN("图集图片加载失败: %s (%s)", name.c_str(), path.c_str());
        return false;
    }

    auto idx = images_.size();
    images_.push_back(std::move(entry));
    nameIndex_[name] = idx;
    LOG_INFO("图集图片已添加: %s (%s)", name.c_str(), path.c_str());
    return true;
}

bool TextureAtlas::AddImageFromMemory(const std::string& name, const sf::Image& image) {
    if (nameIndex_.find(name) != nameIndex_.end()) {
        LOG_WARN("图集图片名称重复: %s", name.c_str());
        return false;
    }

    AtlasEntry entry;
    entry.name = name;
    if (!entry.source.loadFromImage(image)) {
        LOG_WARN("图集图片从内存加载失败: %s", name.c_str());
        return false;
    }

    auto idx = images_.size();
    images_.push_back(std::move(entry));
    nameIndex_[name] = idx;
    return true;
}

bool TextureAtlas::LoadManifest(const std::string& manifestPath) {
    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        LOG_WARN("图集清单文件无法打开: %s", manifestPath.c_str());
        return false;
    }

    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        ++lineNum;
        // 跳过空行与注释（# 开头）
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string name, path;
        if (!(iss >> name >> path)) {
            LOG_WARN("清单第 %d 行格式错误: %s", lineNum, line.c_str());
            continue;
        }
        (void)AddImage(name, path);
    }

    LOG_INFO("图集清单加载完成: %s (%zu 张图片)", manifestPath.c_str(), images_.size());
    return true;
}

bool TextureAtlas::Build() {
    if (images_.empty()) {
        LOG_WARN("图集为空，无法构建");
        return false;
    }

    // 计算总宽度与最大高度（横向排列算法）
    int totalWidth = 0;
    int maxHeight = 0;
    for (const auto& img : images_) {
        sf::Vector2u sz = img.source.getSize();
        totalWidth += static_cast<int>(sz.x);
        if (static_cast<int>(sz.y) > maxHeight) {
            maxHeight = static_cast<int>(sz.y);
        }
    }

    // 创建 CPU 侧图集 Image（避免 RenderTexture 的 FBO/blit 路径可能失效）
    sf::Image atlasImage;
    atlasImage.create(static_cast<unsigned>(totalWidth),
                      static_cast<unsigned>(maxHeight),
                      sf::Color(0, 0, 0, 0)); // 透明背景

    // 逐张将源纹理的像素复制到图集 Image
    int currentX = 0;
    for (auto& img : images_) {
        sf::Image srcImage = img.source.copyToImage();
        sf::Vector2u sz = img.source.getSize();
        atlasImage.copy(srcImage, static_cast<unsigned>(currentX), 0);

        img.pixelRect = sf::IntRect(currentX, 0, static_cast<int>(sz.x), static_cast<int>(sz.y));
        currentX += static_cast<int>(sz.x);
    }

    // 一次性上传到 GPU 纹理（loadFromImage 内部调用 glTexImage2D，数据必定在 GPU 上）
    if (!texture_.loadFromImage(atlasImage)) {
        LOG_ERROR("图集纹理 loadFromImage 失败: %dx%d", totalWidth, maxHeight);
        return false;
    }

    built_ = true;

    LOG_INFO("图集构建完成: %dx%d (%zu 张图片)", totalWidth, maxHeight, images_.size());
    return true;
}

sf::FloatRect TextureAtlas::GetUV(const std::string& name) const {
    auto it = nameIndex_.find(name);
    if (it == nameIndex_.end()) {
        LOG_WARN("图集中未找到图片: %s", name.c_str());
        return sf::FloatRect(0.f, 0.f, 1.f, 1.f); // 返回整张图集作为回退
    }

    const auto& entry = images_[it->second];
    sf::Vector2u atlasSize = texture_.getSize();
    if (atlasSize.x == 0 || atlasSize.y == 0) {
        return sf::FloatRect(0.f, 0.f, 1.f, 1.f);
    }

    float fw = 1.f / static_cast<float>(atlasSize.x);
    float fh = 1.f / static_cast<float>(atlasSize.y);
    return sf::FloatRect(
        entry.pixelRect.left * fw,
        entry.pixelRect.top * fh,
        entry.pixelRect.width * fw,
        entry.pixelRect.height * fh
    );
}

sf::IntRect TextureAtlas::GetPixelRect(const std::string& name) const {
    auto it = nameIndex_.find(name);
    if (it == nameIndex_.end()) {
        return sf::IntRect(0, 0, 0, 0);
    }
    return images_[it->second].pixelRect;
}

void TextureAtlas::Clear() {
    images_.clear();
    nameIndex_.clear();
    built_ = false;
}

} // namespace cu
