#include "gameplay/FlowField.h"
#include "utils/Logger.h"
#include <cmath>
#include <chrono>
#include <algorithm>

namespace cu {

FlowField::FlowField() = default;

void FlowField::Initialize(float worldWidth, float worldHeight, float cellSize) {
    worldWidth_ = worldWidth;
    worldHeight_ = worldHeight;
    cellSize_ = cellSize;

    // 计算网格尺寸，向上取整
    gridWidth_ = static_cast<int>(std::ceil(worldWidth_ / cellSize_));
    gridHeight_ = static_cast<int>(std::ceil(worldHeight_ / cellSize_));
    if (gridWidth_ < 1) gridWidth_ = 1;
    if (gridHeight_ < 1) gridHeight_ = 1;

    std::size_t totalCells = static_cast<std::size_t>(gridWidth_) * static_cast<std::size_t>(gridHeight_);
    distanceField_.assign(totalCells, kInfinity);
    directionField_.assign(totalCells, sf::Vector2f(0.f, 0.f));
    blocked_.assign(totalCells, false);

    LOG_INFO("流场已初始化: %dx%d 网格, cellSize=%.0f", gridWidth_, gridHeight_, cellSize_);
}

void FlowField::SetTarget(sf::Vector2f worldPos) {
    int cx = worldToCellX(worldPos.x);
    int cy = worldToCellY(worldPos.y);
    if (!isValidCell(cx, cy)) return;

    // 若目标单元格未变化，跳过重算（避免每帧重复计算）
    if (cx == targetCellX_ && cy == targetCellY_) return;
    targetCellX_ = cx;
    targetCellY_ = cy;

    auto startTime = std::chrono::steady_clock::now();

    computeDistanceField(cx, cy);
    computeDirectionField();

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    lastRecomputeTimeMs_ = static_cast<float>(duration.count()) / 1000.f;
}

sf::Vector2f FlowField::GetDirection(sf::Vector2f worldPos) const {
    int cx = worldToCellX(worldPos.x);
    int cy = worldToCellY(worldPos.y);
    if (!isValidCell(cx, cy)) return sf::Vector2f(0.f, 0.f);
    return directionField_[cellIndex(cx, cy)];
}

void FlowField::SetBlocked(int cellX, int cellY, bool blocked) {
    if (!isValidCell(cellX, cellY)) return;
    blocked_[cellIndex(cellX, cellY)] = blocked;
}

bool FlowField::IsBlocked(int cellX, int cellY) const {
    if (!isValidCell(cellX, cellY)) return true;
    return blocked_[cellIndex(cellX, cellY)];
}

int FlowField::worldToCellX(float x) const noexcept {
    // 世界坐标原点在 (0,0)，但世界范围可能为 [-W/2, W/2]
    // 这里假设世界坐标从 0 开始，若实际从负数开始需调整
    // 为兼容以 (0,0) 为中心的世界，将坐标偏移 worldWidth/2
    float adjustedX = x + worldWidth_ * 0.5f;
    int cx = static_cast<int>(adjustedX / cellSize_);
    if (cx < 0) cx = 0;
    if (cx >= gridWidth_) cx = gridWidth_ - 1;
    return cx;
}

int FlowField::worldToCellY(float y) const noexcept {
    float adjustedY = y + worldHeight_ * 0.5f;
    int cy = static_cast<int>(adjustedY / cellSize_);
    if (cy < 0) cy = 0;
    if (cy >= gridHeight_) cy = gridHeight_ - 1;
    return cy;
}

int FlowField::cellIndex(int cx, int cy) const noexcept {
    return cy * gridWidth_ + cx;
}

bool FlowField::isValidCell(int cx, int cy) const noexcept {
    return cx >= 0 && cx < gridWidth_ && cy >= 0 && cy < gridHeight_;
}

// ============================================================================
// computeDistanceField —— Dijkstra/BFS 反向扩散计算距离场
// ----------------------------------------------------------------------------
// 从目标单元格开始，使用 BFS 队列向四周扩散。
// 因为所有边权为 1（或对角线为 √2），BFS 天然按距离递增顺序访问，
// 等价于 Dijkstra 但无需优先队列，效率更高。
//
// 8 方向邻居：上、下、左、右（代价 1）+ 四个对角线（代价 √2）
// 对角线移动需检查是否"角切"（corner cutting）：若两个正交邻居均为障碍，
// 则不允许对角线穿过（避免穿越墙角）。
// ============================================================================
void FlowField::computeDistanceField(int targetCellX, int targetCellY) {
    // 重置距离场
    std::fill(distanceField_.begin(), distanceField_.end(), kInfinity);

    // BFS 队列：存储待处理的单元格坐标
    // 使用 std::queue<pair<int,int>>，简单可靠
    std::queue<std::pair<int, int>> q;

    // 目标单元格距离为 0
    int targetIdx = cellIndex(targetCellX, targetCellY);
    distanceField_[targetIdx] = 0.f;
    q.push({targetCellX, targetCellY});

    // 8 方向偏移与对应代价
    // 顺序：右、左、下、上、右下、左下、右上、左上
    static const int dx[8] = {1, -1, 0, 0, 1, -1, 1, -1};
    static const int dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
    static const float cost[8] = {1.f, 1.f, 1.f, 1.f,
                                  1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f};

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        float currentDist = distanceField_[cellIndex(cx, cy)];

        // 遍历 8 个邻居
        for (int i = 0; i < 8; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            // 越界检查
            if (!isValidCell(nx, ny)) continue;

            // 障碍检查
            if (blocked_[cellIndex(nx, ny)]) continue;

            // 对角线移动的角切检查：
            // 若两个正交邻居均为障碍，则不允许对角线穿过
            if (i >= 4) { // 对角线方向
                int ax = cx + dx[i]; // 水平邻居
                int ay = cy;
                int bx = cx;          // 垂直邻居
                int by = cy + dy[i];
                if (isValidCell(ax, ay) && isValidCell(bx, by)) {
                    if (blocked_[cellIndex(ax, ay)] && blocked_[cellIndex(bx, by)]) {
                        continue; // 角切，跳过
                    }
                }
            }

            float newDist = currentDist + cost[i];
            int nIdx = cellIndex(nx, ny);
            if (newDist < distanceField_[nIdx]) {
                distanceField_[nIdx] = newDist;
                q.push({nx, ny});
            }
        }
    }
}

