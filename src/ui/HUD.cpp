#include "ui/HUD.h"
#include "gameplay/DungeonGenerator.h"
#include "utils/TextureGenerator.h"
#include "utils/Logger.h"
#include <algorithm>
#include <string>
#include <cmath>

namespace cu {

HUD::HUD() {
    // 默认初始化文本对象
    textCache_.setFillColor(sf::Color::White);
}

void HUD::Initialize(const sf::Font& font) {
    font_ = &font;
    textCache_.setFont(font);

    // 过程化生成技能图标纹理
    // 0=普攻（剑）, 1=闪避（盾）, 2=AOE（爆炸）
    skillIcons_[0] = TextureGenerator::CreateSkillIcon(0);
    skillIcons_[1] = TextureGenerator::CreateSkillIcon(1);
    skillIcons_[2] = TextureGenerator::CreateSkillIcon(2);
    // 3-6=占位技能槽（锁图标，表示未解锁/未制作）
    for (int i = 3; i < kHudSkillCount; ++i) {
        skillIcons_[i] = TextureGenerator::CreateSkillIcon(3); // 锁图标
    }

    // 小地图背景纹理
    minimapBg_ = TextureGenerator::CreateMinimapBg(160, 120);

    LOG_INFO("HUD 初始化完成");
}

void HUD::Update(const PlayerStats& stats, int wave, int enemyCount, int fps,
                 const Dungeon& dungeon, int currentRoomIndex) {
    hpValue_ = stats.currentHp;
    hpMax_ = stats.maxHp;
    mpValue_ = stats.currentMp;
    mpMax_ = stats.maxMp;
    expValue_ = stats.exp;
    expMax_ = stats.expToNext;
    level_ = stats.level;
    coins_ = stats.coins;

    wave_ = wave;
    enemyCount_ = enemyCount;
    fps_ = fps;

    dungeon_ = &dungeon;
    currentRoomIndex_ = currentRoomIndex;
}

void HUD::SetSkillCooldown(int skillIndex, float progress) {
    if (skillIndex >= 0 && skillIndex < kHudSkillCount) {
        skillCooldown_[skillIndex] = std::max(0.f, std::min(1.f, progress));
    }
}

void HUD::SetSkillSlotData(const std::array<SkillInstance, kSkillSlotCount>& slots) {
    for (int i = 0; i < kSkillSlotCount; ++i) {
        skillSlots_[i] = slots[i];
    }
}

void HUD::Render(sf::RenderTarget& target) const {
    if (!font_) return;

    // ---- 左下：血条 + 蓝条 ----
    // 血条位置：(20, 640) 尺寸 (240, 16)
    sf::Vector2f hpBarPos(20.f, 640.f);
    sf::Vector2f hpBarSize(240.f, 16.f);
    float hpProgress = (hpMax_ > 0.f) ? (hpValue_ / hpMax_) : 0.f;
    drawProgressBar(target, hpBarPos, hpBarSize, hpProgress,
                   sf::Color(200, 40, 40), true); // 渐变填充

    // 蓝条位置：(20, 660) 尺寸 (240, 12)
    sf::Vector2f mpBarPos(20.f, 660.f);
    sf::Vector2f mpBarSize(240.f, 12.f);
    float mpProgress = (mpMax_ > 0.f) ? (mpValue_ / mpMax_) : 0.f;
    drawProgressBar(target, mpBarPos, mpBarSize, mpProgress,
                    sf::Color(40, 80, 200), false);

    // ---- 左上：等级 + HP/MP 数值 ----
    drawText(target, "等级 " + std::to_string(level_),
             sf::Vector2f(20.f, 20.f), 24, sf::Color::Yellow, sf::Text::Bold);

    drawText(target,
             "生命: " + std::to_string(static_cast<int>(hpValue_)) +
             " / " + std::to_string(static_cast<int>(hpMax_)),
             sf::Vector2f(20.f, 50.f), 16, sf::Color::White);

    drawText(target,
             "法力: " + std::to_string(static_cast<int>(mpValue_)) +
             " / " + std::to_string(static_cast<int>(mpMax_)),
             sf::Vector2f(20.f, 70.f), 16, sf::Color::White);

    // 金币显示（金色）
    drawText(target, "金币: " + std::to_string(coins_),
             sf::Vector2f(20.f, 90.f), 16, sf::Color(255, 215, 0), sf::Text::Bold);

    // ---- 右上：波次信息 + 敌人数 ----
    drawText(target, "波次 " + std::to_string(wave_),
             sf::Vector2f(1100.f, 20.f), 24, sf::Color::White, sf::Text::Bold);
    drawText(target, "敌人: " + std::to_string(enemyCount_),
             sf::Vector2f(1100.f, 50.f), 18, sf::Color::White);

    // ---- 右上角小字 FPS ----
    drawText(target, "FPS: " + std::to_string(fps_),
             sf::Vector2f(1200.f, 80.f), 14, sf::Color::Cyan);

    // ---- 技能图标（左下角上方）----
    // 7 个图标水平排列：3 个实际技能 + 4 个技能系统槽位
    // 图标放大到 48x48，间距 56px，确保4字技能名能完整显示
    sf::Vector2f skillStart(20.f, 560.f);
    for (int i = 0; i < kHudSkillCount; ++i) {
        sf::Vector2f iconPos(skillStart.x + i * 56.f, skillStart.y);
        drawSkillIcon(target, iconPos, i);
    }

    // ---- 经验条（底部全宽）----
    sf::Vector2f expBarPos(0.f, 710.f);
    sf::Vector2f expBarSize(1280.f, 10.f);
    float expProgress = (expMax_ > 0.f) ? (expValue_ / expMax_) : 0.f;
    drawProgressBar(target, expBarPos, expBarSize, expProgress,
                    sf::Color(220, 200, 50), false);

    // ---- 技能点提示（有未使用技能点时显示，屏幕中上方）----
    if (skillPoints_ > 0) {
        // 闪烁效果（基于静态时钟，避免依赖外部时间）
        static float blinkPhase = 0.f;
        blinkPhase += 0.05f;
        float alpha = 180.f + 75.f * std::sin(blinkPhase);

        std::string hintStr = "有未使用的技能点 " + std::to_string(skillPoints_) +
                              " 点  按 J 开启新技能选择界面";
        drawText(target, hintStr,
                 sf::Vector2f(380.f, 120.f), 20,
                 sf::Color(255, 220, 100, static_cast<uint8_t>(alpha)),
                 sf::Text::Bold);
    }

    // ---- 右下：小地图（200x150, 含图例）----
    // 右下角，避免超出 1280x720 边界
    drawMinimap(target, sf::Vector2f(1070.f, 540.f));

    // ---- 第十八轮新增：屏幕中央连击指示器（combo >= 5 时显示）----
    drawCombo(target);

    // ---- 第二十轮新增：极限闪避 buff 金色光环（buffTimer > 0 时显示）----
    drawPerfectDodge(target);

    // ---- 右上：成就解锁 Toast 通知（堆叠显示，自动淡出）----
    drawAchievementToasts(target);
}

void HUD::drawProgressBar(sf::RenderTarget& target, sf::Vector2f pos,
                          sf::Vector2f size, float progress,
                          sf::Color fillColor, bool gradient) const {
    // 背景
    sf::RectangleShape bg(size);
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(20, 20, 20, 200));
    bg.setOutlineThickness(1.f);
    bg.setOutlineColor(sf::Color(60, 60, 60));
    target.draw(bg);

