#include "utils/TextureGenerator.h"
#include "utils/Logger.h"
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace cu {

sf::Image TextureGenerator::CreateColorBlock(
    sf::Color fillColor, sf::Color borderColor, int borderWidth) {
    const int size = 32;
    sf::Image img;
    img.create(size, size, fillColor);

    // 绘制边框：上下左右 borderWidth 像素
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            if (x < borderWidth || x >= size - borderWidth ||
                y < borderWidth || y >= size - borderWidth) {
                img.setPixel(x, y, borderColor);
            }
        }
    }
    return img;
}

sf::Image TextureGenerator::CreatePlayerPlaceholder(
    sf::Color fillColor, sf::Color borderColor) {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0)); // 透明背景

    // 绘制三角形（指向上方）：顶点在 (16, 4)，底边在 y=28
    // 用扫描线填充：对每一行 y，计算三角形左右边界
    int topY = 4;
    int bottomY = 28;
    int centerX = size / 2;
    int halfBase = 12; // 底边半宽

    for (int y = topY; y <= bottomY; ++y) {
        // 线性插值计算当前行的半宽
        float t = static_cast<float>(y - topY) / (bottomY - topY);
        int hw = static_cast<int>(t * halfBase);
        for (int x = centerX - hw; x <= centerX + hw; ++x) {
            if (x >= 0 && x < size) {
                img.setPixel(x, y, fillColor);
            }
        }
    }

    // 绘制三角形边框（简单描边：检查像素是否为边界）
    for (int y = topY; y <= bottomY; ++y) {
        float t = static_cast<float>(y - topY) / (bottomY - topY);
        int hw = static_cast<int>(t * halfBase);
        int left = centerX - hw;
        int right = centerX + hw;
        if (left >= 0 && left < size) img.setPixel(left, y, borderColor);
        if (right >= 0 && right < size) img.setPixel(right, y, borderColor);
    }
    // 顶点与底边
    for (int x = centerX - 1; x <= centerX + 1; ++x) {
        if (x >= 0 && x < size) img.setPixel(x, topY, borderColor);
    }
    for (int x = centerX - halfBase; x <= centerX + halfBase; ++x) {
        if (x >= 0 && x < size) img.setPixel(x, bottomY, borderColor);
    }

    return img;
}

sf::Image TextureGenerator::CreateCirclePlaceholder(
    sf::Color fillColor, sf::Color borderColor) {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0)); // 透明背景

    float centerX = size / 2.f;
    float centerY = size / 2.f;
    float radius = 14.f;
    float innerRadius = radius - 2.f; // 边框宽度 2px

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - centerX + 0.5f;
            float dy = y - centerY + 0.5f;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= radius) {
                if (dist > innerRadius) {
                    img.setPixel(x, y, borderColor);
                } else {
                    img.setPixel(x, y, fillColor);
                }
            }
        }
    }
    return img;
}

sf::Texture TextureGenerator::ImageToTexture(const sf::Image& image) {
    sf::Texture tex;
    tex.loadFromImage(image);
    return tex;
}

// ============================================================================
// CreatePlayerSpriteSheet —— 过程化生成 128x128 玩家 Sprite Sheet
// ----------------------------------------------------------------------------
// 绘制简单像素角色（俯视角）：
//   - 头部：8x6 矩形（肤色）
//   - 身体：10x10 矩形（衣服色）
//   - 手臂：3x6 矩形 × 2（左右）
//   - 腿部：3x5 矩形 × 2（左右，行走时偏移）
//
// 方向差异：
//   Down  ：正面（有眼睛），手臂在两侧
//   Left  ：左侧脸（一只眼睛），手臂前后
//   Right ：右侧脸（一只眼睛），手臂前后（镜像 Left）
//   Up    ：背面（无眼睛），手臂在两侧
//
// 行走动画（4 帧）：
//   帧 0 (idle)：双腿并拢
//   帧 1 (walk1)：左腿前迈
//   帧 2 (walk2)：双腿并拢
//   帧 3 (walk3)：右腿前迈
// ============================================================================

sf::Image TextureGenerator::CreatePlayerSpriteSheet(
    sf::Color bodyColor, sf::Color headColor, sf::Color outlineColor) {

    const int sheetSize = 128;     // 128x128 总尺寸
    const int frameSize = 32;      // 每帧 32x32
    const int framesPerRow = 4;    // 每行 4 帧
    const int rows = 4;            // 4 行方向

    sf::Image sheet;
    sheet.create(sheetSize, sheetSize, sf::Color(0, 0, 0, 0)); // 透明背景

    // 辅助：在 sheet 的指定像素位置绘制实心矩形
    auto fillRect = [&sheet](int x, int y, int w, int h, sf::Color color) {
        int sheetW = static_cast<int>(sheet.getSize().x);
        int sheetH = static_cast<int>(sheet.getSize().y);
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                int px = x + dx;
                int py = y + dy;
                if (px >= 0 && px < sheetW &&
                    py >= 0 && py < sheetH) {
                    sheet.setPixel(static_cast<unsigned>(px), static_cast<unsigned>(py), color);
                }
            }
        }
    };

    // 辅助：在指定帧位置绘制角色
    // frameCol, frameRow: 帧在 sheet 中的行列
    // direction: 0=Down, 1=Left, 2=Right, 3=Up
    // walkFrame: 0=idle, 1=left leg forward, 2=idle, 3=right leg forward
    auto drawCharacter = [&](int frameCol, int frameRow, int direction, int walkFrame) {
        // 帧左上角在 sheet 中的像素坐标
        int ox = frameCol * frameSize;
        int oy = frameRow * frameSize;

        // 角色中心 (16, 16)，各部位相对中心定位
        // 头部：8x6，位于上方
        int headW = 8, headH = 6;
        int headX = ox + (frameSize - headW) / 2; // 水平居中
        int headY = oy + 6;

        // 身体：10x8，位于中部
        int bodyW = 10, bodyH = 8;
        int bodyX = ox + (frameSize - bodyW) / 2;
        int bodyY = headY + headH;

        // 手臂：3x6，位于身体两侧
        int armW = 3, armH = 6;
        int armY = bodyY;

        // 腿部：3x5，位于下方
        int legW = 3, legH = 5;
        int legY = bodyY + bodyH;

        // ---- 绘制描边（黑色外轮廓，1px）----
        // 简化：在身体外围画一圈黑色
        fillRect(bodyX - 1, bodyY - 1, bodyW + 2, bodyH + 2, outlineColor);
        fillRect(headX - 1, headY - 1, headW + 2, headH + 2, outlineColor);

        // ---- 绘制身体 ----
        fillRect(bodyX, bodyY, bodyW, bodyH, bodyColor);

        // ---- 绘制头部 ----
        fillRect(headX, headY, headW, headH, headColor);

        // ---- 根据方向绘制细节 ----
        if (direction == 0) {
            // Down：正面，画两只眼睛
            fillRect(headX + 1, headY + 2, 2, 2, outlineColor); // 左眼
            fillRect(headX + headW - 3, headY + 2, 2, 2, outlineColor); // 右眼
            // 手臂在两侧
            fillRect(bodyX - armW, armY, armW, armH, bodyColor);
            fillRect(bodyX + bodyW, armY, armW, armH, bodyColor);
            fillRect(bodyX - armW - 1, armY - 1, armW + 2, armH + 2, outlineColor);
            fillRect(bodyX + bodyW - 1, armY - 1, armW + 2, armH + 2, outlineColor);
        } else if (direction == 3) {
            // Up：背面，无眼睛
            // 头发色（深色头部）
            fillRect(headX, headY, headW, headH, sf::Color(80, 60, 40));
            // 手臂在两侧
            fillRect(bodyX - armW, armY, armW, armH, bodyColor);
            fillRect(bodyX + bodyW, armY, armW, armH, bodyColor);
            fillRect(bodyX - armW - 1, armY - 1, armW + 2, armH + 2, outlineColor);
            fillRect(bodyX + bodyW - 1, armY - 1, armW + 2, armH + 2, outlineColor);
        } else if (direction == 1) {
            // Left：左侧脸，一只眼睛在左侧
            fillRect(headX + 1, headY + 2, 2, 2, outlineColor); // 左眼
            // 手臂前后（左侧只看到一只手臂）
            fillRect(bodyX, armY, armW, armH, bodyColor); // 前臂
            fillRect(bodyX - 1, armY - 1, armW + 2, armH + 2, outlineColor);
        } else {
            // Right：右侧脸，一只眼睛在右侧
            fillRect(headX + headW - 3, headY + 2, 2, 2, outlineColor); // 右眼
            // 手臂前后
            fillRect(bodyX + bodyW - armW, armY, armW, armH, bodyColor); // 前臂
            fillRect(bodyX + bodyW - armW - 1, armY - 1, armW + 2, armH + 2, outlineColor);
        }

        // ---- 绘制腿部（行走动画）----
        int legLeftX = ox + frameSize / 2 - legW - 1;  // 左腿 X
        int legRightX = ox + frameSize / 2 + 1;         // 右腿 X

        // 描边
        fillRect(legLeftX - 1, legY - 1, legW + 2, legH + 2, outlineColor);
        fillRect(legRightX - 1, legY - 1, legW + 2, legH + 2, outlineColor);

        // 根据行走帧调整腿部位置
        int legOffset = 0;
        if (walkFrame == 1) {
            // 左腿前迈：左腿向下延伸，右腿缩短
            legOffset = 1;
            fillRect(legLeftX, legY, legW, legH + legOffset, sf::Color(40, 40, 60));
            fillRect(legRightX, legY + 1, legW, legH - 1, sf::Color(40, 40, 60));
        } else if (walkFrame == 3) {
            // 右腿前迈：右腿向下延伸，左腿缩短
            legOffset = 1;
            fillRect(legLeftX, legY + 1, legW, legH - 1, sf::Color(40, 40, 60));
            fillRect(legRightX, legY, legW, legH + legOffset, sf::Color(40, 40, 60));
        } else {
            // idle (帧 0 或 2)：双腿正常
            fillRect(legLeftX, legY, legW, legH, sf::Color(40, 40, 60));
            fillRect(legRightX, legY, legW, legH, sf::Color(40, 40, 60));
        }
    };

    // 生成 4 行 × 4 列 = 16 帧
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < framesPerRow; ++col) {
            // walkFrame = col（0=idle, 1=walk1, 2=walk2, 3=walk3）
            drawCharacter(col, row, row, col);
        }
    }

    return sheet;
}

