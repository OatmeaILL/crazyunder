#include "ui/Menus.h"
#include "utils/Logger.h"
#include "utils/TextureGenerator.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace cu {

// ============================================================================
// wrapText —— 按指定宽度将字符串自动换行
// ----------------------------------------------------------------------------
// 根据字体、字号、最大宽度，在超宽位置插入 '\n'，返回换行后的字符串。
// 用于升级卡片描述文字，避免超出卡片边界。
// ============================================================================
static std::string wrapText(const std::string& text, const sf::Font& font,
                            unsigned int charSize, float maxWidth) {
    std::string result;
    result.reserve(text.size() + 8);
    std::string line;
    sf::Text measure;
    measure.setFont(font);
    measure.setCharacterSize(charSize);

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        // 遇到已有换行符直接保留
        if (c == '\n') {
            result += line;
            result += '\n';
            line.clear();
            continue;
        }
        std::string trial = line + c;
        measure.setString(utf8ToSfString(trial));
        float w = measure.getLocalBounds().width;
        if (w > maxWidth && !line.empty()) {
            // 当前行已满，换行
            result += line;
            result += '\n';
            line.clear();
            line += c;
        } else {
            line = trial;
        }
    }
    result += line; // 最后一行
    return result;
}

// ============================================================================
// drawSkillTooltip —— 技能 tooltip 辅助绘制函数
// ----------------------------------------------------------------------------
// 在鼠标位置附近绘制半透明黑色背景的技能提示框，包含：
//   - 技能名称（粗体，金色）
//   - 技能描述（白色）
//   - 冷却时间（蓝色）
// ============================================================================
static void drawSkillTooltip(sf::RenderTarget& target, const sf::Font& font,
                              SkillType skillType, sf::Vector2f mousePos) {
    if (skillType == SkillType::Count) return;

    const SkillData& sd = GetSkillData(skillType);

    // 构建文本
    sf::Text nameText;
    nameText.setFont(font);
    nameText.setString(utf8ToSfString(GetSkillName(skillType)));
    nameText.setCharacterSize(14);
    nameText.setStyle(sf::Text::Bold);
    nameText.setFillColor(sf::Color(255, 220, 100));

    sf::Text descText;
    descText.setFont(font);
    descText.setString(utf8ToSfString(sd.desc));
    descText.setCharacterSize(12);
    descText.setFillColor(sf::Color(220, 220, 220));

    sf::Text cdText;
    cdText.setFont(font);
    cdText.setString(U8("冷却: ") + std::to_string(static_cast<int>(sd.cooldown + 0.5f)) + U8("s"));
    cdText.setCharacterSize(12);
    cdText.setFillColor(sf::Color(180, 180, 255));

    // 计算 tooltip 尺寸
    const float padding = 8.f;
    const float lineSpacing = 4.f;
    float nameH = nameText.getLocalBounds().height;
    float descH = descText.getLocalBounds().height;
    float cdH = cdText.getLocalBounds().height;
    float maxWidth = std::max({nameText.getLocalBounds().width,
                               descText.getLocalBounds().width,
                               cdText.getLocalBounds().width});
    float tooltipW = maxWidth + padding * 2;
    float tooltipH = nameH + descH + cdH + lineSpacing * 2 + padding * 2;

    // 定位 tooltip（鼠标右下方，超出屏幕则翻转）
    float tooltipX = mousePos.x + 15.f;
    float tooltipY = mousePos.y + 15.f;
    if (tooltipX + tooltipW > 1280.f) tooltipX = mousePos.x - tooltipW - 15.f;
    if (tooltipY + tooltipH > 720.f) tooltipY = mousePos.y - tooltipH - 15.f;
    if (tooltipX < 0.f) tooltipX = 0.f;
    if (tooltipY < 0.f) tooltipY = 0.f;

    // 绘制半透明黑色背景
    sf::RectangleShape bg(sf::Vector2f(tooltipW, tooltipH));
    bg.setPosition(tooltipX, tooltipY);
    bg.setFillColor(sf::Color(0, 0, 0, 200));
    bg.setOutlineColor(sf::Color(200, 200, 200, 150));
    bg.setOutlineThickness(1.f);
    target.draw(bg);

    // 绘制文本
    float yPos = tooltipY + padding;
    nameText.setPosition(tooltipX + padding, yPos);
    target.draw(nameText);
    yPos += nameH + lineSpacing;

    descText.setPosition(tooltipX + padding, yPos);
    target.draw(descText);
    yPos += descH + lineSpacing;

    cdText.setPosition(tooltipX + padding, yPos);
    target.draw(cdText);
}

// ============================================================================
// MainMenu 实现
// ============================================================================

MainMenu::MainMenu() {
    // 设置主菜单覆盖整个屏幕
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
}

void MainMenu::Initialize(const sf::Font& font) {
    font_ = &font;

    // 创建 Start 按钮（新游戏，选槽位）
    auto startBtn = std::make_unique<Button>();
    startBtn->SetFont(font);
    startBtn->SetText("开始游戏");
    startBtn->SetPosition(sf::Vector2f(540.f, 350.f));
    startBtn->SetSize(sf::Vector2f(200.f, 50.f));
    startBtn->SetBackgroundColor(sf::Color(60, 120, 80));
    startBtn->SetHoverColor(sf::Color(90, 180, 120));
    startBtn->SetPressedColor(sf::Color(40, 80, 60));
    startBtn->SetTextColor(sf::Color::White);
    startBtn_ = startBtn.get();
    AddChild(std::move(startBtn));

    // 创建 LoadGame 按钮（读取存档）
    auto loadGameBtn = std::make_unique<Button>();
    loadGameBtn->SetFont(font);
    loadGameBtn->SetText("读取存档");
    loadGameBtn->SetPosition(sf::Vector2f(540.f, 420.f));
    loadGameBtn->SetSize(sf::Vector2f(200.f, 50.f));
    loadGameBtn->SetBackgroundColor(sf::Color(70, 100, 140));
    loadGameBtn->SetHoverColor(sf::Color(110, 150, 200));
    loadGameBtn->SetPressedColor(sf::Color(50, 70, 100));
    loadGameBtn->SetTextColor(sf::Color::White);
    loadGameBtn_ = loadGameBtn.get();
    AddChild(std::move(loadGameBtn));

    // 第二十四轮新增：灵魂之井按钮（Meta Progression 入口）
    // 设计意图：将"灵魂之井"放在主菜单显眼位置，让玩家死亡后知道有永久成长路径
    auto soulWellBtn = std::make_unique<Button>();
    soulWellBtn->SetFont(font);
    soulWellBtn->SetText("灵魂之井");
    soulWellBtn->SetPosition(sf::Vector2f(540.f, 490.f));
    soulWellBtn->SetSize(sf::Vector2f(200.f, 50.f));
    soulWellBtn->SetBackgroundColor(sf::Color(100, 60, 140));
    soulWellBtn->SetHoverColor(sf::Color(150, 90, 200));
    soulWellBtn->SetPressedColor(sf::Color(70, 40, 100));
    soulWellBtn->SetTextColor(sf::Color::White);
    soulWellBtn_ = soulWellBtn.get();
    AddChild(std::move(soulWellBtn));

    // 创建 Settings 按钮
    auto settingsBtn = std::make_unique<Button>();
    settingsBtn->SetFont(font);
    settingsBtn->SetText("设置");
    settingsBtn->SetPosition(sf::Vector2f(540.f, 560.f));
    settingsBtn->SetSize(sf::Vector2f(200.f, 50.f));
    settingsBtn->SetBackgroundColor(sf::Color(80, 80, 120));
    settingsBtn->SetHoverColor(sf::Color(120, 120, 180));
    settingsBtn->SetPressedColor(sf::Color(60, 60, 80));
    settingsBtn->SetTextColor(sf::Color::White);
    settingsBtn_ = settingsBtn.get();
    AddChild(std::move(settingsBtn));

    // 创建 Quit 按钮（最底部）
    auto quitBtn = std::make_unique<Button>();
    quitBtn->SetFont(font);
    quitBtn->SetText("退出游戏");
    quitBtn->SetPosition(sf::Vector2f(540.f, 630.f));
    quitBtn->SetSize(sf::Vector2f(200.f, 50.f));
    quitBtn->SetBackgroundColor(sf::Color(120, 60, 60));
    quitBtn->SetHoverColor(sf::Color(180, 90, 90));
    quitBtn->SetPressedColor(sf::Color(80, 40, 40));
    quitBtn->SetTextColor(sf::Color::White);
    quitBtn_ = quitBtn.get();
    AddChild(std::move(quitBtn));

    // 初始化粒子
    particles_.reserve(30);
    for (int i = 0; i < 30; ++i) {
        Particle p;
        p.pos.x = static_cast<float>(std::rand() % 1280);
        p.pos.y = static_cast<float>(std::rand() % 720);
        p.vel.x = (std::rand() % 100 - 50) * 0.5f;
        p.vel.y = (std::rand() % 100 - 50) * 0.5f;
        p.life = static_cast<float>(std::rand() % 100) / 100.f;
        particles_.push_back(p);
    }
}

void MainMenu::Update(float dt) {
    UIElement::Update(dt);
    updateParticles(dt);
}

void MainMenu::updateParticles(float dt) {
    particleTimer_ += dt;
    for (auto& p : particles_) {
        p.pos += p.vel * dt * 20.f;
        p.life -= dt * 0.5f;
        if (p.life <= 0.f || p.pos.x < 0 || p.pos.x > 1280 ||
            p.pos.y < 0 || p.pos.y > 720) {
            p.pos.x = static_cast<float>(std::rand() % 1280);
            p.pos.y = static_cast<float>(std::rand() % 720);
            p.vel.x = (std::rand() % 100 - 50) * 0.5f;
            p.vel.y = (std::rand() % 100 - 50) * 0.5f;
            p.life = 1.f;
        }
    }
}

void MainMenu::Render(sf::RenderTarget& target) const {
    if (!visible_) return;

    // 绘制深色背景
    sf::RectangleShape bg(sf::Vector2f(1280.f, 720.f));
    bg.setFillColor(sf::Color(15, 20, 40));
    target.draw(bg);

    // 绘制粒子
    for (const auto& p : particles_) {
        sf::CircleShape particle(2.f);
        particle.setPosition(p.pos);
        uint8_t alpha = static_cast<uint8_t>(p.life * 255);
        particle.setFillColor(sf::Color(100, 150, 200, alpha));
        target.draw(particle);
    }

    // 绘制标题 "CRAZYUNDER"（大字号用英文，避免中文字体大字号渲染问题）
    if (font_) {
        sf::Text title;
        title.setFont(*font_);
        title.setString("CRAZzzzyUNDErrrrrR!");
        title.setCharacterSize(72);
        title.setFillColor(sf::Color(255, 220, 100));
        title.setStyle(sf::Text::Bold);
        title.setOutlineColor(sf::Color(80, 40, 0));
        title.setOutlineThickness(3.f);
        sf::FloatRect bounds = title.getLocalBounds();
        title.setPosition(
            (1280.f - bounds.width) * 0.5f,
            150.f);
        target.draw(title);

        // 副标题
        sf::Text subtitle;
        subtitle.setFont(*font_);
        subtitle.setString(U8("2.5D 像素风 Roguelike"));
        subtitle.setCharacterSize(28);
        subtitle.setFillColor(sf::Color(180, 180, 200));
        bounds = subtitle.getLocalBounds();
        subtitle.setPosition(
            (1280.f - bounds.width) * 0.5f,
            240.f);
        target.draw(subtitle);

        // 操作提示
        sf::Text hint;
        hint.setFont(*font_);
        hint.setString(U8("点击开始游戏  |  ESC 退出"));
        hint.setCharacterSize(16);
        hint.setFillColor(sf::Color(150, 150, 150));
        bounds = hint.getLocalBounds();
        hint.setPosition(
            (1280.f - bounds.width) * 0.5f,
            680.f);
        target.draw(hint);
    }

    // 渲染子元素（按钮）
    for (const auto& child : children_) {
        if (child) child->Render(target);
    }
}

// ============================================================================
// PauseMenu 实现
// ============================================================================

PauseMenu::PauseMenu() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
}

void PauseMenu::Initialize(const sf::Font& font) {
    font_ = &font;

    // Resume 按钮
    auto resumeBtn = std::make_unique<Button>();
    resumeBtn->SetFont(font);
    resumeBtn->SetText("继续游戏");
    resumeBtn->SetPosition(sf::Vector2f(540.f, 280.f));
    resumeBtn->SetSize(sf::Vector2f(200.f, 50.f));
    resumeBtn->SetBackgroundColor(sf::Color(60, 120, 80));
    resumeBtn->SetHoverColor(sf::Color(90, 180, 120));
    resumeBtn->SetPressedColor(sf::Color(40, 80, 60));
    resumeBtn_ = resumeBtn.get();
    AddChild(std::move(resumeBtn));

    // Save 按钮（保存进度到当前槽位）
    auto saveBtn = std::make_unique<Button>();
    saveBtn->SetFont(font);
    saveBtn->SetText("保存进度");
    saveBtn->SetPosition(sf::Vector2f(540.f, 350.f));
    saveBtn->SetSize(sf::Vector2f(200.f, 50.f));
    saveBtn->SetBackgroundColor(sf::Color(70, 100, 140));
    saveBtn->SetHoverColor(sf::Color(110, 150, 200));
    saveBtn->SetPressedColor(sf::Color(50, 70, 100));
    saveBtn->SetTextColor(sf::Color::White);
    saveBtn_ = saveBtn.get();
    AddChild(std::move(saveBtn));

    // Restart 按钮
    auto restartBtn = std::make_unique<Button>();
    restartBtn->SetFont(font);
    restartBtn->SetText("重新开始");
    restartBtn->SetPosition(sf::Vector2f(540.f, 420.f));
    restartBtn->SetSize(sf::Vector2f(200.f, 50.f));
    restartBtn->SetBackgroundColor(sf::Color(80, 80, 120));
    restartBtn->SetHoverColor(sf::Color(120, 120, 180));
    restartBtn->SetPressedColor(sf::Color(60, 60, 80));
    restartBtn_ = restartBtn.get();
    AddChild(std::move(restartBtn));

    // Quit to Menu 按钮
    auto quitBtn = std::make_unique<Button>();
    quitBtn->SetFont(font);
    quitBtn->SetText("返回主菜单");
    quitBtn->SetPosition(sf::Vector2f(540.f, 490.f));
    quitBtn->SetSize(sf::Vector2f(200.f, 50.f));
    quitBtn->SetBackgroundColor(sf::Color(120, 60, 60));
    quitBtn->SetHoverColor(sf::Color(180, 90, 90));
    quitBtn->SetPressedColor(sf::Color(80, 40, 40));
    quitBtn_ = quitBtn.get();
    AddChild(std::move(quitBtn));
}

void PauseMenu::Render(sf::RenderTarget& target) const {
    if (!visible_) return;

    // 半透明遮罩
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(0, 0, 0, 180));
    target.draw(overlay);

    // 标题
    if (font_) {
        sf::Text title;
        title.setFont(*font_);
        title.setString(U8("已暂停"));
        title.setCharacterSize(48);
        title.setFillColor(sf::Color::White);
        title.setStyle(sf::Text::Bold);
        sf::FloatRect bounds = title.getLocalBounds();
        title.setPosition(
            (1280.f - bounds.width) * 0.5f,
            220.f);
        target.draw(title);
    }

    // 渲染子元素（按钮）
    for (const auto& child : children_) {
        if (child) child->Render(target);
    }
}

// ============================================================================
// DeathScreen 实现
// ============================================================================

DeathScreen::DeathScreen() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
}

