#include "utils/UniformGrid.h"
#include <cmath>
#include <algorithm>

namespace cu {

UniformGrid::UniformGrid(float worldX, float worldY, float worldW, float worldH, float cellSize)
    : worldX_(worldX), worldY_(worldY), worldW_(worldW), worldH_(worldH), cellSize_(cellSize) {
    // 计算网格行列数，向上取整确保覆盖整个世界范围
    cols_ = static_cast<int>(std::ceil(worldW_ / cellSize_));
    rows_ = static_cast<int>(std::ceil(worldH_ / cellSize_));
    if (cols_ < 1) cols_ = 1;
    if (rows_ < 1) rows_ = 1;
    // 预分配所有单元格（空 vector，Insert 时按需增长）
    cells_.resize(static_cast<std::size_t>(cols_) * static_cast<std::size_t>(rows_));
}

UniformGrid::UniformGrid() = default;

void UniformGrid::Resize(float worldX, float worldY, float worldW, float worldH, float cellSize) {
    worldX_ = worldX;
    worldY_ = worldY;
    worldW_ = worldW;
    worldH_ = worldH;
    cellSize_ = cellSize;
    cols_ = static_cast<int>(std::ceil(worldW_ / cellSize_));
    rows_ = static_cast<int>(std::ceil(worldH_ / cellSize_));
    if (cols_ < 1) cols_ = 1;
    if (rows_ < 1) rows_ = 1;
    cells_.clear();
    cells_.resize(static_cast<std::size_t>(cols_) * static_cast<std::size_t>(rows_));
}

void UniformGrid::Clear() noexcept {
    // 仅清空每个单元格的内容，保留已分配容量
    // 这样下一帧 Insert 时不会触发 vector 重新分配，性能稳定
    for (auto& cell : cells_) {
        cell.clear();
    }
}

int UniformGrid::worldToCellX(float x) const noexcept {
    // 世界坐标 → 单元格列索引：减去世界原点，除以单元格大小
    int cx = static_cast<int>((x - worldX_) / cellSize_);
    // clamp 到有效范围，防止越界
    if (cx < 0) cx = 0;
    if (cx >= cols_) cx = cols_ - 1;
    return cx;
}

int UniformGrid::worldToCellY(float y) const noexcept {
    int cy = static_cast<int>((y - worldY_) / cellSize_);
    if (cy < 0) cy = 0;
    if (cy >= rows_) cy = rows_ - 1;
    return cy;
}

int UniformGrid::cellIndex(int cellX, int cellY) const noexcept {
    return cellY * cols_ + cellX;
}

bool UniformGrid::isValidCell(int cellX, int cellY) const noexcept {
    return cellX >= 0 && cellX < cols_ && cellY >= 0 && cellY < rows_;
}

void UniformGrid::collectCell(int cellX, int cellY, std::vector<EntityId>& out) const {
    if (!isValidCell(cellX, cellY)) return;
    const auto& cell = cells_[cellIndex(cellX, cellY)];
    // 追加该单元格所有实体到输出
    out.insert(out.end(), cell.begin(), cell.end());
}

void UniformGrid::Insert(EntityId id, sf::Vector2f position) {
    int cx = worldToCellX(position.x);
    int cy = worldToCellY(position.y);
    if (!isValidCell(cx, cy)) return;
    cells_[cellIndex(cx, cy)].push_back(id);
}

void UniformGrid::QueryRange(sf::Vector2f center, float radius, std::vector<EntityId>& out) const {
    // 计算查询圆的外接矩形对应的单元格范围
    int minCX = worldToCellX(center.x - radius);
    int minCY = worldToCellY(center.y - radius);
    int maxCX = worldToCellX(center.x + radius);
    int maxCY = worldToCellY(center.y + radius);

    // clamp 到有效范围
    if (minCX < 0) minCX = 0;
    if (minCY < 0) minCY = 0;
    if (maxCX >= cols_) maxCX = cols_ - 1;
    if (maxCY >= rows_) maxCY = rows_ - 1;

    // 遍历覆盖范围内的所有单元格
    // 注意：这里先收集所有候选实体，精确的距离过滤由调用者完成
    // （空间网格的职责是缩小候选集，而非精确碰撞判断）
    for (int cy = minCY; cy <= maxCY; ++cy) {
        for (int cx = minCX; cx <= maxCX; ++cx) {
            const auto& cell = cells_[cellIndex(cx, cy)];
            out.insert(out.end(), cell.begin(), cell.end());
        }
    }
}

void UniformGrid::QueryPoint(sf::Vector2f pos, std::vector<EntityId>& out) const {
    // 查询点所在单元格及 8 个相邻单元格（3×3 范围）
    int cx = worldToCellX(pos.x);
    int cy = worldToCellY(pos.y);

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            collectCell(cx + dx, cy + dy, out);
        }
    }
}

std::size_t UniformGrid::GetTotalEntityCount() const noexcept {
    std::size_t total = 0;
    for (const auto& cell : cells_) {
        total += cell.size();
    }
    return total;
}

} // namespace cu