    // 填充
    if (progress > 0.f) {
        progress = std::min(progress, 1.f);
        sf::Color fillColorVar = fillColor;
        // 渐变填充：进度低于 30% 时变红
        if (gradient && progress < 0.3f) {
            float t = progress / 0.3f; // 0~1
            fillColorVar.r = static_cast<uint8_t>(200 + (fillColor.r - 200) * t);
            fillColorVar.g = static_cast<uint8_t>(0 + (fillColor.g - 0) * t);
            fillColorVar.b = static_cast<uint8_t>(0 + (fillColor.b - 0) * t);
        }

        sf::RectangleShape fillRect(sf::Vector2f(size.x * progress, size.y));
        fillRect.setPosition(pos);
        fillRect.setFillColor(fillColorVar);
        target.draw(fillRect);
    }
}

void HUD::drawSkillIcon(sf::RenderTarget& target, sf::Vector2f pos, int skillIndex) const {
    if (skillIndex < 0 || skillIndex >= kHudSkillCount) return;

    // 技能颜色映射（用于技能系统槽位）
    auto getSkillColor = [](SkillType type) -> sf::Color {
        switch (type) {
            case SkillType::GroundSlam:  return sf::Color(180, 120, 60);
            case SkillType::LeechStrike: return sf::Color(100, 255, 100);
            case SkillType::Berserk:     return sf::Color(255, 60, 30);
            case SkillType::GravityWell: return sf::Color(160, 60, 255);
            case SkillType::SpikeGround: return sf::Color(200, 180, 60);
            default: return sf::Color(80, 80, 80);
        }
    };

    // 索引 3-6 是技能系统槽位（按键 1-4）
    if (skillIndex >= 3) {
        int slotIdx = skillIndex - 3;
        const auto& slot = skillSlots_[slotIdx];

        // 背景 48x48（从32放大到48，确保4字技能名能显示）
        const float iconSize = 48.f;
        sf::RectangleShape bg(sf::Vector2f(iconSize, iconSize));
        bg.setPosition(pos);
        bg.setFillColor(sf::Color(20, 20, 30, 210));
        if (slot.type != SkillType::Count) {
            bg.setOutlineColor(getSkillColor(slot.type));
            bg.setOutlineThickness(2.f);
        } else {
            bg.setOutlineColor(sf::Color(60, 60, 60));
            bg.setOutlineThickness(1.f);
        }
        target.draw(bg);

        // 技能名称：按字数自适应字号和位置
        // 2字（狂暴）→ 字号16, 3字（吸血打击→4字?）按实际长度
        if (slot.type != SkillType::Count) {
            const char* name = GetSkillName(slot.type);
            size_t nameLen = strlen(name);
            // UTF-8 中文每字3字节，估算字符数
            int charCount = static_cast<int>(nameLen / 3);
            unsigned int fontSize = 14;
            float textX = pos.x + 4.f;
            float textY = pos.y + 3.f;
            if (charCount <= 2) {
                fontSize = 16;
                textX = pos.x + 6.f;
            } else if (charCount == 3) {
                fontSize = 13;
                textX = pos.x + 3.f;
            } else {
                // 4字（"吸血打击"）→ 字号11, 2行显示更清晰
                fontSize = 11;
                textX = pos.x + 2.f;
            }
            drawText(target, name,
                     sf::Vector2f(textX, textY), fontSize,
                     getSkillColor(slot.type), sf::Text::Bold);

            // 4字技能名下方画一行小字标识（避免单行挤压）
            if (charCount >= 4) {
                // 显示技能类型简称（如"打击"/"波动"），但这里简化：留空
            }
        }

        // 冷却遮罩
        if (slot.type != SkillType::Count && slot.cooldownRemain > 0.f) {
            const SkillData& sd = GetSkillData(slot.type);
            float cdProgress = (sd.cooldown > 0.f) ? (slot.cooldownRemain / sd.cooldown) : 0.f;
            cdProgress = std::min(cdProgress, 1.f);
            float coverHeight = iconSize * cdProgress;
            sf::RectangleShape cover(sf::Vector2f(iconSize, coverHeight));
            cover.setPosition(pos.x, pos.y + iconSize - coverHeight);
            cover.setFillColor(sf::Color(0, 0, 0, 160));
            target.draw(cover);

            // 冷却倒计时数字（图标内中央）
            int cdSeconds = static_cast<int>(std::ceil(slot.cooldownRemain));
            if (cdSeconds > 0) {
                drawText(target, std::to_string(cdSeconds),
                         sf::Vector2f(pos.x + iconSize * 0.4f, pos.y + iconSize * 0.35f), 16,
                         sf::Color(255, 255, 100), sf::Text::Bold);
            }
        }

        // 快捷键提示（图标下方）
        drawText(target, std::to_string(slotIdx + 1),
                 sf::Vector2f(pos.x + 2.f, pos.y + iconSize + 2.f), 12,
                 sf::Color(200, 200, 200), sf::Text::Bold);

        // 技能等级显示（右下角，Lv.2/3 时显示）
        if (slot.type != SkillType::Count && slot.level > 1) {
            std::string lvStr = "Lv" + std::to_string(slot.level);
            drawText(target, lvStr,
                     sf::Vector2f(pos.x + iconSize - 22.f, pos.y + iconSize + 2.f), 11,
                     sf::Color(255, 220, 100), sf::Text::Bold);
        }
        return;
    }

    // 索引 0-2 是原始技能图标（普攻/闪避/AOE）
    bool isPlaceholder = false;

    // 绘制图标背景（48x48，与技能系统槽位一致）
    const float iconSize = 48.f;
    sf::RectangleShape bg(sf::Vector2f(iconSize, iconSize));
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(20, 20, 30, 200));
    bg.setOutlineThickness(1.f);
    bg.setOutlineColor(sf::Color(80, 80, 100));
    target.draw(bg);

    // 绘制图标纹理（拉伸到 48x48）
    sf::Sprite iconSprite;
    iconSprite.setTexture(skillIcons_[skillIndex]);
    iconSprite.setScale(iconSize / 32.f, iconSize / 32.f);
    iconSprite.setPosition(pos);
    target.draw(iconSprite);

    // 绘制冷却覆盖（从下到上的暗色遮罩）
    float cd = skillCooldown_[skillIndex];
    if (cd > 0.f) {
        float coverHeight = iconSize * cd;
        sf::RectangleShape cover(sf::Vector2f(iconSize, coverHeight));
        cover.setPosition(pos.x, pos.y + iconSize - coverHeight);
        cover.setFillColor(sf::Color(0, 0, 0, 150));
        target.draw(cover);
    }

    // 绘制快捷键提示
    const char* keyHints[] = {"LMB", "RMB", "SPC"};
    drawText(target, keyHints[skillIndex],
             sf::Vector2f(pos.x + 2.f, pos.y + iconSize + 2.f), 11,
             sf::Color(200, 200, 200), sf::Text::Bold);
}