void DeathScreen::Initialize(const sf::Font& font) {
    font_ = &font;

    // Restart 按钮
    auto restartBtn = std::make_unique<Button>();
    restartBtn->SetFont(font);
    restartBtn->SetText("重新开始");
    restartBtn->SetPosition(sf::Vector2f(440.f, 500.f));
    restartBtn->SetSize(sf::Vector2f(180.f, 50.f));
    restartBtn->SetBackgroundColor(sf::Color(120, 60, 60));
    restartBtn->SetHoverColor(sf::Color(180, 90, 90));
    restartBtn->SetPressedColor(sf::Color(80, 40, 40));
    restartBtn_ = restartBtn.get();
    AddChild(std::move(restartBtn));

    // Main Menu 按钮
    auto menuBtn = std::make_unique<Button>();
    menuBtn->SetFont(font);
    menuBtn->SetText("返回主菜单");
    menuBtn->SetPosition(sf::Vector2f(660.f, 500.f));
    menuBtn->SetSize(sf::Vector2f(180.f, 50.f));
    menuBtn->SetBackgroundColor(sf::Color(60, 80, 120));
    menuBtn->SetHoverColor(sf::Color(90, 120, 180));
    menuBtn->SetPressedColor(sf::Color(40, 60, 80));
    menuBtn_ = menuBtn.get();
    AddChild(std::move(menuBtn));
}

void DeathScreen::SetStats(int kills, int level, float survivalTime) {
    kills_ = kills;
    level_ = level;
    survivalTime_ = survivalTime;
}

void DeathScreen::Render(sf::RenderTarget& target) const {
    if (!visible_) return;

    // 暗红色背景
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(40, 10, 10, 220));
    target.draw(overlay);

    if (font_) {
        // "YOU DIED" 红色大字
        sf::Text title;
        title.setFont(*font_);
        title.setString(U8("你死了"));
        title.setCharacterSize(80);
        title.setFillColor(sf::Color(220, 30, 30));
        title.setStyle(sf::Text::Bold);
        title.setOutlineColor(sf::Color(60, 0, 0));
        title.setOutlineThickness(4.f);
        sf::FloatRect bounds = title.getLocalBounds();
        title.setPosition(
            (1280.f - bounds.width) * 0.5f,
            180.f);
        target.draw(title);

        // 统计信息
        sf::Text stats;
        stats.setFont(*font_);
        stats.setString(
            U8("击杀: ") + std::to_string(kills_) +
            U8("\n等级: ") + std::to_string(level_) +
            U8("\n存活: ") + std::to_string(static_cast<int>(survivalTime_)) + U8("秒"));
        stats.setCharacterSize(28);
        stats.setFillColor(sf::Color(220, 200, 200));
        bounds = stats.getLocalBounds();
        stats.setPosition(
            (1280.f - bounds.width) * 0.5f,
            340.f);
        target.draw(stats);

        // 第三十轮新增：死亡回顾信息
        if (!killerName_.empty() || comboAtDeath_ > 0 || dps_ > 0.f) {
            float reviewY = 430.f;
            if (!killerName_.empty()) {
                sf::Text killerText;
                killerText.setFont(*font_);
                killerText.setString(U8("击杀者: ") + utf8ToSfString(killerName_));
                killerText.setCharacterSize(20);
                killerText.setFillColor(sf::Color(255, 150, 150));
                bounds = killerText.getLocalBounds();
                killerText.setPosition((1280.f - bounds.width) * 0.5f, reviewY);
                target.draw(killerText);
                reviewY += 28.f;
            }
            if (comboAtDeath_ > 0) {
                sf::Text comboText;
                comboText.setFont(*font_);
                comboText.setString(U8("连击中断: ") + std::to_string(comboAtDeath_));
                comboText.setCharacterSize(20);
                comboText.setFillColor(sf::Color(255, 220, 100));
                bounds = comboText.getLocalBounds();
                comboText.setPosition((1280.f - bounds.width) * 0.5f, reviewY);
                target.draw(comboText);
                reviewY += 28.f;
            }
            if (dps_ > 0.f) {
                sf::Text dpsText;
                dpsText.setFont(*font_);
                dpsText.setString(U8("每秒伤害: ") + std::to_string(static_cast<int>(dps_)));
                dpsText.setCharacterSize(20);
                dpsText.setFillColor(sf::Color(200, 200, 255));
                bounds = dpsText.getLocalBounds();
                dpsText.setPosition((1280.f - bounds.width) * 0.5f, reviewY);
                target.draw(dpsText);
            }
        }

        // 第二十四轮新增：灵魂碎片获得提示（Meta Progression 反馈）
        // 设计意图：让玩家在死亡时看到"获得了什么"，将挫败感转化为"下一局更强"的期待
        if (shardsGained_ > 0) {
            sf::Text shardText;
            shardText.setFont(*font_);
            shardText.setString(
                U8("灵魂碎片 +") + std::to_string(shardsGained_) +
                U8("  (可在主菜单\"灵魂之井\"中兑换永久强化)"));
            shardText.setCharacterSize(20);
            shardText.setFillColor(sf::Color(220, 180, 255));
            bounds = shardText.getLocalBounds();
            shardText.setPosition(
                (1280.f - bounds.width) * 0.5f,
                460.f);
            target.draw(shardText);
        }
    }

    // 渲染子元素（按钮）
    for (const auto& child : children_) {
        if (child) child->Render(target);
    }
}

// ============================================================================
// VictoryScreen 实现
// ============================================================================

VictoryScreen::VictoryScreen() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
}

void VictoryScreen::Initialize(const sf::Font& font) {
    font_ = &font;

    // Continue 按钮
    auto continueBtn = std::make_unique<Button>();
    continueBtn->SetFont(font);
    continueBtn->SetText("继续游戏");
    continueBtn->SetPosition(sf::Vector2f(440.f, 500.f));
    continueBtn->SetSize(sf::Vector2f(180.f, 50.f));
    continueBtn->SetBackgroundColor(sf::Color(120, 100, 40));
    continueBtn->SetHoverColor(sf::Color(180, 150, 60));
    continueBtn->SetPressedColor(sf::Color(80, 60, 30));
    continueBtn_ = continueBtn.get();
    AddChild(std::move(continueBtn));

    // Main Menu 按钮
    auto menuBtn = std::make_unique<Button>();
    menuBtn->SetFont(font);
    menuBtn->SetText("返回主菜单");
    menuBtn->SetPosition(sf::Vector2f(660.f, 500.f));
    menuBtn->SetSize(sf::Vector2f(180.f, 50.f));
    menuBtn->SetBackgroundColor(sf::Color(60, 80, 120));
    menuBtn->SetHoverColor(sf::Color(90, 120, 180));
    menuBtn->SetPressedColor(sf::Color(40, 60, 80));
    menuBtn_ = menuBtn.get();
    AddChild(std::move(menuBtn));
}

void VictoryScreen::SetStats(int kills, int level, float survivalTime) {
    kills_ = kills;
    level_ = level;
    survivalTime_ = survivalTime;
}

void VictoryScreen::Render(sf::RenderTarget& target) const {
    if (!visible_) return;

    // 金色背景
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(40, 30, 10, 220));
    target.draw(overlay);

    if (font_) {
        // "VICTORY" 金色大字
        sf::Text title;
        title.setFont(*font_);
        title.setString(U8("胜利"));
        title.setCharacterSize(80);
        title.setFillColor(sf::Color(255, 220, 80));
        title.setStyle(sf::Text::Bold);
        title.setOutlineColor(sf::Color(80, 60, 0));
        title.setOutlineThickness(4.f);
        sf::FloatRect bounds = title.getLocalBounds();
        title.setPosition(
            (1280.f - bounds.width) * 0.5f,
            180.f);
        target.draw(title);

        // 统计信息
        sf::Text stats;
        stats.setFont(*font_);
        stats.setString(
            U8("击杀: ") + std::to_string(kills_) +
            U8("\n等级: ") + std::to_string(level_) +
            U8("\n存活: ") + std::to_string(static_cast<int>(survivalTime_)) + U8("秒"));
        stats.setCharacterSize(28);
        stats.setFillColor(sf::Color(255, 240, 200));
        bounds = stats.getLocalBounds();
        stats.setPosition(
            (1280.f - bounds.width) * 0.5f,
            340.f);
        target.draw(stats);
    }

    // 渲染子元素（按钮）
    for (const auto& child : children_) {
        if (child) child->Render(target);
    }
}

// ============================================================================
// UpgradeChoiceMenu 实现
// ============================================================================

UpgradeChoiceMenu::UpgradeChoiceMenu() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
    selectedIndex_ = -1;
    hoveredCard_ = -1;
}

void UpgradeChoiceMenu::Initialize(const sf::Font& font) {
    font_ = &font;

    // 初始化 3 张卡片边界（屏幕中央水平排列）
    float cardW = 240.f;
    float cardH = 320.f;
    float spacing = 40.f;
    float totalW = cardW * 3 + spacing * 2;
    float startX = (1280.f - totalW) * 0.5f;
    float startY = (720.f - cardH) * 0.5f;

    for (int i = 0; i < 3; ++i) {
        cardBounds_[i] = sf::FloatRect(
            startX + i * (cardW + spacing),
            startY,
            cardW, cardH);
    }
}

void UpgradeChoiceMenu::SetOptions(const std::array<UpgradeOption, 3>& options) {
    options_ = options;
    selectedIndex_ = -1;
    hoveredCard_ = -1;
}

int UpgradeChoiceMenu::HandleKeyInput(int key) {
    // SFML 按键代码：Num1=27, Num2=28, Num3=29, Key1=4, Key2=5, Key3=6
    int index = -1;
    if (key == 27 || key == 4) index = 0;      // 1
    else if (key == 28 || key == 5) index = 1;  // 2
    else if (key == 29 || key == 6) index = 2;  // 3

    if (index >= 0 && index < 3) {
        // 检查选项是否有效
        if (options_[index].type != UpgradeType::Count) {
            selectedIndex_ = index;
            LOG_INFO("升级选择: [%d] %s", index + 1, options_[index].name.c_str());
            return index;
        }
    }
    return -1;
}

int UpgradeChoiceMenu::HandleMouseClick(sf::Vector2f mousePos) const {
    for (int i = 0; i < 3; ++i) {
        if (cardBounds_[i].contains(mousePos)) {
            if (options_[i].type != UpgradeType::Count) {
                return i;
            }
        }
    }
    return -1;
}

void UpgradeChoiceMenu::SetHoveredCard(int index) {
    hoveredCard_ = index;
}

void UpgradeChoiceMenu::Update(float dt) {
    (void)dt;
    // 更新逻辑由 Game 处理
}

sf::Color UpgradeChoiceMenu::getQualityColor(const UpgradeOption& opt) const {
    // 根据升级类型返回品质颜色
    // 简化：按 currentLevel 区分品质
    if (opt.currentLevel == 0) {
        return sf::Color(200, 200, 200); // 白色（新升级）
    } else if (opt.currentLevel < opt.maxLevel / 2) {
        return sf::Color(100, 150, 220); // 蓝色（中级）
    } else {
        return sf::Color(220, 180, 50); // 金色（高级）
    }
}

void UpgradeChoiceMenu::Render(sf::RenderTarget& target) const {
    if (!visible_ || !font_) return;

    // 半透明遮罩
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(0, 0, 0, 200));
    target.draw(overlay);

    // 标题 "CHOOSE UPGRADE"
    sf::Text title;
    title.setFont(*font_);
    title.setString(U8("选择升级"));
    title.setCharacterSize(40);
    title.setFillColor(sf::Color(255, 220, 100));
    title.setStyle(sf::Text::Bold);
    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setPosition(
        (1280.f - titleBounds.width) * 0.5f,
        100.f);
    target.draw(title);

    // 提示文字：引导玩家选择升级奖励
    sf::Text hint;
    hint.setFont(*font_);
    hint.setString(U8("请选择一项升级奖励"));
    hint.setCharacterSize(22);
    hint.setFillColor(sf::Color(255, 220, 100));
    hint.setStyle(sf::Text::Bold);
    sf::FloatRect hintBounds = hint.getLocalBounds();
    hint.setPosition(
        (1280.f - hintBounds.width) * 0.5f,
        150.f);
    target.draw(hint);

    sf::Text subHint;
    subHint.setFont(*font_);
    subHint.setString(U8("点击卡片或按 1/2/3 键选择"));
    subHint.setCharacterSize(16);
    subHint.setFillColor(sf::Color(180, 180, 180));
    sf::FloatRect subHintBounds = subHint.getLocalBounds();
    subHint.setPosition(
        (1280.f - subHintBounds.width) * 0.5f,
        180.f);
    target.draw(subHint);

    // 绘制 3 张卡片
    for (int i = 0; i < 3; ++i) {
        const auto& bounds = cardBounds_[i];
        const auto& opt = options_[i];

        // 卡片背景
        sf::RectangleShape cardBg(sf::Vector2f(bounds.width, bounds.height));
        cardBg.setPosition(bounds.left, bounds.top);

        if (opt.type == UpgradeType::Count) {
            // 无效选项：灰色
            cardBg.setFillColor(sf::Color(40, 40, 40, 200));
            cardBg.setOutlineColor(sf::Color(80, 80, 80));
        } else if (i == hoveredCard_) {
            // 悬停高亮
            cardBg.setFillColor(sf::Color(60, 60, 80, 230));
            cardBg.setOutlineColor(sf::Color(255, 220, 100));
            cardBg.setOutlineThickness(3.f);
        } else {
            // 正常状态
            cardBg.setFillColor(sf::Color(30, 30, 40, 220));
            sf::Color qualityColor = getQualityColor(opt);
            cardBg.setOutlineColor(qualityColor);
            cardBg.setOutlineThickness(2.f);
        }
        target.draw(cardBg);

        // 跳过无效选项的内容绘制
        if (opt.type == UpgradeType::Count) {
            sf::Text emptyText;
            emptyText.setFont(*font_);
            emptyText.setString(U8("无可用\n升级"));
            emptyText.setCharacterSize(20);
            emptyText.setFillColor(sf::Color(120, 120, 120));
            sf::FloatRect emptyBounds = emptyText.getLocalBounds();
            emptyText.setPosition(
                bounds.left + (bounds.width - emptyBounds.width) * 0.5f,
                bounds.top + bounds.height * 0.5f - emptyBounds.height);
            target.draw(emptyText);
            continue;
        }

        // 卡片编号
        sf::Text number;
        number.setFont(*font_);
        number.setString(std::to_string(i + 1));
        number.setCharacterSize(24);
        number.setFillColor(sf::Color(255, 220, 100));
        number.setStyle(sf::Text::Bold);
        number.setPosition(bounds.left + 10.f, bounds.top + 10.f);
        target.draw(number);

        // 升级名称（大字）
        sf::Text name;
        name.setFont(*font_);
        name.setString(utf8ToSfString(opt.name));
        name.setCharacterSize(24);
        name.setFillColor(sf::Color::White);
        name.setStyle(sf::Text::Bold);
        sf::FloatRect nameBounds = name.getLocalBounds();
        name.setPosition(
            bounds.left + (bounds.width - nameBounds.width) * 0.5f,
            bounds.top + 60.f);
        target.draw(name);

        // 描述（小字，自动换行避免超出卡片）
        {
            constexpr float kDescPadding = 12.f;
            float descMaxWidth = bounds.width - kDescPadding * 2.f;
            std::string wrapped = wrapText(opt.description, *font_, 14, descMaxWidth);
            sf::Text desc;
            desc.setFont(*font_);
            desc.setString(utf8ToSfString(wrapped));
            desc.setCharacterSize(14);
            desc.setFillColor(sf::Color(200, 200, 200));
            sf::FloatRect descBounds = desc.getLocalBounds();
            // 水平居中，垂直从卡片 120px 处开始
            desc.setPosition(
                bounds.left + (bounds.width - descBounds.width) * 0.5f,
                bounds.top + 120.f);
            target.draw(desc);
        }

        // 当前等级 → 新等级
        std::string levelStr = "Lv." + std::to_string(opt.currentLevel) +
                                " -> Lv." + std::to_string(opt.currentLevel + 1) +
                                " (Max " + std::to_string(opt.maxLevel) + ")";
        sf::Text levelText;
        levelText.setFont(*font_);
        levelText.setString(levelStr);
        levelText.setCharacterSize(18);
        levelText.setFillColor(sf::Color(220, 200, 100));
        sf::FloatRect levelBounds = levelText.getLocalBounds();
        levelText.setPosition(
            bounds.left + (bounds.width - levelBounds.width) * 0.5f,
            bounds.top + 200.f);
        target.draw(levelText);

        // 满级标记
        if (opt.currentLevel >= opt.maxLevel) {
            sf::Text maxText;
            maxText.setFont(*font_);
            maxText.setString(U8("已满级"));
            maxText.setCharacterSize(20);
            maxText.setFillColor(sf::Color(255, 100, 100));
            maxText.setStyle(sf::Text::Bold);
            sf::FloatRect maxBounds = maxText.getLocalBounds();
            maxText.setPosition(
                bounds.left + (bounds.width - maxBounds.width) * 0.5f,
                bounds.top + 250.f);
            target.draw(maxText);
        }
    }
}

