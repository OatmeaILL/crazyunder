#pragma once

// ============================================================================
// TextureAtlas —— 运行时纹理图集
// ----------------------------------------------------------------------------
// 为什么用纹理图集（Texture Atlas）？
//   GPU 每次切换纹理（Draw Call 切换绑定纹理）都有开销：驱动验证状态、
//   刷新缓存等。若 2000 个精灵各用独立纹理，需要 2000 次 Draw Call，
//   每次 ~0.1ms 就要 200ms，完全无法 60FPS。
//
//   纹理图集将多张小图合并为一张大图，所有精灵引用同一张纹理的不同区域
//   （通过 UV 坐标区分）。Renderer 可以将同图集的所有精灵合并为单个
//   VertexArray，一次 Draw Call 绘制全部，Draw Call 从 2000 降到 1。
//
// 打包算法（本实现：横向排列）：
//   将所有图片从左到右排成一行，总宽度 = 各图宽度之和，高度 = 最高图。
//   简单但有效，适合演示。生产环境可用矩形打包算法（如 Shelf、Skyline、
//   MaxRects）减少空白区域。
//
// UV 坐标：
//   归一化到 [0,1] 的纹理坐标。sf::FloatRect(left, top, width, height)
//   表示图集中某子区域的归一化位置与尺寸。
// ============================================================================

#include <SFML/Graphics.hpp>
#include <string>
#include <unordered_map>

namespace cu {

class TextureAtlas {
public:
    TextureAtlas() = default;
    ~TextureAtlas() = default;

    // 添加图片到图集（加载后记录尺寸，Build 时统一打包）
    // name: 逻辑名称，path: 文件路径
    [[nodiscard]] bool AddImage(const std::string& name, const std::string& path);

    // 从内存图像添加到图集（用于过程化生成的纹理，无需文件 IO）
    [[nodiscard]] bool AddImageFromMemory(const std::string& name, const sf::Image& image);

    // 从清单文件加载（每行格式：name path）
    [[nodiscard]] bool LoadManifest(const std::string& manifestPath);

    // 构建图集：将所有已添加的图片打包为单张大纹理
    // 必须在 AddImage/LoadManifest 之后、GetUV/GetTexture 之前调用
    [[nodiscard]] bool Build();

    // 获取某图片在图集中的 UV 坐标（归一化）
    [[nodiscard]] sf::FloatRect GetUV(const std::string& name) const;

    // 获取某图片在图集中的像素矩形
    [[nodiscard]] sf::IntRect GetPixelRect(const std::string& name) const;

    // 获取打包后的最终纹理
    [[nodiscard]] const sf::Texture* GetTexture() const noexcept { return &texture_; }

    // 获取图集中图片数量
    [[nodiscard]] std::size_t GetImageCount() const noexcept { return images_.size(); }

    // 清空所有数据
    void Clear();

private:
    struct AtlasEntry {
        std::string name;
        sf::Texture source;  // 源纹理（临时，Build 后可释放）
        sf::IntRect pixelRect; // 在图集中的像素位置
    };

    std::vector<AtlasEntry> images_;
    std::unordered_map<std::string, std::size_t> nameIndex_;
    sf::Texture texture_; // 最终打包纹理
    bool built_ = false;
};

} // namespace cu