void HUD::drawMinimap(sf::RenderTarget& target, sf::Vector2f pos) const {
    if (!dungeon_) return;

    // 小地图尺寸（放大并加图例）
    const float mapW = 200.f;
    const float mapH = 150.f;

    // ---- 1. 半透明黑色背景板 ----
    sf::RectangleShape bg(sf::Vector2f(mapW + 8.f, mapH + 28.f));
    bg.setPosition(pos.x - 4.f, pos.y - 22.f);
    bg.setFillColor(sf::Color(10, 12, 18, 220));
    bg.setOutlineColor(sf::Color(120, 120, 140));
    bg.setOutlineThickness(1.f);
    target.draw(bg);

    // 标题
    drawText(target, "地图",
             sf::Vector2f(pos.x, pos.y - 18.f), 13,
             sf::Color(220, 220, 220), sf::Text::Bold);

    // ---- 2. 计算地牢在缩略图中的缩放比例（保持比例，居中）----
    float dungeonPixelW = dungeon_->width * 32.f;
    float dungeonPixelH = dungeon_->height * 32.f;
    float scale = std::min(mapW / dungeonPixelW, mapH / dungeonPixelH);
    float offsetX = pos.x + (mapW - dungeonPixelW * scale) * 0.5f;
    float offsetY = pos.y + (mapH - dungeonPixelH * scale) * 0.5f;

    // 辅助：地牢世界坐标 → 小地图坐标
    auto worldToMap = [&](sf::Vector2f worldPos) {
        return sf::Vector2f(
            offsetX + worldPos.x * scale,
            offsetY + worldPos.y * scale);
    };

    // 辅助：Tile 坐标 → 小地图坐标
    auto tileToMap = [&](int tx, int ty) {
        return worldToMap(sf::Vector2f(tx * 32.f + 16.f, ty * 32.f + 16.f));
    };

    // ---- 3. 绘制走廊通道（Floor tile, 暗灰色小方块）----
    // 遍历地牢 Tile 数据，只绘制 Floor 类型（墙不绘制，背景已是黑色）
    for (int y = 0; y < dungeon_->height; ++y) {
        for (int x = 0; x < dungeon_->width; ++x) {
            TileType t = dungeon_->GetTile(x, y);
            if (t == TileType::Floor || t == TileType::Door || t == TileType::Stairs) {
                sf::Vector2f p = tileToMap(x, y);
                float s = 32.f * scale;
                sf::RectangleShape pixel(sf::Vector2f(s + 0.5f, s + 0.5f));
                pixel.setPosition(p.x - s * 0.5f, p.y - s * 0.5f);
                // 门用稍亮的颜色标识
                if (t == TileType::Door) {
                    pixel.setFillColor(sf::Color(160, 120, 60));
                } else if (t == TileType::Stairs) {
                    pixel.setFillColor(sf::Color(80, 220, 220));
                } else {
                    pixel.setFillColor(sf::Color(80, 85, 95));
                }
                target.draw(pixel);
            }
        }
    }

    // ---- 4. 绘制房间类型标记（仅在房间中心绘制图标，避免矩形混乱）----
    for (const auto& room : dungeon_->rooms) {
        sf::Vector2f center = worldToMap(sf::Vector2f(
            (room.bounds.left + room.bounds.width * 0.5f) * 32.f,
            (room.bounds.top + room.bounds.height * 0.5f) * 32.f));

        // 当前房间高亮圆圈
        if (room.index == currentRoomIndex_) {
            sf::CircleShape highlight(5.f);
            highlight.setOrigin(5.f, 5.f);
            highlight.setPosition(center);
            highlight.setFillColor(sf::Color(255, 255, 100, 180));
            highlight.setOutlineColor(sf::Color(255, 255, 50));
            highlight.setOutlineThickness(1.f);
            target.draw(highlight);
        }

        // 特殊房间图标
        const char* label = nullptr;
        sf::Color labelColor = sf::Color::White;
        switch (room.type) {
            case RoomType::Boss:     label = "B"; labelColor = sf::Color(255, 80, 80); break;
            case RoomType::Treasure: label = "T"; labelColor = sf::Color(255, 220, 80); break;
            case RoomType::Elite:    label = "E"; labelColor = sf::Color(220, 120, 220); break;
            case RoomType::Stairs:   label = "S"; labelColor = sf::Color(80, 220, 220); break;
            case RoomType::Hidden:   label = "?"; labelColor = sf::Color(150, 150, 150); break;
            default: break;
        }
        if (label) {
            drawText(target, label, center, 11, labelColor, sf::Text::Bold);
        }
    }

    // ---- 5. 绘制玩家位置（绿色三角形 + 朝向）----
    if (playerPos_.x > 0.f && playerPos_.y > 0.f) {
        sf::Vector2f pp = worldToMap(playerPos_);
        // 玩家位置绿色圆点
        sf::CircleShape playerDot(3.f);
        playerDot.setOrigin(3.f, 3.f);
        playerDot.setPosition(pp);
        playerDot.setFillColor(sf::Color(80, 255, 80));
        playerDot.setOutlineColor(sf::Color(255, 255, 255));
        playerDot.setOutlineThickness(1.f);
        target.draw(playerDot);
    }

    // ---- 6. 绘制图例（底部一行）----
    float legendY = pos.y + mapH + 4.f;
    float legendX = pos.x;
    auto drawLegend = [&](const char* txt, sf::Color color) {
        sf::RectangleShape sw(sf::Vector2f(8.f, 8.f));
        sw.setPosition(legendX, legendY);
        sw.setFillColor(color);
        target.draw(sw);
        drawText(target, txt, sf::Vector2f(legendX + 11.f, legendY - 2.f), 9,
                 sf::Color(200, 200, 200));
        legendX += 11.f + strlen(txt) * 6.f + 8.f;
    };
    drawLegend("你", sf::Color(80, 255, 80));
    drawLegend("B-BOSS", sf::Color(255, 80, 80));
    drawLegend("T-宝箱", sf::Color(255, 220, 80));
    drawLegend("S-楼梯", sf::Color(80, 220, 220));
}