// ============================================================================
// CreateEnemySprite —— 根据敌人类型生成过程化像素贴图
// ----------------------------------------------------------------------------
// 5 种敌人各有独特外观，便于玩家识别：
//   Melee:   红色圆形带尖刺（攻击性外观）
//   Ranged:  紫色长方形带法杖（施法者外观）
//   Suicide: 橙色三角形带引信（爆炸物外观）
//   Elite:   金色带光环（精英感）
//   Boss:    暗红色大圆形带角（Boss 威慑感，64x64）
//
// 注：可尝试调用 text_to_image API 下载 AI 生成贴图，但本实现使用过程化生成
//     作为可靠回退方案。API 下载需网络访问，此处不实现。
// ============================================================================
sf::Image TextureGenerator::CreateEnemySprite(EnemyType type) {
    switch (type) {
        case EnemyType::Melee: {
            // 32x32 红色圆形带尖刺
            const int size = 32;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(200, 50, 50);
            sf::Color spikeColor(160, 30, 30);
            sf::Color outlineColor = sf::Color::Black;
            float cx = size / 2.f, cy = size / 2.f;
            float radius = 10.f;

            // 绘制主体圆形
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= radius) {
                        if (dist > radius - 2.f) {
                            img.setPixel(x, y, outlineColor);
                        } else {
                            img.setPixel(x, y, bodyColor);
                        }
                    }
                }
            }

            // 绘制 8 个尖刺（上下左右 + 四个对角线方向）
            for (int i = 0; i < 8; ++i) {
                float angle = i * 45.f * 3.14159265f / 180.f;
                int sx = static_cast<int>(cx + std::cos(angle) * (radius - 1));
                int sy = static_cast<int>(cy + std::sin(angle) * (radius - 1));
                int ex = static_cast<int>(cx + std::cos(angle) * (radius + 4));
                int ey = static_cast<int>(cy + std::sin(angle) * (radius + 4));
                // 画尖刺线段
                int steps = 6;
                for (int s = 0; s <= steps; ++s) {
                    float t = static_cast<float>(s) / steps;
                    int px = static_cast<int>(sx + (ex - sx) * t);
                    int py = static_cast<int>(sy + (ey - sy) * t);
                    if (px >= 0 && px < size && py >= 0 && py < size) {
                        img.setPixel(px, py, spikeColor);
                    }
                }
            }

            // 绘制眼睛（白色 + 黑色瞳孔）
            img.setPixel(13, 15, sf::Color::White);
            img.setPixel(18, 15, sf::Color::White);
            img.setPixel(14, 15, outlineColor);
            img.setPixel(17, 15, outlineColor);
            return img;
        }

        case EnemyType::Ranged: {
            // 32x32 紫色长方形带法杖
            const int size = 32;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(150, 50, 200);
            sf::Color staffColor(120, 80, 40);
            sf::Color gemColor(100, 200, 255);
            sf::Color outlineColor = sf::Color::Black;

            // 辅助：填充矩形
            auto fillRect = [&img, size](int x, int y, int w, int h, sf::Color c) {
                for (int dy = 0; dy < h; ++dy) {
                    for (int dx = 0; dx < w; ++dx) {
                        int px = x + dx, py = y + dy;
                        if (px >= 0 && px < size && py >= 0 && py < size) {
                            img.setPixel(px, py, c);
                        }
                    }
                }
            };

            // 身体（紫色长方形）
            fillRect(8, 10, 12, 16, outlineColor);
            fillRect(9, 11, 10, 14, bodyColor);

            // 头部
            fillRect(10, 6, 8, 6, outlineColor);
            fillRect(11, 7, 6, 4, bodyColor);

            // 法杖（右侧竖线 + 顶部宝石）
            fillRect(23, 8, 2, 18, staffColor);
            fillRect(22, 6, 4, 4, gemColor);
            fillRect(23, 7, 2, 2, sf::Color(200, 240, 255));

            // 眼睛
            fillRect(12, 8, 1, 1, sf::Color::White);
            fillRect(15, 8, 1, 1, sf::Color::White);
            return img;
        }

        case EnemyType::Suicide: {
            // 32x32 橙色三角形带引信
            const int size = 32;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(230, 130, 30);
            sf::Color outlineColor = sf::Color::Black;
            sf::Color fuseColor(80, 60, 40);
            sf::Color sparkColor(255, 220, 50);

            // 绘制倒三角形（顶点朝下，模拟炸弹/水滴）
            int topY = 6, bottomY = 28;
            int centerX = 16;
            int halfTopBase = 10;

            for (int y = topY; y <= bottomY; ++y) {
                float t = static_cast<float>(y - topY) / (bottomY - topY);
                int hw = static_cast<int>(halfTopBase * (1.f - t));
                for (int x = centerX - hw; x <= centerX + hw; ++x) {
                    if (x >= 0 && x < size) {
                        // 边缘描边
                        if (x == centerX - hw || x == centerX + hw || y == topY) {
                            img.setPixel(x, y, outlineColor);
                        } else {
                            img.setPixel(x, y, bodyColor);
                        }
                    }
                }
            }

            // 引信（顶部小线段）
            img.setPixel(centerX, topY - 1, fuseColor);
            img.setPixel(centerX + 1, topY - 2, fuseColor);
            img.setPixel(centerX + 2, topY - 3, fuseColor);

            // 引信顶端的火花
            img.setPixel(centerX + 3, topY - 4, sparkColor);
            img.setPixel(centerX + 2, topY - 4, sf::Color(255, 150, 0));
            img.setPixel(centerX + 4, topY - 3, sf::Color(255, 100, 0));

            // 眼睛（疯狂表情）
            img.setPixel(13, 16, outlineColor);
            img.setPixel(19, 16, outlineColor);
            return img;
        }

        case EnemyType::Elite: {
            // 32x32 金色带光环（多层圆形）
            const int size = 32;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(220, 180, 50);
            sf::Color haloColor(255, 220, 100, 100);
            sf::Color outlineColor = sf::Color::Black;
            float cx = size / 2.f, cy = size / 2.f;

            // 外层光环（半透明大圆）
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= 14.f && dist > 10.f) {
                        img.setPixel(x, y, haloColor);
                    }
                }
            }

            // 主体圆形
            float radius = 10.f;
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= radius) {
                        if (dist > radius - 2.f) {
                            img.setPixel(x, y, outlineColor);
                        } else {
                            img.setPixel(x, y, bodyColor);
                        }
                    }
                }
            }

            // 中心高光（更亮的金色）
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= 4.f) {
                        img.setPixel(x, y, sf::Color(255, 240, 150));
                    }
                }
            }

            // 眼睛
            img.setPixel(13, 15, outlineColor);
            img.setPixel(18, 15, outlineColor);
            return img;
        }

        case EnemyType::Boss: {
            // 64x64 暗红色大圆形带角
            const int size = 64;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(120, 20, 20);
            sf::Color hornColor(80, 60, 40);
            sf::Color outlineColor = sf::Color::Black;
            float cx = size / 2.f, cy = size / 2.f;
            float radius = 24.f;

            // 主体大圆形
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= radius) {
                        if (dist > radius - 3.f) {
                            img.setPixel(x, y, outlineColor);
                        } else {
                            img.setPixel(x, y, bodyColor);
                        }
                    }
                }
            }

            // 两只角（左上和右上）
            // 左角
            for (int i = 0; i < 8; ++i) {
                int hx = static_cast<int>(cx - 12 + i * 0.5);
                int hy = static_cast<int>(cy - radius + 2 - i);
                for (int w = 0; w < 3; ++w) {
                    if (hx + w >= 0 && hx + w < size && hy >= 0 && hy < size) {
                        img.setPixel(hx + w, hy, hornColor);
                    }
                }
            }
            // 右角
            for (int i = 0; i < 8; ++i) {
                int hx = static_cast<int>(cx + 12 - i * 0.5 - 3);
                int hy = static_cast<int>(cy - radius + 2 - i);
                for (int w = 0; w < 3; ++w) {
                    if (hx + w >= 0 && hx + w < size && hy >= 0 && hy < size) {
                        img.setPixel(hx + w, hy, hornColor);
                    }
                }
            }

            // 眼睛（发光的红色）
            auto fillRect = [&img, size](int x, int y, int w, int h, sf::Color c) {
                for (int dy = 0; dy < h; ++dy) {
                    for (int dx = 0; dx < w; ++dx) {
                        int px = x + dx, py = y + dy;
                        if (px >= 0 && px < size && py >= 0 && py < size) {
                            img.setPixel(px, py, c);
                        }
                    }
                }
            };
            fillRect(24, 30, 4, 3, sf::Color(255, 50, 50));
            fillRect(36, 30, 4, 3, sf::Color(255, 50, 50));
            fillRect(25, 31, 2, 1, sf::Color(255, 200, 200));
            fillRect(37, 31, 2, 1, sf::Color(255, 200, 200));

            // 嘴巴（锯齿状）
            for (int x = 24; x < 40; ++x) {
                if ((x % 2) == 0) {
                    img.setPixel(x, 40, outlineColor);
                    img.setPixel(x, 41, sf::Color(200, 200, 200));
                }
            }
            return img;
        }

        // ---- 新增怪物外观 ----
        case EnemyType::StealthMelee: {
            // 32x32 青色半透明幽灵状（带波纹边缘，暗示隐身能力）
            const int size = 32;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(100, 200, 200, 180);
            sf::Color outlineColor(40, 80, 80, 220);
            float cx = size / 2.f, cy = size / 2.f;
            float radius = 10.f;

            // 主体圆形（半透明）
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= radius) {
                        if (dist > radius - 2.f) {
                            img.setPixel(x, y, outlineColor);
                        } else {
                            img.setPixel(x, y, bodyColor);
                        }
                    }
                }
            }

            // 波纹边缘（暗示隐身波动，4 个小弧形）
            for (int i = 0; i < 4; ++i) {
                float angle = (i * 90.f + 45.f) * 3.14159265f / 180.f;
                int px = static_cast<int>(cx + std::cos(angle) * (radius + 2));
                int py = static_cast<int>(cy + std::sin(angle) * (radius + 2));
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int fx = px + dx, fy = py + dy;
                        if (fx >= 0 && fx < size && fy >= 0 && fy < size) {
                            img.setPixel(fx, fy, sf::Color(150, 220, 220, 100));
                        }
                    }
                }
            }

            // 眼睛（发光的青色）
            img.setPixel(13, 14, sf::Color(200, 255, 255));
            img.setPixel(18, 14, sf::Color(200, 255, 255));
            img.setPixel(14, 15, outlineColor);
            img.setPixel(17, 15, outlineColor);
            return img;
        }

        case EnemyType::CountdownSuicide: {
            // 32x32 亮红色圆形带数字 "3"（暗示倒计时）
            const int size = 32;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(255, 80, 80);
            sf::Color outlineColor = sf::Color::Black;
            sf::Color numberColor(255, 255, 100); // 黄色数字
            float cx = size / 2.f, cy = size / 2.f;
            float radius = 11.f;

            // 主体圆形
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= radius) {
                        if (dist > radius - 2.f) {
                            img.setPixel(x, y, outlineColor);
                        } else {
                            img.setPixel(x, y, bodyColor);
                        }
                    }
                }
            }

            // 顶部引信（红色短线 + 火花）
            img.setPixel(15, 3, sf::Color(80, 60, 40));
            img.setPixel(16, 3, sf::Color(80, 60, 40));
            img.setPixel(16, 2, sf::Color(255, 220, 50));
            img.setPixel(17, 1, sf::Color(255, 150, 0));

            // 中心数字 "3"（用像素绘制简单数字形状）
            // 数字 3 的像素图案（5x7）
            int numX = 14, numY = 12;
            // 顶部横
            img.setPixel(numX + 1, numY, numberColor);
            img.setPixel(numX + 2, numY, numberColor);
            img.setPixel(numX + 3, numY, numberColor);
            // 右上竖
            img.setPixel(numX + 3, numY + 1, numberColor);
            // 中间横
            img.setPixel(numX + 2, numY + 2, numberColor);
            img.setPixel(numX + 3, numY + 2, numberColor);
            // 右下竖
            img.setPixel(numX + 3, numY + 3, numberColor);
            // 底部横
            img.setPixel(numX + 1, numY + 4, numberColor);
            img.setPixel(numX + 2, numY + 4, numberColor);
            img.setPixel(numX + 3, numY + 4, numberColor);

            // 眼睛（疯狂表情）
            img.setPixel(11, 20, outlineColor);
            img.setPixel(20, 20, outlineColor);
            return img;
        }

        case EnemyType::Splitter: {
            // 32x32 绿色椭圆形带裂纹（暗示分裂能力）
            const int size = 32;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(80, 200, 80);
            sf::Color crackColor(40, 100, 40);
            sf::Color outlineColor = sf::Color::Black;
            float cx = size / 2.f, cy = size / 2.f;
            float radiusX = 11.f, radiusY = 9.f; // 椭圆形

            // 主体椭圆
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = (x - cx + 0.5f) / radiusX;
                    float dy = (y - cy + 0.5f) / radiusY;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= 1.f) {
                        if (dist > 0.8f) {
                            img.setPixel(x, y, outlineColor);
                        } else {
                            img.setPixel(x, y, bodyColor);
                        }
                    }
                }
            }

            // 中心裂纹（暗示即将分裂）
            img.setPixel(15, 10, crackColor);
            img.setPixel(16, 11, crackColor);
            img.setPixel(15, 12, crackColor);
            img.setPixel(16, 13, crackColor);
            img.setPixel(15, 14, crackColor);
            img.setPixel(16, 15, crackColor);
            img.setPixel(15, 16, crackColor);
            img.setPixel(16, 17, crackColor);
            img.setPixel(15, 18, crackColor);
            img.setPixel(16, 19, crackColor);
            img.setPixel(15, 20, crackColor);

            // 眼睛（两只靠在一起）
            img.setPixel(12, 14, outlineColor);
            img.setPixel(19, 14, outlineColor);
            img.setPixel(13, 15, sf::Color::White);
            img.setPixel(18, 15, sf::Color::White);
            return img;
        }

        case EnemyType::Shielded: {
            // 32x32 蓝灰色圆形带前方盾牌
            const int size = 32;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(80, 120, 180);
            sf::Color shieldColor(180, 200, 230);
            sf::Color shieldOutline(60, 80, 120);
            sf::Color outlineColor = sf::Color::Black;
            float cx = size / 2.f, cy = size / 2.f;
            float radius = 9.f;

            // 主体圆形（略小，给盾牌留位置）
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= radius) {
                        if (dist > radius - 2.f) {
                            img.setPixel(x, y, outlineColor);
                        } else {
                            img.setPixel(x, y, bodyColor);
                        }
                    }
                }
            }

            // 前方盾牌（朝下方的盾形，覆盖正面）
            // 盾牌轮廓（梯形 + 圆弧底）
            auto fillRect = [&img, size](int x, int y, int w, int h, sf::Color c) {
                for (int dy = 0; dy < h; ++dy) {
                    for (int dx = 0; dx < w; ++dx) {
                        int px = x + dx, py = y + dy;
                        if (px >= 0 && px < size && py >= 0 && py < size) {
                            img.setPixel(px, py, c);
                        }
                    }
                }
            };
            // 盾牌主体（下方）
            fillRect(11, 18, 10, 2, shieldOutline);
            fillRect(12, 20, 8, 4, shieldColor);
            fillRect(13, 24, 6, 2, shieldColor);
            fillRect(14, 26, 4, 1, shieldOutline);
            // 盾牌中心十字纹章
            fillRect(15, 21, 2, 4, shieldOutline);
            fillRect(13, 22, 6, 1, shieldOutline);

            // 眼睛
            img.setPixel(13, 14, outlineColor);
            img.setPixel(18, 14, outlineColor);
            return img;
        }

        case EnemyType::SniperRanged: {
            // 32x32 暗青色狙击手（带长枪管 + 瞄准镜）
            const int size = 32;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(60, 180, 160);
            sf::Color barrelColor(40, 40, 50);
            sf::Color scopeColor(200, 200, 220);
            sf::Color outlineColor = sf::Color::Black;

            // 辅助：填充矩形
            auto fillRect = [&img, size](int x, int y, int w, int h, sf::Color c) {
                for (int dy = 0; dy < h; ++dy) {
                    for (int dx = 0; dx < w; ++dx) {
                        int px = x + dx, py = y + dy;
                        if (px >= 0 && px < size && py >= 0 && py < size) {
                            img.setPixel(px, py, c);
                        }
                    }
                }
            };

            // 身体（暗青色圆形，略小）
            float cx = size / 2.f, cy = size / 2.f;
            float radius = 8.f;
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= radius) {
                        if (dist > radius - 2.f) {
                            img.setPixel(x, y, outlineColor);
                        } else {
                            img.setPixel(x, y, bodyColor);
                        }
                    }
                }
            }

            // 长枪管（朝右方延伸）
            fillRect(16, 14, 12, 2, barrelColor);
            fillRect(16, 13, 12, 1, outlineColor);
            fillRect(16, 16, 12, 1, outlineColor);
            // 枪口
            fillRect(27, 13, 2, 4, outlineColor);

            // 瞄准镜（身体上方）
            fillRect(12, 8, 6, 2, scopeColor);
            fillRect(12, 8, 6, 1, outlineColor);
            fillRect(12, 9, 1, 1, outlineColor);
            fillRect(17, 9, 1, 1, outlineColor);

            // 眼睛（红色，狙击手特征）
            img.setPixel(13, 15, sf::Color(255, 60, 60));
            img.setPixel(16, 15, sf::Color(255, 60, 60));
            return img;
        }

        case EnemyType::Caster: {
            // 32x32 深紫色施法者（带魔法法阵标记）
            const int size = 32;
            sf::Image img;
            img.create(size, size, sf::Color(0, 0, 0, 0));

            sf::Color bodyColor(150, 50, 200);
            sf::Color magicColor(200, 100, 255);
            sf::Color outlineColor = sf::Color::Black;
            float cx = size / 2.f, cy = size / 2.f;
            float radius = 10.f;

            // 主体圆形
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= radius) {
                        if (dist > radius - 2.f) {
                            img.setPixel(x, y, outlineColor);
                        } else {
                            img.setPixel(x, y, bodyColor);
                        }
                    }
                }
            }

            // 法阵标记：身体中心画一个五角星（简化为十字 + 圆环）
            img.setPixel(15, 13, magicColor);
            img.setPixel(16, 13, magicColor);
            img.setPixel(17, 13, magicColor);
            img.setPixel(15, 14, magicColor);
            img.setPixel(17, 14, magicColor);
            img.setPixel(15, 15, magicColor);
            img.setPixel(16, 15, magicColor);
            img.setPixel(17, 15, magicColor);
            img.setPixel(15, 16, magicColor);
            img.setPixel(17, 16, magicColor);
            img.setPixel(15, 17, magicColor);
            img.setPixel(16, 17, magicColor);
            img.setPixel(17, 17, magicColor);

            // 眼睛（亮紫色，施法者特征）
            img.setPixel(12, 14, sf::Color(220, 180, 255));
            img.setPixel(19, 14, sf::Color(220, 180, 255));
            return img;
        }
    }
    // 回退：返回空图
    return sf::Image();
}