// ============================================================================
// RelicChoiceMenu 实现（第十五轮新增）
// ============================================================================

RelicChoiceMenu::RelicChoiceMenu() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
    options_.fill(RelicType::None);
}

void RelicChoiceMenu::Initialize(const sf::Font& font) {
    font_ = &font;
    // 复用 UpgradeChoiceMenu 的卡片布局：3 张 240x320 卡片水平居中排列
    float cardW = 240.f;
    float cardH = 320.f;
    float spacing = 40.f;
    float totalW = cardW * 3 + spacing * 2;
    float startX = (1280.f - totalW) * 0.5f;
    float startY = (720.f - cardH) * 0.5f;
    for (int i = 0; i < 3; ++i) {
        cardBounds_[i] = sf::FloatRect(
            startX + i * (cardW + spacing),
            startY,
            cardW, cardH);
    }
}

void RelicChoiceMenu::SetOptions(const std::vector<RelicType>& relics) {
    options_.fill(RelicType::None);
    for (size_t i = 0; i < 3 && i < relics.size(); ++i) {
        options_[i] = relics[i];
    }
    selectedIndex_ = -1;
    hoveredCard_ = -1;
}

int RelicChoiceMenu::HandleKeyInput(int key) {
    int index = -1;
    if (key == 27 || key == 4) index = 0;      // 1
    else if (key == 28 || key == 5) index = 1; // 2
    else if (key == 29 || key == 6) index = 2; // 3
    if (index < 0 || index >= 3) return -1;
    if (options_[index] == RelicType::None) return -1;
    selectedIndex_ = index;
    LOG_INFO("圣物选择: [%d] %s", index + 1, GetRelicName(options_[index]));
    return index;
}

int RelicChoiceMenu::HandleMouseClick(sf::Vector2f mousePos) const {
    for (int i = 0; i < 3; ++i) {
        if (options_[i] == RelicType::None) continue;
        if (cardBounds_[i].contains(mousePos)) return i;
    }
    return -1;
}

void RelicChoiceMenu::Update(float dt) {
    (void)dt;
}

void RelicChoiceMenu::Render(sf::RenderTarget& target) const {
    if (!visible_ || !font_) return;

    // 半透明遮罩
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(0, 0, 0, 200));
    target.draw(overlay);

    // 标题 "选择圣物"
    sf::Text title;
    title.setFont(*font_);
    title.setString(U8("选择圣物"));
    title.setCharacterSize(40);
    title.setFillColor(sf::Color(255, 220, 100));
    title.setStyle(sf::Text::Bold);
    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setPosition((1280.f - titleBounds.width) * 0.5f, 100.f);
    target.draw(title);

    // 副标题
    sf::Text hint;
    hint.setFont(*font_);
    hint.setString(U8("击败 Boss！请选择一项圣物作为奖励"));
    hint.setCharacterSize(22);
    hint.setFillColor(sf::Color(255, 220, 100));
    hint.setStyle(sf::Text::Bold);
    sf::FloatRect hintBounds = hint.getLocalBounds();
    hint.setPosition((1280.f - hintBounds.width) * 0.5f, 150.f);
    target.draw(hint);

    sf::Text subHint;
    subHint.setFont(*font_);
    subHint.setString(U8("点击卡片或按 1/2/3 键选择"));
    subHint.setCharacterSize(16);
    subHint.setFillColor(sf::Color(180, 180, 180));
    sf::FloatRect subHintBounds = subHint.getLocalBounds();
    subHint.setPosition((1280.f - subHintBounds.width) * 0.5f, 180.f);
    target.draw(subHint);

    // 绘制 3 张卡片
    for (int i = 0; i < 3; ++i) {
        const auto& bounds = cardBounds_[i];
        RelicType rt = options_[i];

        sf::RectangleShape cardBg(sf::Vector2f(bounds.width, bounds.height));
        cardBg.setPosition(bounds.left, bounds.top);

        if (rt == RelicType::None) {
            cardBg.setFillColor(sf::Color(40, 40, 40, 200));
            cardBg.setOutlineColor(sf::Color(80, 80, 80));
            cardBg.setOutlineThickness(2.f);
        } else if (i == hoveredCard_) {
            cardBg.setFillColor(sf::Color(60, 60, 80, 230));
            cardBg.setOutlineColor(sf::Color(255, 220, 100));
            cardBg.setOutlineThickness(3.f);
        } else {
            cardBg.setFillColor(sf::Color(30, 30, 40, 220));
            const RelicData& rd = GetRelicData(rt);
            cardBg.setOutlineColor(sf::Color(rd.r, rd.g, rd.b));
            cardBg.setOutlineThickness(2.f);
        }
        target.draw(cardBg);

        if (rt == RelicType::None) {
            sf::Text emptyText;
            emptyText.setFont(*font_);
            emptyText.setString(U8("无可用\n圣物"));
            emptyText.setCharacterSize(20);
            emptyText.setFillColor(sf::Color(120, 120, 120));
            sf::FloatRect emptyBounds = emptyText.getLocalBounds();
            emptyText.setPosition(
                bounds.left + (bounds.width - emptyBounds.width) * 0.5f,
                bounds.top + bounds.height * 0.5f - emptyBounds.height);
            target.draw(emptyText);
            continue;
        }

        const RelicData& rd = GetRelicData(rt);

        // 卡片编号
        sf::Text number;
        number.setFont(*font_);
        number.setString(std::to_string(i + 1));
        number.setCharacterSize(24);
        number.setFillColor(sf::Color(255, 220, 100));
        number.setStyle(sf::Text::Bold);
        number.setPosition(bounds.left + 10.f, bounds.top + 10.f);
        target.draw(number);

        // 圣物图标色块（用边框色作为图标占位）
        sf::RectangleShape icon(sf::Vector2f(64.f, 64.f));
        icon.setFillColor(sf::Color(rd.r, rd.g, rd.b, 180));
        icon.setOutlineColor(sf::Color(255, 255, 255, 200));
        icon.setOutlineThickness(2.f);
        icon.setPosition(bounds.left + (bounds.width - 64.f) * 0.5f, bounds.top + 70.f);
        target.draw(icon);

        // 圣物名称（大字）
        sf::Text name;
        name.setFont(*font_);
        name.setString(utf8ToSfString(rd.name));
        name.setCharacterSize(24);
        name.setFillColor(sf::Color::White);
        name.setStyle(sf::Text::Bold);
        sf::FloatRect nameBounds = name.getLocalBounds();
        name.setPosition(
            bounds.left + (bounds.width - nameBounds.width) * 0.5f,
            bounds.top + 160.f);
        target.draw(name);

        // 圣物描述（小字）
        sf::Text desc;
        desc.setFont(*font_);
        desc.setString(utf8ToSfString(rd.desc));
        desc.setCharacterSize(16);
        desc.setFillColor(sf::Color(220, 220, 220));
        sf::FloatRect descBounds = desc.getLocalBounds();
        desc.setPosition(
            bounds.left + (bounds.width - descBounds.width) * 0.5f,
            bounds.top + 210.f);
        target.draw(desc);

        // "圣物"标签
        sf::Text label;
        label.setFont(*font_);
        label.setString(U8("【圣物】"));
        label.setCharacterSize(18);
        label.setFillColor(sf::Color(rd.r, rd.g, rd.b));
        label.setStyle(sf::Text::Bold);
        sf::FloatRect labelBounds = label.getLocalBounds();
        label.setPosition(
            bounds.left + (bounds.width - labelBounds.width) * 0.5f,
            bounds.top + 250.f);
        target.draw(label);
    }
}

// ============================================================================
// InventoryMenu 实现
// ============================================================================

InventoryMenu::InventoryMenu() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
}

void InventoryMenu::Initialize(const sf::Font& font) {
    font_ = &font;

    // 初始化 6 个装备槽位边界（2 行 x 3 列，左侧）
    // 左侧区域 x: 40-490，装备槽 140x80
    float slotW = 140.f;
    float slotH = 80.f;
    float slotSpacing = 12.f;
    float slotStartX = 40.f;
    float slotStartY = 130.f;

    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col) {
            int idx = row * 3 + col;
            slotBounds_[idx] = sf::FloatRect(
                slotStartX + col * (slotW + slotSpacing),
                slotStartY + row * (slotH + slotSpacing),
                slotW, slotH);
        }
    }

    // 初始化 25 格大背包边界（5 行 x 5 列，右侧）
    // 右侧区域 x: 560-1230，背包格 80x70
    float bpCellW = 80.f;
    float bpCellH = 70.f;
    float bpSpacing = 8.f;
    float bpStartX = 560.f;
    float bpStartY = 130.f;
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
            int idx = row * 5 + col;
            backpackBounds_[idx] = sf::FloatRect(
                bpStartX + col * (bpCellW + bpSpacing),
                bpStartY + row * (bpCellH + bpSpacing),
                bpCellW, bpCellH);
        }
    }

    // 生成 6 种装备图标纹理（24x24 像素图标）
    // 索引与 ItemSlot 枚举值一致：Weapon=0...Amulet=5
    for (int i = 0; i < 6; ++i) {
        itemIcons_[i].loadFromImage(
            TextureGenerator::CreateItemIcon(static_cast<ItemSlot>(i)));
    }

    // 初始化技能槽位边界（4个，一行排列，位于装备栏下方）
    // 注意：大背包5行底部到 y=512，技能区从 y=545 开始，避免重叠
    float skillSlotW = 100.f;
    float skillSlotH = 70.f;
    float skillSlotSpacing = 12.f;
    float skillSlotStartX = 40.f;
    float skillSlotStartY = 545.f;
    for (int i = 0; i < kSkillSlotCount; ++i) {
        skillSlotBounds_[i] = sf::FloatRect(
            skillSlotStartX + i * (skillSlotW + skillSlotSpacing),
            skillSlotStartY,
            skillSlotW, skillSlotH);
    }

    // 初始化技能背包边界（5个，一行排列，位于大背包下方）
    float skillBpW = 80.f;
    float skillBpH = 70.f;
    float skillBpSpacing = 8.f;
    float skillBpStartX = 560.f;
    float skillBpStartY = 545.f;
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        skillBackpackBounds_[i] = sf::FloatRect(
            skillBpStartX + i * (skillBpW + skillBpSpacing),
            skillBpStartY,
            skillBpW, skillBpH);
    }
}

void InventoryMenu::SetInventory(const InventorySystem& inventory) {
    const auto& equipped = inventory.GetEquippedItems();
    for (size_t i = 0; i < slots_.size() && i < equipped.size(); ++i) {
        slots_[i] = equipped[i];
    }
    const auto& backpack = inventory.GetBackpackItems();
    for (size_t i = 0; i < backpack_.size() && i < backpack.size(); ++i) {
        backpack_[i] = backpack[i];
    }
    totalAffixes_ = inventory.GetTotalAffixes();
}

void InventoryMenu::SetSkillData(const PlayerComponent& pc) {
    for (int i = 0; i < kSkillSlotCount; ++i) {
        skillSlots_[i] = pc.skillSlots[i];
    }
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        skillBackpack_[i] = pc.skillBackpack[i];
    }
}

sf::Color InventoryMenu::getQualityColor(ItemQuality q) const {
    // 与 LootSystem::GetQualityColor 保持一致
    // 普通=浅灰白, 稀有=蓝, 史诗=紫, 传说=亮金
    switch (q) {
        case ItemQuality::White:    return sf::Color(220, 220, 220);
        case ItemQuality::Blue:     return sf::Color( 80, 140, 255);
        case ItemQuality::Yellow:   return sf::Color(180,  80, 255);
        case ItemQuality::DarkGold: return sf::Color(255, 180,  30);
    }
    return sf::Color::White;
}

const char* InventoryMenu::getSlotName(ItemSlot s) const {
    switch (s) {
        case ItemSlot::Weapon: return "武器";
        case ItemSlot::Helmet: return "头盔";
        case ItemSlot::Chest:  return "胸甲";
        case ItemSlot::Boots:  return "靴子";
        case ItemSlot::Ring:   return "戒指";
        case ItemSlot::Amulet: return "项链";
    }
    return "?";
}

const char* InventoryMenu::getAffixName(AffixType t) const {
    switch (t) {
        case AffixType::AddedDamage:   return "伤害";
        case AffixType::AddedDefense:  return "防御";
        case AffixType::CritRate:     return "暴击率";
        case AffixType::CritDamage:   return "暴击伤害";
        case AffixType::MoveSpeed:    return "移速";
        case AffixType::AttackSpeed:  return "攻速";
        case AffixType::Lifesteal:    return "吸血";
        case AffixType::MaxHp:        return "最大生命";
        case AffixType::MaxMp:        return "最大法力";
    }
    return "?";
}

std::string InventoryMenu::formatAffixValue(const Affix& affix) {
    // 百分比词缀 value 是小数（如 0.05），显示时 ×100
    float displayVal = affix.isPercent ? affix.value * 100.f : affix.value;
    return std::to_string(static_cast<int>(displayVal + 0.5f)) +
           (affix.isPercent ? "%" : "");
}

// 第二十三轮新增：套装加成简短描述（用于背包激活套装汇总行）
// 示例：DamageMul 0.10 -> "+10%伤害"，DefenseAdd 10 -> "+10防御"
std::string InventoryMenu::formatSetBonusShort(SetBonusType type, float val) {
    switch (type) {
        case SetBonusType::DamageMul:      return "+" + std::to_string(static_cast<int>(val * 100.f + 0.5f)) + "%伤害";
        case SetBonusType::MaxHpMul:        return "+" + std::to_string(static_cast<int>(val * 100.f + 0.5f)) + "%生命";
        case SetBonusType::MoveSpeedMul:    return "+" + std::to_string(static_cast<int>(val * 100.f + 0.5f)) + "%移速";
        case SetBonusType::AttackSpeedMul: return "+" + std::to_string(static_cast<int>(val * 100.f + 0.5f)) + "%攻速";
        case SetBonusType::CritRateAdd:    return "+" + std::to_string(static_cast<int>(val * 100.f + 0.5f)) + "%暴击率";
        case SetBonusType::CritDamageAdd:  return "+" + std::to_string(static_cast<int>(val * 100.f + 0.5f)) + "%暴击伤害";
        case SetBonusType::ExpMulAdd:      return "+" + std::to_string(static_cast<int>(val * 100.f + 0.5f)) + "%经验";
        case SetBonusType::DefenseAdd:    return "+" + std::to_string(static_cast<int>(val + 0.5f)) + "防御";
        case SetBonusType::None:           return "";
    }
    return "";
}

const sf::Texture& InventoryMenu::getIconTexture(ItemSlot slot) const {
    // itemIcons_ 索引与 ItemSlot 枚举值一致（Weapon=0...Amulet=5）
    return itemIcons_[static_cast<size_t>(slot)];
}

void InventoryMenu::Update(float dt) {
    blinkTimer_ += dt;
    if (blinkTimer_ > 10.f) blinkTimer_ = 0.f; // 防止浮点累积
}

void InventoryMenu::UpdateHover(sf::Vector2f mousePos) {
    mousePos_ = mousePos;
    hoveredSlot_ = -1;
    hoveredBackpack_ = -1;
    hoveredSkillSlot_ = -1;
    hoveredSkillBackpack_ = -1;
    for (int i = 0; i < 6; ++i) {
        if (slotBounds_[i].contains(mousePos)) {
            hoveredSlot_ = i;
            return;
        }
    }
    for (int i = 0; i < InventorySystem::kBackpackSize; ++i) {
        if (backpackBounds_[i].contains(mousePos)) {
            hoveredBackpack_ = i;
            return;
        }
    }
    for (int i = 0; i < kSkillSlotCount; ++i) {
        if (skillSlotBounds_[i].contains(mousePos)) {
            hoveredSkillSlot_ = i;
            return;
        }
    }
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        if (skillBackpackBounds_[i].contains(mousePos)) {
            hoveredSkillBackpack_ = i;
            return;
        }
    }
}

