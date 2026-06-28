#pragma once

// ============================================================================
// TileMap —— 高效 Tile 地图渲染
// ----------------------------------------------------------------------------
// 核心原理：批量合并（Batching）
//   将所有 Tile 合并为单个 sf::VertexArray(sf::Quads)，一次 Draw Call 绘制全部。
//   每个 Tile = 4 个顶点（四角），包含位置、颜色、纹理坐标。
//   所有 Tile 使用同一图集纹理，完美合并为 1 次 Draw Call。
//
// 视野裁剪（Camera Culling）：
//   只构建可见区域内的 Tile 顶点，减少顶点数与 GPU 带宽。
//   每帧根据 Camera 的视图边界重新构建可见 Tile 的 VertexArray。
//
// Tile 贴图映射：
//   Floor                  → "tile_floor"                  暗灰石砖纹理
//   Wall                   → "tile_wall"                   亮灰砖块（顶部高亮）
//   Door                   → "tile_door"                   木门
//   Obstacle               → "tile_obstacle"               木桶
//   Stairs                 → "tile_stairs"                 下楼楼梯
//   Chest                  → "tile_chest"                  宝箱
//   IndestructibleObstacle → "tile_indestructible_obstacle" 不可破坏石柱
// ============================================================================

#include <SFML/Graphics.hpp>
#include <vector>
#include "gameplay/DungeonGenerator.h"

namespace cu {

class TextureAtlas;
class Renderer;
class Camera;

class TileMap {
public:
    TileMap();
    ~TileMap() = default;

    // 初始化：根据地牢数据构建顶点数组
    // atlas: 已构建的纹理图集（需包含 tile_* 贴图）
    // dungeon: 地牢数据
    void Initialize(const TextureAtlas& atlas, const Dungeon& dungeon);

    // 渲染：提交到 Renderer（作为 Background 层，不参与 Y-Sort）
    // renderer: 渲染器
    // camera: 摄像机（用于视野裁剪）
    void Render(Renderer& renderer, const Camera& camera) const;

    // 获取 tile 的世界边界
    [[nodiscard]] sf::FloatRect GetTileWorldBounds(int tileX, int tileY) const noexcept;

    // 获取地牢引用
    [[nodiscard]] const Dungeon& GetDungeon() const noexcept { return *dungeon_; }

    // 是否已初始化
    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }

    // 获取可见 tile 数量（调试用）
    [[nodiscard]] std::size_t GetVisibleTileCount() const noexcept { return visibleTileCount_; }

    // 标记顶点缓存为脏（tile 改变后调用，确保下帧重建顶点）
    void MarkDirty() noexcept { verticesDirty_ = true; }

private:
    // 构建可见区域的顶点数组
    void buildVisibleVertices(const Camera& camera) const;

    // 获取 tile 类型对应的图集像素矩形
    [[nodiscard]] sf::IntRect getTileRect(TileType type) const noexcept;

    const TextureAtlas* atlas_ = nullptr;
    const Dungeon* dungeon_ = nullptr; // 地牢数据指针（引用 Game::dungeon_，避免副本不同步）
    bool initialized_ = false;

    // 图集中各 tile 类型的像素矩形
    sf::IntRect tileRects_[8]; // 索引 = TileType 枚举值

    // 门打开状态的贴图矩形（与 tileRects_[Door] 区分）
    sf::IntRect doorOpenRect_;

    // 可变缓存（mutable 允许 const 方法修改）
    mutable std::vector<sf::Vertex> visibleVertices_; // 可见区域的顶点数据
    mutable sf::VertexArray visibleVertexArray_;       // 用于 DrawRaw 的 VertexArray
    mutable sf::View lastView_;               // 上次构建时的视图（用于检测变化）
    mutable bool verticesDirty_ = true;       // 顶点是否需要重建
    mutable std::size_t visibleTileCount_ = 0; // 可见 tile 数量
};

} // namespace cu
