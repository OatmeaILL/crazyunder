#pragma once

// ============================================================================
// Renderer —— 批量渲染器
// ----------------------------------------------------------------------------
// 核心原理：批量合并（Batching）
//   GPU 每次 Draw Call 都有固定开销（驱动状态切换、缓存刷新）。若每个精灵
//   单独 draw，2000 个精灵 = 2000 次 Draw Call，帧率暴跌。
//
//   批量渲染将使用相同纹理的精灵合并为单个 sf::VertexArray(sf::Quads)，
//   一次 Draw Call 绘制全部。每个精灵 = 4 个顶点（四角），包含位置、颜色、
//   纹理坐标。同纹理的精灵顶点连续存放，GPU 一次提交全部绘制。
//
//   关键：纹理切换会打断批量。因此同图集的精灵能完美合并为 1 次 Draw Call。
//
// Y-Sort（2.5D 遮挡处理）：
//   2.5D 游戏中精灵是平面贴图，但需要模拟深度遮挡。规则：世界 Y 坐标
//   越大（越靠下/越靠近屏幕底部）的精灵后绘制，从而覆盖 Y 较小的精灵。
//   这模拟了"近处遮挡远处"的透视效果。
//
//   实现方式：收集所有精灵命令后，按 (Layer, worldY) 排序，再按纹理分组
//   构建 VertexArray。同层内 Y 小的先填充顶点（先绘制），Y 大的后填充
//   （后绘制，覆盖前者）。
//
// 多层渲染：
//   Layer 枚举定义渲染顺序：Background < Tile < Entity < Particle < UI。
//   先渲染的层被后渲染的层覆盖。同层内按 Y-Sort。
// ============================================================================

#include <SFML/Graphics.hpp>
#include <vector>
#include "rendering/Camera.h"

namespace cu {

// 渲染层级（值小的先绘制，被值大的覆盖）
enum class Layer : uint8_t {
    Background = 0, // 背景层（天空、远景）
    Tile        = 1, // 瓦片层（地面）
    Entity      = 2, // 实体层（角色、敌人、道具）
    Particle    = 3, // 粒子层（特效）
    UI          = 4  // UI 层（界面）
};

class Renderer {
public:
    Renderer();
    ~Renderer() = default;

    // 开始场景：设置渲染目标与摄像机
    void BeginScene(sf::RenderTarget& target, const Camera& camera);

    // 入队精灵（worldPos 为精灵中心的世界坐标）
    void DrawSprite(const sf::Texture* texture, sf::Vector2f worldPos,
                    sf::IntRect sourceRect, sf::Color color,
                    sf::Vector2f scale, Layer layer = Layer::Entity);

    // 入队纯色四边形（调试用，使用内部 1x1 白色纹理）
    void DrawQuad(sf::Vector2f worldPos, sf::Vector2f size,
                  sf::Color color, Layer layer = Layer::Entity);

    // 直接绘制预构建的 VertexArray（绕过命令队列）
    // 用于 TileMap 等已批量化的几何体，必须 在 BeginScene 与 EndScene 之间调用
    // texture: 绑定的纹理（nullptr = 用白色纹理）
    // vertices: 预构建的顶点数组（sf::Quads）
    // layer: 渲染层级（仅用于统计，不参与排序）
    void DrawRaw(const sf::Texture* texture, const sf::VertexArray& vertices,
                 Layer layer = Layer::Background);

    // 结束场景：排序 + 批量绘制 + 统计
    void EndScene();

    // 统计信息
    [[nodiscard]] int GetDrawCallCount() const noexcept { return drawCallCount_; }
    [[nodiscard]] int GetVertexCount() const noexcept { return vertexCount_; }
    [[nodiscard]] int GetSpriteCount() const noexcept { return static_cast<int>(commands_.size()); }

    // 重置统计（每帧 BeginScene 自动调用）
    void ResetStats() noexcept;

    // 直接访问渲染目标（供 TileMap 等绕过命令队列直接绘制）
    [[nodiscard]] sf::RenderTarget* GetTarget() noexcept { return target_; }
    void IncrementDrawCallCount() noexcept { ++drawCallCount_; }
    void IncrementVertexCount(int count) noexcept { vertexCount_ += count; }

private:
    // 绘制命令（入队后排序、批量绘制）
    struct DrawCommand {
        const sf::Texture* texture;  // 纹理（nullptr = 用白色纹理）
        sf::Vector2f worldPos;       // 世界坐标（精灵中心）
        sf::IntRect sourceRect;      // 纹理源矩形（像素）
        sf::Color color;             // 顶点颜色（着色/透明度）
        sf::Vector2f scale;          // 缩放
        Layer layer;                 // 渲染层级
        float worldY;                // Y-Sort 排序键（= worldPos.y）
    };

    std::vector<DrawCommand> commands_;
    sf::RenderTarget* target_ = nullptr;

    // 内部 1x1 白色纹理（用于无纹理的四边形）
    sf::Texture whiteTexture_;

    // 预分配顶点缓冲（复用，避免每帧重新分配）
    // 支持 5000 精灵 = 20000 顶点，约 320KB
    std::vector<sf::Vertex> vertexBuffer_;

    // 统计
    int drawCallCount_ = 0;
    int vertexCount_ = 0;

    // 构建并绘制一个 VertexArray（同纹理的连续命令）
    void flushBatch(const sf::Texture* texture,
                    std::vector<sf::Vertex>& vertices);
};

} // namespace cu
