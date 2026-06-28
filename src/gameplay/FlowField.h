#pragma once

// ============================================================================
// FlowField —— 流场寻路（Flow Field Pathfinding）
// ----------------------------------------------------------------------------
// 核心思想：
//   传统 A* 寻路为每个敌人单独计算路径，500 个敌人 = 500 次 A*，开销巨大。
//   流场寻路只需一次计算：以目标点（玩家位置）为源，反向扩散计算每个网格
//   单元格到目标的最小代价（距离场），然后为每个单元格生成指向最优邻居的
//   方向向量（方向场）。所有敌人共享同一方向场，只需查询自身所在单元格的
//   方向即可获得移动指引。
//
// 相比 A* 的优势：
//   - 一次计算，全局共享：500 敌人共享同一流场，AI 查询 O(1)。
//   - 天然平滑：方向场是连续向量，敌人移动自然平滑，无路径折角。
//   - 动态适应：目标移动后重算流场（每 0.5s），所有敌人自动跟随新路径。
//   - 内存友好：方向场是固定大小网格，无需存储路径节点列表。
//
// 算法：Dijkstra 反向扩散
//   1. 将目标单元格距离设为 0，加入优先队列（或 BFS 队列，因边权均为 1）。
//   2. 从队列取出当前单元格，遍历其 8 个邻居：
//      - 若邻居未被访问或当前路径更短，更新邻居距离 = 当前距离 + 1（或 √2 对角线）。
//      - 将邻居加入队列。
//   3. 重复直到队列空。障碍单元格距离设为无穷大，不参与扩散。
//   4. 方向场：对每个非障碍单元格，找到 8 邻居中距离最小的，方向 = 指向该邻居的向量。
//
// 性能目标：
//   - 100×100 网格重算 < 5ms（BFS 队列，O(W*H)）
//   - GetDirection 查询 O(1)
// ============================================================================

#include <SFML/System/Vector2.hpp>
#include <vector>
#include <cstdint>
#include <queue>

namespace cu {

class FlowField {
public:
    FlowField();
    ~FlowField() = default;

    // 初始化流场网格
    // worldWidth/worldHeight: 世界尺寸（像素）
    // cellSize: 网格单元大小（像素，如 32px）
    void Initialize(float worldWidth, float worldHeight, float cellSize);

    // 设置目标点（玩家位置），触发距离场与方向场重算
    // worldPos: 目标世界坐标
    void SetTarget(sf::Vector2f worldPos);

    // 获取某世界坐标处的移动方向（归一化向量）
    // 若该位置不可达或为障碍，返回零向量
    [[nodiscard]] sf::Vector2f GetDirection(sf::Vector2f worldPos) const;

    // 设置/清除某单元格的阻挡状态（墙壁）
    // cellX/cellY: 单元格索引（非世界坐标）
    void SetBlocked(int cellX, int cellY, bool blocked);

    // 检查某单元格是否被阻挡
    [[nodiscard]] bool IsBlocked(int cellX, int cellY) const;

    // ---- 调试/统计接口 ----
    [[nodiscard]] int GetGridWidth() const noexcept { return gridWidth_; }
    [[nodiscard]] int GetGridHeight() const noexcept { return gridHeight_; }
    [[nodiscard]] float GetCellSize() const noexcept { return cellSize_; }

    // 获取距离场（调试可视化用）
    [[nodiscard]] const std::vector<float>& GetDistanceField() const noexcept { return distanceField_; }
    // 获取方向场（调试可视化用）
    [[nodiscard]] const std::vector<sf::Vector2f>& GetDirectionField() const noexcept { return directionField_; }

    // 上一次重算耗时（毫秒，调试用）
    [[nodiscard]] float GetLastRecomputeTime() const noexcept { return lastRecomputeTimeMs_; }

private:
    // 世界坐标 → 单元格索引
    [[nodiscard]] int worldToCellX(float x) const noexcept;
    [[nodiscard]] int worldToCellY(float y) const noexcept;

    // 单元格索引 → 一维数组下标
    [[nodiscard]] int cellIndex(int cx, int cy) const noexcept;

    // 检查单元格索引是否有效
    [[nodiscard]] bool isValidCell(int cx, int cy) const noexcept;

    // Dijkstra/BFS 反向扩散计算距离场
    void computeDistanceField(int targetCellX, int targetCellY);

    // 根据距离场计算方向场（每个单元格指向距离最小的邻居）
    void computeDirectionField();

private:
    float worldWidth_ = 0.f;
    float worldHeight_ = 0.f;
    float cellSize_ = 32.f;

    int gridWidth_ = 0;   // 列数
    int gridHeight_ = 0;  // 行数

    // 距离场：每个单元格到目标的最小代价
    // 使用 float 而非 int 以支持对角线代价 √2
    std::vector<float> distanceField_;

    // 方向场：每个单元格的移动方向（归一化向量）
    std::vector<sf::Vector2f> directionField_;

    // 阻挡标记：true 表示该单元格不可通行
    std::vector<bool> blocked_;

    // 目标单元格索引
    int targetCellX_ = -1;
    int targetCellY_ = -1;

    // 上一次重算耗时（毫秒）
    float lastRecomputeTimeMs_ = 0.f;

    // 无穷大距离常量（表示不可达）
    static constexpr float kInfinity = 1e30f;
};

} // namespace cu