void HUD::drawText(sf::RenderTarget& target, const std::string& str,
                   sf::Vector2f pos, unsigned int size, sf::Color color,
                   sf::Text::Style style) const {
    textCache_.setString(utf8ToSfString(str));
    textCache_.setPosition(pos);
    textCache_.setCharacterSize(size);
    textCache_.setFillColor(color);
    textCache_.setStyle(style);
    target.draw(textCache_);
}

// ============================================================================
// 成就 Toast 通知实现
// ----------------------------------------------------------------------------
// 设计要点：
//   1. 新通知从底部追加，最多保留 kMaxToasts(4) 条，超出丢弃最旧
//   2. 每条通知 4s 生命周期：前 0.3s 从右侧滑入，最后 0.5s 淡出
//   3. 渲染在右上角（x=990, y=110 起），向下堆叠，避免与波次/FPS 信息冲突
//   4. 使用金色边框 + 深色半透明背景，突出成就解锁的仪式感
// ============================================================================

void HUD::AddAchievementToast(const std::string& name, const std::string& description) {
    AchievementToast toast;
    toast.name = name;
    toast.description = description;
    toast.maxLifetime = 4.0f;
    toast.lifetime = toast.maxLifetime;
    toast.slideInTimer = 0.f;

    toasts_.push_back(std::move(toast));

    // 超出上限则丢弃最旧（头部弹出）
    while (static_cast<int>(toasts_.size()) > kMaxToasts) {
        toasts_.pop_front();
    }
}

