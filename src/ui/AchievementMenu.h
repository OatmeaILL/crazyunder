#pragma once

// ============================================================================
// AchievementMenu  成就面板 UI
// ----------------------------------------------------------------------------
// 布局：
//   顶部：标题 + 解锁进度统计（已解锁/总数 + 百分比）
//   主体：成就网格（4 列布局，按分类分组）
//     - 已解锁：完整显示（图标+名称+描述+解锁时间）
//     - 未解锁：灰色显示（隐藏成就显示 ???）
//     - 进行中：显示当前进度/目标（百分比进度条）
//
// 交互：
//   按 Tab 键打开/关闭面板
//   ESC 键关闭面板
// ============================================================================

#include <SFML/Graphics.hpp>
#include <vector>
#include "ui/UIManager.h"
#include "gameplay/AchievementSystem.h"

namespace cu {

class AchievementMenu : public UIElement {
public:
    AchievementMenu();

    void Initialize(const sf::Font& font);

    // 设置成就数据（打开时调用）
    void SetAchievementData(const AchievementSystem& achievementSystem);

    void Update(float dt) override { (void)dt; }

    void Render(sf::RenderTarget& target) const override;

private:
    const sf::Font* font_ = nullptr;

    // 缓存的成就显示数据
    struct AchievementDisplay {
        int id = 0;
        AchievementCategory category = AchievementCategory::Combat;
        std::string name;
        std::string description;
        bool unlocked = false;
        bool isHidden = false;
        int64_t currentValue = 0;
        int64_t targetValue = 0;
        int64_t unlockedTimestamp = 0;
    };
    std::vector<AchievementDisplay> achievements_;
    int unlockedCount_ = 0;
    int totalCount_ = 0;

    // 分类颜色
    [[nodiscard]] sf::Color getCategoryColor(AchievementCategory cat) const;
    [[nodiscard]] const char* getCategoryName(AchievementCategory cat) const;
    // 格式化解锁时间为可读字符串
    [[nodiscard]] static std::string formatTimestamp(int64_t ts);
    // 自动换行（与 QuestMenu 同款实现，但简化版）
    void drawWrappedText(sf::RenderTarget& target, const std::string& text,
                         sf::Vector2f pos, float maxWidth, float lineSpacing,
                         unsigned int charSize, sf::Color color) const;
};

} // namespace cu
