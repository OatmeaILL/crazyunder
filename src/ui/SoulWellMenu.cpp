#include "ui/SoulWellMenu.h"
#include "utils/Logger.h"
#include <algorithm>
#include <string>

namespace cu {

// ============================================================================
// SoulWellMenu 实现
// ============================================================================

SoulWellMenu::SoulWellMenu() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
    buttonBounds_.fill(sf::FloatRect{});
    // 卡片配色：每条强化路径一种主色调
    kCardColors = {{
        sf::Color(220,  80,  80), // Vitality  红
        sf::Color(100, 160, 240), // Wisdom    蓝
        sf::Color(240, 200,  80), // Fortune   金
        sf::Color(220, 120,  60), // Strength  橙
        sf::Color(120, 220, 240), // Swiftness 青
        sf::Color(180, 180, 200), // Aegis     银灰
    }};
}

void SoulWellMenu::Initialize(const sf::Font& font) {
    font_ = &font;
}

void SoulWellMenu::SetSoulMemoryData(const SoulMemorySystem& sys) {
    shards_ = sys.GetShards();
    totalEarned_ = sys.GetTotalShardsEarned();
    for (int i = 0; i < kSoulUpgradeCount; ++i) {
        SoulUpgradeType type = static_cast<SoulUpgradeType>(i);
        upgradeLevels_[i] = sys.GetUpgradeLevel(type);
        upgradeCosts_[i] = sys.GetUpgradeCost(type); // -1 表示已满级
    }
}

void SoulWellMenu::Update(float dt) {
    UIElement::Update(dt);
}

void SoulWellMenu::UpdateHover(sf::Vector2f mousePos) {
    hoveredButton_ = -1;
    for (int i = 0; i < 8; ++i) {
        if (buttonBounds_[i].contains(mousePos)) {
            hoveredButton_ = i;
            break;
        }
    }
}

int SoulWellMenu::CheckClick(sf::Vector2f mousePos) const {
    for (int i = 0; i < 8; ++i) {
        if (buttonBounds_[i].contains(mousePos)) {
            // 已满级的强化（i<6 且 costs_==-1）点击不返回操作
            if (i < 6 && upgradeCosts_[i] < 0) return 0;
            if (i == 7) return 7; // 返回按钮
            return i + 1; // 1-6=购买强化
        }
    }
    return 0;
}

