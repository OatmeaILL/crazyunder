#include "ui/QuestMenu.h"
#include "utils/Logger.h"
#include <algorithm>

namespace cu {

QuestMenu::QuestMenu() {
    // 面板尺寸 1100x650，居中（逻辑 1280x720）
    size_ = sf::Vector2f(1100.f, 650.f);
    position_ = sf::Vector2f(90.f, 35.f);
    anchor_ = UIAnchor::TopLeft;
    visible_ = false;
}

void QuestMenu::Initialize(const sf::Font& font) {
    font_ = &font;
    // 初始化卡片数据为默认
    for (auto& c : cards_) {
        c.id = 0;
        c.state = QuestState::Locked;
        c.claimBtnBounds = sf::FloatRect();
    }
}

// ============================================================================
// SetQuestData —— 从 QuestSystem 拷贝任务数据到 UI 缓存
// ============================================================================
void QuestMenu::SetQuestData(const QuestSystem& questSystem) {
    const auto& quests = questSystem.GetAllQuests();
    for (size_t i = 0; i < cards_.size(); ++i) {
        auto& card = cards_[i];
        if (i < quests.size()) {
            const auto& q = quests[i];
            const QuestDef* def = questSystem.GetQuestDef(q.id);
            card.id = q.id;
            card.state = q.state;
            card.title = def ? def->title : "?";
            card.description = def ? def->description : "?";
            card.currentProgress = q.currentProgress;
            card.targetProgress = q.targetProgress;
            card.reward = def ? def->reward : QuestReward{};
        } else {
            // 超出实际任务数的卡片清空（避免渲染残留）
            card.id = 0;
            card.state = QuestState::Locked;
            card.title.clear();
            card.description.clear();
            card.currentProgress = 0;
            card.targetProgress = 1;
            card.reward = QuestReward{};
            card.claimBtnBounds = sf::FloatRect();
        }
    }

    // ---- 第二十二轮新增：计算最大滚动偏移 ----
    // 卡片布局参数（与 Render 中保持一致）
    const float cardH = 110.f;
    const float cardGap = 6.f;
    // 统计有效任务数（id != 0）
    int validCount = 0;
    for (const auto& c : cards_) {
        if (c.id != 0) ++validCount;
    }
    if (validCount > 0) {
        const float totalContentHeight = validCount * cardH + (validCount - 1) * cardGap;
        // 可见区域：面板高度 - 顶部偏移(75) - 底部留白(10)
        const float visibleHeight = size_.y - 75.f - 10.f;
        maxScrollOffset_ = (totalContentHeight > visibleHeight)
                           ? (totalContentHeight - visibleHeight) : 0.f;
    } else {
        maxScrollOffset_ = 0.f;
    }
    // 重置滚动偏移到合理范围（避免数据刷新后越界）
    if (scrollOffset_ > maxScrollOffset_) scrollOffset_ = maxScrollOffset_;
    if (scrollOffset_ < 0.f) scrollOffset_ = 0.f;
}

// ============================================================================
// Update —— 每帧累加闪烁计时器
// ============================================================================
void QuestMenu::Update(float dt) {
    blinkTimer_ += dt;
}

void QuestMenu::UpdateHover(sf::Vector2f mousePos) {
    hoveredClaimBtn_ = -1;
    for (size_t i = 0; i < cards_.size(); ++i) {
        if (cards_[i].state != QuestState::Completed) continue;
        if (cards_[i].claimBtnBounds.contains(mousePos)) {
            hoveredClaimBtn_ = static_cast<int>(i);
            return;
        }
    }
}

std::pair<int, int> QuestMenu::CheckClick(sf::Vector2f mousePos) const {
    for (size_t i = 0; i < cards_.size(); ++i) {
        if (cards_[i].state != QuestState::Completed) continue;
        if (cards_[i].claimBtnBounds.contains(mousePos)) {
            return {1, cards_[i].id};
        }
    }
    return {0, 0};
}

// ============================================================================
// 第二十二轮新增：OnMouseWheel —— 处理鼠标滚轮滚动
// ----------------------------------------------------------------------------
// delta: 正值=向上滚（scrollOffset_ 减小），负值=向下滚（scrollOffset_ 增大）
// 滚动范围限制在 [0, maxScrollOffset_]，超出则 clamp
// ============================================================================
void QuestMenu::OnMouseWheel(float delta) {
    if (maxScrollOffset_ <= 0.f) return; // 无可滚动内容
    scrollOffset_ -= delta * kScrollStep;
    if (scrollOffset_ < 0.f) scrollOffset_ = 0.f;
    if (scrollOffset_ > maxScrollOffset_) scrollOffset_ = maxScrollOffset_;
}

// ============================================================================
// Render —— 渲染任务面板
// ============================================================================
void QuestMenu::Render(sf::RenderTarget& target) const {
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
    bg.setOutlineColor(sf::Color(180, 150, 80));
    bg.setOutlineThickness(3.f);
    target.draw(bg);

    // 标题
    sf::Text title;
    title.setFont(*font_);
    title.setString(U8("任务日志  (按 Q 关闭)"));
    title.setCharacterSize(28);
    title.setFillColor(sf::Color(255, 215, 100));
    title.setStyle(sf::Text::Bold);
    title.setPosition(position_.x + 30.f, position_.y + 15.f);
    target.draw(title);

    // 分隔线
    sf::RectangleShape sep(sf::Vector2f(size_.x - 60.f, 2.f));
    sep.setPosition(position_.x + 30.f, position_.y + 55.f);
    sep.setFillColor(sf::Color(180, 150, 80, 180));
    target.draw(sep);

    // ---- 左侧：任务卡片列表 ----
    // 卡片布局（cardH=110）：
    //   y=4   标题(字号18) + 状态标签(右上)
    //   y=28  描述(字号12, 最多2行, 行高26)
    //   y=72  奖励摘要(字号11)
    //   y=90  进度条(高8) + 领取按钮(右侧, 高24)
    const float cardX = position_.x + 30.f;
    const float cardW = 620.f;
    // 第二十二轮新增：cardY 起始减去 scrollOffset_ 实现垂直滚动
    float cardY = position_.y + 75.f - scrollOffset_;
    const float cardH = 110.f;
    const float cardGap = 6.f;

    // 第二十二轮新增：可见区域边界（用于裁剪溢出卡片）
    const float listTop = position_.y + 75.f;
    const float listBottom = position_.y + size_.y - 10.f;

    for (size_t i = 0; i < cards_.size(); ++i) {
        const auto& card = cards_[i];
        if (card.id == 0) {
            continue;
        }

        // 第二十二轮新增：裁剪完全在可见区域外的卡片（性能优化）
        // 注意：部分可见的卡片仍渲染，溢出部分由后续遮罩覆盖
        if (cardY + cardH < listTop || cardY > listBottom) {
            cardY += cardH + cardGap;
            continue;
        }

        // 卡片背景
        sf::RectangleShape cardBg(sf::Vector2f(cardW, cardH));
        cardBg.setPosition(cardX, cardY);
        sf::Color stateColor = getStateColor(card.state);

        // Completed 状态闪烁高亮
        if (card.state == QuestState::Completed) {
            float blink = 0.5f + 0.5f * std::sin(blinkTimer_ * 4.f);
            cardBg.setFillColor(sf::Color(
                40 + static_cast<uint8_t>(blink * 30),
                50 + static_cast<uint8_t>(blink * 40),
                35,
                230));
        } else {
            cardBg.setFillColor(sf::Color(30, 35, 50, 220));
        }
        cardBg.setOutlineColor(stateColor);
        cardBg.setOutlineThickness(2.f);
        target.draw(cardBg);

        // 状态标识条（左侧竖条）
        sf::RectangleShape stateBar(sf::Vector2f(5.f, cardH));
        stateBar.setPosition(cardX, cardY);
        stateBar.setFillColor(stateColor);
        target.draw(stateBar);

        // 标题（y=4）
        sf::Text t;
        t.setFont(*font_);
        t.setString(sf::String::fromUtf8(card.title.begin(), card.title.end()));
        t.setCharacterSize(18);
        t.setFillColor(stateColor);
        t.setStyle(sf::Text::Bold);
        t.setPosition(cardX + 15.f, cardY + 4.f);
        target.draw(t);

        // 状态标签（右上, y=6）
        sf::Text sLabel;
        sLabel.setFont(*font_);
        sLabel.setString(U8(getStateText(card.state)));
        sLabel.setCharacterSize(13);
        sLabel.setFillColor(stateColor);
        sLabel.setPosition(cardX + cardW - 100.f, cardY + 6.f);
        target.draw(sLabel);

        // 描述（自动换行, y=28, 最多2行, 限制宽度避免和右侧按钮重合）
        drawWrappedText(target, card.description,
                        sf::Vector2f(cardX + 15.f, cardY + 28.f),
                        cardW - 220.f, 12.f, 12, sf::Color(200, 200, 210));

        // 奖励摘要（y=72）
        std::string rewardStr = formatReward(card.reward);
        if (!rewardStr.empty()) {
            sf::Text rTxt;
            rTxt.setFont(*font_);
            rTxt.setString(sf::String::fromUtf8(rewardStr.begin(), rewardStr.end()));
            rTxt.setCharacterSize(11);
            rTxt.setFillColor(sf::Color(255, 215, 100));
            rTxt.setPosition(cardX + 15.f, cardY + 72.f);
            target.draw(rTxt);
        }

        // 进度条（y=90, 左侧）
        const float barX = cardX + 15.f;
        const float barY = cardY + 92.f;
        const float barW = 380.f;
        const float barH = 8.f;

        sf::RectangleShape barBg(sf::Vector2f(barW, barH));
        barBg.setPosition(barX, barY);
        barBg.setFillColor(sf::Color(15, 15, 20, 220));
        barBg.setOutlineColor(sf::Color(80, 80, 90));
        barBg.setOutlineThickness(1.f);
        target.draw(barBg);

        float ratio = (card.targetProgress > 0)
            ? static_cast<float>(card.currentProgress) / static_cast<float>(card.targetProgress)
            : 0.f;
        if (ratio > 1.f) ratio = 1.f;
        if (ratio > 0.f) {
            sf::RectangleShape barFg(sf::Vector2f(barW * ratio, barH));
            barFg.setPosition(barX, barY);
            barFg.setFillColor(stateColor);
            target.draw(barFg);
        }

        // 进度文字（进度条右侧）
        sf::Text pTxt;
        pTxt.setFont(*font_);
        pTxt.setString(sf::String(std::to_wstring(card.currentProgress) + L" / " +
                                   std::to_wstring(card.targetProgress)));
        pTxt.setCharacterSize(11);
        pTxt.setFillColor(sf::Color(220, 220, 220));
        pTxt.setPosition(barX + barW + 8.f, barY - 2.f);
        target.draw(pTxt);

        // 领取奖励按钮（仅 Completed 状态, 右下角）
        if (card.state == QuestState::Completed) {
            const float btnW = 90.f;
            const float btnH = 26.f;
            const float btnX = cardX + cardW - btnW - 15.f;
            const float btnY = cardY + 82.f;
            const_cast<QuestCardData&>(card).claimBtnBounds = sf::FloatRect(btnX, btnY, btnW, btnH);

            sf::RectangleShape btn(sf::Vector2f(btnW, btnH));
            btn.setPosition(btnX, btnY);
            bool hovered = (hoveredClaimBtn_ == static_cast<int>(i));
            btn.setFillColor(hovered ? sf::Color(120, 100, 40) : sf::Color(80, 65, 25));
            btn.setOutlineColor(sf::Color(255, 215, 100));
            btn.setOutlineThickness(2.f);
            target.draw(btn);

            sf::Text btnTxt;
            btnTxt.setFont(*font_);
            btnTxt.setString(U8("领取奖励"));
            btnTxt.setCharacterSize(13);
            btnTxt.setFillColor(sf::Color(255, 235, 150));
            btnTxt.setPosition(btnX + 12.f, btnY + 5.f);
            target.draw(btnTxt);
        }

        cardY += cardH + cardGap;
    }

    // ---- 第二十二轮新增：遮罩覆盖卡片列表的溢出区域 ----
    // 滚动后部分卡片会溢出面板顶部/底部，用与面板背景同色的矩形遮盖
    // 顶部遮罩：从分隔线下方到列表起始（覆盖向上溢出的卡片）
    if (scrollOffset_ > 0.f) {
        sf::RectangleShape topMask(sf::Vector2f(cardW + 20.f, 20.f));
        topMask.setPosition(cardX - 5.f, position_.y + 56.f);
        topMask.setFillColor(sf::Color(20, 25, 40, 255));
        target.draw(topMask);
    }
    // 底部遮罩：从列表底部到面板底部（覆盖向下溢出的卡片）
    if (scrollOffset_ < maxScrollOffset_) {
        sf::RectangleShape bottomMask(sf::Vector2f(cardW + 20.f, 20.f));
        bottomMask.setPosition(cardX - 5.f, listBottom - 10.f);
        bottomMask.setFillColor(sf::Color(20, 25, 40, 255));
        target.draw(bottomMask);
    }

    // ---- 第二十二轮新增：滚动条指示器（仅当内容可滚动时显示）----
    if (maxScrollOffset_ > 0.f) {
        const float trackX = cardX + cardW + 8.f;
        const float trackY = listTop;
        const float trackH = listBottom - listTop;
        const float trackW = 4.f;

        // 滚动槽（背景）
        sf::RectangleShape track(sf::Vector2f(trackW, trackH));
        track.setPosition(trackX, trackY);
        track.setFillColor(sf::Color(60, 60, 70, 200));
        target.draw(track);

        // 滚动滑块（位置反映当前 scrollOffset_）
        const float thumbH = std::max(30.f, trackH * (trackH / (trackH + maxScrollOffset_)));
        const float thumbY = trackY + (trackH - thumbH) * (scrollOffset_ / maxScrollOffset_);
        sf::RectangleShape thumb(sf::Vector2f(trackW, thumbH));
        thumb.setPosition(trackX, thumbY);
        thumb.setFillColor(sf::Color(180, 150, 80, 220));
        target.draw(thumb);
    }

    // ---- 右侧：剧情说明面板 ----
    const float storyX = cardX + cardW + 20.f;
    const float storyW = size_.x - (cardW + 80.f);
    const float storyY = position_.y + 75.f;
    const float storyH = size_.y - 110.f;

    sf::RectangleShape storyBg(sf::Vector2f(storyW, storyH));
    storyBg.setPosition(storyX, storyY);
    storyBg.setFillColor(sf::Color(25, 20, 35, 220));
    storyBg.setOutlineColor(sf::Color(120, 100, 160));
    storyBg.setOutlineThickness(2.f);
    target.draw(storyBg);

    // 剧情标题
    sf::Text sTitle;
    sTitle.setFont(*font_);
    sTitle.setString(U8("=== 地牢往事 ==="));
    sTitle.setCharacterSize(20);
    sTitle.setFillColor(sf::Color(200, 180, 255));
    sTitle.setStyle(sf::Text::Bold);
    sTitle.setPosition(storyX + 15.f, storyY + 10.f);
    target.draw(sTitle);

    // 剧情正文
    const std::string story =
        u8"地下城深处流传着一个古老的传说：\n\n"
        u8"勇者须先击败首层守关者，方能踏入更深的\n"
        u8"未知世界。\n\n"
        u8"沿途散布着远古祭坛与奇人异士，与他们交\n"
        u8"互可获得意想不到的力量。\n\n"
        u8"传说中，地牢深处的商人藏有传说装备，\n"
        u8"唯有累积足够财富者方能换取。\n\n"
        u8"掌握更多技能，方能应对地牢深处的危机。\n\n"
        u8"— 摘自《地下世界探险指南》";
    drawWrappedText(target, story,
                    sf::Vector2f(storyX + 15.f, storyY + 45.f),
                    storyW - 30.f, 18.f, 14, sf::Color(180, 170, 200));
}

// ============================================================================
// 辅助方法
// ============================================================================
sf::Color QuestMenu::getStateColor(QuestState state) const {
    switch (state) {
        case QuestState::Locked:    return sf::Color(120, 120, 130);
        case QuestState::Active:    return sf::Color(100, 180, 255);
        case QuestState::Completed: return sf::Color(255, 215, 80);
        case QuestState::Claimed:   return sf::Color(100, 200, 100);
    }
    return sf::Color::White;
}

const char* QuestMenu::getStateText(QuestState state) const {
    switch (state) {
        case QuestState::Locked:    return "[未解锁]";
        case QuestState::Active:    return "[进行中]";
        case QuestState::Completed: return "[待领取]";
        case QuestState::Claimed:   return "[已完成]";
    }
    return "?";
}

std::string QuestMenu::formatReward(const QuestReward& reward) const {
    std::string s;
    auto append = [&](const std::string& part) {
        if (!s.empty()) s += "  ";
        s += part;
    };
    if (reward.exp > 0)        append("经验+" + std::to_string(reward.exp));
    if (reward.coins > 0)      append("金币+" + std::to_string(reward.coins));
    else if (reward.coins < 0) append("金币" + std::to_string(reward.coins));
    if (reward.skillPoints > 0) append("技能点+" + std::to_string(reward.skillPoints));
    if (reward.addLevels > 0)  append("等级+" + std::to_string(reward.addLevels));
    if (reward.itemCount > 0) {
        const char* qName = "?";
        switch (reward.itemQuality) {
            case ItemQuality::White:    qName = "普通"; break;
            case ItemQuality::Blue:     qName = "稀有"; break;
            case ItemQuality::Yellow:   qName = "史诗"; break;
            case ItemQuality::DarkGold: qName = "暗金"; break;
        }
        append(std::string(qName) + "装备×" + std::to_string(reward.itemCount));
    }
    return s;
}

void QuestMenu::drawWrappedText(sf::RenderTarget& target, const std::string& text,
                                sf::Vector2f pos, float maxWidth, float lineSpacing,
                                unsigned int charSize, sf::Color color) const {
    if (!font_ || maxWidth <= 0.f) return;

    // 按 UTF-8 字符边界切分，用 sf::Text 实际测量宽度，避免：
    //   1. substr 按 byte 切分切断多字节中文字符导致乱码
    //   2. 估算字符宽度不准导致换行过早或过晚
    sf::Text measurer;
    measurer.setFont(*font_);
    measurer.setCharacterSize(charSize);
    measurer.setFillColor(color);

    float y = pos.y;
    size_t i = 0; // 当前 byte 索引

    while (i < text.size()) {
        // 找到下一个换行符或字符串末尾
        size_t nl = text.find('\n', i);
        if (nl == std::string::npos) nl = text.size();

        // 在 [i, nl) 范围内按宽度切分
        size_t lineStart = i;
        size_t lineEnd = i; // 当前行已包含的字符的末尾 byte 索引

        while (lineEnd < nl) {
            // 计算下一个 UTF-8 字符的字节长度
            size_t charLen = 1;
            unsigned char c = static_cast<unsigned char>(text[lineEnd]);
            if (c < 0x80) charLen = 1;
            else if ((c & 0xE0) == 0xC0) charLen = 2;
            else if ((c & 0xF0) == 0xE0) charLen = 3;
            else if ((c & 0xF8) == 0xF0) charLen = 4;
            else charLen = 1; // 无效 UTF-8，按单字节处理

            size_t nextEnd = lineEnd + charLen;
            if (nextEnd > nl) nextEnd = nl;

            // 测量 [lineStart, nextEnd) 的宽度
            std::string sub = text.substr(lineStart, nextEnd - lineStart);
            measurer.setString(sf::String::fromUtf8(sub.begin(), sub.end()));
            float w = measurer.getLocalBounds().width;

            if (w > maxWidth && lineEnd > lineStart) {
                // 当前行已满，先渲染当前行 [lineStart, lineEnd)
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

        // 渲染最后一行 [lineStart, lineEnd)
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