// ============================================================================
// Phase 6: 地牢 Tile 贴图生成
// ============================================================================
// 所有 Tile 贴图均为 32x32 像素，使用过程化生成：
//   - Floor: 暗灰石砖纹理（带噪点）
//   - Wall:  亮灰砖块（顶部高亮，侧面阴影）
//   - Door:  木门（关闭状态）
//   - Obstacle: 木桶
//   - Stairs: 下楼楼梯（暗色螺旋）
//   - Chest: 宝箱（关闭状态）
// ============================================================================

// 辅助：填充矩形
static void FillRect(sf::Image& img, int x, int y, int w, int h, sf::Color c) {
    int imgW = static_cast<int>(img.getSize().x);
    int imgH = static_cast<int>(img.getSize().y);
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < imgW && py >= 0 && py < imgH) {
                img.setPixel(static_cast<unsigned>(px), static_cast<unsigned>(py), c);
            }
        }
    }
}

// 辅助：简单噪点（基于坐标的伪随机）
static sf::Color NoiseColor(sf::Color base, int x, int y, int variation) {
    int n = (x * 73856093) ^ (y * 19349663);
    n = (n % variation) - variation / 2;
    int r = std::max(0, std::min(255, static_cast<int>(base.r) + n));
    int g = std::max(0, std::min(255, static_cast<int>(base.g) + n));
    int b = std::max(0, std::min(255, static_cast<int>(base.b) + n));
    return sf::Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), base.a);
}

