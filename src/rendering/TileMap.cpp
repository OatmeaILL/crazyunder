#include "rendering/TileMap.h"
#include "rendering/Renderer.h"
#include "rendering/TextureAtlas.h"
#include "rendering/Camera.h"
#include "utils/Logger.h"
#include <cmath>

namespace cu {

// ============================================================================
// 构造函数
// ============================================================================
TileMap::TileMap() {
    visibleVertexArray_.setPrimitiveType(sf::Quads);
}

// ============================================================================
// Initialize —— 初始化：根据地牢数据构建顶点数组
// ============================================================================
void TileMap::Initialize(const TextureAtlas& atlas, const Dungeon& dungeon) {
    atlas_ = &atlas;
    dungeon_ = &dungeon; // 存储指针，引用原始地牢数据
    initialized_ = true;

    // 获取各 tile 类型在图集中的像素矩形
    tileRects_[static_cast<int>(TileType::Empty)]                = atlas.GetPixelRect("tile_floor"); // Empty 不渲染，但保留引用
    tileRects_[static_cast<int>(TileType::Floor)]                = atlas.GetPixelRect("tile_floor");
    tileRects_[static_cast<int>(TileType::Wall)]                 = atlas.GetPixelRect("tile_wall");
    tileRects_[static_cast<int>(TileType::Door)]                 = atlas.GetPixelRect("tile_door");
    tileRects_[static_cast<int>(TileType::Obstacle)]             = atlas.GetPixelRect("tile_obstacle");
    tileRects_[static_cast<int>(TileType::Stairs)]               = atlas.GetPixelRect("tile_stairs");
    tileRects_[static_cast<int>(TileType::Chest)]                = atlas.GetPixelRect("tile_chest");
    tileRects_[static_cast<int>(TileType::IndestructibleObstacle)] = atlas.GetPixelRect("tile_indestructible_obstacle");

    // 门打开状态的贴图矩形
    doorOpenRect_ = atlas.GetPixelRect("tile_door_open");

    verticesDirty_ = true;
    LOG_INFO("TileMap 已初始化: 地牢 %dx%d", dungeon_->width, dungeon_->height);
}

// ============================================================================
// Render —— 渲染：提交到 Renderer（作为 Background 层）
// ============================================================================
void TileMap::Render(Renderer& renderer, const Camera& camera) const {
    if (!initialized_ || !atlas_ || !dungeon_) return;

    // 检查视图是否变化（避免每帧重建）
    const sf::View& currentView = camera.GetView();
    sf::Vector2f currentCenter = currentView.getCenter();
    sf::Vector2f currentSize = currentView.getSize();
    sf::Vector2f lastCenter = lastView_.getCenter();
    sf::Vector2f lastSize = lastView_.getSize();

    // 若视图中心或尺寸变化超过 1 像素，重建顶点
    if (verticesDirty_ ||
        std::abs(currentCenter.x - lastCenter.x) > 1.f ||
        std::abs(currentCenter.y - lastCenter.y) > 1.f ||
        std::abs(currentSize.x - lastSize.x) > 1.f ||
        std::abs(currentSize.y - lastSize.y) > 1.f) {
        buildVisibleVertices(camera);
        lastView_ = currentView;
        verticesDirty_ = false;
    }

    // 提交到 Renderer（作为 Background 层，绕过命令队列直接绘制）
    // 将 vector<Vertex> 转换为 VertexArray 供 DrawRaw 使用
    if (!visibleVertices_.empty()) {
        visibleVertexArray_.clear();
        visibleVertexArray_.resize(visibleVertices_.size());
        for (std::size_t i = 0; i < visibleVertices_.size(); ++i) {
            visibleVertexArray_[i] = visibleVertices_[i];
        }
        const sf::Texture* tex = atlas_->GetTexture();
        renderer.DrawRaw(tex, visibleVertexArray_, Layer::Background);
    }
}

// ============================================================================
// buildVisibleVertices —— 构建可见区域的顶点数组
// ----------------------------------------------------------------------------
// 视野裁剪算法：
//   1. 获取 Camera 的视图边界（世界坐标）
//   2. 转换为 tile 坐标范围
//   3. 遍历可见 tile，为每个非 Empty tile 添加 4 个顶点（四边形）
//   4. 顶点包含：位置（世界坐标）、颜色（白色）、纹理坐标（图集 UV）
// ============================================================================
void TileMap::buildVisibleVertices(const Camera& camera) const {
    visibleVertices_.clear();
    visibleTileCount_ = 0;

    if (!atlas_ || !dungeon_) return;

    // 获取视图边界（世界坐标）
    const sf::View& view = camera.GetView();
    sf::Vector2f viewCenter = view.getCenter();
    sf::Vector2f viewSize = view.getSize();
    sf::FloatRect viewBounds(
        viewCenter.x - viewSize.x * 0.5f,
        viewCenter.y - viewSize.y * 0.5f,
        viewSize.x,
        viewSize.y
    );

    // 扩展边界 1 tile（避免边缘闪烁）
    float margin = static_cast<float>(kTileSize);
    viewBounds.left -= margin;
    viewBounds.top -= margin;
    viewBounds.width += margin * 2.f;
    viewBounds.height += margin * 2.f;

    // 转换为 tile 坐标范围
    sf::Vector2i minTile = dungeon_->WorldToTile(sf::Vector2f(viewBounds.left, viewBounds.top));
    sf::Vector2i maxTile = dungeon_->WorldToTile(
        sf::Vector2f(viewBounds.left + viewBounds.width, viewBounds.top + viewBounds.height)
    );

    // 裁剪到地牢范围
    minTile.x = std::max(0, minTile.x);
    minTile.y = std::max(0, minTile.y);
    maxTile.x = std::min(dungeon_->width - 1, maxTile.x);
    maxTile.y = std::min(dungeon_->height - 1, maxTile.y);

    // 获取图集纹理尺寸（用于 UV 计算）
    const sf::Texture* tex = atlas_->GetTexture();
    (void)tex; // tex 仅用于检查有效性，UV 使用像素坐标（SFML 自动归一化）

    // 预留空间（最坏情况：所有可见 tile 都非 Empty，部分需双层渲染）
    int tileCountX = maxTile.x - minTile.x + 1;
    int tileCountY = maxTile.y - minTile.y + 1;
    visibleVertices_.resize(static_cast<std::size_t>(tileCountX) * tileCountY * 8);

    std::size_t vertexIndex = 0;

    // 辅助：添加一个四边形到顶点数组
    auto addQuad = [&](float worldX, float worldY, float sz,
                       const sf::IntRect& srcRect) {
        if (vertexIndex + 3 >= visibleVertices_.size()) {
            visibleVertices_.resize(vertexIndex + 4);
        }
        float u0 = static_cast<float>(srcRect.left);
        float v0 = static_cast<float>(srcRect.top);
        float u1 = static_cast<float>(srcRect.left + srcRect.width);
        float v1 = static_cast<float>(srcRect.top + srcRect.height);
        visibleVertices_[vertexIndex + 0] = sf::Vertex(
            sf::Vector2f(worldX, worldY), sf::Color::White, sf::Vector2f(u0, v0));
        visibleVertices_[vertexIndex + 1] = sf::Vertex(
            sf::Vector2f(worldX + sz, worldY), sf::Color::White, sf::Vector2f(u1, v0));
        visibleVertices_[vertexIndex + 2] = sf::Vertex(
            sf::Vector2f(worldX + sz, worldY + sz), sf::Color::White, sf::Vector2f(u1, v1));
        visibleVertices_[vertexIndex + 3] = sf::Vertex(
            sf::Vector2f(worldX, worldY + sz), sf::Color::White, sf::Vector2f(u0, v1));
        vertexIndex += 4;
        ++visibleTileCount_;
    };

    for (int ty = minTile.y; ty <= maxTile.y; ++ty) {
        for (int tx = minTile.x; tx <= maxTile.x; ++tx) {
            TileType tile = dungeon_->GetTile(tx, ty);
            if (tile == TileType::Empty) continue;

            sf::IntRect srcRect = getTileRect(tile);
            if (srcRect.width <= 0 || srcRect.height <= 0) continue;

            // 门：根据开关状态选择不同贴图
            if (tile == TileType::Door) {
                const DoorState* ds = dungeon_->GetDoorState(tx, ty);
                if (ds && ds->open && doorOpenRect_.width > 0) {
                    srcRect = doorOpenRect_;
                }
            }

            // 计算 tile 的世界坐标（左上角）
            float worldX = tx * static_cast<float>(kTileSize) + dungeon_->worldOffset.x;
            float worldY = ty * static_cast<float>(kTileSize) + dungeon_->worldOffset.y;
            float size = static_cast<float>(kTileSize);

            // 对于"放置在地板上"的 tile（Obstacle/IndestructibleObstacle/Door/Stairs/Chest），
            // 先渲染一层 Floor 底色，避免背景透出 clear color
            if (tile == TileType::Obstacle || tile == TileType::IndestructibleObstacle ||
                tile == TileType::Door || tile == TileType::Stairs ||
                tile == TileType::Chest) {
                sf::IntRect floorRect = getTileRect(TileType::Floor);
                if (floorRect.width > 0 && floorRect.height > 0) {
                    addQuad(worldX, worldY, size, floorRect);
                }
            }

            // 渲染实际 tile
            addQuad(worldX, worldY, size, srcRect);
        }
    }

    // 裁剪到实际使用的顶点数
    visibleVertices_.resize(vertexIndex);
}

// ============================================================================
// getTileRect —— 获取 tile 类型对应的图集像素矩形
// ============================================================================
sf::IntRect TileMap::getTileRect(TileType type) const noexcept {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= 8) return sf::IntRect(0, 0, 0, 0);
    return tileRects_[idx];
}

// ============================================================================
// GetTileWorldBounds —— 获取 tile 的世界边界
// ============================================================================
sf::FloatRect TileMap::GetTileWorldBounds(int tileX, int tileY) const noexcept {
    return dungeon_->GetTileWorldBounds(tileX, tileY);
}

} // namespace cu
