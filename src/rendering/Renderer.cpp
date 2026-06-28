#include "rendering/Renderer.h"
#include "utils/Logger.h"
#include <algorithm>

namespace cu {

Renderer::Renderer() {
    // 预分配顶点缓冲，支持 5000 精灵（20000 顶点，约 320KB）
    // 避免每帧在 EndScene 中重新分配内存
    vertexBuffer_.reserve(20000);

    // 创建 1x1 白色纹理，用于无纹理四边形（DrawQuad）
    // 顶点颜色乘以白色纹理 = 顶点颜色本身，实现纯色绘制
    whiteTexture_.create(1, 1);
    sf::Image img;
    img.create(1, 1, sf::Color::White);
    whiteTexture_.update(img);
}

void Renderer::BeginScene(sf::RenderTarget& target, const Camera& camera) {
    target_ = &target;
    commands_.clear();
    ResetStats();
    // 应用摄像机视图
    target.setView(camera.GetView());
}

void Renderer::DrawSprite(const sf::Texture* texture, sf::Vector2f worldPos,
                          sf::IntRect sourceRect, sf::Color color,
                          sf::Vector2f scale, Layer layer) {
    DrawCommand cmd;
    cmd.texture = texture ? texture : &whiteTexture_;
    cmd.worldPos = worldPos;
    cmd.sourceRect = sourceRect;
    cmd.color = color;
    cmd.scale = scale;
    cmd.layer = layer;
    cmd.worldY = worldPos.y;
    commands_.push_back(cmd);
}

void Renderer::DrawQuad(sf::Vector2f worldPos, sf::Vector2f size,
                        sf::Color color, Layer layer) {
    DrawCommand cmd;
    cmd.texture = &whiteTexture_; // 白色纹理，颜色由顶点决定
    cmd.worldPos = worldPos;
    // sourceRect 设为 1x1，缩放后即为指定尺寸
    cmd.sourceRect = sf::IntRect(0, 0, 1, 1);
    cmd.color = color;
    cmd.scale = size; // 用 scale 表示四边形尺寸（1x1 纹理 * size = 目标尺寸）
    cmd.layer = layer;
    cmd.worldY = worldPos.y;
    commands_.push_back(cmd);
}

void Renderer::DrawRaw(const sf::Texture* texture, const sf::VertexArray& vertices,
                       Layer layer) {
    if (!target_ || vertices.getVertexCount() == 0) return;

    sf::RenderStates states;
    states.texture = texture ? texture : &whiteTexture_;
    target_->draw(vertices, states);

    // 更新统计（layer 仅用于记录，不影响绘制顺序）
    ++drawCallCount_;
    vertexCount_ += static_cast<int>(vertices.getVertexCount());
    (void)layer;
}

void Renderer::EndScene() {
    if (!target_ || commands_.empty()) return;

    // 1. 排序：先按 Layer，同层内按 worldY（Y 小的先绘制）
    std::sort(commands_.begin(), commands_.end(),
        [](const DrawCommand& a, const DrawCommand& b) {
            if (a.layer != b.layer) return a.layer < b.layer;
            return a.worldY < b.worldY;
        });

    // 2. 批量绘制：同纹理的连续命令合并为一个 VertexArray
    const sf::Texture* currentTexture = nullptr;
    auto& vertices = vertexBuffer_;  // 复用成员缓冲，避免每帧重新分配
    vertices.clear();                // 仅重置 size，不释放容量

    for (const auto& cmd : commands_) {
        if (cmd.texture != currentTexture) {
            // 纹理切换：刷新前一批
            if (!vertices.empty()) {
                flushBatch(currentTexture, vertices);
                vertices.clear();
            }
            currentTexture = cmd.texture;
        }

        // 计算四边形四个顶点（worldPos 为中心）
        float halfW = cmd.sourceRect.width * cmd.scale.x * 0.5f;
        float halfH = cmd.sourceRect.height * cmd.scale.y * 0.5f;
        float x = cmd.worldPos.x;
        float y = cmd.worldPos.y;

        // 计算 UV（SFML 的 texCoords 使用像素坐标，非归一化坐标）
        // SFML 的 RenderTarget::draw 会通过纹理矩阵将像素坐标转换为归一化坐标
        // 因此这里直接使用 sourceRect 的像素值，不要除以纹理尺寸
        float u0 = static_cast<float>(cmd.sourceRect.left);
        float v0 = static_cast<float>(cmd.sourceRect.top);
        float u1 = static_cast<float>(cmd.sourceRect.left + cmd.sourceRect.width);
        float v1 = static_cast<float>(cmd.sourceRect.top + cmd.sourceRect.height);

        // 四个顶点：左上、右上、右下、左下（sf::Quads 顺序）
        vertices.push_back(sf::Vertex(sf::Vector2f(x - halfW, y - halfH), cmd.color, sf::Vector2f(u0, v0)));
        vertices.push_back(sf::Vertex(sf::Vector2f(x + halfW, y - halfH), cmd.color, sf::Vector2f(u1, v0)));
        vertices.push_back(sf::Vertex(sf::Vector2f(x + halfW, y + halfH), cmd.color, sf::Vector2f(u1, v1)));
        vertices.push_back(sf::Vertex(sf::Vector2f(x - halfW, y + halfH), cmd.color, sf::Vector2f(u0, v1)));
    }

    // 刷新最后一批
    if (!vertices.empty()) {
        flushBatch(currentTexture, vertices);
    }
}

void Renderer::flushBatch(const sf::Texture* texture,
                          std::vector<sf::Vertex>& vertices) {
    if (!target_ || vertices.empty() || !texture) return;

    // 零拷贝绘制：直接用顶点指针，避免创建临时 sf::VertexArray
    sf::RenderStates states;
    states.texture = texture;
    target_->draw(vertices.data(), vertices.size(), sf::Quads, states);

    // 更新统计
    ++drawCallCount_;
    vertexCount_ += static_cast<int>(vertices.size());
}

void Renderer::ResetStats() noexcept {
    drawCallCount_ = 0;
    vertexCount_ = 0;
}

} // namespace cu