sf::Image TextureGenerator::CreateFloorTile() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 暗灰石砖底色
    sf::Color baseColor(60, 60, 70);
    sf::Color groutColor(40, 40, 50); // 砖缝颜色

    // 填充底色 + 噪点
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            img.setPixel(x, y, NoiseColor(baseColor, x, y, 20));
        }
    }

    // 绘制砖块缝线（2x2 砖块布局）
    // 水平缝线在 y=15, 垂直缝线在 x=15
    for (int x = 0; x < size; ++x) {
        img.setPixel(x, 15, groutColor);
        img.setPixel(x, 16, groutColor);
    }
    for (int y = 0; y < 16; ++y) {
        img.setPixel(15, y, groutColor);
        img.setPixel(16, y, groutColor);
    }
    // 第二行砖块错位（砖块图案）
    for (int y = 16; y < size; ++y) {
        img.setPixel(7, y, groutColor);
        img.setPixel(8, y, groutColor);
        img.setPixel(23, y, groutColor);
        img.setPixel(24, y, groutColor);
    }

    // 添加一些随机噪点（苔藓/污渍）
    for (int i = 0; i < 8; ++i) {
        int x = (i * 37) % size;
        int y = (i * 53) % size;
        img.setPixel(x, y, sf::Color(50, 60, 50));
    }

    return img;
}

sf::Image TextureGenerator::CreateWallTile() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 亮灰砖块底色
    sf::Color baseColor(110, 110, 120);
    sf::Color highlightColor(140, 140, 150); // 顶部高亮
    sf::Color shadowColor(70, 70, 80);      // 侧面阴影
    sf::Color groutColor(50, 50, 60);       // 砖缝

    // 填充底色 + 噪点
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            img.setPixel(x, y, NoiseColor(baseColor, x, y, 15));
        }
    }

    // 顶部高亮（前 4 行）
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < size; ++x) {
            sf::Color c = NoiseColor(highlightColor, x, y, 10);
            img.setPixel(x, y, c);
        }
    }

    // 底部阴影（后 4 行）
    for (int y = size - 4; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            sf::Color c = NoiseColor(shadowColor, x, y, 10);
            img.setPixel(x, y, c);
        }
    }

    // 砖块缝线（3 行砖块，每行高约 10px）
    // 水平缝线
    for (int x = 0; x < size; ++x) {
        img.setPixel(x, 10, groutColor);
        img.setPixel(x, 11, groutColor);
        img.setPixel(x, 21, groutColor);
        img.setPixel(x, 22, groutColor);
    }

    // 垂直缝线（错位）
    // 第一行（y=0-9）：缝线在 x=15
    for (int y = 0; y < 10; ++y) {
        img.setPixel(15, y, groutColor);
        img.setPixel(16, y, groutColor);
    }
    // 第二行（y=12-20）：缝线在 x=7 和 x=23
    for (int y = 12; y < 21; ++y) {
        img.setPixel(7, y, groutColor);
        img.setPixel(8, y, groutColor);
        img.setPixel(23, y, groutColor);
        img.setPixel(24, y, groutColor);
    }
    // 第三行（y=23-31）：缝线在 x=15
    for (int y = 23; y < size; ++y) {
        img.setPixel(15, y, groutColor);
        img.setPixel(16, y, groutColor);
    }

    return img;
}

sf::Image TextureGenerator::CreateDoorTile() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 木门颜色
    sf::Color woodColor(120, 80, 40);
    sf::Color darkWoodColor(90, 60, 30);
    sf::Color metalColor(180, 180, 190);
    sf::Color outlineColor(40, 30, 20);

    // 门框（石质）
    FillRect(img, 0, 0, size, size, sf::Color(80, 80, 90));

    // 门体（木质，留 4px 边框）
    FillRect(img, 4, 2, size - 8, size - 4, outlineColor);
    FillRect(img, 5, 3, size - 10, size - 6, woodColor);

    // 木纹（垂直线条）
    for (int y = 3; y < size - 3; ++y) {
        for (int x = 5; x < size - 5; x += 4) {
            img.setPixel(x, y, darkWoodColor);
        }
    }

    // 门板分隔线（水平）
    FillRect(img, 5, 10, size - 10, 1, darkWoodColor);
    FillRect(img, 5, 20, size - 10, 1, darkWoodColor);

    // 门把手（金属圆形）
    FillRect(img, 22, 15, 3, 3, metalColor);
    img.setPixel(23, 16, sf::Color(220, 220, 230));

    // 顶部高光
    for (int x = 5; x < size - 5; ++x) {
        img.setPixel(x, 3, sf::Color(150, 110, 60));
    }

    return img;
}

sf::Image TextureGenerator::CreateDoorOpenTile() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 开门状态：仅保留门框（石质），中间为空门洞（深色背景）
    sf::Color frameColor(80, 80, 90);
    sf::Color frameHighlight(110, 110, 120);
    sf::Color openingColor(20, 20, 30); // 空门洞（深色，表示可通过）

    // 门框（石质，4px 边框）
    FillRect(img, 0, 0, size, size, frameColor);

    // 门框高光（顶部）
    FillRect(img, 0, 0, size, 2, frameHighlight);

    // 空门洞（中间区域，表示门已打开）
    FillRect(img, 4, 4, size - 8, size - 8, openingColor);

    // 门洞内添加地板纹理提示（暗色石砖，表示地面可见）
    for (int y = 4; y < size - 4; ++y) {
        for (int x = 4; x < size - 4; ++x) {
            sf::Color c = NoiseColor(sf::Color(40, 40, 50), x, y, 15);
            img.setPixel(x, y, c);
        }
    }

    // 门框装饰（四角金属铆钉）
    FillRect(img, 2, 2, 2, 2, sf::Color(180, 180, 190));
    FillRect(img, size - 4, 2, 2, 2, sf::Color(180, 180, 190));
    FillRect(img, 2, size - 4, 2, 2, sf::Color(180, 180, 190));
    FillRect(img, size - 4, size - 4, 2, 2, sf::Color(180, 180, 190));

    return img;
}