void HUD::UpdateToasts(float dt) {
    for (auto& toast : toasts_) {
        toast.lifetime -= dt;
        toast.slideInTimer += dt;
        if (toast.slideInTimer > kToastSlideInDuration) {
            toast.slideInTimer = kToastSlideInDuration;
        }
    }

    // 移除过期通知（lifetime <= 0）
    while (!toasts_.empty() && toasts_.front().lifetime <= 0.f) {
        toasts_.pop_front();
    }
}

void HUD::drawAchievementToasts(sf::RenderTarget& target) const {
    if (toasts_.empty()) return;

    // 右上角起始位置（避开波次信息 y=20-50 和 FPS y=80）
    const float startX = 1280.f - kToastWidth - 10.f; // 右边距 10px
    const float startY = 110.f;
    const float kFadeOutDuration = 0.5f;

    int index = 0;
    for (const auto& toast : toasts_) {
        // 计算淡入淡出 alpha
        // 滑入阶段（前 0.3s）：alpha 从 0 → 255
        // 稳定阶段：alpha = 255
        // 淡出阶段（最后 0.5s）：alpha 从 255 → 0
        uint8_t alpha = 255;
        if (toast.slideInTimer < kToastSlideInDuration) {
            float fadeIn = toast.slideInTimer / kToastSlideInDuration;
            alpha = static_cast<uint8_t>(255.f * fadeIn);
        } else if (toast.lifetime < kFadeOutDuration) {
            float fadeOut = toast.lifetime / kFadeOutDuration;
            alpha = static_cast<uint8_t>(255.f * fadeOut);
        }

        // 滑入偏移：从右侧 30px 滑入到目标位置（ease-out 缓动）
        float xOffset = 0.f;
        if (toast.slideInTimer < kToastSlideInDuration) {
            float slideProgress = toast.slideInTimer / kToastSlideInDuration;
            float eased = 1.f - (1.f - slideProgress) * (1.f - slideProgress);
            xOffset = (1.f - eased) * 30.f;
        }

        float toastX = startX + xOffset;
        float toastY = startY + index * (kToastHeight + kToastSpacing);

        // ---- 绘制背景（深色半透明 + 金色边框）----
        sf::RectangleShape bg(sf::Vector2f(kToastWidth, kToastHeight));
        bg.setPosition(toastX, toastY);
        bg.setFillColor(sf::Color(20, 20, 35, static_cast<uint8_t>(220 * (alpha / 255.f))));
        bg.setOutlineColor(sf::Color(255, 200, 50, alpha));
        bg.setOutlineThickness(2.f);
        target.draw(bg);

        // ---- 顶部金色装饰条（成就标识）----
        sf::RectangleShape accent(sf::Vector2f(kToastWidth, 3.f));
        accent.setPosition(toastX, toastY);
        accent.setFillColor(sf::Color(255, 215, 0, alpha));
        target.draw(accent);

        // ---- "成就解锁" 标签（小字，金色）----
        drawText(target, "成就解锁",
                 sf::Vector2f(toastX + 8.f, toastY + 6.f), 11,
                 sf::Color(255, 215, 0, alpha), sf::Text::Bold);

        // ---- 成就名称（大字，白色加粗）----
        drawText(target, toast.name,
                 sf::Vector2f(toastX + 8.f, toastY + 20.f), 16,
                 sf::Color(255, 255, 255, alpha), sf::Text::Bold);

        // ---- 成就描述（小字，浅灰色）----
        drawText(target, toast.description,
                 sf::Vector2f(toastX + 8.f, toastY + 40.f), 12,
                 sf::Color(200, 200, 200, alpha));

        ++index;
    }
}