std::pair<int, int> InventoryMenu::HandleClick(sf::Vector2f mousePos) const {
    // 检查装备槽点击（卸下）
    for (int i = 0; i < 6; ++i) {
        if (slotBounds_[i].contains(mousePos)) {
            return {1, i}; // 卸下装备槽[i]
        }
    }
    // 检查背包点击（装备）
    for (int i = 0; i < InventorySystem::kBackpackSize; ++i) {
        if (backpackBounds_[i].contains(mousePos)) {
            return {2, i}; // 装备背包[i]
        }
    }
    return {0, -1};
}

std::pair<int, int> InventoryMenu::HandleSkillClick(sf::Vector2f mousePos) const {
    // 检查技能槽点击（卸下）
    for (int i = 0; i < kSkillSlotCount; ++i) {
        if (skillSlotBounds_[i].contains(mousePos)) {
            if (skillSlots_[i].type != SkillType::Count) {
                return {3, i}; // 卸下技能槽[i]
            }
            return {0, -1};
        }
    }
    // 检查技能背包点击（装备）
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        if (skillBackpackBounds_[i].contains(mousePos)) {
            if (skillBackpack_[i] != SkillType::Count) {
                return {4, i}; // 装备技能背包[i]
            }
            return {0, -1};
        }
    }
    return {0, -1};
}

// ============================================================================
// 右键上下文菜单实现
// ============================================================================
bool InventoryMenu::HandleRightClick(sf::Vector2f mousePos) {
    contextMenuVisible_ = false;

    // 检查装备槽（type=1）：仅已装备的槽位才弹菜单
    for (int i = 0; i < 6; ++i) {
        if (slotBounds_[i].contains(mousePos)) {
            if (!slots_[i].item.has_value()) return false;
            contextTargetType_ = 1;
            contextTargetIndex_ = i;
            contextMenuVisible_ = true;
            contextMenuPos_ = sf::Vector2f(mousePos.x + 8.f, mousePos.y + 8.f);
            // 防止菜单超出屏幕右下边界
            const float menuW = 120.f, menuH = 68.f;
            if (contextMenuPos_.x + menuW > 1280.f) contextMenuPos_.x = 1280.f - menuW - 4.f;
            if (contextMenuPos_.y + menuH > 720.f) contextMenuPos_.y = 720.f - menuH - 4.f;
            contextMenuBounds_ = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y, menuW, menuH);
            contextItemBounds_[0] = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y, menuW, 32.f);
            contextItemBounds_[1] = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y + 34.f, menuW, 32.f);
            return true;
        }
    }
    // 检查背包格（type=2）：仅有物品的格子才弹菜单
    for (int i = 0; i < InventorySystem::kBackpackSize; ++i) {
        if (backpackBounds_[i].contains(mousePos)) {
            if (!backpack_[i].has_value()) return false;
            contextTargetType_ = 2;
            contextTargetIndex_ = i;
            contextMenuVisible_ = true;
            contextMenuPos_ = sf::Vector2f(mousePos.x + 8.f, mousePos.y + 8.f);
            const float menuW = 120.f, menuH = 68.f;
            if (contextMenuPos_.x + menuW > 1280.f) contextMenuPos_.x = 1280.f - menuW - 4.f;
            if (contextMenuPos_.y + menuH > 720.f) contextMenuPos_.y = 720.f - menuH - 4.f;
            contextMenuBounds_ = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y, menuW, menuH);
            contextItemBounds_[0] = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y, menuW, 32.f);
            contextItemBounds_[1] = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y + 34.f, menuW, 32.f);
            return true;
        }
    }
    // 检查技能槽（type=3）
    for (int i = 0; i < kSkillSlotCount; ++i) {
        if (skillSlotBounds_[i].contains(mousePos)) {
            if (skillSlots_[i].type == SkillType::Count) return false;
            contextTargetType_ = 3;
            contextTargetIndex_ = i;
            contextMenuVisible_ = true;
            contextMenuPos_ = sf::Vector2f(mousePos.x + 8.f, mousePos.y + 8.f);
            const float menuW = 120.f, menuH = 68.f;
            if (contextMenuPos_.x + menuW > 1280.f) contextMenuPos_.x = 1280.f - menuW - 4.f;
            if (contextMenuPos_.y + menuH > 720.f) contextMenuPos_.y = 720.f - menuH - 4.f;
            contextMenuBounds_ = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y, menuW, menuH);
            contextItemBounds_[0] = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y, menuW, 32.f);
            contextItemBounds_[1] = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y + 34.f, menuW, 32.f);
            return true;
        }
    }
    // 检查技能背包格（type=4）
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        if (skillBackpackBounds_[i].contains(mousePos)) {
            if (skillBackpack_[i] == SkillType::Count) return false;
            contextTargetType_ = 4;
            contextTargetIndex_ = i;
            contextMenuVisible_ = true;
            contextMenuPos_ = sf::Vector2f(mousePos.x + 8.f, mousePos.y + 8.f);
            const float menuW = 120.f, menuH = 68.f;
            if (contextMenuPos_.x + menuW > 1280.f) contextMenuPos_.x = 1280.f - menuW - 4.f;
            if (contextMenuPos_.y + menuH > 720.f) contextMenuPos_.y = 720.f - menuH - 4.f;
            contextMenuBounds_ = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y, menuW, menuH);
            contextItemBounds_[0] = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y, menuW, 32.f);
            contextItemBounds_[1] = sf::FloatRect(contextMenuPos_.x, contextMenuPos_.y + 34.f, menuW, 32.f);
            return true;
        }
    }
    return false;
}

std::pair<int, std::pair<int, int>> InventoryMenu::HandleContextMenuClick(sf::Vector2f mousePos) const {
    if (!contextMenuVisible_) return {0, {0, -1}};
    // 点击菜单外部 → 关闭
    if (!contextMenuBounds_.contains(mousePos)) {
        return {0, {0, -1}};
    }
    // 第1项：装备/卸下
    if (contextItemBounds_[0].contains(mousePos)) {
        return {1, {contextTargetType_, contextTargetIndex_}};
    }
    // 第2项：丢弃
    if (contextItemBounds_[1].contains(mousePos)) {
        return {2, {contextTargetType_, contextTargetIndex_}};
    }
    return {0, {0, -1}};
}

std::string InventoryMenu::getContextItemText(int itemIdx) const {
    // itemIdx 0 = 装备/卸下, 1 = 丢弃
    if (itemIdx == 0) {
        // 装备槽/技能槽已装备 → "卸下"；背包格/技能背包 → "装备"
        if (contextTargetType_ == 1 || contextTargetType_ == 3) return "卸下";
        return "装备";
    }
    return "丢弃";
}