sf::Image TextureGenerator::CreateObstacleTile() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 木桶颜色
    sf::Color woodColor(140, 90, 50);
    sf::Color darkWoodColor(100, 65, 35);
    sf::Color metalColor(160, 160, 170);
    sf::Color outlineColor(50, 35, 20);

    // 木桶主体（椭圆形）
    float cx = size / 2.f, cy = size / 2.f + 2;
    float radiusX = 11.f, radiusY = 13.f;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = (x - cx) / radiusX;
            float dy = (y - cy) / radiusY;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= 1.f) {
                if (dist > 0.85f) {
                    img.setPixel(x, y, outlineColor);
                } else {
                    img.setPixel(x, y, NoiseColor(woodColor, x, y, 15));
                }
            }
        }
    }

    // 金属箍（上下两条横线）
    for (int x = 4; x < size - 4; ++x) {
        if (img.getPixel(x, 10).a > 0) {
            img.setPixel(x, 10, metalColor);
            img.setPixel(x, 11, metalColor);
        }
        if (img.getPixel(x, 21).a > 0) {
            img.setPixel(x, 21, metalColor);
            img.setPixel(x, 22, metalColor);
        }
    }

    // 木板分隔线（垂直）
    for (int y = 4; y < size - 4; ++y) {
        if (img.getPixel(11, y).a > 0) img.setPixel(11, y, darkWoodColor);
        if (img.getPixel(20, y).a > 0) img.setPixel(20, y, darkWoodColor);
    }

    // 顶部高光
    for (int x = 8; x < size - 8; ++x) {
        if (img.getPixel(x, 5).a > 0) {
            img.setPixel(x, 5, sf::Color(170, 120, 70));
        }
    }

    return img;
}

// ============================================================================
// CreateIndestructibleObstacleTile —— 生成不可破坏障碍物贴图
// ----------------------------------------------------------------------------
// 深灰色石柱/铁块，带裂纹和金属边框，与木桶区分明显
// ============================================================================
sf::Image TextureGenerator::CreateIndestructibleObstacleTile() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 石柱颜色
    sf::Color stoneColor(90, 95, 100);
    sf::Color darkStoneColor(60, 65, 70);
    sf::Color metalColor(120, 125, 130);
    sf::Color outlineColor(40, 45, 50);

    // 主体（圆角矩形石柱）
    int left = 7, top = 4, right = size - 7, bottom = size - 4;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            // 圆角裁剪
            bool cornerCut = false;
            if ((x == left || x == right - 1) && (y == top || y == bottom - 1)) {
                cornerCut = true;
            }
            if (!cornerCut) {
                sf::Color c = stoneColor;
                // 简单噪点变化
                int noise = ((x * 7 + y * 13) % 20) - 10;
                c.r = static_cast<uint8_t>(std::clamp(c.r + noise, 0, 255));
                c.g = static_cast<uint8_t>(std::clamp(c.g + noise, 0, 255));
                c.b = static_cast<uint8_t>(std::clamp(c.b + noise, 0, 255));
                img.setPixel(x, y, c);
            }
        }
    }

    // 描边
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            bool isEdge = (x == left || x == right - 1 || y == top || y == bottom - 1);
            if (isEdge && img.getPixel(x, y).a > 0) {
                img.setPixel(x, y, outlineColor);
            }
        }
    }

    // 金属箍（上下）
    for (int x = left; x < right; ++x) {
        if (img.getPixel(x, top + 3).a > 0) {
            img.setPixel(x, top + 3, metalColor);
            img.setPixel(x, top + 4, metalColor);
        }
        if (img.getPixel(x, bottom - 4).a > 0) {
            img.setPixel(x, bottom - 4, metalColor);
            img.setPixel(x, bottom - 5, metalColor);
        }
    }

    // 裂纹（深灰色斜线）
    for (int i = 0; i < 10; ++i) {
        int x = left + 4 + i;
        int y = top + 8 + i / 2;
        if (img.getPixel(x, y).a > 0) {
            img.setPixel(x, y, darkStoneColor);
        }
    }

    return img;
}

sf::Image TextureGenerator::CreateStairsTile() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 楼梯颜色（暗色螺旋）
    sf::Color darkColor(30, 30, 40);
    sf::Color midColor(50, 50, 60);
    sf::Color lightColor(70, 70, 80);
    sf::Color outlineColor(20, 20, 30);

    // 背景（深色）
    FillRect(img, 0, 0, size, size, darkColor);

    // 螺旋楼梯（同心方形，从外到内逐渐变亮）
    for (int i = 0; i < 6; ++i) {
        int margin = i * 2;
        int w = size - margin * 2;
        if (w <= 0) break;

        sf::Color c;
        if (i % 2 == 0) {
            c = (i < 3) ? midColor : lightColor;
        } else {
            c = darkColor;
        }

        // 绘制方形边框
        for (int x = margin; x < size - margin; ++x) {
            img.setPixel(x, margin, c);
            img.setPixel(x, size - 1 - margin, c);
        }
        for (int y = margin; y < size - margin; ++y) {
            img.setPixel(margin, y, c);
            img.setPixel(size - 1 - margin, y, c);
        }
    }

    // 中心暗点（楼梯井）
    FillRect(img, 14, 14, 4, 4, outlineColor);

    // 顶部高光（模拟光源）
    for (int x = 2; x < size - 2; ++x) {
        img.setPixel(x, 2, sf::Color(90, 90, 100));
        img.setPixel(x, 3, sf::Color(80, 80, 90));
    }

    return img;
}

sf::Image TextureGenerator::CreateChestTile() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 宝箱颜色
    sf::Color woodColor(130, 85, 45);
    sf::Color darkWoodColor(95, 60, 30);
    sf::Color goldColor(220, 180, 50);
    sf::Color metalColor(180, 180, 190);
    sf::Color outlineColor(40, 30, 20);

    // 宝箱底座（木质）
    FillRect(img, 4, 14, 24, 14, outlineColor);
    FillRect(img, 5, 15, 22, 12, woodColor);

    // 木纹
    for (int y = 15; y < 27; ++y) {
        for (int x = 5; x < 27; x += 5) {
            img.setPixel(x, y, darkWoodColor);
        }
    }

    // 宝箱盖（弧形，简化为矩形）
    FillRect(img, 4, 8, 24, 8, outlineColor);
    FillRect(img, 5, 9, 22, 6, woodColor);

    // 盖子高光
    for (int x = 5; x < 27; ++x) {
        img.setPixel(x, 9, sf::Color(160, 110, 60));
    }

    // 金属箍（垂直）
    FillRect(img, 14, 8, 4, 20, outlineColor);
    FillRect(img, 15, 9, 2, 18, metalColor);

    // 锁（金色圆形）
    FillRect(img, 14, 14, 4, 4, outlineColor);
    FillRect(img, 15, 15, 2, 2, goldColor);
    img.setPixel(15, 16, sf::Color(180, 140, 30));

    // 金色装饰角
    FillRect(img, 4, 8, 2, 2, goldColor);
    FillRect(img, 26, 8, 2, 2, goldColor);
    FillRect(img, 4, 26, 2, 2, goldColor);
    FillRect(img, 26, 26, 2, 2, goldColor);

    return img;
}

// ============================================================================
// CreateMerchantSprite —— 生成 32x32 商人 NPC 贴图
// ----------------------------------------------------------------------------
// 商人外观：紫色斗篷 + 棕色帽子 + 金色腰带 + 货架（暗示交易功能）
// ============================================================================
sf::Image TextureGenerator::CreateMerchantSprite() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    sf::Color cloakColor(120, 60, 180);    // 紫色斗篷
    sf::Color cloakDark(80, 40, 120);      // 斗篷阴影
    sf::Color hatColor(100, 70, 40);       // 棕色帽子
    sf::Color hatDark(70, 50, 30);        // 帽子阴影
    sf::Color beltColor(220, 180, 50);     // 金色腰带
    sf::Color skinColor(220, 180, 140);    // 肤色（脸部）
    sf::Color outlineColor = sf::Color::Black;
    sf::Color goldAccent(255, 220, 80);   // 金色装饰

    // 辅助：填充矩形
    auto fillRect = [&img, size](int x, int y, int w, int h, sf::Color c) {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                int px = x + dx, py = y + dy;
                if (px >= 0 && px < size && py >= 0 && py < size) {
                    img.setPixel(px, py, c);
                }
            }
        }
    };

    // ---- 斗篷主体（梯形，上窄下宽）----
    // 描边
    fillRect(8, 12, 16, 16, outlineColor);
    // 斗篷填充（上窄）
    fillRect(10, 13, 12, 2, cloakColor);
    fillRect(9, 15, 14, 13, cloakColor);
    // 斗篷阴影（左侧）
    fillRect(9, 15, 2, 13, cloakDark);

    // ---- 脸部（斗篷上方）----
    fillRect(12, 8, 8, 5, outlineColor);
    fillRect(13, 9, 6, 3, skinColor);
    // 眼睛
    fillRect(14, 10, 1, 1, outlineColor);
    fillRect(17, 10, 1, 1, outlineColor);

    // ---- 帽子（尖顶）----
    fillRect(11, 4, 10, 5, outlineColor);
    fillRect(12, 5, 8, 3, hatColor);
    // 帽尖
    fillRect(15, 2, 2, 3, hatColor);
    fillRect(15, 1, 2, 2, hatDark);
    // 帽檐
    fillRect(10, 8, 12, 1, hatDark);
    fillRect(10, 7, 12, 1, hatColor);

    // ---- 金色腰带 ----
    fillRect(9, 20, 14, 2, beltColor);
    fillRect(9, 20, 14, 1, goldAccent);
    // 腰带扣
    fillRect(15, 20, 2, 2, outlineColor);
    fillRect(15, 20, 2, 1, goldAccent);

    // ---- 货架（右侧小袋子，暗示交易）----
    fillRect(22, 16, 6, 8, outlineColor);
    fillRect(23, 17, 4, 6, hatColor);
    // 袋口
    fillRect(22, 16, 6, 1, outlineColor);
    // 金币装饰
    fillRect(24, 19, 2, 2, goldAccent);
    fillRect(24, 22, 2, 1, beltColor);

    return img;
}