// ============================================================================
// drawCombo —— 第十八轮新增：连击系统 HUD 渲染
// ----------------------------------------------------------------------------
// 视觉设计：
//   - 屏幕中央上方（x=640, y=180）显示连击信息
//   - 仅在 combo >= 5 时显示（小连击不打扰玩家视线）
//   - 三层视觉：连击数（大字 + 颜色阶梯）+ 伤害加成（中等字）+ 保持时间进度条
//   - 颜色阶梯（与伤害乘数同步，让玩家直观感知当前阶段）：
//       combo 5-9:   白色（无加成阶段，准备期）
//       combo 10-24:  黄色（+20%，前期甜头）
//       combo 25-49:  橙色（+35%，中期核心）
//       combo 50-99:  红色（+50%，激进 build 收益）
//       combo 100+:   紫色（+75%，巅峰阶段）
//   - 脉冲动画：每次新击杀时数字放大 1.3 倍后回弹（基于 comboPulseTimer）
// ============================================================================
void HUD::drawCombo(sf::RenderTarget& target) const {
    if (comboCount_ < 5) return;

    // ---- 推进脉冲动画计时器（mutable 允许 const 方法修改）----
    // 每帧衰减 1/60s，配合固定步长 1/30s 约两帧衰减
    if (comboPulseTimer_ > 0.f) {
        comboPulseTimer_ -= 1.f / 60.f;
        if (comboPulseTimer_ < 0.f) comboPulseTimer_ = 0.f;
    }

    // ---- 颜色阶梯（与 CombatSystem::GetComboDamageMultiplier 同步）----
    sf::Color comboColor;
    if (comboCount_ < 10) {
        comboColor = sf::Color::White;                                // 准备期
    } else if (comboCount_ < 25) {
        comboColor = sf::Color(255, 220, 80);                         // 黄色 +20%
    } else if (comboCount_ < 50) {
        comboColor = sf::Color(255, 140, 50);                         // 橙色 +35%
    } else if (comboCount_ < 100) {
        comboColor = sf::Color(255, 70, 70);                          // 红色 +50%
    } else {
        comboColor = sf::Color(220, 80, 255);                         // 紫色 +75%
    }

    // ---- 脉冲缩放（新击杀时 comboPulseTimer 由外部重置，0.3s 衰减）----
    // 缩放曲线：1.0 + 0.3 * (pulseTimer / 0.3)，从 1.3 缓动到 1.0
    float pulseScale = 1.f + 0.3f * (comboPulseTimer_ / 0.3f);
    if (pulseScale > 1.3f) pulseScale = 1.3f;
    if (pulseScale < 1.f)  pulseScale = 1.f;

    // 居中位置
    const float centerX = 640.f;
    const float baseY = 180.f;

    // ---- 1. "连击 X" 大字（颜色阶梯，脉冲缩放）----
    {
        sf::Text comboText;
        comboText.setFont(*font_);
        comboText.setString(utf8ToSfString("连击 " + std::to_string(comboCount_)));
        comboText.setCharacterSize(static_cast<unsigned int>(36 * pulseScale));
        comboText.setFillColor(comboColor);
        comboText.setStyle(sf::Text::Bold);
        // 居中
        sf::FloatRect bounds = comboText.getLocalBounds();
        comboText.setOrigin(bounds.width / 2.f, bounds.height / 2.f);
        comboText.setPosition(centerX, baseY);
        target.draw(comboText);
    }

    // ---- 2. 伤害加成百分比（中等字号，亮金色）----
    if (comboDamageMul_ > 1.001f) {
        int bonusPercent = static_cast<int>((comboDamageMul_ - 1.f) * 100.f + 0.5f);
        drawText(target, "伤害 +" + std::to_string(bonusPercent) + "%",
                 sf::Vector2f(centerX - 60.f, baseY + 28.f), 18,
                 sf::Color(255, 215, 0), sf::Text::Bold);
    } else {
        // 无加成阶段（5-9）显示提示，鼓励玩家上 10
        drawText(target, "继续连击解锁加成",
                 sf::Vector2f(centerX - 80.f, baseY + 28.f), 14,
                 sf::Color(180, 180, 180));
    }

    // ---- 3. 保持时间进度条（位于连击数字下方）----
    // 进度 = comboTimer / 3.0s（每次击杀重置）
    // 颜色随剩余时间衰减：>2s 绿色 / >1s 黄色 / <1s 红色（紧迫感）
    {
        const float barWidth = 120.f;
        const float barHeight = 4.f;
        const float barX = centerX - barWidth / 2.f;
        const float barY = baseY + 54.f;

        // 背景
        sf::RectangleShape bg(sf::Vector2f(barWidth, barHeight));
        bg.setPosition(barX, barY);
        bg.setFillColor(sf::Color(40, 40, 40, 180));
        target.draw(bg);

        // 进度
        float progress = (comboTimer_ > 0.f) ? (comboTimer_ / 3.0f) : 0.f;
        if (progress > 1.f) progress = 1.f;
        if (progress < 0.f) progress = 0.f;

        sf::Color timerColor;
        if (comboTimer_ > 2.f) {
            timerColor = sf::Color(100, 220, 100); // 充足
        } else if (comboTimer_ > 1.f) {
            timerColor = sf::Color(255, 220, 80);  // 中等
        } else {
            timerColor = sf::Color(255, 80, 80);   // 紧迫
        }

        sf::RectangleShape fill(sf::Vector2f(barWidth * progress, barHeight));
        fill.setPosition(barX, barY);
        fill.setFillColor(timerColor);
        target.draw(fill);
    }
}