void InventoryMenu::Render(sf::RenderTarget& target) const {
    if (!visible_ || !font_) return;

    // 半透明遮罩
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(0, 0, 0, 200));
    target.draw(overlay);

    // ---- 顶部标题 "背包" ----
    sf::Text title;
    title.setFont(*font_);
    title.setString(U8("背包（左键快速穿卸 | 右键弹出菜单 | G/ESC 关闭）"));
    title.setCharacterSize(36);
    title.setFillColor(sf::Color(255, 220, 100));
    title.setStyle(sf::Text::Bold);
    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setPosition(
        (1280.f - titleBounds.width) * 0.5f,
        30.f);
    target.draw(title);

    // ---- 第二十三轮新增：激活套装汇总（标题下方 y=78）----
    // 显示所有装备套装及其进度，方便玩家规划 build
    {
        std::array<std::pair<EquipmentSet, int>, 4> setCounts = {{
            {EquipmentSet::Warrior,  0},
            {EquipmentSet::Sage,     0},
            {EquipmentSet::Wind,      0},
            {EquipmentSet::Guardian, 0},
        }};
        for (const auto& slot : slots_) {
            if (!slot.item.has_value()) continue;
            const EquipmentSet sid = slot.item->setId;
            if (sid == EquipmentSet::None) continue;
            for (auto& entry : setCounts) {
                if (entry.first == sid) { ++entry.second; break; }
            }
        }
        // 拼接所有套装的进度，即使未激活也显示件数
        std::string summary = "套装: ";
        bool any = false;
        for (const auto& [sid, pieces] : setCounts) {
            if (pieces == 0) continue;
            if (any) summary += "  |  ";
            any = true;
            summary += std::string(LootSystem::GetSetName(sid)) + "(" +
                       std::to_string(pieces) + "/3)";
            if (pieces >= 2) {
                auto [bt2, bv2] = LootSystem::GetSetBonus(sid, 2);
                if (bt2 != SetBonusType::None) {
                    summary += " +" + formatSetBonusShort(bt2, bv2);
                }
            }
            if (pieces >= 3) {
                auto [bt3, bv3] = LootSystem::GetSetBonus(sid, 3);
                if (bt3 != SetBonusType::None) {
                    summary += " +" + formatSetBonusShort(bt3, bv3);
                }
            }
        }
        if (!any) summary += "（无）";

        sf::Text setSummary;
        setSummary.setFont(*font_);
        setSummary.setString(utf8ToSfString(summary));
        setSummary.setCharacterSize(13);
        setSummary.setFillColor(any ? sf::Color(255, 220, 120) : sf::Color(140, 140, 140));
        setSummary.setStyle(sf::Text::Bold);
        sf::FloatRect sb = setSummary.getLocalBounds();
        setSummary.setPosition((1280.f - sb.width) * 0.5f, 78.f);
        target.draw(setSummary);
    }

    // ---- 左侧标题：装备栏 ----
    {
        sf::Text sec;
        sec.setFont(*font_);
        sec.setString(U8("装备栏"));
        sec.setCharacterSize(16);
        sec.setFillColor(sf::Color::White);
        sec.setStyle(sf::Text::Bold);
        sec.setOutlineColor(sf::Color::Black);
        sec.setOutlineThickness(2.f);
        sec.setPosition(40.f, 100.f);
        target.draw(sec);
    }

    // ---- 中间竖线分隔（仅分隔装备区，不延伸到技能区）----
    {
        sf::RectangleShape divider(sf::Vector2f(3.f, 415.f));
        divider.setPosition(520.f, 100.f);
        divider.setFillColor(sf::Color(120, 100, 60, 200));
        target.draw(divider);
    }

    // ---- 右侧标题：装备背包 ----
    {
        sf::Text sec;
        sec.setFont(*font_);
        sec.setString(U8("装备背包"));
        sec.setCharacterSize(16);
        sec.setFillColor(sf::Color::White);
        sec.setStyle(sf::Text::Bold);
        sec.setOutlineColor(sf::Color::Black);
        sec.setOutlineThickness(2.f);
        sec.setPosition(560.f, 100.f);
        target.draw(sec);
    }

    // 闪烁透明度（0.4-1.0 周期 ~1s）
    float blinkPhase = std::sin(blinkTimer_ * 6.28318f) * 0.5f + 0.5f; // 0-1
    int blinkAlpha = static_cast<int>(120 + blinkPhase * 135); // 120-255

    // ---- 绘制 6 个装备槽位 ----
    for (int i = 0; i < 6; ++i) {
        const auto& bounds = slotBounds_[i];
        const auto& slot = slots_[i];

        sf::RectangleShape slotBg(sf::Vector2f(bounds.width, bounds.height));
        slotBg.setPosition(bounds.left, bounds.top);
        slotBg.setFillColor(sf::Color(30, 30, 40, 220));

        if (slot.item.has_value()) {
            slotBg.setOutlineColor(getQualityColor(slot.item->quality));
            slotBg.setOutlineThickness(2.f);
        } else {
            slotBg.setOutlineColor(sf::Color(80, 80, 80));
            slotBg.setOutlineThickness(1.f);
        }

        // 悬停高亮：边框闪烁
        if (hoveredSlot_ == i && slot.item.has_value()) {
            sf::Color blinkColor(255, 255, 100, static_cast<sf::Uint8>(blinkAlpha));
            slotBg.setOutlineColor(blinkColor);
            slotBg.setOutlineThickness(3.f);
        }
        target.draw(slotBg);

        // 装备图标（右侧居中，32x32 缩放显示）
        // 空槽位也显示对应类型图标（暗淡），便于识别槽位用途
        {
            sf::Sprite iconSpr(getIconTexture(slot.slot));
            float iconSize = 32.f;
            iconSpr.setScale(iconSize / 24.f, iconSize / 24.f);
            iconSpr.setPosition(bounds.left + bounds.width - iconSize - 8.f,
                                bounds.top + (bounds.height - iconSize) * 0.5f);
            if (!slot.item.has_value()) {
                iconSpr.setColor(sf::Color(255, 255, 255, 80)); // 空槽位暗淡显示
            }
            target.draw(iconSpr);
        }

        // 槽位名称（左上角小字）
        sf::Text slotName;
        slotName.setFont(*font_);
        slotName.setString(utf8ToSfString(getSlotName(slot.slot)));
        slotName.setCharacterSize(12);
        slotName.setFillColor(sf::Color(150, 150, 150));
        slotName.setPosition(bounds.left + 6.f, bounds.top + 4.f);
        target.draw(slotName);

        // 装备信息
        if (slot.item.has_value()) {
            const auto& item = *slot.item;

            // 装备名称
            sf::Text itemName;
            itemName.setFont(*font_);
            itemName.setString(utf8ToSfString(item.name));
            itemName.setCharacterSize(13);
            itemName.setFillColor(getQualityColor(item.quality));
            itemName.setStyle(sf::Text::Bold);
            itemName.setPosition(bounds.left + 6.f, bounds.top + 20.f);
            target.draw(itemName);

            // 词缀列表（最多 2 条）
            std::string affixStr;
            for (size_t a = 0; a < item.affixes.size() && a < 2; ++a) {
                const auto& affix = item.affixes[a];
                affixStr += std::string(getAffixName(affix.type)) + ":+" +
                           formatAffixValue(affix) + "\n";
            }
            if (!affixStr.empty()) {
                sf::Text affixText;
                affixText.setFont(*font_);
                affixText.setString(utf8ToSfString(affixStr));
                affixText.setCharacterSize(11);
                affixText.setFillColor(sf::Color(200, 200, 200));
                affixText.setPosition(bounds.left + 6.f, bounds.top + 38.f);
                target.draw(affixText);
            }

            // 第二十三轮新增：套装标识（cell 底部，套装主色显示套装名）
            // 设计意图：让玩家在装备槽直观看到所属套装，便于凑套装 build
            if (item.setId != EquipmentSet::None) {
                sf::Text setName;
                setName.setFont(*font_);
                setName.setString(utf8ToSfString(std::string("[") + LootSystem::GetSetName(item.setId) + "]"));
                setName.setCharacterSize(10);
                setName.setFillColor(LootSystem::GetSetColor(item.setId));
                setName.setStyle(sf::Text::Bold);
                setName.setPosition(bounds.left + 6.f, bounds.top + 62.f);
                target.draw(setName);
            }
        } else {
            // 空槽位提示
            sf::Text empty;
            empty.setFont(*font_);
            empty.setString(U8("空"));
            empty.setCharacterSize(14);
            empty.setFillColor(sf::Color(100, 100, 100));
            empty.setPosition(bounds.left + bounds.width * 0.5f - 8.f,
                              bounds.top + bounds.height * 0.5f - 8.f);
            target.draw(empty);
        }
    }

    // ---- 绘制 25 格大背包 ----
    for (int i = 0; i < InventorySystem::kBackpackSize; ++i) {
        const auto& bounds = backpackBounds_[i];

        sf::RectangleShape cellBg(sf::Vector2f(bounds.width, bounds.height));
        cellBg.setPosition(bounds.left, bounds.top);
        cellBg.setFillColor(sf::Color(25, 25, 35, 220));

        if (backpack_[i].has_value()) {
            cellBg.setOutlineColor(getQualityColor(backpack_[i]->quality));
            cellBg.setOutlineThickness(2.f);
        } else {
            cellBg.setOutlineColor(sf::Color(60, 60, 60));
            cellBg.setOutlineThickness(1.f);
        }

        // 悬停高亮：边框闪烁
        if (hoveredBackpack_ == i && backpack_[i].has_value()) {
            sf::Color blinkColor(255, 255, 100, static_cast<sf::Uint8>(blinkAlpha));
            cellBg.setOutlineColor(blinkColor);
            cellBg.setOutlineThickness(3.f);
        }
        target.draw(cellBg);

        // 物品信息
        if (backpack_[i].has_value()) {
            const auto& item = *backpack_[i];

            // 装备图标（右上角，20x20 缩放显示）
            {
                sf::Sprite iconSpr(getIconTexture(item.slot));
                float iconSize = 20.f;
                iconSpr.setScale(iconSize / 24.f, iconSize / 24.f);
                iconSpr.setPosition(bounds.left + bounds.width - iconSize - 4.f,
                                    bounds.top + 4.f);
                target.draw(iconSpr);
            }

            // 物品名称（简短显示）
            sf::Text itemName;
            itemName.setFont(*font_);
            itemName.setString(utf8ToSfString(item.name));
            itemName.setCharacterSize(10);
            itemName.setFillColor(getQualityColor(item.quality));
            itemName.setPosition(bounds.left + 4.f, bounds.top + 4.f);
            target.draw(itemName);

            // 词缀列表（背包只显示首条，装备槽显示 2 条）
            // 修正：背包格显示 2 条词缀 + 套装标识，让玩家在背包中也能看到完整属性
            std::string affixStr;
            for (size_t a = 0; a < item.affixes.size() && a < 2; ++a) {
                const auto& affix = item.affixes[a];
                affixStr += std::string(getAffixName(affix.type)) + ":+" +
                           formatAffixValue(affix) + "\n";
            }
            if (!affixStr.empty()) {
                sf::Text affixText;
                affixText.setFont(*font_);
                affixText.setString(utf8ToSfString(affixStr));
                affixText.setCharacterSize(10);
                affixText.setFillColor(sf::Color(200, 200, 200));
                affixText.setPosition(bounds.left + 4.f, bounds.top + 24.f);
                target.draw(affixText);
            }
            // 背包格显示套装名
            if (item.setId != EquipmentSet::None) {
                sf::Text setName;
                setName.setFont(*font_);
                setName.setString(utf8ToSfString(std::string("[") + LootSystem::GetSetName(item.setId) + "]"));
                setName.setCharacterSize(9);
                setName.setFillColor(LootSystem::GetSetColor(item.setId));
                setName.setStyle(sf::Text::Bold);
                setName.setPosition(bounds.left + 4.f, bounds.top + 44.f);
                target.draw(setName);
            }
        }
    }

    // ---- 技能栏 ----
    // 左侧标题：技能栏
    {
        sf::Text sec;
        sec.setFont(*font_);
        sec.setString(U8("技能栏"));
        sec.setCharacterSize(16);
        sec.setFillColor(sf::Color::White);
        sec.setStyle(sf::Text::Bold);
        sec.setOutlineColor(sf::Color::Black);
        sec.setOutlineThickness(2.f);
        sec.setPosition(40.f, 525.f);
        target.draw(sec);
    }

    // 右侧标题：技能背包
    {
        sf::Text sec;
        sec.setFont(*font_);
        sec.setString(U8("技能背包"));
        sec.setCharacterSize(16);
        sec.setFillColor(sf::Color::White);
        sec.setStyle(sf::Text::Bold);
        sec.setOutlineColor(sf::Color::Black);
        sec.setOutlineThickness(2.f);
        sec.setPosition(560.f, 525.f);
        target.draw(sec);
    }

    // 技能颜色映射
    auto getSkillColor = [](SkillType type) -> sf::Color {
        switch (type) {
            case SkillType::GroundSlam:  return sf::Color(180, 120, 60);  // 震地波=棕色
            case SkillType::LeechStrike: return sf::Color(100, 255, 100); // 吸血打击=绿色
            case SkillType::Berserk:     return sf::Color(255, 60, 30);   // 狂暴=红色
            case SkillType::GravityWell: return sf::Color(160, 60, 255);  // 引力井=紫色
            case SkillType::SpikeGround: return sf::Color(200, 180, 60);  // 地刺=黄色
            default: return sf::Color(80, 80, 80);
        }
    };

    // 绘制 4 个技能槽
    for (int i = 0; i < kSkillSlotCount; ++i) {
        const auto& bounds = skillSlotBounds_[i];
        const auto& skill = skillSlots_[i];

        sf::RectangleShape slotBg(sf::Vector2f(bounds.width, bounds.height));
        slotBg.setPosition(bounds.left, bounds.top);
        slotBg.setFillColor(sf::Color(30, 30, 40, 220));

        if (skill.type != SkillType::Count) {
            slotBg.setOutlineColor(getSkillColor(skill.type));
            slotBg.setOutlineThickness(2.f);
        } else {
            slotBg.setOutlineColor(sf::Color(80, 80, 80));
            slotBg.setOutlineThickness(1.f);
        }

        // 悬停闪烁
        if (hoveredSkillSlot_ == i && skill.type != SkillType::Count) {
            sf::Color blinkColor(255, 255, 100, static_cast<sf::Uint8>(blinkAlpha));
            slotBg.setOutlineColor(blinkColor);
            slotBg.setOutlineThickness(3.f);
        }
        target.draw(slotBg);

        // 槽位编号
        sf::Text slotNum;
        slotNum.setFont(*font_);
        slotNum.setString(std::to_string(i + 1));
        slotNum.setCharacterSize(12);
        slotNum.setFillColor(sf::Color(150, 150, 150));
        slotNum.setPosition(bounds.left + 4.f, bounds.top + 2.f);
        target.draw(slotNum);

        if (skill.type != SkillType::Count) {
            // 技能名称
            sf::Text skillName;
            skillName.setFont(*font_);
            skillName.setString(utf8ToSfString(GetSkillName(skill.type)));
            skillName.setCharacterSize(13);
            skillName.setFillColor(getSkillColor(skill.type));
            skillName.setStyle(sf::Text::Bold);
            skillName.setPosition(bounds.left + 6.f, bounds.top + 18.f);
            target.draw(skillName);

            // 冷却状态
            if (skill.cooldownRemain > 0.f) {
                sf::Text cdText;
                cdText.setFont(*font_);
                cdText.setString(U8("CD:") + std::to_string(static_cast<int>(skill.cooldownRemain + 0.5f)) + U8("s"));
                cdText.setCharacterSize(11);
                cdText.setFillColor(sf::Color(255, 100, 100));
                cdText.setPosition(bounds.left + 6.f, bounds.top + 38.f);
                target.draw(cdText);

                // 冷却遮罩
                const SkillData& sd = GetSkillData(skill.type);
                float cdProgress = (sd.cooldown > 0.f) ? (skill.cooldownRemain / sd.cooldown) : 0.f;
                cdProgress = std::min(cdProgress, 1.f);
                float coverHeight = bounds.height * cdProgress;
                sf::RectangleShape cover(sf::Vector2f(bounds.width, coverHeight));
                cover.setPosition(bounds.left, bounds.top + bounds.height - coverHeight);
                cover.setFillColor(sf::Color(0, 0, 0, 100));
                target.draw(cover);
            } else {
                sf::Text readyText;
                readyText.setFont(*font_);
                readyText.setString(U8("就绪"));
                readyText.setCharacterSize(11);
                readyText.setFillColor(sf::Color(100, 255, 100));
                readyText.setPosition(bounds.left + 6.f, bounds.top + 38.f);
                target.draw(readyText);
            }
        } else {
            // 空槽位
            sf::Text empty;
            empty.setFont(*font_);
            empty.setString(U8("空"));
            empty.setCharacterSize(14);
            empty.setFillColor(sf::Color(100, 100, 100));
            empty.setPosition(bounds.left + bounds.width * 0.5f - 8.f,
                              bounds.top + bounds.height * 0.5f - 8.f);
            target.draw(empty);
        }
    }

    // 绘制 5 个技能背包格
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        const auto& bounds = skillBackpackBounds_[i];
        SkillType type = skillBackpack_[i];

        sf::RectangleShape cellBg(sf::Vector2f(bounds.width, bounds.height));
        cellBg.setPosition(bounds.left, bounds.top);
        cellBg.setFillColor(sf::Color(25, 25, 35, 220));

        if (type != SkillType::Count) {
            cellBg.setOutlineColor(getSkillColor(type));
            cellBg.setOutlineThickness(2.f);
        } else {
            cellBg.setOutlineColor(sf::Color(60, 60, 60));
            cellBg.setOutlineThickness(1.f);
        }

        // 悬停闪烁
        if (hoveredSkillBackpack_ == i && type != SkillType::Count) {
            sf::Color blinkColor(255, 255, 100, static_cast<sf::Uint8>(blinkAlpha));
            cellBg.setOutlineColor(blinkColor);
            cellBg.setOutlineThickness(3.f);
        }
        target.draw(cellBg);

        if (type != SkillType::Count) {
            // 技能名称
            sf::Text skillName;
            skillName.setFont(*font_);
            skillName.setString(utf8ToSfString(GetSkillName(type)));
            skillName.setCharacterSize(12);
            skillName.setFillColor(getSkillColor(type));
            skillName.setStyle(sf::Text::Bold);
            skillName.setPosition(bounds.left + 4.f, bounds.top + 4.f);
            target.draw(skillName);
        }
    }

    // ---- 底部：总词缀加成 + 套装加成 ----
    if (!totalAffixes_.empty()) {
        sf::Text totalTitle;
        totalTitle.setFont(*font_);
        totalTitle.setString(U8("总加成："));
        totalTitle.setCharacterSize(18);
        totalTitle.setFillColor(sf::Color(255, 220, 100));
        totalTitle.setStyle(sf::Text::Bold);
        totalTitle.setPosition(40.f, 660.f);
        target.draw(totalTitle);

        std::string totalStr;
        int line = 0;
        for (const auto& [type, value] : totalAffixes_) {
            // 百分比词缀显示 ×100
            bool isPercent = (type == AffixType::CritRate || type == AffixType::CritDamage ||
                              type == AffixType::MoveSpeed || type == AffixType::AttackSpeed ||
                              type == AffixType::Lifesteal);
            float displayVal = isPercent ? value * 100.f : value;
            totalStr += std::string(getAffixName(type)) + ":+" +
                       std::to_string(static_cast<int>(displayVal + 0.5f)) +
                       (isPercent ? "%" : "") + "  ";
            ++line;
            if (line % 5 == 0) totalStr += "\n";
        }
        // 追加套装加成
        {
            std::array<std::pair<EquipmentSet, int>, 4> setCounts = {{
                {EquipmentSet::Warrior,  0},
                {EquipmentSet::Sage,     0},
                {EquipmentSet::Wind,      0},
                {EquipmentSet::Guardian, 0},
            }};
            for (const auto& slot : slots_) {
                if (!slot.item.has_value()) continue;
                const EquipmentSet sid = slot.item->setId;
                if (sid == EquipmentSet::None) continue;
                for (auto& entry : setCounts) {
                    if (entry.first == sid) { ++entry.second; break; }
                }
            }
            for (const auto& [sid, pieces] : setCounts) {
                if (pieces < 2) continue;
                auto [bt2, bv2] = LootSystem::GetSetBonus(sid, 2);
                if (bt2 != SetBonusType::None) {
                    totalStr += U8("套装:") + LootSystem::GetSetName(sid) +
                                " 2件:" + formatSetBonusShort(bt2, bv2);
                }
                if (pieces >= 3) {
                    auto [bt3, bv3] = LootSystem::GetSetBonus(sid, 3);
                    if (bt3 != SetBonusType::None) {
                        totalStr += " 3件:" + formatSetBonusShort(bt3, bv3);
                    }
                }
                totalStr += "  ";
            }
        }
        sf::Text totalText;
        totalText.setFont(*font_);
        totalText.setString(utf8ToSfString(totalStr));
        totalText.setCharacterSize(14);
        totalText.setFillColor(sf::Color(200, 230, 200));
        totalText.setPosition(120.f, 690.f);
        target.draw(totalText);
    }

    // ---- 技能 tooltip（鼠标悬停在技能槽或技能背包上时显示）----
    if (hoveredSkillSlot_ >= 0 && hoveredSkillSlot_ < kSkillSlotCount) {
        const auto& skill = skillSlots_[hoveredSkillSlot_];
        if (skill.type != SkillType::Count) {
            drawSkillTooltip(target, *font_, skill.type, mousePos_);
        }
    } else if (hoveredSkillBackpack_ >= 0 && hoveredSkillBackpack_ < kSkillBackpackSize) {
        SkillType type = skillBackpack_[hoveredSkillBackpack_];
        if (type != SkillType::Count) {
            drawSkillTooltip(target, *font_, type, mousePos_);
        }
    }

    // ---- 右键上下文菜单（最上层渲染）----
    if (contextMenuVisible_) {
        const float menuW = 120.f, menuH = 68.f;
        sf::RectangleShape menuBg(sf::Vector2f(menuW, menuH));
        menuBg.setPosition(contextMenuPos_);
        menuBg.setFillColor(sf::Color(30, 30, 40, 245));
        menuBg.setOutlineColor(sf::Color(200, 200, 220));
        menuBg.setOutlineThickness(2.f);
        target.draw(menuBg);

        // 菜单项1：装备/卸下
        std::string txt1 = getContextItemText(0);
        sf::Text item1;
        item1.setFont(*font_);
        item1.setString(sf::String::fromUtf8(txt1.begin(), txt1.end()));
        item1.setCharacterSize(16);
        item1.setFillColor(sf::Color(220, 240, 220));
        item1.setPosition(contextMenuPos_.x + 18.f, contextMenuPos_.y + 7.f);
        target.draw(item1);

        // 分隔线
        sf::RectangleShape sep(sf::Vector2f(menuW - 8.f, 1.f));
        sep.setPosition(contextMenuPos_.x + 4.f, contextMenuPos_.y + 33.f);
        sep.setFillColor(sf::Color(120, 120, 140));
        target.draw(sep);

        // 菜单项2：丢弃
        std::string txt2 = getContextItemText(1);
        sf::Text item2;
        item2.setFont(*font_);
        item2.setString(sf::String::fromUtf8(txt2.begin(), txt2.end()));
        item2.setCharacterSize(16);
        item2.setFillColor(sf::Color(240, 120, 120));
        item2.setPosition(contextMenuPos_.x + 18.f, contextMenuPos_.y + 41.f);
        target.draw(item2);
    }
}

// ============================================================================
// MerchantMenu 实现
// ============================================================================
MerchantMenu::MerchantMenu() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
    for (auto& b : stockBounds_) b = sf::FloatRect();
    for (auto& b : skillStockBounds_) b = sf::FloatRect();
    for (auto& b : backpackBounds_) b = sf::FloatRect();
    for (auto& b : skillBackpackBounds_) b = sf::FloatRect();
    for (auto& s : skillBackpack_) s = SkillType::Count;
}

void MerchantMenu::Initialize(const sf::Font& font) {
    font_ = &font;

    // 生成 6 种装备图标纹理（24x24 像素图标）
    for (int i = 0; i < 6; ++i) {
        itemIcons_[i].loadFromImage(
            TextureGenerator::CreateItemIcon(static_cast<ItemSlot>(i)));
    }
}

void MerchantMenu::SetMerchantStock(const MerchantSystem& merchant) {
    const auto& stock = merchant.GetStock();
    for (int i = 0; i < MerchantSystem::kMerchantStockSize; ++i) {
        stock_[i] = stock[i];
    }
    const auto& skillStock = merchant.GetSkillStock();
    for (int i = 0; i < MerchantSystem::kMerchantSkillSize; ++i) {
        skillStock_[i] = skillStock[i];
    }
}

void MerchantMenu::SetBackpack(const InventorySystem& inventory, int playerCoins,
                               const PlayerComponent* pc) {
    playerCoins_ = playerCoins;
    for (int i = 0; i < InventorySystem::kBackpackSize; ++i) {
        backpack_[i] = inventory.GetBackpackItem(i);
    }
    // 同步玩家技能背包数据（用于右侧技能背包显示）
    if (pc) {
        for (int i = 0; i < kSkillBackpackSize; ++i) {
            skillBackpack_[i] = pc->skillBackpack[i];
        }
    }
}

sf::Color MerchantMenu::getQualityColor(ItemQuality q) const {
    switch (q) {
        case ItemQuality::White:    return sf::Color(220, 220, 220);
        case ItemQuality::Blue:     return sf::Color( 80, 140, 255);
        case ItemQuality::Yellow:   return sf::Color(180,  80, 255);
        case ItemQuality::DarkGold: return sf::Color(255, 180,  30);
    }
    return sf::Color::White;
}