sf::Image TextureGenerator::TryDownloadTileSheet(const std::string& prompt,
                                                  const std::string& outputPath) {
    // text_to_image API 下载功能占位
    // 实际实现需要网络访问与 API 集成，此处返回空 Image 作为回退
    // 调用者应检查返回的 Image 是否为空，若空则使用过程化生成
    (void)prompt;
    (void)outputPath;
    LOG_WARN("text_to_image API 下载未实现，使用过程化生成");
    return sf::Image();
}

// ============================================================================
// Phase 8: UI 资源过程化生成
// ============================================================================
// 为 HUD 与菜单系统生成 UI 用纹理：
//   - 血条框：64x8 像素深色边框
//   - 技能图标：32x32 像素，3 种类型（剑/盾/爆炸）
//   - 小地图背景：深色半透明底 + 网格线
//   - 按钮背景：圆角矩形 + 边框
//   - 升级卡片边框：64x64 品质颜色边框
// ============================================================================

// ============================================================================
// CreateBeggarSprite —— 生成 32x32 乞丐 NPC 贴图
// ----------------------------------------------------------------------------
// 外观：灰褐色破烂衣衫 + 蓬乱头发 + 讨饭碗 + 弯腰姿态
// ============================================================================
sf::Image TextureGenerator::CreateBeggarSprite() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    sf::Color clothColor(120, 100, 70);    // 灰褐色破衣
    sf::Color clothDark(85, 70, 50);       // 衣服阴影
    sf::Color clothPatch(160, 140, 90);    // 补丁（浅色）
    sf::Color skinColor(220, 190, 150);    // 肤色
    sf::Color hairColor(80, 60, 40);       // 蓬乱头发
    sf::Color bowlColor(140, 100, 60);     // 木碗
    sf::Color outlineColor = sf::Color::Black;

    auto fillRect = [&img, size](int x, int y, int w, int h, sf::Color c) {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                int px = x + dx, py = y + dy;
                if (px >= 0 && px < size && py >= 0 && py < size) {
                    img.setPixel(px, py, c);
                }
            }
        }
    };

    // ---- 身体（弯腰姿态，略前倾）----
    fillRect(8, 16, 16, 12, outlineColor);
    fillRect(9, 17, 14, 10, clothColor);
    // 衣服阴影
    fillRect(9, 17, 3, 10, clothDark);
    // 补丁
    fillRect(13, 20, 4, 3, clothPatch);
    fillRect(17, 24, 3, 2, clothPatch);

    // ---- 头部（略低，弯腰感）----
    fillRect(11, 9, 10, 8, outlineColor);
    fillRect(12, 10, 8, 6, skinColor);
    // 蓬乱头发（不规则）
    fillRect(10, 8, 12, 3, hairColor);
    fillRect(11, 7, 3, 2, hairColor);
    fillRect(18, 7, 3, 2, hairColor);
    // 眼睛（疲惫感，半闭）
    fillRect(13, 12, 2, 1, outlineColor);
    fillRect(17, 12, 2, 1, outlineColor);
    // 胡子茬
    fillRect(13, 14, 6, 1, hairColor);

    // ---- 讨饭碗（手中端着）----
    fillRect(20, 20, 8, 5, outlineColor);
    fillRect(21, 21, 6, 3, bowlColor);
    // 碗内反光
    fillRect(22, 21, 2, 1, sf::Color(180, 140, 80));

    // ---- 破鞋（底部）----
    fillRect(9, 28, 5, 2, outlineColor);
    fillRect(18, 28, 5, 2, outlineColor);

    return img;
}

// ============================================================================
// CreateMageSprite —— 生成 32x32 神秘法师 NPC 贴图
// ----------------------------------------------------------------------------
// 外观：深紫色长袍 + 尖顶帽 + 法杖 + 神秘光环
// ============================================================================
sf::Image TextureGenerator::CreateMageSprite() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    sf::Color robeColor(80, 40, 120);      // 深紫长袍
    sf::Color robeDark(55, 25, 85);        // 长袍阴影
    sf::Color hatColor(60, 30, 90);        // 尖帽
    sf::Color hatDark(40, 20, 60);         // 帽子阴影
    sf::Color skinColor(210, 180, 150);    // 肤色
    sf::Color staffColor(120, 80, 40);     // 法杖木杆
    sf::Color gemColor(100, 200, 255);     // 法杖宝石（青色发光）
    sf::Color gemGlow(180, 230, 255);      // 宝石高光
    sf::Color outlineColor = sf::Color::Black;
    sf::Color runeColor(180, 100, 220);    // 袍子上的符文

    auto fillRect = [&img, size](int x, int y, int w, int h, sf::Color c) {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                int px = x + dx, py = y + dy;
                if (px >= 0 && px < size && py >= 0 && py < size) {
                    img.setPixel(px, py, c);
                }
            }
        }
    };

    // ---- 长袍（下宽上窄，魔法师风范）----
    fillRect(7, 16, 18, 14, outlineColor);
    fillRect(8, 17, 16, 12, robeColor);
    // 长袍下摆展开
    fillRect(6, 26, 20, 4, robeColor);
    fillRect(6, 26, 20, 1, outlineColor);
    // 长袍阴影
    fillRect(8, 17, 3, 12, robeDark);
    // 袍子符文
    fillRect(14, 22, 2, 2, runeColor);
    fillRect(18, 25, 1, 1, runeColor);

    // ---- 脸部----
    fillRect(12, 10, 8, 6, outlineColor);
    fillRect(13, 11, 6, 4, skinColor);
    // 眼睛（发光的魔法感）
    fillRect(14, 12, 1, 1, gemColor);
    fillRect(17, 12, 1, 1, gemColor);
    // 胡子（白色长须）
    fillRect(13, 14, 6, 3, sf::Color(220, 220, 220));

    // ---- 尖顶帽（弯曲尖角）----
    fillRect(10, 6, 12, 5, outlineColor);
    fillRect(11, 7, 10, 3, hatColor);
    // 帽尖向左弯
    fillRect(13, 3, 2, 4, hatColor);
    fillRect(12, 2, 2, 2, hatDark);
    fillRect(11, 4, 1, 2, hatDark);
    // 帽檐
    fillRect(10, 10, 12, 1, hatDark);
    // 帽子上的星星装饰
    fillRect(15, 8, 1, 1, gemGlow);

    // ---- 法杖（右侧）----
    fillRect(24, 8, 2, 22, outlineColor);
    fillRect(25, 8, 1, 22, staffColor);
    // 法杖顶部宝石
    fillRect(22, 5, 6, 6, outlineColor);
    fillRect(23, 6, 4, 4, gemColor);
    fillRect(24, 7, 2, 2, gemGlow);
    // 宝石光芒
    fillRect(21, 7, 1, 2, gemGlow);
    fillRect(28, 7, 1, 2, gemGlow);

    return img;
}

// ============================================================================
// CreateAltarSprite —— 生成 32x32 祭坛贴图
// ----------------------------------------------------------------------------
// 外观：石质祭坛台 + 紫色符文圆 + 两侧蜡烛
// ============================================================================
sf::Image TextureGenerator::CreateAltarSprite() {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    sf::Color stoneColor(90, 85, 95);      // 石质主体
    sf::Color stoneDark(60, 55, 65);       // 石头阴影
    sf::Color stoneLight(120, 115, 125);   // 石头高光
    sf::Color runeColor(180, 80, 220);     // 紫色符文
    sf::Color runeGlow(220, 120, 255);     // 符文光芒
    sf::Color candleColor(230, 220, 200);  // 蜡烛主体
    sf::Color flameColor(255, 180, 60);    // 火焰
    sf::Color flameCore(255, 240, 180);    // 火焰核心
    sf::Color outlineColor = sf::Color::Black;

    auto fillRect = [&img, size](int x, int y, int w, int h, sf::Color c) {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                int px = x + dx, py = y + dy;
                if (px >= 0 && px < size && py >= 0 && py < size) {
                    img.setPixel(px, py, c);
                }
            }
        }
    };

    // ---- 祭坛底座（梯形）----
    fillRect(5, 24, 22, 6, outlineColor);
    fillRect(6, 25, 20, 4, stoneColor);
    fillRect(6, 25, 20, 1, stoneLight); // 顶面高光
    fillRect(6, 28, 20, 1, stoneDark);  // 底部阴影

    // ---- 祭坛台面（中部）----
    fillRect(7, 18, 18, 7, outlineColor);
    fillRect(8, 19, 16, 5, stoneColor);
    fillRect(8, 19, 16, 1, stoneLight);
    fillRect(8, 23, 16, 1, stoneDark);

    // ---- 祭坛顶面（符文台）----
    fillRect(9, 14, 14, 5, outlineColor);
    fillRect(10, 15, 12, 3, stoneDark);

    // ---- 中央符文圆（发光）----
    fillRect(13, 15, 6, 3, runeColor);
    fillRect(14, 16, 4, 1, runeGlow);
    // 符文标记
    fillRect(14, 15, 1, 1, runeGlow);
    fillRect(17, 15, 1, 1, runeGlow);

    // ---- 两侧蜡烛 ----
    // 左蜡烛
    fillRect(3, 16, 3, 10, outlineColor);
    fillRect(4, 17, 1, 8, candleColor);
    // 左蜡烛火焰
    fillRect(3, 13, 3, 4, outlineColor);
    fillRect(4, 14, 1, 3, flameColor);
    fillRect(4, 12, 1, 2, flameCore);

    // 右蜡烛
    fillRect(26, 16, 3, 10, outlineColor);
    fillRect(27, 17, 1, 8, candleColor);
    // 右蜡烛火焰
    fillRect(26, 13, 3, 4, outlineColor);
    fillRect(27, 14, 1, 3, flameColor);
    fillRect(27, 12, 1, 2, flameCore);

    // ---- 祭坛上的献祭纹路 ----
    fillRect(11, 21, 2, 1, runeColor);
    fillRect(19, 21, 2, 1, runeColor);
    fillRect(15, 22, 2, 1, runeColor);

    return img;
}