void SoulWellMenu::Render(sf::RenderTarget& target) const {
    if (!visible_ || !font_) return;

    // ---- 半透明遮罩 ----
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(10, 10, 20, 220));
    target.draw(overlay);

    // ---- 主面板（800x600 居中）----
    constexpr float kPanelX = 240.f;
    constexpr float kPanelY = 60.f;
    constexpr float kPanelW = 800.f;
    constexpr float kPanelH = 600.f;

    sf::RectangleShape panel(sf::Vector2f(kPanelW, kPanelH));
    panel.setPosition(kPanelX, kPanelY);
    panel.setFillColor(sf::Color(30, 25, 50, 240));
    panel.setOutlineColor(sf::Color(180, 140, 240));
    panel.setOutlineThickness(3.f);
    target.draw(panel);

    // ---- 标题 ----
    sf::Text title;
    title.setFont(*font_);
    title.setString(U8("灵魂之井"));
    title.setCharacterSize(36);
    title.setFillColor(sf::Color(220, 180, 255));
    title.setStyle(sf::Text::Bold);
    title.setPosition(kPanelX + kPanelW * 0.5f - 90.f, kPanelY + 15.f);
    target.draw(title);

    // ---- 副标题：碎片信息 ----
    sf::Text shardsText;
    shardsText.setFont(*font_);
    shardsText.setString(
        U8("灵魂碎片: ") + std::to_string(shards_) +
        U8("    累计获得: ") + std::to_string(totalEarned_));
    shardsText.setCharacterSize(20);
    shardsText.setFillColor(sf::Color(240, 220, 180));
    shardsText.setPosition(kPanelX + 30.f, kPanelY + 65.f);
    target.draw(shardsText);

    // 提示文字
    sf::Text hint;
    hint.setFont(*font_);
    hint.setString(U8("死亡时根据本局表现获得碎片，消耗碎片购买永久强化"));
    hint.setCharacterSize(14);
    hint.setFillColor(sf::Color(160, 160, 180));
    hint.setPosition(kPanelX + 30.f, kPanelY + 92.f);
    target.draw(hint);

    // ---- 6 张卡片（3列 x 2行）----
    constexpr float kCardW = 240.f;
    constexpr float kCardH = 200.f;
    constexpr float kCardGapX = 10.f;
    constexpr float kCardGapY = 10.f;
    constexpr float kCardsStartX = kPanelX + 15.f;
    constexpr float kCardsStartY = kPanelY + 120.f;

    for (int i = 0; i < kSoulUpgradeCount; ++i) {
        int col = i % 3;
        int row = i / 3;
        float cx = kCardsStartX + col * (kCardW + kCardGapX);
        float cy = kCardsStartY + row * (kCardH + kCardGapY);

        sf::Color cardColor = kCardColors[i];
        int level = upgradeLevels_[i];
        int cost = upgradeCosts_[i];
        bool maxed = (cost < 0);
        bool canAfford = (!maxed && shards_ >= cost);

        // 卡片背景
        sf::RectangleShape card(sf::Vector2f(kCardW, kCardH));
        card.setPosition(cx, cy);
        card.setFillColor(sf::Color(40, 35, 55, 230));
        card.setOutlineColor(maxed ? sf::Color(100, 100, 100) : cardColor);
        card.setOutlineThickness(hoveredButton_ == i ? 3.f : 2.f);
        target.draw(card);

        // 顶部色条
        sf::RectangleShape colorBar(sf::Vector2f(kCardW, 6.f));
        colorBar.setPosition(cx, cy);
        colorBar.setFillColor(cardColor);
        target.draw(colorBar);

        // 强化名
        SoulUpgradeType type = static_cast<SoulUpgradeType>(i);
        // 注意：U8 宏对 const char* 变量有 sizeof 陷阱（取指针大小 8 字节），
        // 必须使用 utf8ToSfString 走 std::string 迭代器路径
        sf::Text nameText;
        nameText.setFont(*font_);
        nameText.setString(utf8ToSfString(SoulMemorySystem::GetUpgradeName(type)));
        nameText.setCharacterSize(22);
        nameText.setFillColor(cardColor);
        nameText.setStyle(sf::Text::Bold);
        nameText.setPosition(cx + 10.f, cy + 12.f);
        target.draw(nameText);

        // 等级显示 + 进度点（5个圆点）
        sf::Text levelText;
        levelText.setFont(*font_);
        levelText.setString(U8("Lv ") + std::to_string(level) + U8("/") +
                            std::to_string(kSoulUpgradeMaxLevel));
        levelText.setCharacterSize(16);
        levelText.setFillColor(sf::Color(200, 200, 200));
        levelText.setPosition(cx + 10.f, cy + 42.f);
        target.draw(levelText);

        // 5 个进度圆点
        for (int dot = 0; dot < kSoulUpgradeMaxLevel; ++dot) {
            sf::CircleShape dotShape(5.f);
            dotShape.setPosition(cx + 95.f + dot * 18.f, cy + 50.f);
            if (dot < level) {
                dotShape.setFillColor(cardColor);
            } else {
                dotShape.setFillColor(sf::Color(60, 60, 70));
            }
            target.draw(dotShape);
        }

        // 描述
        sf::Text descText;
        descText.setFont(*font_);
        descText.setString(utf8ToSfString(SoulMemorySystem::GetUpgradeDescription(type)));
        descText.setCharacterSize(14);
        descText.setFillColor(sf::Color(180, 180, 180));
        descText.setPosition(cx + 10.f, cy + 75.f);
        target.draw(descText);

        // 当前总效果
        int effectPerLevel = SoulMemorySystem::GetUpgradeEffectPerLevel(type);
        int totalEffect = effectPerLevel * level;
        sf::String effectStr;
        switch (type) {
            case SoulUpgradeType::Vitality:
                effectStr = U8("当前加成: +") + std::to_string(totalEffect) + U8(" 生命");
                break;
            case SoulUpgradeType::Wisdom:
                effectStr = U8("当前加成: +") + std::to_string(totalEffect * 10) +
                            U8("% 经验 (") + std::to_string(totalEffect) + U8("级)");
                break;
            case SoulUpgradeType::Fortune:
                effectStr = U8("当前加成: +") + std::to_string(totalEffect * 15) +
                            U8("% 金币 (") + std::to_string(totalEffect) + U8("级)");
                break;
            case SoulUpgradeType::Strength:
                effectStr = U8("当前加成: +") + std::to_string(totalEffect * 5) +
                            U8("% 伤害 (") + std::to_string(totalEffect) + U8("级)");
                break;
            case SoulUpgradeType::Swiftness:
                effectStr = U8("当前加成: +") + std::to_string(totalEffect * 5) +
                            U8("% 移速 (") + std::to_string(totalEffect) + U8("级)");
                break;
            case SoulUpgradeType::Aegis:
                effectStr = U8("当前加成: +") + std::to_string(totalEffect * 3) + U8(" 防御");
                break;
            default:
                break;
        }
        sf::Text effectText;
        effectText.setFont(*font_);
        effectText.setString(effectStr);
        effectText.setCharacterSize(13);
        effectText.setFillColor(sf::Color(150, 200, 150));
        effectText.setPosition(cx + 10.f, cy + 100.f);
        target.draw(effectText);

        // 购买按钮 / 满级提示
        constexpr float kBtnW = 200.f;
        constexpr float kBtnH = 36.f;
        float btnX = cx + (kCardW - kBtnW) * 0.5f;
        float btnY = cy + kCardH - kBtnH - 12.f;
        buttonBounds_[i] = sf::FloatRect(btnX, btnY, kBtnW, kBtnH);

        sf::RectangleShape btn(sf::Vector2f(kBtnW, kBtnH));
        btn.setPosition(btnX, btnY);
        if (maxed) {
            btn.setFillColor(sf::Color(60, 60, 60));
            btn.setOutlineColor(sf::Color(100, 100, 100));
        } else if (!canAfford) {
            btn.setFillColor(sf::Color(80, 50, 50));
            btn.setOutlineColor(sf::Color(120, 80, 80));
        } else {
            btn.setFillColor(hoveredButton_ == i ? sf::Color(cardColor.r * 2 / 3,
                                                              cardColor.g * 2 / 3,
                                                              cardColor.b * 2 / 3)
                                                 : sf::Color(50, 45, 65));
            btn.setOutlineColor(cardColor);
        }
        btn.setOutlineThickness(2.f);
        target.draw(btn);

        sf::Text btnText;
        btnText.setFont(*font_);
        if (maxed) {
            btnText.setString(U8("已满级"));
            btnText.setFillColor(sf::Color(150, 150, 150));
        } else if (!canAfford) {
            btnText.setString(U8("碎片不足 ") + std::to_string(cost));
            btnText.setFillColor(sf::Color(200, 150, 150));
        } else {
            btnText.setString(U8("强化  消耗 ") + std::to_string(cost));
            btnText.setFillColor(sf::Color::White);
        }
        btnText.setCharacterSize(15);
        sf::FloatRect btnBounds = btnText.getLocalBounds();
        btnText.setPosition(
            btnX + (kBtnW - btnBounds.width) * 0.5f,
            btnY + (kBtnH - btnBounds.height) * 0.5f - 3.f);
        target.draw(btnText);
    }

    // ---- 返回按钮（底部居中）----
    constexpr float kBackBtnW = 180.f;
    constexpr float kBackBtnH = 40.f;
    float backX = kPanelX + (kPanelW - kBackBtnW) * 0.5f;
    float backY = kPanelY + kPanelH - kBackBtnH - 15.f;
    buttonBounds_[7] = sf::FloatRect(backX, backY, kBackBtnW, kBackBtnH);

    sf::RectangleShape backBtn(sf::Vector2f(kBackBtnW, kBackBtnH));
    backBtn.setPosition(backX, backY);
    backBtn.setFillColor(hoveredButton_ == 7 ? sf::Color(90, 90, 110) : sf::Color(60, 60, 80));
    backBtn.setOutlineColor(sf::Color(140, 140, 160));
    backBtn.setOutlineThickness(2.f);
    target.draw(backBtn);

    sf::Text backText;
    backText.setFont(*font_);
    backText.setString(U8("返回 (ESC)"));
    backText.setCharacterSize(18);
    backText.setFillColor(sf::Color::White);
    sf::FloatRect backBounds = backText.getLocalBounds();
    backText.setPosition(
        backX + (kBackBtnW - backBounds.width) * 0.5f,
        backY + (kBackBtnH - backBounds.height) * 0.5f - 3.f);
    target.draw(backText);
}

} // namespace cu