const char* MerchantMenu::getSlotName(ItemSlot s) const {
    switch (s) {
        case ItemSlot::Weapon: return "武器";
        case ItemSlot::Helmet: return "头盔";
        case ItemSlot::Chest:  return "胸甲";
        case ItemSlot::Boots:  return "靴子";
        case ItemSlot::Ring:   return "戒指";
        case ItemSlot::Amulet: return "项链";
    }
    return "?";
}

const char* MerchantMenu::getAffixName(AffixType t) const {
    switch (t) {
        case AffixType::AddedDamage:   return "伤害";
        case AffixType::AddedDefense:  return "防御";
        case AffixType::CritRate:     return "暴击率";
        case AffixType::CritDamage:   return "暴击伤害";
        case AffixType::MoveSpeed:    return "移速";
        case AffixType::AttackSpeed:  return "攻速";
        case AffixType::Lifesteal:    return "吸血";
        case AffixType::MaxHp:        return "最大生命";
        case AffixType::MaxMp:        return "最大法力";
    }
    return "?";
}

std::string MerchantMenu::formatAffixValue(const Affix& affix) {
    // 百分比词缀 value 是小数（如 0.05），显示时 ×100
    float displayVal = affix.isPercent ? affix.value * 100.f : affix.value;
    return std::to_string(static_cast<int>(displayVal + 0.5f)) +
           (affix.isPercent ? "%" : "");
}

const sf::Texture& MerchantMenu::getIconTexture(ItemSlot slot) const {
    return itemIcons_[static_cast<size_t>(slot)];
}

std::pair<int, int> MerchantMenu::CheckClick(sf::Vector2f mousePos) const {
    // 检查商人库存点击（购买）
    for (int i = 0; i < MerchantSystem::kMerchantStockSize; ++i) {
        if (stockBounds_[i].contains(mousePos)) {
            return {1, i}; // 购买
        }
    }
    // 检查技能库存点击（购买技能）
    for (int i = 0; i < MerchantSystem::kMerchantSkillSize; ++i) {
        if (skillStockBounds_[i].contains(mousePos)) {
            return {3, i}; // 购买技能
        }
    }
    // 检查背包点击（出售）
    for (int i = 0; i < InventorySystem::kBackpackSize; ++i) {
        if (backpackBounds_[i].contains(mousePos)) {
            return {2, i}; // 出售
        }
    }
    return {0, -1}; // 无操作
}

void MerchantMenu::UpdateHover(sf::Vector2f mousePos) {
    mousePos_ = mousePos;
    hoveredSkillStock_ = -1;
    hoveredSkillBackpack_ = -1;
    // 检查商人技能库存悬停
    for (int i = 0; i < MerchantSystem::kMerchantSkillSize; ++i) {
        if (skillStockBounds_[i].contains(mousePos)) {
            hoveredSkillStock_ = i;
            return;
        }
    }
    // 检查玩家技能背包悬停
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        if (skillBackpackBounds_[i].contains(mousePos)) {
            hoveredSkillBackpack_ = i;
            return;
        }
    }
}

void MerchantMenu::Render(sf::RenderTarget& target) const {
    if (!visible_ || !font_) return;

    // 半透明遮罩
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(0, 0, 0, 200));
    target.draw(overlay);

    // ---- 顶部标题 "神秘商人" ----
    sf::Text title;
    title.setFont(*font_);
    title.setString(U8("神秘商人（左键购买/出售 | G/ESC 关闭）"));
    title.setCharacterSize(36);
    title.setFillColor(sf::Color(255, 220, 100));
    title.setStyle(sf::Text::Bold);
    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setPosition((1280.f - titleBounds.width) * 0.5f, 20.f);
    target.draw(title);

    // ---- 顶部右侧：金币显示 ----
    {
        sf::Text coinText;
        coinText.setFont(*font_);
        coinText.setString(U8("金币: ") + std::to_string(playerCoins_));
        coinText.setCharacterSize(24);
        coinText.setFillColor(sf::Color(255, 220, 100));
        coinText.setStyle(sf::Text::Bold);
        coinText.setOutlineColor(sf::Color(80, 60, 0));
        coinText.setOutlineThickness(2.f);
        sf::FloatRect coinBounds = coinText.getLocalBounds();
        coinText.setPosition(1280.f - coinBounds.width - 20.f, 25.f);
        target.draw(coinText);
    }

    // ---- 左侧标题：购买 ----
    {
        sf::Text sec;
        sec.setFont(*font_);
        sec.setString(U8("购买"));
        sec.setCharacterSize(16);
        sec.setFillColor(sf::Color::White);
        sec.setStyle(sf::Text::Bold);
        sec.setOutlineColor(sf::Color::Black);
        sec.setOutlineThickness(2.f);
        sec.setPosition(40.f, 100.f);
        target.draw(sec);
    }

    // ---- 中间竖线分隔 ----
    {
        sf::RectangleShape divider(sf::Vector2f(3.f, 580.f));
        divider.setPosition(620.f, 100.f);
        divider.setFillColor(sf::Color(120, 100, 60, 200));
        target.draw(divider);
    }

    // ---- 右侧标题：背包（出售）----
    {
        sf::Text sec;
        sec.setFont(*font_);
        sec.setString(U8("背包（出售）"));
        sec.setCharacterSize(16);
        sec.setFillColor(sf::Color::White);
        sec.setStyle(sf::Text::Bold);
        sec.setOutlineColor(sf::Color::Black);
        sec.setOutlineThickness(2.f);
        sec.setPosition(660.f, 100.f);
        target.draw(sec);
    }

    // ---- 左侧：商人售卖物品（6 件，2 列 x 3 行）----
    for (int i = 0; i < MerchantSystem::kMerchantStockSize; ++i) {
        int col = i % 2;
        int row = i / 2;
        float x = 40.f + col * 290.f;
        float y = 110.f + row * 180.f;
        float w = 270.f;
        float h = 160.f;
        stockBounds_[i] = sf::FloatRect(x, y, w, h);

        const MerchantItem& mi = stock_[i];
        sf::Color qColor = getQualityColor(mi.item.quality);

        // 卡片背景
        sf::RectangleShape cardBg(sf::Vector2f(w, h));
        cardBg.setPosition(x, y);
        if (mi.sold) {
            cardBg.setFillColor(sf::Color(40, 40, 40, 200));
        } else {
            cardBg.setFillColor(sf::Color(30, 30, 50, 220));
        }
        cardBg.setOutlineColor(qColor);
        cardBg.setOutlineThickness(2.f);
        target.draw(cardBg);

        // 装备图标（右上角，36x36 缩放显示）
        {
            sf::Sprite iconSpr(getIconTexture(mi.item.slot));
            float iconSize = 36.f;
            iconSpr.setScale(iconSize / 24.f, iconSize / 24.f);
            iconSpr.setPosition(x + w - iconSize - 8.f, y + 8.f);
            if (mi.sold) {
                iconSpr.setColor(sf::Color(255, 255, 255, 80));
            }
            target.draw(iconSpr);
        }

        // 物品名
        sf::Text nameText;
        nameText.setFont(*font_);
        nameText.setString(utf8ToSfString(mi.item.name));
        nameText.setCharacterSize(16);
        nameText.setFillColor(mi.sold ? sf::Color(100, 100, 100) : qColor);
        nameText.setStyle(sf::Text::Bold);
        nameText.setPosition(x + 8.f, y + 6.f);
        target.draw(nameText);

        // 词缀
        if (!mi.sold) {
            std::string affixStr;
            for (const auto& affix : mi.item.affixes) {
                affixStr += std::string(getAffixName(affix.type)) + ":+" +
                            formatAffixValue(affix) + "\n";
            }
            sf::Text affixText;
            affixText.setFont(*font_);
            affixText.setString(utf8ToSfString(affixStr));
            affixText.setCharacterSize(12);
            affixText.setFillColor(sf::Color(200, 200, 200));
            affixText.setPosition(x + 8.f, y + 30.f);
            target.draw(affixText);
        }

        // 价格
        sf::Text priceText;
        priceText.setFont(*font_);
        if (mi.sold) {
            priceText.setString(U8("已售出"));
            priceText.setFillColor(sf::Color(120, 120, 120));
        } else {
            priceText.setString(U8("价格: ") + std::to_string(mi.price) + U8(" 金"));
            priceText.setFillColor(sf::Color(255, 220, 100));
        }
        priceText.setCharacterSize(14);
        priceText.setPosition(x + 8.f, y + h - 24.f);
        target.draw(priceText);
    }

    // ---- 左侧：商人售卖技能（2 个，1 行 x 2 列）----
    {
        // 技能区域标题
        sf::Text skillSec;
        skillSec.setFont(*font_);
        skillSec.setString(U8("购买技能"));
        skillSec.setCharacterSize(16);
        skillSec.setFillColor(sf::Color::White);
        skillSec.setStyle(sf::Text::Bold);
        skillSec.setOutlineColor(sf::Color::Black);
        skillSec.setOutlineThickness(2.f);
        skillSec.setPosition(40.f, 640.f);
        target.draw(skillSec);
    }

    // 技能颜色映射
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

    for (int i = 0; i < MerchantSystem::kMerchantSkillSize; ++i) {
        float x = 40.f + i * 290.f;
        float y = 660.f;
        float w = 270.f;
        float h = 50.f;
        skillStockBounds_[i] = sf::FloatRect(x, y, w, h);

        const MerchantSkill& ms = skillStock_[i];
        sf::Color sColor = (ms.type != SkillType::Count && !ms.sold) ? getSkillColor(ms.type) : sf::Color(80, 80, 80);

        // 卡片背景
        sf::RectangleShape cardBg(sf::Vector2f(w, h));
        cardBg.setPosition(x, y);
        if (ms.sold || ms.type == SkillType::Count) {
            cardBg.setFillColor(sf::Color(40, 40, 40, 200));
        } else {
            cardBg.setFillColor(sf::Color(30, 30, 50, 220));
        }
        cardBg.setOutlineColor(sColor);
        cardBg.setOutlineThickness(2.f);
        target.draw(cardBg);

        if (ms.type != SkillType::Count) {
            // 技能名称
            sf::Text nameText;
            nameText.setFont(*font_);
            nameText.setString(utf8ToSfString(GetSkillName(ms.type)));
            nameText.setCharacterSize(14);
            nameText.setFillColor(ms.sold ? sf::Color(100, 100, 100) : sColor);
            nameText.setStyle(sf::Text::Bold);
            nameText.setPosition(x + 8.f, y + 4.f);
            target.draw(nameText);

            // 价格
            sf::Text priceText;
            priceText.setFont(*font_);
            if (ms.sold) {
                priceText.setString(U8("已售出"));
                priceText.setFillColor(sf::Color(120, 120, 120));
            } else {
                priceText.setString(U8("价格: ") + std::to_string(ms.price) + U8(" 金"));
                priceText.setFillColor(sf::Color(255, 220, 100));
            }
            priceText.setCharacterSize(12);
            priceText.setPosition(x + 8.f, y + 26.f);
            target.draw(priceText);
        }
    }

    // ---- 右侧：玩家背包（25 格，5 列 x 5 行）----
    for (int i = 0; i < InventorySystem::kBackpackSize; ++i) {
        int col = i % 5;
        int row = i / 5;
        float x = 660.f + col * 110.f;
        float y = 110.f + row * 100.f;
        float w = 100.f;
        float h = 90.f;
        backpackBounds_[i] = sf::FloatRect(x, y, w, h);

        sf::RectangleShape cellBg(sf::Vector2f(w, h));
        cellBg.setPosition(x, y);

        if (backpack_[i].has_value()) {
            const Item& item = backpack_[i].value();
            sf::Color qColor = getQualityColor(item.quality);
            cellBg.setFillColor(sf::Color(30, 40, 30, 220));
            cellBg.setOutlineColor(qColor);
            cellBg.setOutlineThickness(2.f);
            target.draw(cellBg);

            // 装备图标（右上角，24x24 缩放显示）
            {
                sf::Sprite iconSpr(getIconTexture(item.slot));
                float iconSize = 24.f;
                iconSpr.setScale(iconSize / 24.f, iconSize / 24.f);
                iconSpr.setPosition(x + w - iconSize - 4.f, y + 4.f);
                target.draw(iconSpr);
            }

            // 物品名
            sf::Text nameText;
            nameText.setFont(*font_);
            nameText.setString(utf8ToSfString(item.name));
            nameText.setCharacterSize(11);
            nameText.setFillColor(qColor);
            nameText.setPosition(x + 4.f, y + 4.f);
            target.draw(nameText);

            // 第一条词缀摘要
            if (!item.affixes.empty()) {
                const auto& affix = item.affixes[0];
                std::string affixStr = std::string(getAffixName(affix.type)) + ":+" +
                                        formatAffixValue(affix);
                sf::Text affixText;
                affixText.setFont(*font_);
                affixText.setString(utf8ToSfString(affixStr));
                affixText.setCharacterSize(10);
                affixText.setFillColor(sf::Color(200, 200, 200));
                affixText.setPosition(x + 4.f, y + 22.f);
                target.draw(affixText);
            }

            // 出售价格
            int sellPrice = MerchantSystem::CalcSellPrice(item);
            sf::Text sellText;
            sellText.setFont(*font_);
            sellText.setString(U8("售:") + std::to_string(sellPrice) + U8("金"));
            sellText.setCharacterSize(11);
            sellText.setFillColor(sf::Color(255, 220, 100));
            sellText.setPosition(x + 4.f, y + h - 18.f);
            target.draw(sellText);
        } else {
            cellBg.setFillColor(sf::Color(20, 20, 20, 180));
            cellBg.setOutlineColor(sf::Color(60, 60, 60));
            cellBg.setOutlineThickness(1.f);
            target.draw(cellBg);
        }
    }

    // ---- 右侧：玩家技能背包（5 格，仅显示）----
    {
        // 技能背包区域标题
        sf::Text skillBpSec;
        skillBpSec.setFont(*font_);
        skillBpSec.setString(U8("技能背包"));
        skillBpSec.setCharacterSize(16);
        skillBpSec.setFillColor(sf::Color::White);
        skillBpSec.setStyle(sf::Text::Bold);
        skillBpSec.setOutlineColor(sf::Color::Black);
        skillBpSec.setOutlineThickness(2.f);
        skillBpSec.setPosition(660.f, 610.f);
        target.draw(skillBpSec);
    }

    // 复用上方已定义的 getSkillColor lambda
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        float x = 660.f + i * 88.f;
        float y = 635.f;
        float w = 80.f;
        float h = 70.f;
        skillBackpackBounds_[i] = sf::FloatRect(x, y, w, h);

        SkillType type = skillBackpack_[i];
        sf::RectangleShape cellBg(sf::Vector2f(w, h));
        cellBg.setPosition(x, y);
        cellBg.setFillColor(sf::Color(25, 25, 35, 220));

        if (type != SkillType::Count) {
            cellBg.setOutlineColor(getSkillColor(type));
            cellBg.setOutlineThickness(2.f);
        } else {
            cellBg.setOutlineColor(sf::Color(60, 60, 60));
            cellBg.setOutlineThickness(1.f);
        }
        target.draw(cellBg);

        if (type != SkillType::Count) {
            // 技能名称
            sf::Text skillName;
            skillName.setFont(*font_);
            skillName.setString(utf8ToSfString(GetSkillName(type)));
            skillName.setCharacterSize(12);
            skillName.setFillColor(getSkillColor(type));
            skillName.setStyle(sf::Text::Bold);
            skillName.setPosition(x + 4.f, y + 4.f);
            target.draw(skillName);
        }
    }

    // ---- 技能 tooltip（鼠标悬停在技能上时显示）----
    if (hoveredSkillStock_ >= 0 && hoveredSkillStock_ < MerchantSystem::kMerchantSkillSize) {
        const MerchantSkill& ms = skillStock_[hoveredSkillStock_];
        if (ms.type != SkillType::Count) {
            drawSkillTooltip(target, *font_, ms.type, mousePos_);
        }
    } else if (hoveredSkillBackpack_ >= 0 && hoveredSkillBackpack_ < kSkillBackpackSize) {
        SkillType type = skillBackpack_[hoveredSkillBackpack_];
        if (type != SkillType::Count) {
            drawSkillTooltip(target, *font_, type, mousePos_);
        }
    }
}

