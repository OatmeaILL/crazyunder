#include "ui/AchievementMenu.h"
#include "utils/Logger.h"
#include <ctime>
#include <cstring>

namespace cu {

AchievementMenu::AchievementMenu() {
    // 面板尺寸 1100x650，居中
    size_ = sf::Vector2f(1100.f, 650.f);
    position_ = sf::Vector2f(90.f, 35.f);
    anchor_ = UIAnchor::TopLeft;
    visible_ = false;
}

void AchievementMenu::Initialize(const sf::Font& font) {
    font_ = &font;
    achievements_.clear();
}

// ============================================================================
// SetAchievementData —— 从 AchievementSystem 拷贝数据
// ============================================================================
void AchievementMenu::SetAchievementData(const AchievementSystem& achievementSystem) {
    achievements_.clear();
    const auto& states = achievementSystem.GetAllAchievements();
    achievements_.reserve(states.size());

    unlockedCount_ = 0;
    totalCount_ = static_cast<int>(states.size());

    for (const auto& s : states) {
        const AchievementDef* def = achievementSystem.GetAchievementDef(s.id);
        if (!def) continue;

        AchievementDisplay d;
        d.id = s.id;
        d.category = def->category;
        d.name = def->name;
        d.description = def->description;
        d.unlocked = s.unlocked;
        d.isHidden = def->isHidden;
        d.currentValue = s.currentValue;
        d.targetValue = def->targetValue;
        d.unlockedTimestamp = s.unlockedTimestamp;
        achievements_.push_back(d);

        if (s.unlocked) ++unlockedCount_;
    }
}

// ============================================================================
// Render —— 渲染成就面板
// ============================================================================
void AchievementMenu::Render(sf::RenderTarget& target) const {
    if (!visible_ || !font_) return;

    // 半透明遮罩
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setPosition(0.f, 0.f);
    overlay.setFillColor(sf::Color(0, 0, 0, 150));
    target.draw(overlay);

    // 主面板背景
    sf::RectangleShape bg(size_);
    bg.setPosition(position_);
    bg.setFillColor(sf::Color(20, 25, 40, 240));
    bg.setOutlineColor(sf::Color(150, 180, 220));
    bg.setOutlineThickness(3.f);
    target.draw(bg);

    // 标题
    sf::Text title;
    title.setFont(*font_);
    title.setString(U8("成就殿堂  (按 Tab 关闭)"));
    title.setCharacterSize(28);
    title.setFillColor(sf::Color(180, 220, 255));
    title.setStyle(sf::Text::Bold);
    title.setPosition(position_.x + 30.f, position_.y + 15.f);
    target.draw(title);

    // 解锁进度统计
    int pct = (totalCount_ > 0) ? (unlockedCount_ * 100 / totalCount_) : 0;
    sf::Text stats;
    stats.setFont(*font_);
    stats.setString(U8("已解锁: ") +
                    sf::String(std::to_wstring(unlockedCount_) + L" / " +
                               std::to_wstring(totalCount_) +
                               L"  (" + std::to_wstring(pct) + L"%)"));
    stats.setCharacterSize(18);
    stats.setFillColor(sf::Color(220, 220, 100));
    stats.setPosition(position_.x + size_.x - 320.f, position_.y + 22.f);
    target.draw(stats);

    // 分隔线
    sf::RectangleShape sep(sf::Vector2f(size_.x - 60.f, 2.f));
    sep.setPosition(position_.x + 30.f, position_.y + 55.f);
    sep.setFillColor(sf::Color(150, 180, 220, 180));
    target.draw(sep);

    // ---- 按分类分组渲染 ----
    // 4 个分类，每个分类一栏
    const float catX[4] = {
        position_.x + 30.f,
        position_.x + 30.f + 260.f,
        position_.x + 30.f + 520.f,
        position_.x + 30.f + 780.f,
    };
    const float catW = 250.f;
    float catY[4] = {
        position_.y + 75.f,
        position_.y + 75.f,
        position_.y + 75.f,
        position_.y + 75.f,
    };

    // 分类标题
    const AchievementCategory cats[4] = {
        AchievementCategory::Combat,
        AchievementCategory::Exploration,
        AchievementCategory::Collection,
        AchievementCategory::Special,
    };
    for (int c = 0; c < 4; ++c) {
        sf::Text cTitle;
        cTitle.setFont(*font_);
        cTitle.setString(U8(getCategoryName(cats[c])));
        cTitle.setCharacterSize(20);
        cTitle.setFillColor(getCategoryColor(cats[c]));
        cTitle.setStyle(sf::Text::Bold);
        cTitle.setPosition(catX[c], catY[c]);
        target.draw(cTitle);
        catY[c] += 30.f;
    }

    // 渲染每个成就（按分类入栏）
    const float cardH = 95.f;
    const float cardGap = 6.f;

    for (const auto& a : achievements_) {
        int c = static_cast<int>(a.category);
        if (c < 0 || c > 3) c = 0;

        // 卡片背景
        sf::RectangleShape cardBg(sf::Vector2f(catW, cardH));
        cardBg.setPosition(catX[c], catY[c]);
        if (a.unlocked) {
            cardBg.setFillColor(sf::Color(35, 50, 40, 230));
            cardBg.setOutlineColor(getCategoryColor(a.category));
        } else {
            cardBg.setFillColor(sf::Color(25, 25, 35, 200));
            cardBg.setOutlineColor(sf::Color(80, 80, 90));
        }
        cardBg.setOutlineThickness(2.f);
        target.draw(cardBg);

        // 名称
        sf::Text name;
        name.setFont(*font_);
        if (a.unlocked) {
            name.setString(sf::String::fromUtf8(a.name.begin(), a.name.end()));
            name.setFillColor(getCategoryColor(a.category));
        } else if (a.isHidden) {
            name.setString(U8("???"));
            name.setFillColor(sf::Color(100, 100, 110));
        } else {
            name.setString(sf::String::fromUtf8(a.name.begin(), a.name.end()));
            name.setFillColor(sf::Color(150, 150, 160));
        }
        name.setCharacterSize(16);
        name.setStyle(sf::Text::Bold);
        name.setPosition(catX[c] + 8.f, catY[c] + 5.f);
        target.draw(name);

        // 描述
        std::string descToShow = a.unlocked ? a.description : (a.isHidden ? "隐藏成就" : a.description);
        drawWrappedText(target, descToShow,
                        sf::Vector2f(catX[c] + 8.f, catY[c] + 26.f),
                        catW - 16.f, 14.f, 12,
                        a.unlocked ? sf::Color(200, 220, 200) : sf::Color(130, 130, 140));

        // 底部信息
        if (a.unlocked) {
            // 解锁时间
            std::string tsStr = formatTimestamp(a.unlockedTimestamp);
            sf::Text ts;
            ts.setFont(*font_);
            ts.setString(sf::String::fromUtf8(tsStr.begin(), tsStr.end()));
            ts.setCharacterSize(11);
            ts.setFillColor(sf::Color(150, 180, 150));
            ts.setPosition(catX[c] + 8.f, catY[c] + cardH - 18.f);
            target.draw(ts);
        } else if (!a.isHidden && a.targetValue > 0) {
            // 进度条
            const float pbX = catX[c] + 8.f;
            const float pbY = catY[c] + cardH - 16.f;
            const float pbW = catW - 60.f;
            const float pbH = 8.f;

            sf::RectangleShape pbBg(sf::Vector2f(pbW, pbH));
            pbBg.setPosition(pbX, pbY);
            pbBg.setFillColor(sf::Color(15, 15, 20, 200));
            target.draw(pbBg);

            float ratio = (a.targetValue > 0)
                ? static_cast<float>(a.currentValue) / static_cast<float>(a.targetValue)
                : 0.f;
            if (ratio > 1.f) ratio = 1.f;
            if (ratio > 0.f) {
                sf::RectangleShape pbFg(sf::Vector2f(pbW * ratio, pbH));
                pbFg.setPosition(pbX, pbY);
                pbFg.setFillColor(sf::Color(120, 150, 200));
                target.draw(pbFg);
            }

            // 进度文字
            std::string pStr = std::to_string(a.currentValue) + "/" + std::to_string(a.targetValue);
            sf::Text pTxt;
            pTxt.setFont(*font_);
            pTxt.setString(sf::String::fromUtf8(pStr.begin(), pStr.end()));
            pTxt.setCharacterSize(11);
            pTxt.setFillColor(sf::Color(180, 180, 200));
            pTxt.setPosition(pbX + pbW + 4.f, pbY - 2.f);
            target.draw(pTxt);
        }

        catY[c] += cardH + cardGap;
    }
}

// ============================================================================
// 辅助方法
// ============================================================================
sf::Color AchievementMenu::getCategoryColor(AchievementCategory cat) const {
    switch (cat) {
        case AchievementCategory::Combat:      return sf::Color(255, 120, 100); // 红
        case AchievementCategory::Exploration: return sf::Color(120, 200, 255); // 蓝
        case AchievementCategory::Collection:  return sf::Color(255, 215, 100); // 金
        case AchievementCategory::Special:     return sf::Color(200, 130, 255); // 紫
    }
    return sf::Color::White;
}

const char* AchievementMenu::getCategoryName(AchievementCategory cat) const {
    switch (cat) {
        case AchievementCategory::Combat:      return "=== 战斗类 ===";
        case AchievementCategory::Exploration: return "=== 探索类 ===";
        case AchievementCategory::Collection:  return "=== 收集类 ===";
        case AchievementCategory::Special:     return "=== 特殊类 ===";
    }
    return "?";
}

std::string AchievementMenu::formatTimestamp(int64_t ts) {
    if (ts <= 0) return "";
    std::time_t t = static_cast<std::time_t>(ts);
    std::tm* lt = std::localtime(&t);
    if (!lt) return "";
    char buf[32] = {0};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", lt);
    return std::string(buf);
}

void AchievementMenu::drawWrappedText(sf::RenderTarget& target, const std::string& text,
                                      sf::Vector2f pos, float maxWidth, float lineSpacing,
                                      unsigned int charSize, sf::Color color) const {
    if (!font_ || maxWidth <= 0.f) return;

    // 按 UTF-8 字符边界切分，用 sf::Text 实际测量宽度
    sf::Text measurer;
    measurer.setFont(*font_);
    measurer.setCharacterSize(charSize);
    measurer.setFillColor(color);

    float y = pos.y;
    size_t i = 0;

    while (i < text.size()) {
        size_t nl = text.find('\n', i);
        if (nl == std::string::npos) nl = text.size();

        size_t lineStart = i;
        size_t lineEnd = i;

        while (lineEnd < nl) {
            // 计算下一个 UTF-8 字符的字节长度
            size_t charLen = 1;
            unsigned char c = static_cast<unsigned char>(text[lineEnd]);
            if (c < 0x80) charLen = 1;
            else if ((c & 0xE0) == 0xC0) charLen = 2;
            else if ((c & 0xF0) == 0xE0) charLen = 3;
            else if ((c & 0xF8) == 0xF0) charLen = 4;

            size_t nextEnd = lineEnd + charLen;
            if (nextEnd > nl) nextEnd = nl;

            std::string sub = text.substr(lineStart, nextEnd - lineStart);
            measurer.setString(sf::String::fromUtf8(sub.begin(), sub.end()));
            float w = measurer.getLocalBounds().width;

            if (w > maxWidth && lineEnd > lineStart) {
                std::string currentLine = text.substr(lineStart, lineEnd - lineStart);
                measurer.setString(sf::String::fromUtf8(currentLine.begin(), currentLine.end()));
                measurer.setPosition(pos.x, y);
                target.draw(measurer);
                y += lineSpacing + static_cast<float>(charSize);
                lineStart = lineEnd;
            } else {
                lineEnd = nextEnd;
            }
        }

        if (lineEnd > lineStart) {
            std::string currentLine = text.substr(lineStart, lineEnd - lineStart);
            measurer.setString(sf::String::fromUtf8(currentLine.begin(), currentLine.end()));
            measurer.setPosition(pos.x, y);
            target.draw(measurer);
            y += lineSpacing + static_cast<float>(charSize);
        }

        i = nl + 1;
    }
}

} // namespace cu