// ============================================================================
// computeDirectionField —— 根据距离场计算方向场
// ----------------------------------------------------------------------------
// 对每个非障碍单元格，找到 8 邻居中距离最小的，方向 = 指向该邻居的向量。
// 若所有邻居距离都 >= 当前单元格距离（局部最小值，如目标点本身），方向为零。
// ============================================================================
void FlowField::computeDirectionField() {
    static const int dx[8] = {1, -1, 0, 0, 1, -1, 1, -1};
    static const int dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
    // 对角线方向需归一化（对角线向量 (1,1) 长度为 √2，归一化后为 (0.707, 0.707)）
    static const float dirX[8] = {1.f, -1.f, 0.f, 0.f,
                                  0.70710678f, -0.70710678f, 0.70710678f, -0.70710678f};
    static const float dirY[8] = {0.f, 0.f, 1.f, -1.f,
                                  0.70710678f, 0.70710678f, -0.70710678f, -0.70710678f};

    for (int cy = 0; cy < gridHeight_; ++cy) {
        for (int cx = 0; cx < gridWidth_; ++cx) {
            int idx = cellIndex(cx, cy);
            if (blocked_[idx]) {
                directionField_[idx] = sf::Vector2f(0.f, 0.f);
                continue;
            }

            float currentDist = distanceField_[idx];
            if (currentDist >= kInfinity) {
                // 不可达单元格
                directionField_[idx] = sf::Vector2f(0.f, 0.f);
                continue;
            }

            // 找到距离最小的邻居
            float minDist = currentDist;
            int bestDir = -1;
            for (int i = 0; i < 8; ++i) {
                int nx = cx + dx[i];
                int ny = cy + dy[i];
                if (!isValidCell(nx, ny)) continue;
                if (blocked_[cellIndex(nx, ny)]) continue;
                float nDist = distanceField_[cellIndex(nx, ny)];
                if (nDist < minDist) {
                    minDist = nDist;
                    bestDir = i;
                }
            }

            if (bestDir >= 0) {
                directionField_[idx] = sf::Vector2f(dirX[bestDir], dirY[bestDir]);
            } else {
                // 目标点本身或局部最小值，方向为零
                directionField_[idx] = sf::Vector2f(0.f, 0.f);
            }
        }
    }
}

} // namespace cu