// ============================================================================
// DebugPanel 实现
// ============================================================================
DebugPanel::DebugPanel() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
    for (auto& b : buttonBounds_) b = sf::FloatRect();
}

void DebugPanel::Initialize(const sf::Font& font) {
    font_ = &font;

    // 创建按钮
    auto createButton = [&](const std::string& text, float x, float y, float w, float h) -> Button* {
        auto btn = std::make_unique<Button>();
        btn->SetFont(font);
        btn->SetText(text);
        btn->SetPosition(sf::Vector2f(x, y));
        btn->SetSize(sf::Vector2f(w, h));
        btn->SetBackgroundColor(sf::Color(60, 60, 80));
        btn->SetHoverColor(sf::Color(100, 100, 140));
        btn->SetPressedColor(sf::Color(40, 40, 60));
        btn->SetTextColor(sf::Color::White);
        Button* ptr = btn.get();
        AddChild(std::move(btn));
        return ptr;
    };

    // 传送按钮（左侧第1列 x=50）
    teleportSpawnBtn_ = createButton("传送到出生房", 50.f, 150.f, 200.f, 36.f);
    teleportTreasureBtn_ = createButton("传送到宝箱房", 50.f, 190.f, 200.f, 36.f);
    teleportTrapBtn_ = createButton("传送到陷阱房", 50.f, 230.f, 200.f, 36.f);
    teleportObstacleBtn_ = createButton("传送到阻碍房", 50.f, 270.f, 200.f, 36.f);
    teleportBossBtn_ = createButton("传送到Boss房", 50.f, 310.f, 200.f, 36.f);
    teleportStairsBtn_ = createButton("传送到楼梯房", 50.f, 350.f, 200.f, 36.f);
    teleportEventBtn_ = createButton("传送到事件房", 50.f, 390.f, 200.f, 36.f);
    teleportCursedBtn_ = createButton("传送到诅咒房", 50.f, 430.f, 200.f, 36.f);

    // 作弊按钮（右侧第2列 x=270）
    godModeBtn_ = createButton("无敌模式", 270.f, 150.f, 200.f, 36.f);
    killAllBtn_ = createButton("秒杀所有敌人", 270.f, 190.f, 200.f, 36.f);
    addCoinsBtn_ = createButton("+1000 金币", 270.f, 230.f, 200.f, 36.f);
    addExpBtn_ = createButton("+1000 经验", 270.f, 270.f, 200.f, 36.f);
    addSkillPointBtn_ = createButton("+1 技能点", 270.f, 310.f, 200.f, 36.f);
    fullHealBtn_ = createButton("满血", 270.f, 350.f, 200.f, 36.f);
    resetCooldownBtn_ = createButton("重置技能冷却", 270.f, 390.f, 200.f, 36.f);
    removeCurseBtn_ = createButton("清除诅咒", 270.f, 430.f, 200.f, 36.f);

    // 系统按钮（第3列 x=490）
    nextLevelBtn_ = createButton("立即下一层", 490.f, 150.f, 200.f, 36.f);
    clearScreenBtn_ = createButton("清屏", 490.f, 190.f, 200.f, 36.f);
}

void DebugPanel::Update(float dt) {
    UIElement::Update(dt);
}

void DebugPanel::Render(sf::RenderTarget& target) const {
    if (!visible_) return;

    // 半透明背景（仅覆盖左侧按钮区 + 右侧信息区，不全屏遮挡）
    sf::RectangleShape bg(sf::Vector2f(720.f, 600.f));
    bg.setPosition(position_);
    bg.setFillColor(sf::Color(0, 0, 0, 200));
    bg.setOutlineColor(sf::Color(100, 100, 140, 180));
    bg.setOutlineThickness(2.f);
    target.draw(bg);

    // 标题
    sf::Text title;
    title.setFont(*font_);
    title.setString(U8("调试面板 (F5 关闭)"));
    title.setCharacterSize(28);
    title.setFillColor(sf::Color(255, 200, 100));
    title.setStyle(sf::Text::Bold);
    title.setPosition(position_.x + 50.f, position_.y + 20.f);
    target.draw(title);

    // 无敌模式状态标记
    if (godMode_) {
        sf::Text godTag;
        godTag.setFont(*font_);
        godTag.setString(U8("[无敌ON]"));
        godTag.setCharacterSize(16);
        godTag.setFillColor(sf::Color(255, 80, 80));
        godTag.setStyle(sf::Text::Bold);
        godTag.setPosition(position_.x + 580.f, position_.y + 30.f);
        target.draw(godTag);
    }

    // 渲染子元素（按钮）
    UIElement::Render(target);

    // ---- 右侧实时状态信息面板（x=720 起）----
    const float infoX = 740.f;
    float infoY = 20.f;
    const float lineH = 22.f;

    sf::Text info;
    info.setFont(*font_);
    info.setCharacterSize(16);
    info.setFillColor(sf::Color(200, 220, 255));

    // 使用 sf::String 拼接，避免 std::string 编码问题
    auto drawInfo = [&](const sf::String& label, const sf::String& value, sf::Color valueColor) {
        sf::Text lbl;
        lbl.setFont(*font_);
        lbl.setString(label);
        lbl.setCharacterSize(16);
        lbl.setFillColor(sf::Color(160, 160, 180));
        lbl.setPosition(infoX, infoY);
        target.draw(lbl);

        sf::Text val;
        val.setFont(*font_);
        val.setString(value);
        val.setCharacterSize(16);
        val.setFillColor(valueColor);
        val.setPosition(infoX + 110.f, infoY);
        target.draw(val);
        infoY += lineH;
    };

    // 标题分隔线
    sf::Text infoTitle;
    infoTitle.setFont(*font_);
    infoTitle.setString(U8("=== 实时状态 ==="));
    infoTitle.setCharacterSize(18);
    infoTitle.setFillColor(sf::Color(255, 200, 100));
    infoTitle.setStyle(sf::Text::Bold);
    infoTitle.setPosition(infoX, infoY);
    target.draw(infoTitle);
    infoY += lineH + 4.f;

    // 基本信息
    drawInfo(U8("FPS"), std::to_string(static_cast<int>(stats_.fps)),
             stats_.fps < 30.f ? sf::Color(255, 100, 100) : sf::Color(100, 255, 100));
    drawInfo(U8("当前层"), std::to_string(stats_.dungeonLevel), sf::Color::White);
    drawInfo(U8("房间"), std::to_string(stats_.currentRoomIndex) + "/" + std::to_string(stats_.totalRooms),
             sf::Color::White);
    drawInfo(U8("房间类型"), sf::String(stats_.currentRoomType), sf::Color(180, 220, 255));
    drawInfo(U8("已清理"), std::to_string(stats_.clearedRooms) + "/" + std::to_string(stats_.totalRooms),
             sf::Color::White);
    drawInfo(U8("存档槽"), std::to_string(stats_.currentSlot), sf::Color::White);

    infoY += 6.f;
    // 实体统计
    sf::Text entityTitle;
    entityTitle.setFont(*font_);
    entityTitle.setString(U8("=== 实体统计 ==="));
    entityTitle.setCharacterSize(18);
    entityTitle.setFillColor(sf::Color(255, 200, 100));
    entityTitle.setStyle(sf::Text::Bold);
    entityTitle.setPosition(infoX, infoY);
    target.draw(entityTitle);
    infoY += lineH + 4.f;

    drawInfo(U8("敌人"), std::to_string(stats_.enemyCount), sf::Color(255, 150, 150));
    drawInfo(U8("弹幕"), std::to_string(stats_.projectileCount), sf::Color(255, 200, 100));
    drawInfo(U8("粒子"), std::to_string(stats_.particleCount), sf::Color(200, 200, 255));
    drawInfo(U8("地裂区"), std::to_string(stats_.fissureCount), sf::Color(255, 120, 80));

    infoY += 6.f;
    // 玩家状态
    sf::Text playerTitle;
    playerTitle.setFont(*font_);
    playerTitle.setString(U8("=== 玩家状态 ==="));
    playerTitle.setCharacterSize(18);
    playerTitle.setFillColor(sf::Color(255, 200, 100));
    playerTitle.setStyle(sf::Text::Bold);
    playerTitle.setPosition(infoX, infoY);
    target.draw(playerTitle);
    infoY += lineH + 4.f;

    drawInfo(U8("坐标"), "(" + std::to_string(static_cast<int>(stats_.playerX)) + "," +
             std::to_string(static_cast<int>(stats_.playerY)) + ")", sf::Color::White);
    drawInfo(U8("HP"), std::to_string(static_cast<int>(stats_.playerHp)) + "/" +
             std::to_string(static_cast<int>(stats_.playerMaxHp)),
             stats_.playerHp < stats_.playerMaxHp * 0.3f ? sf::Color(255, 80, 80) : sf::Color(100, 255, 100));
    drawInfo(U8("等级"), std::to_string(stats_.playerLevel), sf::Color::White);
    drawInfo(U8("经验"), std::to_string(stats_.playerExp) + "/" + std::to_string(stats_.playerExpToNext),
             sf::Color(255, 220, 100));
    drawInfo(U8("金币"), std::to_string(stats_.playerCoins), sf::Color(255, 220, 100));
    drawInfo(U8("技能点"), std::to_string(stats_.playerSkillPoints),
             stats_.playerSkillPoints > 0 ? sf::Color(255, 200, 50) : sf::Color::White);
    drawInfo(U8("诅咒"), stats_.playerCursed ? U8("是") : U8("否"),
             stats_.playerCursed ? sf::Color(200, 80, 220) : sf::Color(100, 200, 100));

    infoY += 6.f;
    // Boss 状态
    sf::Text bossTitle;
    bossTitle.setFont(*font_);
    bossTitle.setString(U8("=== Boss 状态 ==="));
    bossTitle.setCharacterSize(18);
    bossTitle.setFillColor(sf::Color(255, 200, 100));
    bossTitle.setStyle(sf::Text::Bold);
    bossTitle.setPosition(infoX, infoY);
    target.draw(bossTitle);
    infoY += lineH + 4.f;

    if (stats_.bossActive) {
        drawInfo(U8("存活"), U8("是"), sf::Color(255, 80, 80));
        char hpBuf[16];
        std::snprintf(hpBuf, sizeof(hpBuf), "%.1f%%", stats_.bossHpPercent * 100.f);
        drawInfo(U8("HP"), hpBuf, sf::Color(255, 100, 100));
    } else {
        drawInfo(U8("存活"), U8("否"), sf::Color(100, 200, 100));
    }

    infoY += 6.f;
    // 性能
    sf::Text perfTitle;
    perfTitle.setFont(*font_);
    perfTitle.setString(U8("=== 性能(ms) ==="));
    perfTitle.setCharacterSize(18);
    perfTitle.setFillColor(sf::Color(255, 200, 100));
    perfTitle.setStyle(sf::Text::Bold);
    perfTitle.setPosition(infoX, infoY);
    target.draw(perfTitle);
    infoY += lineH + 4.f;

    char perfBuf[32];
    std::snprintf(perfBuf, sizeof(perfBuf), "%.2f", stats_.aiTimeMs);
    drawInfo(U8("AI"), perfBuf, sf::Color::White);
    std::snprintf(perfBuf, sizeof(perfBuf), "%.2f", stats_.combatTimeMs);
    drawInfo(U8("战斗"), perfBuf, sf::Color::White);
    std::snprintf(perfBuf, sizeof(perfBuf), "%.2f", stats_.projectileTimeMs);
    drawInfo(U8("弹幕"), perfBuf, sf::Color::White);
}

int DebugPanel::CheckClick(sf::Vector2f mousePos) const {
    if (!visible_) return 0;

    // 检查每个按钮的点击
    auto checkButton = [](Button* btn, sf::Vector2f pos) -> bool {
        if (!btn) return false;
        sf::FloatRect bounds = btn->GetBounds();
        return bounds.contains(pos);
    };

    if (checkButton(teleportSpawnBtn_, mousePos)) return 1;
    if (checkButton(teleportTreasureBtn_, mousePos)) return 2;
    if (checkButton(teleportTrapBtn_, mousePos)) return 3;
    if (checkButton(teleportObstacleBtn_, mousePos)) return 4;
    if (checkButton(godModeBtn_, mousePos)) return 5;
    if (checkButton(killAllBtn_, mousePos)) return 6;
    if (checkButton(addCoinsBtn_, mousePos)) return 7;
    if (checkButton(addExpBtn_, mousePos)) return 8;
    if (checkButton(clearScreenBtn_, mousePos)) return 9;
    // 新增按钮
    if (checkButton(teleportBossBtn_, mousePos)) return 10;
    if (checkButton(teleportStairsBtn_, mousePos)) return 11;
    if (checkButton(nextLevelBtn_, mousePos)) return 12;
    if (checkButton(addSkillPointBtn_, mousePos)) return 13;
    if (checkButton(removeCurseBtn_, mousePos)) return 14;
    if (checkButton(fullHealBtn_, mousePos)) return 15;
    if (checkButton(resetCooldownBtn_, mousePos)) return 16;
    if (checkButton(teleportEventBtn_, mousePos)) return 17;
    if (checkButton(teleportCursedBtn_, mousePos)) return 18;

    return 0;
}

// ============================================================================
// SettingsMenu 实现
// ============================================================================

// 预设分辨率列表
constexpr SettingsMenu::Resolution SettingsMenu::kResolutions[kResolutionCount] = {
    {1280, 720,  "1280x720"},
    {1600, 900,  "1600x900"},
    {1920, 1080, "1920x1080"},
    {1280, 800,  "1280x800"},
};

SettingsMenu::SettingsMenu() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
}

void SettingsMenu::Initialize(const sf::Font& font) {
    font_ = &font;
}

int SettingsMenu::GetCurrentResolutionIndex() const {
    for (int i = 0; i < kResolutionCount; ++i) {
        if (kResolutions[i].w == resW_ && kResolutions[i].h == resH_) return i;
    }
    return 0; // 未匹配则返回第一个
}

