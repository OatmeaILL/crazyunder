#pragma once

// ============================================================================
// SoulWellMenu —— 灵魂之井面板 UI
// ----------------------------------------------------------------------------
// 入口：主菜单"灵魂之井"按钮（在 MainMenu 中添加）
// 显示：当前碎片数 + 6 条强化路径卡片（名称/等级/效果/成本/购买按钮）
// 关闭：点击返回按钮或按 ESC
//
// 操作返回值（CheckClick）：
//   0  = 无操作
//   1-6 = 购买对应索引的强化（Vitality/Wisdom/Fortune/Strength/Swiftness/Aegis）
//   7  = 返回（关闭面板）
// ============================================================================

#include <SFML/Graphics.hpp>
#include <array>
#include "ui/UIManager.h"
#include "gameplay/SoulMemorySystem.h"

namespace cu {

class SoulWellMenu : public UIElement {
public:
    SoulWellMenu();

    void Initialize(const sf::Font& font);

    // 同步灵魂之忆数据（每次打开前调用）
    void SetSoulMemoryData(const SoulMemorySystem& sys);

    void Update(float dt) override;
    void Render(sf::RenderTarget& target) const override;

    // 检测鼠标点击，返回操作类型（0-7）
    [[nodiscard]] int CheckClick(sf::Vector2f mousePos) const;

    // 每帧更新悬停状态（用于按钮高亮）
    void UpdateHover(sf::Vector2f mousePos);

private:
    const sf::Font* font_ = nullptr;

    // 当前快照数据（由 SetSoulMemoryData 同步）
    int shards_ = 0;
    int totalEarned_ = 0;
    std::array<int, kSoulUpgradeCount> upgradeLevels_{};
    std::array<int, kSoulUpgradeCount> upgradeCosts_{}; // 下一级成本，-1 表示已满级

    // 6 个购买按钮 + 1 个返回按钮的边界矩形
    mutable std::array<sf::FloatRect, 8> buttonBounds_;
    // 当前悬停的按钮索引（-1 表示无）
    int hoveredButton_ = -1;

    // 卡片配色（每条强化路径一种主色调）
    // 注：sf::Color 构造函数非 constexpr，故用非静态成员在构造函数中初始化
    std::array<sf::Color, kSoulUpgradeCount> kCardColors;
};

} // namespace cu
