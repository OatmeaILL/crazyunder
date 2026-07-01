#pragma once

// ============================================================================
// TextureGenerator —— 过程化占位纹理生成器
// ----------------------------------------------------------------------------
// 在美术资源就绪前，用代码生成简单占位纹理用于演示：
//   - 纯色带边框方块：区分不同实体类型（敌人、道具等）
//   - 方向指示三角形：标识玩家朝向
// 生成结果为 sf::Image，可添加到 TextureAtlas 或直接转为 sf::Texture。
// ============================================================================

#include <SFML/Graphics.hpp>
#include <string>
#include "gameplay/EnemyAI.h"
#include "gameplay/LootSystem.h"

namespace cu {

class TextureGenerator {
public:
    // 生成 32x32 纯色带边框方块
    // fillColor: 填充色, borderColor: 边框色, borderWidth: 边框宽度（像素）
    [[nodiscard]] static sf::Image CreateColorBlock(
        sf::Color fillColor, sf::Color borderColor, int borderWidth = 2);

    // 生成 32x32 带方向指示的玩家占位（三角形指向上方）
    // fillColor: 三角形填充色, borderColor: 边框色
    [[nodiscard]] static sf::Image CreatePlayerPlaceholder(
        sf::Color fillColor = sf::Color(80, 200, 80),
        sf::Color borderColor = sf::Color::Black);

    // 生成 32x32 圆形占位（用于敌人等）
    [[nodiscard]] static sf::Image CreateCirclePlaceholder(
        sf::Color fillColor, sf::Color borderColor = sf::Color::Black);

    // ---- Phase 3: 玩家 Sprite Sheet 过程化生成 ----

    // 生成 128x128 玩家 Sprite Sheet（4 方向 × 4 帧，每帧 32x32）
    // 布局：
    //   行 0：Down  方向（idle, walk1, walk2, walk3）
    //   行 1：Left  方向
    //   行 2：Right 方向
    //   行 3：Up    方向
    // 每帧绘制简单像素角色（头/身/四肢），行走帧腿部位置变化模拟动画。
    [[nodiscard]] static sf::Image CreatePlayerSpriteSheet(
        sf::Color bodyColor = sf::Color(80, 180, 220),
        sf::Color headColor = sf::Color(220, 180, 140),
        sf::Color outlineColor = sf::Color::Black);

    // ---- Phase 4: 敌人像素贴图生成 ----

    // 根据敌人类型生成过程化像素贴图
    // Melee:   32x32 红色圆形带尖刺
    // Ranged:  32x32 紫色长方形带法杖
    // Suicide: 32x32 橙色三角形带引信
    // Elite:   32x32 金色带光环（多层圆形）
    // Boss:    64x64 暗红色大圆形带角
    [[nodiscard]] static sf::Image CreateEnemySprite(EnemyType type);

    // 将 Image 转为 Texture（便捷方法）
    [[nodiscard]] static sf::Texture ImageToTexture(const sf::Image& image);

    // ---- Phase 6: 地牢 Tile 贴图生成 ----

    // 生成 32x32 暗灰石砖地板（带噪点纹理）
    [[nodiscard]] static sf::Image CreateFloorTile();

    // 生成 32x32 亮灰砖块墙壁（顶部高亮，侧面阴影）
    [[nodiscard]] static sf::Image CreateWallTile();

    // 生成 32x32 木门（关闭状态）
    [[nodiscard]] static sf::Image CreateDoorTile();

    // 生成 32x32 木门（打开状态：门框 + 空门洞）
    [[nodiscard]] static sf::Image CreateDoorOpenTile();

    // 生成 32x32 木桶障碍物
    [[nodiscard]] static sf::Image CreateObstacleTile();

    // 生成 32x32 不可破坏障碍物（深灰色石柱/铁块）
    [[nodiscard]] static sf::Image CreateIndestructibleObstacleTile();

    // 生成 32x32 下楼楼梯（暗色螺旋）
    [[nodiscard]] static sf::Image CreateStairsTile();

    // 生成 32x32 宝箱（关闭状态）
    [[nodiscard]] static sf::Image CreateChestTile();

    // 生成 32x32 商人 NPC（斗篷 + 帽子 + 货架，用于商人实体）
    [[nodiscard]] static sf::Image CreateMerchantSprite();

    // ---- 事件房 NPC 贴图（4 种）----
    // 乞丐：灰褐色破衣 + 讨饭碗
    [[nodiscard]] static sf::Image CreateBeggarSprite();
    // 神秘法师：深紫长袍 + 尖帽 + 法杖
    [[nodiscard]] static sf::Image CreateMageSprite();
    // 祭坛：石质祭坛 + 紫色符文 + 蜡烛
    [[nodiscard]] static sf::Image CreateAltarSprite();
    // 宝箱怪：外观与宝箱相同（伪装），此处复用 CreateChestTile 即可
    // 注意：宝箱怪使用 TileType::Chest 渲染，无需独立贴图

    // ---- 剑士武器贴图 ----
    // 生成 32x32 长剑贴图（剑身 + 护手 + 剑柄），用于剑士攻击视觉
    [[nodiscard]] static sf::Image CreateSwordSprite();

    // 生成 24x24 装备图标（按槽位类型）
    // Weapon: 剑形（对角线 + 护手）
    // Helmet: 头盔（半圆 + 顶饰）
    // Chest: 胸甲（方形 + 十字）
    // Boots: 靴子（L 形 ×2）
    // Ring: 戒指（圆环）
    // Amulet: 项链（三角形吊坠 + 链）
    [[nodiscard]] static sf::Image CreateItemIcon(ItemSlot slot);

    // 尝试从 text_to_image API 下载 Tile 贴图（可选，失败返回空 Image）
    // prompt: 生成贴图的提示词
    // outputPath: 下载后保存的路径
    // 失败时返回空 Image，调用者应回退到过程化生成
    [[nodiscard]] static sf::Image TryDownloadTileSheet(const std::string& prompt,
                                                         const std::string& outputPath);

    // ---- Phase 8: UI 资源过程化生成 ----

    // 生成血条框 Image（64x8 像素，深色边框 + 透明内部）
    // 用于 HUD 血条/蓝条/经验条的外框装饰
    [[nodiscard]] static sf::Image CreateHealthBarFrameImage(
        sf::Color borderColor = sf::Color(40, 40, 40));

    // 生成血条框纹理（直接返回 sf::Texture，便于 HUD 使用）
    [[nodiscard]] static sf::Texture CreateHealthBarFrame(
        sf::Color borderColor = sf::Color(40, 40, 40));

    // 生成技能图标（32x32）
    // type=0：普攻（剑形：对角线 + 横向护手）
    // type=1：闪避（盾形：圆形 + 十字）
    // type=2：AOE（爆炸：星形 + 中心点）
    [[nodiscard]] static sf::Texture CreateSkillIcon(int type);

    // 生成小地图背景（w x h 像素，深色半透明底 + 网格线）
    [[nodiscard]] static sf::Texture CreateMinimapBg(int w, int h);

    // 生成按钮背景纹理（w x h，圆角矩形 + 边框）
    // color: 按钮主色（用于填充）
    [[nodiscard]] static sf::Texture CreateButtonBg(int w, int h, sf::Color color);

    // 生成升级卡片边框纹理（64x64，按品质颜色）
    // color: 品质颜色（白/蓝/黄/暗金）
    [[nodiscard]] static sf::Texture CreateCardBorder(sf::Color color);
};

} // namespace cu