// 生成血条框 Image（64x8 像素，深色边框 + 透明内部）
sf::Image TextureGenerator::CreateHealthBarFrameImage(sf::Color borderColor) {
    const int w = 64, h = 8;
    sf::Image img;
    img.create(w, h, sf::Color(0, 0, 0, 0)); // 透明背景

    // 绘制 1px 边框
    for (int x = 0; x < w; ++x) {
        img.setPixel(x, 0, borderColor);
        img.setPixel(x, h - 1, borderColor);
    }
    for (int y = 0; y < h; ++y) {
        img.setPixel(0, y, borderColor);
        img.setPixel(w - 1, y, borderColor);
    }

    return img;
}

// 生成血条框纹理
sf::Texture TextureGenerator::CreateHealthBarFrame(sf::Color borderColor) {
    sf::Image img = CreateHealthBarFrameImage(borderColor);
    sf::Texture tex;
    tex.loadFromImage(img);
    return tex;
}

// 生成技能图标（32x32）
sf::Texture TextureGenerator::CreateSkillIcon(int type) {
    const int size = 32;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 通用：深色圆角背景
    sf::Color bgColor(40, 40, 50, 200);
    sf::Color borderColor(80, 80, 90);
    sf::Color iconColor(240, 240, 250);

    // 填充圆角背景（简化为圆形）
    float cx = size / 2.f, cy = size / 2.f;
    float radius = 14.f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - cx + 0.5f;
            float dy = y - cy + 0.5f;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= radius) {
                if (dist > radius - 1.5f) {
                    img.setPixel(x, y, borderColor);
                } else {
                    img.setPixel(x, y, bgColor);
                }
            }
        }
    }

    // 根据类型绘制不同符号
    if (type == 0) {
        // 普攻：剑形（对角线 + 横向护手）
        // 剑身：从左下到右上的对角线
        for (int i = 4; i <= 24; ++i) {
            int x = 6 + i - 4;
            int y = 26 - (i - 4);
            if (x >= 0 && x < size && y >= 0 && y < size) {
                img.setPixel(x, y, iconColor);
                img.setPixel(x + 1, y, iconColor);
            }
        }
        // 护手：横向短线
        for (int x = 8; x <= 14; ++x) {
            int y = 20;
            if (x >= 0 && x < size && y >= 0 && y < size) {
                img.setPixel(x, y, iconColor);
            }
        }
        // 剑柄
        for (int i = 0; i < 4; ++i) {
            int x = 6 + i;
            int y = 24 - i;
            if (x >= 0 && x < size && y >= 0 && y < size) {
                img.setPixel(x, y, sf::Color(140, 100, 60));
            }
        }
    } else if (type == 1) {
        // 闪避：盾形（圆形 + 十字）
        // 绘制盾牌轮廓（U 形）
        for (int y = 8; y <= 24; ++y) {
            int halfW = (y < 16) ? 8 : (8 - (y - 16) / 2);
            for (int x = 16 - halfW; x <= 16 + halfW; ++x) {
                if (x >= 0 && x < size && y >= 0 && y < size) {
                    if (x == 16 - halfW || x == 16 + halfW || y == 8) {
                        img.setPixel(x, y, iconColor);
                    }
                }
            }
        }
        // 底部尖角
        img.setPixel(16, 25, iconColor);
        img.setPixel(16, 26, iconColor);

        // 十字标志
        for (int i = 12; i <= 20; ++i) {
            img.setPixel(i, 15, iconColor);
        }
        for (int i = 12; i <= 18; ++i) {
            img.setPixel(16, i, iconColor);
        }
    } else if (type == 2) {
        // AOE：爆炸（星形 + 中心点）
        // 中心点
        for (int y = 14; y <= 18; ++y) {
            for (int x = 14; x <= 18; ++x) {
                img.setPixel(x, y, iconColor);
            }
        }
        // 8 个方向的射线
        for (int i = 0; i < 8; ++i) {
            float angle = i * 45.f * 3.14159265f / 180.f;
            for (int r = 6; r <= 12; ++r) {
                int x = static_cast<int>(16 + std::cos(angle) * r);
                int y = static_cast<int>(16 + std::sin(angle) * r);
                if (x >= 0 && x < size && y >= 0 && y < size) {
                    img.setPixel(x, y, iconColor);
                }
            }
        }
        // 中心高亮
        img.setPixel(16, 16, sf::Color(255, 240, 150));
    } else {
        // type >= 3：锁图标（占位技能槽，表示未解锁/未制作）
        sf::Color lockColor(160, 160, 170);
        // 锁体（矩形）
        for (int y = 16; y <= 24; ++y) {
            for (int x = 11; x <= 21; ++x) {
                img.setPixel(x, y, lockColor);
            }
        }
        // 锁体内部挖空（深色）
        for (int y = 18; y <= 22; ++y) {
            for (int x = 13; x <= 19; ++x) {
                img.setPixel(x, y, sf::Color(30, 30, 35, 200));
            }
        }
        // 锁孔（小圆点）
        img.setPixel(16, 19, lockColor);
        img.setPixel(16, 20, lockColor);
        // 锁环（U 形）
        for (int x = 12; x <= 20; ++x) {
            img.setPixel(x, 16, lockColor);
        }
        img.setPixel(12, 15, lockColor);
        img.setPixel(20, 15, lockColor);
        img.setPixel(12, 14, lockColor);
        img.setPixel(20, 14, lockColor);
        img.setPixel(12, 13, lockColor);
        img.setPixel(20, 13, lockColor);
    }

    sf::Texture tex;
    tex.loadFromImage(img);
    return tex;
}

// 生成小地图背景（w x h 像素，深色半透明底 + 网格线）
sf::Texture TextureGenerator::CreateMinimapBg(int w, int h) {
    sf::Image img;
    img.create(w, h, sf::Color(20, 20, 30, 180)); // 深色半透明底

    // 绘制网格线（每 16px 一条）
    sf::Color gridColor(50, 50, 60, 180);
    for (int x = 0; x < w; x += 16) {
        for (int y = 0; y < h; ++y) {
            img.setPixel(x, y, gridColor);
        }
    }
    for (int y = 0; y < h; y += 16) {
        for (int x = 0; x < w; ++x) {
            img.setPixel(x, y, gridColor);
        }
    }

    // 边框
    sf::Color borderColor(100, 100, 120, 255);
    for (int x = 0; x < w; ++x) {
        img.setPixel(x, 0, borderColor);
        img.setPixel(x, h - 1, borderColor);
    }
    for (int y = 0; y < h; ++y) {
        img.setPixel(0, y, borderColor);
        img.setPixel(w - 1, y, borderColor);
    }

    sf::Texture tex;
    tex.loadFromImage(img);
    return tex;
}

// 生成按钮背景纹理（w x h，圆角矩形 + 边框）
sf::Texture TextureGenerator::CreateButtonBg(int w, int h, sf::Color color) {
    sf::Image img;
    img.create(w, h, sf::Color(0, 0, 0, 0));

    sf::Color borderColor(20, 20, 30);
    sf::Color highlightColor(
        std::min(255, color.r + 40),
        std::min(255, color.g + 40),
        std::min(255, color.b + 40));
    int cornerRadius = 4;

    // 填充圆角矩形
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // 检查是否在圆角矩形内
            bool inCorner = false;
            int dx = 0, dy = 0;
            if (x < cornerRadius && y < cornerRadius) { dx = cornerRadius - x; dy = cornerRadius - y; inCorner = true; }
            else if (x >= w - cornerRadius && y < cornerRadius) { dx = x - (w - cornerRadius - 1); dy = cornerRadius - y; inCorner = true; }
            else if (x < cornerRadius && y >= h - cornerRadius) { dx = cornerRadius - x; dy = y - (h - cornerRadius - 1); inCorner = true; }
            else if (x >= w - cornerRadius && y >= h - cornerRadius) { dx = x - (w - cornerRadius - 1); dy = y - (h - cornerRadius - 1); inCorner = true; }

            bool inside = true;
            if (inCorner) {
                float dist = std::sqrt(dx * dx + dy * dy);
                inside = (dist <= cornerRadius);
            }

            if (inside) {
                // 顶部高光
                if (y < 2) {
                    img.setPixel(x, y, highlightColor);
                } else {
                    img.setPixel(x, y, color);
                }
            }
        }
    }

    // 绘制边框（圆角）
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool onEdge = false;
            // 检查是否在边框上
            int dx = 0, dy = 0;
            bool inCorner = false;
            if (x < cornerRadius && y < cornerRadius) { dx = cornerRadius - x; dy = cornerRadius - y; inCorner = true; }
            else if (x >= w - cornerRadius && y < cornerRadius) { dx = x - (w - cornerRadius - 1); dy = cornerRadius - y; inCorner = true; }
            else if (x < cornerRadius && y >= h - cornerRadius) { dx = cornerRadius - x; dy = y - (h - cornerRadius - 1); inCorner = true; }
            else if (x >= w - cornerRadius && y >= h - cornerRadius) { dx = x - (w - cornerRadius - 1); dy = y - (h - cornerRadius - 1); inCorner = true; }

            if (inCorner) {
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist <= cornerRadius && dist >= cornerRadius - 1.5f) {
                    onEdge = true;
                }
            } else if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
                onEdge = true;
            }

            if (onEdge) {
                img.setPixel(x, y, borderColor);
            }
        }
    }

    sf::Texture tex;
    tex.loadFromImage(img);
    return tex;
}

