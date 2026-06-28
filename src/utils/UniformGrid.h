#pragma once

// ============================================================================
// UniformGrid —— 均匀网格空间划分
// ----------------------------------------------------------------------------
// 核心原理：将连续世界空间离散化为固定大小的二维单元格网格。
//   每个实体根据其世界坐标被分配到对应的单元格。查询某实体附近的邻居时，
//   只需检查该实体所在单元格及相邻单元格（通常 3×3 范围），而非遍历所有实体。
//
// 为什么空间划分能大幅降低碰撞检测复杂度？
//   假设有 N=10000 个实体，每帧需要做碰撞检测：
//     - 暴力法：两两比较，O(N²) = 10^8 次比较，约 100ms，帧率暴跌。
//     - 均匀网格：每个实体插入到对应单元格 O(1)，查询邻居只检查 3×3 单元格，
//       若实体均匀分布，每单元格约 N/(W*H) 个实体，3×3 查询约 9*N/(W*H) 次。
//       当网格足够大时，查询复杂度接近 O(N)，10000 实体 < 1ms。
//
// 单元格大小选择：
//   - 太小：查询时需检查更多单元格，且单元格数量爆炸（内存浪费）。
//   - 太大：每单元格实体过多，查询退化为暴力法。
//   - 经验值：单元格边长 ≈ 查询半径的 1~2 倍。割草游戏敌人半径约 16px，
//     查询半径约 32px，故单元格设为 64px 较优。
//
// 性能目标：
//   - 10000 实体插入 + 1000 次范围查询 < 1ms
//   - Clear() 仅清空每单元格的 vector 内容（保留容量），避免反复分配
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include <vector>
#include <cstdint>
#include "ecs/Entity.h"

namespace cu {

class UniformGrid {
public:
    // 构造：传入世界范围（左上角 + 宽高）与单元格大小
    // worldX/worldY: 世界左上角坐标（通常为 -worldW/2, -worldH/2）
    // worldW/worldH: 世界宽高
    // cellSize: 单元格边长（像素）
    UniformGrid(float worldX, float worldY, float worldW, float worldH, float cellSize);

    // 默认构造：需后续调用 Resize 配置范围
    UniformGrid();

    // 配置/重新配置世界范围与单元格大小
    void Resize(float worldX, float worldY, float worldW, float worldH, float cellSize);

    // 每帧清空所有单元格（保留容量，避免反复堆分配）
    void Clear() noexcept;

    // 插入实体到对应单元格
    // position: 实体世界坐标
    // 超出世界范围的实体会被 clamp 到边界单元格
    void Insert(EntityId id, sf::Vector2f position);

    // 查询圆形范围内的所有实体
    // center: 圆心世界坐标, radius: 半径
    // out: 输出结果向量（追加，不清空调用者的内容）
    void QueryRange(sf::Vector2f center, float radius, std::vector<EntityId>& out) const;

    // 查询某点所在单元格及 8 个相邻单元格（3×3）的所有实体
    // pos: 查询点世界坐标
    // out: 输出结果向量（追加）
    void QueryPoint(sf::Vector2f pos, std::vector<EntityId>& out) const;

    // ---- 调试/统计接口 ----
    [[nodiscard]] int GetCols() const noexcept { return cols_; }
    [[nodiscard]] int GetRows() const noexcept { return rows_; }
    [[nodiscard]] float GetCellSize() const noexcept { return cellSize_; }
    [[nodiscard]] std::size_t GetTotalEntityCount() const noexcept;

private:
    // 将世界坐标转换为单元格索引
    [[nodiscard]] int worldToCellX(float x) const noexcept;
    [[nodiscard]] int worldToCellY(float y) const noexcept;

    // 将一维单元格索引（row * cols_ + col）转换为单元格在 cells_ 中的下标
    [[nodiscard]] int cellIndex(int cellX, int cellY) const noexcept;

    // 检查单元格索引是否在有效范围内
    [[nodiscard]] bool isValidCell(int cellX, int cellY) const noexcept;

    // 收集指定单元格的所有实体（边界检查）
    void collectCell(int cellX, int cellY, std::vector<EntityId>& out) const;

private:
    float worldX_ = 0.f;       // 世界左上角 X
    float worldY_ = 0.f;       // 世界左上角 Y
    float worldW_ = 0.f;       // 世界宽
    float worldH_ = 0.f;       // 世界高
    float cellSize_ = 64.f;    // 单元格边长

    int cols_ = 0;             // 列数 = ceil(worldW / cellSize)
    int rows_ = 0;             // 行数 = ceil(worldH / cellSize)

    // 一维数组存储所有单元格，每个单元格是一个 EntityId 向量
    // 使用一维数组而非二维，减少内存分配次数，提高缓存局部性
    std::vector<std::vector<EntityId>> cells_;
};

} // namespace cu