// ============================================================================
// drawPerfectDodge —— 极限闪避 buff 金色光环渲染（第二十轮新增）
// ----------------------------------------------------------------------------
// 设计意图：在屏幕四周渲染金色脉冲边框，直观标识极限闪避 buff 激活状态。
// 玩家触发极限闪避后 2 秒内，屏幕边缘有金色光晕脉冲，强化"防御反击成功"的视觉反馈。
// 与连击指示器（中央）形成"攻防双反馈"的视觉层次：
//   - 连击（攻击端）：屏幕中央数字+进度条
//   - 极限闪避（防御端）：屏幕四周金色光环
// 视觉：4 条边框矩形（上下左右），金色 (255,200,50) 半透明，alpha 随时间脉冲
// ============================================================================
void HUD::drawPerfectDodge(sf::RenderTarget& target) const {
    if (perfectDodgeBuffTimer_ <= 0.f) return;

    // 脉冲效果：基于剩余时间的正弦波动，让光环有"呼吸感"
    // buff 持续 2s，脉冲频率约 4Hz（2*PI/0.5s）
    float pulsePhase = (2.0f - perfectDodgeBuffTimer_) * 12.f; // 0~24
    float pulse = 0.5f + 0.5f * std::sin(pulsePhase); // 0~1
    // 剩余时间不足 0.3s 时淡出，避免突然消失
    float fadeOut = perfectDodgeBuffTimer_ < 0.3f ? (perfectDodgeBuffTimer_ / 0.3f) : 1.f;

    // 金色光环基础 alpha：80（半透明）+ 脉冲 60 = 80~140
    uint8_t alpha = static_cast<uint8_t>((80.f + 60.f * pulse) * fadeOut);
    sf::Color glowColor(255, 200, 50, alpha);

    // 屏幕逻辑分辨率 1280x720，4 条边框矩形
    const float thickness = 6.f; // 边框厚度
    const float margin = 0.f;    // 距离屏幕边缘

    // 上边框
    sf::RectangleShape top(sf::Vector2f(1280.f - margin * 2, thickness));
    top.setPosition(margin, margin);
    top.setFillColor(glowColor);
    target.draw(top);

    // 下边框
    sf::RectangleShape bottom(sf::Vector2f(1280.f - margin * 2, thickness));
    bottom.setPosition(margin, 720.f - thickness - margin);
    bottom.setFillColor(glowColor);
    target.draw(bottom);

    // 左边框
    sf::RectangleShape left(sf::Vector2f(thickness, 720.f - margin * 2));
    left.setPosition(margin, margin);
    left.setFillColor(glowColor);
    target.draw(left);

    // 右边框
    sf::RectangleShape right(sf::Vector2f(thickness, 720.f - margin * 2));
    right.setPosition(1280.f - thickness - margin, margin);
    right.setFillColor(glowColor);
    target.draw(right);

    // 屏幕中央上方显示"极限闪避 +50%"文字（buff 激活期间持续显示）
    if (font_) {
        sf::Text text;
        text.setFont(*font_);
        text.setString(U8("极限闪避  伤害 +50%"));
        text.setCharacterSize(20);
        text.setFillColor(sf::Color(255, 220, 100, static_cast<uint8_t>(255 * fadeOut)));
        text.setStyle(sf::Text::Bold);
        // 位置：屏幕中央上方，combo 指示器下方（combo 在 y=180，这里 y=230）
        sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
        text.setPosition(640.f, 230.f);
        target.draw(text);
    }
}

} // namespace cu