// 生成升级卡片边框纹理（64x64，按品质颜色）
sf::Texture TextureGenerator::CreateCardBorder(sf::Color color) {
    const int size = 64;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 绘制 2px 边框
    for (int x = 0; x < size; ++x) {
        img.setPixel(x, 0, color);
        img.setPixel(x, 1, color);
        img.setPixel(x, size - 1, color);
        img.setPixel(x, size - 2, color);
    }
    for (int y = 0; y < size; ++y) {
        img.setPixel(0, y, color);
        img.setPixel(1, y, color);
        img.setPixel(size - 1, y, color);
        img.setPixel(size - 2, y, color);
    }

    // 四角装饰（L 形）
    int cornerSize = 8;
    sf::Color brightColor(
        std::min(255, color.r + 60),
        std::min(255, color.g + 60),
        std::min(255, color.b + 60));
    // 左上
    for (int i = 0; i < cornerSize; ++i) {
        img.setPixel(i, 3, brightColor);
        img.setPixel(3, i, brightColor);
    }
    // 右上
    for (int i = 0; i < cornerSize; ++i) {
        img.setPixel(size - 1 - i, 3, brightColor);
        img.setPixel(size - 4, i, brightColor);
    }
    // 左下
    for (int i = 0; i < cornerSize; ++i) {
        img.setPixel(i, size - 4, brightColor);
        img.setPixel(3, size - 1 - i, brightColor);
    }
    // 右下
    for (int i = 0; i < cornerSize; ++i) {
        img.setPixel(size - 1 - i, size - 4, brightColor);
        img.setPixel(size - 4, size - 1 - i, brightColor);
    }

    sf::Texture tex;
    tex.loadFromImage(img);
    return tex;
}

// ============================================================================
// CreateItemIcon —— 生成 24x24 装备图标（按槽位类型）
// ----------------------------------------------------------------------------
// 6 种装备各有独特外观，便于玩家识别：
//   Weapon: 剑形（对角线剑身 + 横向护手）
//   Helmet: 头盔（半圆 + 顶饰羽毛）
//   Chest:  胸甲（方形 + 十字纹章）
//   Boots:  靴子（L 形 ×2）
//   Ring:   戒指（圆环 + 宝石）
//   Amulet: 项链（三角形吊坠 + 链条）
// ============================================================================
sf::Image TextureGenerator::CreateItemIcon(ItemSlot slot) {
    const int size = 24;
    sf::Image img;
    img.create(size, size, sf::Color(0, 0, 0, 0));

    // 辅助：填充矩形
    auto fillRect = [&img, size](int x, int y, int w, int h, sf::Color c) {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                int px = x + dx, py = y + dy;
                if (px >= 0 && px < size && py >= 0 && py < size) {
                    img.setPixel(px, py, c);
                }
            }
        }
    };
    // 辅助：画线段
    auto drawLine = [&img, size](int x0, int y0, int x1, int y1, sf::Color c) {
        int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;
        int x = x0, y = y0;
        while (true) {
            if (x >= 0 && x < size && y >= 0 && y < size) {
                img.setPixel(x, y, c);
            }
            if (x == x1 && y == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx)  { err += dx; y += sy; }
        }
    };

    switch (slot) {
        case ItemSlot::Weapon: {
            // 剑：对角线剑身（银色）+ 横向护手（金色）+ 剑柄（棕色）
            sf::Color blade(200, 200, 220);
            sf::Color guard(220, 180, 60);
            sf::Color hilt(120, 80, 40);
            // 剑身（左上到右下）
            drawLine(4, 4, 16, 16, blade);
            drawLine(5, 4, 17, 16, blade);
            drawLine(4, 5, 16, 17, blade);
            // 护手（横向）
            fillRect(2, 14, 8, 2, guard);
            fillRect(14, 2, 2, 8, guard); // 对角护手
            // 剑柄
            fillRect(16, 16, 4, 4, hilt);
            break;
        }
        case ItemSlot::Helmet: {
            // 头盔：半圆（银色）+ 顶饰羽毛（红色）
            sf::Color metal(180, 180, 200);
            sf::Color dark(100, 100, 120);
            sf::Color plume(200, 50, 50);
            // 半圆盔顶
            for (int y = 6; y <= 14; ++y) {
                for (int x = 4; x <= 18; ++x) {
                    float dx = x - 11, dy = y - 14;
                    if (dx * dx + dy * dy <= 36) {
                        img.setPixel(x, y, metal);
                    }
                }
            }
            // 底边
            fillRect(4, 14, 15, 2, dark);
            // 顶饰羽毛
            fillRect(11, 2, 2, 5, plume);
            fillRect(13, 3, 2, 3, plume);
            break;
        }
        case ItemSlot::Chest: {
            // 胸甲：板甲造型（银色矩形 + 金属铆钉 + 肩甲轮廓）
            sf::Color metal(160, 160, 180);
            sf::Color dark(80, 80, 100);
            sf::Color highlight(200, 200, 220);
            // 主体（胸甲轮廓 - 梯形，上窄下宽）
            fillRect(5, 5, 14, 14, metal);           // 主体
            // 肩甲（两侧突出）
            fillRect(2, 3, 3, 4, dark);               // 左肩
            fillRect(19, 3, 3, 4, dark);              // 右肩
            fillRect(2, 3, 3, 1, highlight);          // 左肩高光
            fillRect(19, 3, 3, 1, highlight);         // 右肩高光
            // 胸甲边框
            fillRect(5, 5, 14, 1, dark);              // 上边
            fillRect(5, 18, 14, 1, dark);             // 下边
            fillRect(5, 5, 1, 14, dark);              // 左边
            fillRect(18, 5, 1, 14, dark);             // 右边
            // 中心垂直脊线（胸甲中缝）
            fillRect(11, 6, 2, 12, dark);
            // 铆钉装饰（四角）
            fillRect(7, 7, 2, 2, highlight);
            fillRect(15, 7, 2, 2, highlight);
            fillRect(7, 15, 2, 2, highlight);
            fillRect(15, 15, 2, 2, highlight);
            break;
        }
        case ItemSlot::Boots: {
            // 靴子：L 形 ×2（棕色）
            sf::Color leather(140, 90, 50);
            sf::Color dark(80, 50, 30);
            // 左靴
            fillRect(3, 6, 6, 10, leather);
            fillRect(3, 14, 9, 4, leather);
            fillRect(3, 17, 9, 1, dark);
            // 右靴
            fillRect(15, 6, 6, 10, leather);
            fillRect(12, 14, 9, 4, leather);
            fillRect(12, 17, 9, 1, dark);
            break;
        }
        case ItemSlot::Ring: {
            // 戒指：圆环（金色）+ 宝石（红色）
            sf::Color gold(220, 180, 60);
            sf::Color gem(200, 50, 50);
            float cx = 12, cy = 13;
            // 外环
            for (int y = 4; y <= 20; ++y) {
                for (int x = 4; x <= 20; ++x) {
                    float dx = x - cx, dy = y - cy;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist <= 8.f && dist >= 5.f) {
                        img.setPixel(x, y, gold);
                    }
                }
            }
            // 宝石（顶部）
            fillRect(11, 3, 2, 3, gem);
            fillRect(10, 4, 4, 1, gem);
            break;
        }
        case ItemSlot::Amulet: {
            // 项链：链条（金色）+ 三角形吊坠（蓝色宝石）
            sf::Color chain(220, 180, 60);
            sf::Color gem(80, 140, 255);
            sf::Color dark(40, 40, 60);
            // 链条（弧形）
            for (int x = 4; x <= 20; ++x) {
                int y = 4 + static_cast<int>(std::sin(static_cast<float>(x - 4) / 16.f * 3.14159f) * 4);
                if (x >= 0 && x < size && y >= 0 && y < size) {
                    img.setPixel(x, y, chain);
                }
            }
            // 吊坠三角形
            fillRect(11, 10, 2, 2, dark);
            for (int y = 12; y <= 20; ++y) {
                int halfW = (20 - y) / 2 + 1;
                for (int x = 12 - halfW; x <= 12 + halfW; ++x) {
                    if (x >= 0 && x < size) {
                        img.setPixel(x, y, gem);
                    }
                }
            }
            // 吊坠边框
            drawLine(9, 12, 12, 20, dark);
            drawLine(15, 12, 12, 20, dark);
            drawLine(9, 12, 15, 12, dark);
            break;
        }
    }
    return img;
}

} // namespace cu