void SettingsMenu::Render(sf::RenderTarget& target) const {
    if (!visible_) return;

    // 半透明背景遮罩
    sf::RectangleShape bg(sf::Vector2f(1280.f, 720.f));
    bg.setPosition(0.f, 0.f);
    bg.setFillColor(sf::Color(0, 0, 0, 200));
    target.draw(bg);

    // 居中面板背景
    float panelW = 500.f;
    float panelH = 460.f;
    float panelX = (1280.f - panelW) * 0.5f;
    float panelY = (720.f - panelH) * 0.5f;
    sf::RectangleShape panel(sf::Vector2f(panelW, panelH));
    panel.setPosition(panelX, panelY);
    panel.setFillColor(sf::Color(30, 30, 40, 240));
    panel.setOutlineColor(sf::Color(255, 220, 100));
    panel.setOutlineThickness(2.f);
    target.draw(panel);

    // 标题
    sf::Text title;
    title.setFont(*font_);
    title.setString(U8("设置"));
    title.setCharacterSize(32);
    title.setFillColor(sf::Color(255, 220, 100));
    title.setStyle(sf::Text::Bold);
    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setPosition(panelX + (panelW - titleBounds.width) * 0.5f, panelY + 20.f);
    target.draw(title);

    // 辅助绘制函数：一行设置项（标签 + 值 + +/- 按钮）
    auto drawRow = [&](const std::string& label, const std::string& value,
                       float y, int idxMinus, int idxPlus) {
        // 标签
        sf::Text lbl;
        lbl.setFont(*font_);
        lbl.setString(utf8ToSfString(label));
        lbl.setCharacterSize(20);
        lbl.setFillColor(sf::Color(220, 220, 220));
        lbl.setPosition(panelX + 40.f, y);
        target.draw(lbl);

        // - 按钮
        float btnW = 36.f, btnH = 36.f;
        float valueX = panelX + 220.f;
        float minusX = valueX - btnW - 10.f;
        sf::RectangleShape minusBtn(sf::Vector2f(btnW, btnH));
        minusBtn.setPosition(minusX, y);
        minusBtn.setFillColor(sf::Color(60, 60, 80));
        minusBtn.setOutlineColor(sf::Color(120, 120, 140));
        target.draw(minusBtn);
        buttonBounds_[idxMinus] = minusBtn.getGlobalBounds();
        sf::Text minusTxt;
        minusTxt.setFont(*font_);
        minusTxt.setString(U8("-"));
        minusTxt.setCharacterSize(24);
        minusTxt.setFillColor(sf::Color::White);
        minusTxt.setStyle(sf::Text::Bold);
        minusTxt.setPosition(minusX + 12.f, y - 2.f);
        target.draw(minusTxt);

        // 值
        sf::Text val;
        val.setFont(*font_);
        val.setString(utf8ToSfString(value));
        val.setCharacterSize(20);
        val.setFillColor(sf::Color(255, 220, 100));
        val.setStyle(sf::Text::Bold);
        sf::FloatRect valBounds = val.getLocalBounds();
        val.setPosition(valueX + (100.f - valBounds.width) * 0.5f, y + 4.f);
        target.draw(val);

        // + 按钮
        float plusX = valueX + 100.f + 10.f;
        sf::RectangleShape plusBtn(sf::Vector2f(btnW, btnH));
        plusBtn.setPosition(plusX, y);
        plusBtn.setFillColor(sf::Color(60, 60, 80));
        plusBtn.setOutlineColor(sf::Color(120, 120, 140));
        target.draw(plusBtn);
        buttonBounds_[idxPlus] = plusBtn.getGlobalBounds();
        sf::Text plusTxt;
        plusTxt.setFont(*font_);
        plusTxt.setString(U8("+"));
        plusTxt.setCharacterSize(24);
        plusTxt.setFillColor(sf::Color::White);
        plusTxt.setStyle(sf::Text::Bold);
        plusTxt.setPosition(plusX + 10.f, y - 2.f);
        target.draw(plusTxt);
    };

    // BGM 音量行（按钮索引 1=BGM-, 2=BGM+）
    drawRow("BGM 音量", std::to_string(static_cast<int>(bgmVolume_)),
            panelY + 90.f, 1, 2);

    // 音效音量行（按钮索引 3=SFX-, 4=SFX+）
    drawRow("音效音量", std::to_string(static_cast<int>(sfxVolume_)),
            panelY + 150.f, 3, 4);

    // 分辨率行（按钮索引 5=前一个, 6=下一个）
    {
        float y = panelY + 210.f;
        sf::Text lbl;
        lbl.setFont(*font_);
        lbl.setString(U8("分辨率"));
        lbl.setCharacterSize(20);
        lbl.setFillColor(sf::Color(220, 220, 220));
        lbl.setPosition(panelX + 40.f, y);
        target.draw(lbl);

        int resIdx = GetCurrentResolutionIndex();
        std::string resStr = kResolutions[resIdx].label;

        // < 按钮
        float btnW = 36.f, btnH = 36.f;
        float valueX = panelX + 220.f;
        float prevX = valueX - btnW - 10.f;
        sf::RectangleShape prevBtn(sf::Vector2f(btnW, btnH));
        prevBtn.setPosition(prevX, y);
        prevBtn.setFillColor(sf::Color(60, 60, 80));
        prevBtn.setOutlineColor(sf::Color(120, 120, 140));
        target.draw(prevBtn);
        buttonBounds_[5] = prevBtn.getGlobalBounds();
        sf::Text prevTxt;
        prevTxt.setFont(*font_);
        prevTxt.setString(U8("<"));
        prevTxt.setCharacterSize(22);
        prevTxt.setFillColor(sf::Color::White);
        prevTxt.setStyle(sf::Text::Bold);
        prevTxt.setPosition(prevX + 11.f, y);
        target.draw(prevTxt);

        // 当前分辨率
        sf::Text val;
        val.setFont(*font_);
        val.setString(utf8ToSfString(resStr));
        val.setCharacterSize(20);
        val.setFillColor(sf::Color(255, 220, 100));
        val.setStyle(sf::Text::Bold);
        sf::FloatRect valBounds = val.getLocalBounds();
        val.setPosition(valueX + (100.f - valBounds.width) * 0.5f, y + 4.f);
        target.draw(val);

        // > 按钮
        float nextX = valueX + 100.f + 10.f;
        sf::RectangleShape nextBtn(sf::Vector2f(btnW, btnH));
        nextBtn.setPosition(nextX, y);
        nextBtn.setFillColor(sf::Color(60, 60, 80));
        nextBtn.setOutlineColor(sf::Color(120, 120, 140));
        target.draw(nextBtn);
        buttonBounds_[6] = nextBtn.getGlobalBounds();
        sf::Text nextTxt;
        nextTxt.setFont(*font_);
        nextTxt.setString(U8(">"));
        nextTxt.setCharacterSize(22);
        nextTxt.setFillColor(sf::Color::White);
        nextTxt.setStyle(sf::Text::Bold);
        nextTxt.setPosition(nextX + 11.f, y);
        target.draw(nextTxt);
    }

    // 应用按钮（索引 7）
    {
        float btnW = 140.f, btnH = 44.f;
        float applyX = panelX + panelW * 0.5f - btnW - 20.f;
        float applyY = panelY + panelH - 70.f;
        sf::RectangleShape applyBtn(sf::Vector2f(btnW, btnH));
        applyBtn.setPosition(applyX, applyY);
        applyBtn.setFillColor(sf::Color(80, 140, 80));
        applyBtn.setOutlineColor(sf::Color(140, 220, 140));
        applyBtn.setOutlineThickness(2.f);
        target.draw(applyBtn);
        buttonBounds_[7] = applyBtn.getGlobalBounds();
        sf::Text applyTxt;
        applyTxt.setFont(*font_);
        applyTxt.setString(U8("应用"));
        applyTxt.setCharacterSize(22);
        applyTxt.setFillColor(sf::Color::White);
        applyTxt.setStyle(sf::Text::Bold);
        sf::FloatRect applyTxtBounds = applyTxt.getLocalBounds();
        applyTxt.setPosition(applyX + (btnW - applyTxtBounds.width) * 0.5f,
                             applyY + (btnH - applyTxtBounds.height) * 0.5f - 4.f);
        target.draw(applyTxt);
    }

    // 返回按钮（索引 8）
    {
        float btnW = 140.f, btnH = 44.f;
        float backX = panelX + panelW * 0.5f + 20.f;
        float backY = panelY + panelH - 70.f;
        sf::RectangleShape backBtn(sf::Vector2f(btnW, btnH));
        backBtn.setPosition(backX, backY);
        backBtn.setFillColor(sf::Color(140, 80, 80));
        backBtn.setOutlineColor(sf::Color(220, 140, 140));
        backBtn.setOutlineThickness(2.f);
        target.draw(backBtn);
        buttonBounds_[8] = backBtn.getGlobalBounds();
        sf::Text backTxt;
        backTxt.setFont(*font_);
        backTxt.setString(U8("返回"));
        backTxt.setCharacterSize(22);
        backTxt.setFillColor(sf::Color::White);
        backTxt.setStyle(sf::Text::Bold);
        sf::FloatRect backTxtBounds = backTxt.getLocalBounds();
        backTxt.setPosition(backX + (btnW - backTxtBounds.width) * 0.5f,
                            backY + (btnH - backTxtBounds.height) * 0.5f - 4.f);
        target.draw(backTxt);
    }
}

int SettingsMenu::CheckClick(sf::Vector2f mousePos) const {
    if (!visible_) return 0;
    for (int i = 1; i <= 8; ++i) {
        if (buttonBounds_[i].contains(mousePos)) return i;
    }
    return 0;
}

// ============================================================================
// SaveLoadMenu 实现
// ============================================================================

SaveLoadMenu::SaveLoadMenu() {
    position_ = sf::Vector2f(0.f, 0.f);
    size_ = sf::Vector2f(1280.f, 720.f);
    anchor_ = UIAnchor::TopLeft;
    for (auto& b : cardBounds_) b = sf::FloatRect{};
    for (auto& b : deleteBtnBounds_) b = sf::FloatRect{};
    backBtnBounds_ = sf::FloatRect{};
}

void SaveLoadMenu::Initialize(const sf::Font& font) {
    font_ = &font;
    // 卡片边界在 Render 中按需更新（位置固定，可预计算）
    // 这里直接预计算，便于 CheckClick 不依赖 Render 调用
    const float cardW = 280.f;
    const float cardH = 360.f;
    const float gap = 30.f;
    const float totalW = cardW * 3.f + gap * 2.f;
    const float startX = (1280.f - totalW) * 0.5f;
    const float cardY = 160.f;
    for (int i = 0; i < SaveSystem::kSlotCount; ++i) {
        float x = startX + i * (cardW + gap);
        cardBounds_[i] = sf::FloatRect(x, cardY, cardW, cardH);
        // 删除按钮在卡片下方
        deleteBtnBounds_[i] = sf::FloatRect(x + 40.f, cardY + cardH + 8.f, cardW - 80.f, 32.f);
    }
    // 返回按钮
    backBtnBounds_ = sf::FloatRect(540.f, 580.f, 200.f, 50.f);
}

void SaveLoadMenu::SetSlotInfo(const std::array<SaveSlotInfo, SaveSystem::kSlotCount>& info) {
    slotInfo_ = info;
}

void SaveLoadMenu::Update(float dt) {
    UIElement::Update(dt);
    blinkTimer_ += dt;
}

std::string SaveLoadMenu::formatTime(float seconds) {
    int total = static_cast<int>(seconds);
    int mm = total / 60;
    int ss = total % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
    return std::string(buf);
}

std::string SaveLoadMenu::formatTimestamp(int64_t unixSec) {
    std::time_t t = static_cast<std::time_t>(unixSec);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    tmv = *std::localtime(&t);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmv);
    return std::string(buf);
}

void SaveLoadMenu::Render(sf::RenderTarget& target) const {
    if (!visible_ || !font_) return;

    // 半透明背景遮罩
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(10, 15, 30, 230));
    target.draw(overlay);

    // 标题
    sf::Text title;
    title.setFont(*font_);
    if (mode_ == Mode::Load) {
        title.setString(U8("读取存档"));
    } else {
        title.setString(U8("选择槽位（将覆盖原有存档）"));
    }
    title.setCharacterSize(40);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(sf::Color(255, 220, 100));
    sf::FloatRect tb = title.getLocalBounds();
    title.setPosition((1280.f - tb.width) * 0.5f, 60.f);
    target.draw(title);

    // 闪烁透明度（用于空槽位边框）
    float blink = 0.5f + 0.5f * std::sin(blinkTimer_ * 4.f);
    uint8_t blinkAlpha = static_cast<uint8_t>(120 + 100 * blink);

    // 渲染 3 张槽位卡片
    for (int i = 0; i < SaveSystem::kSlotCount; ++i) {
        const sf::FloatRect& b = cardBounds_[i];
        const SaveSlotInfo& info = slotInfo_[i];

        // 卡片背景
        sf::RectangleShape card(sf::Vector2f(b.width, b.height));
        card.setPosition(b.left, b.top);
        if (info.exists) {
            card.setFillColor(sf::Color(30, 40, 70, 220));
            card.setOutlineColor(sf::Color(180, 180, 220, 200));
        } else {
            // 空槽位：闪烁边框
            card.setFillColor(sf::Color(25, 25, 35, 200));
            card.setOutlineColor(sf::Color(120, 120, 120, blinkAlpha));
        }
        card.setOutlineThickness(2.f);
        target.draw(card);

        // 槽位号
        sf::Text slotNum;
        slotNum.setFont(*font_);
        slotNum.setString(U8("槽位 ") + std::to_string(i + 1));
        slotNum.setCharacterSize(26);
        slotNum.setStyle(sf::Text::Bold);
        slotNum.setFillColor(info.exists ? sf::Color(255, 220, 100) : sf::Color(140, 140, 140));
        slotNum.setPosition(b.left + 20.f, b.top + 16.f);
        target.draw(slotNum);

        if (info.exists) {
            // 显示存档详情
            sf::Text detail;
            detail.setFont(*font_);
            detail.setCharacterSize(18);
            detail.setFillColor(sf::Color(220, 220, 230));

            float yPos = b.top + 70.f;
            // 注意：参数必须用 sf::String 接收，避免 sf::String→std::string 隐式转换
            // （Windows 中文系统下 toAnsiString 用 GBK，再被 utf8ToSfString 当 UTF-8 解析会乱码）
            auto drawLine = [&](const sf::String& s) {
                detail.setString(s);
                detail.setPosition(b.left + 20.f, yPos);
                target.draw(detail);
                yPos += 32.f;
            };
            drawLine(U8("层数：") + std::to_string(info.level));
            drawLine(U8("玩家等级：") + std::to_string(info.playerLevel));
            drawLine(U8("击杀：") + std::to_string(info.kills));
            drawLine(U8("存活：") + formatTime(info.survivalTime));
            drawLine(U8("金币：") + std::to_string(info.coins));

            // 时间戳（小字）
            sf::Text ts;
            ts.setFont(*font_);
            ts.setString(utf8ToSfString(formatTimestamp(info.timestamp)));
            ts.setCharacterSize(14);
            ts.setFillColor(sf::Color(160, 160, 180));
            ts.setPosition(b.left + 20.f, b.top + b.height - 30.f);
            target.draw(ts);

            // 删除按钮
            const sf::FloatRect& db = deleteBtnBounds_[i];
            sf::RectangleShape delBtn(sf::Vector2f(db.width, db.height));
            delBtn.setPosition(db.left, db.top);
            delBtn.setFillColor(sf::Color(120, 50, 50, 220));
            delBtn.setOutlineColor(sf::Color(200, 100, 100, 200));
            delBtn.setOutlineThickness(1.f);
            target.draw(delBtn);
            sf::Text delTxt;
            delTxt.setFont(*font_);
            delTxt.setString(U8("删除"));
            delTxt.setCharacterSize(16);
            delTxt.setFillColor(sf::Color::White);
            sf::FloatRect dtb = delTxt.getLocalBounds();
            delTxt.setPosition(db.left + (db.width - dtb.width) * 0.5f,
                               db.top + (db.height - dtb.height) * 0.5f - 3.f);
            target.draw(delTxt);
        } else {
            // 空槽位提示
            sf::Text empty;
            empty.setFont(*font_);
            empty.setString(mode_ == Mode::Load ? U8("无存档") : U8("空槽位\n点击保存"));
            empty.setCharacterSize(22);
            empty.setFillColor(sf::Color(120, 120, 140));
            sf::FloatRect eb = empty.getLocalBounds();
            empty.setPosition(b.left + (b.width - eb.width) * 0.5f,
                              b.top + (b.height - eb.height) * 0.5f - 10.f);
            target.draw(empty);
        }
    }

    // 返回按钮
    sf::RectangleShape backBtn(sf::Vector2f(backBtnBounds_.width, backBtnBounds_.height));
    backBtn.setPosition(backBtnBounds_.left, backBtnBounds_.top);
    backBtn.setFillColor(sf::Color(80, 80, 120, 220));
    backBtn.setOutlineColor(sf::Color(180, 180, 220, 200));
    backBtn.setOutlineThickness(2.f);
    target.draw(backBtn);
    sf::Text backTxt;
    backTxt.setFont(*font_);
    backTxt.setString(U8("返回"));
    backTxt.setCharacterSize(22);
    backTxt.setStyle(sf::Text::Bold);
    backTxt.setFillColor(sf::Color::White);
    sf::FloatRect bb = backTxt.getLocalBounds();
    backTxt.setPosition(backBtnBounds_.left + (backBtnBounds_.width - bb.width) * 0.5f,
                        backBtnBounds_.top + (backBtnBounds_.height - bb.height) * 0.5f - 4.f);
    target.draw(backTxt);
}

int SaveLoadMenu::CheckClick(sf::Vector2f mousePos) const {
    if (!visible_) return 0;
    // 优先检测删除按钮和返回按钮（避免与卡片重叠冲突）
    for (int i = 0; i < SaveSystem::kSlotCount; ++i) {
        if (deleteBtnBounds_[i].contains(mousePos) && slotInfo_[i].exists) {
            return 5 + i; // 5=删槽1, 6=删槽2, 7=删槽3
        }
    }
    if (backBtnBounds_.contains(mousePos)) return 4;
    // 卡片点击
    for (int i = 0; i < SaveSystem::kSlotCount; ++i) {
        if (cardBounds_[i].contains(mousePos)) {
            // Load 模式：仅已有存档可点
            if (mode_ == Mode::Load && !slotInfo_[i].exists) return 0;
            return 1 + i; // 1=选槽1, 2=选槽2, 3=选槽3
        }
    }
    return 0;
}

} // namespace cu
