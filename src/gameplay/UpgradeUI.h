#pragma once

// ============================================================================
// UpgradeUI  升级选择 UI 数据接口（Phase 7）
// ----------------------------------------------------------------------------
// 职责：
//   1. Show：接收 3 个升级选项，显示选择界面
//   2. SetHovered：设置当前悬停的选项索引
//   3. ConfirmSelection：确认选择（返回选中的索引）
//   4. GetSelectedIndex：获取当前选中的索引（-1 表示未选）
//
// 注意：本类仅管理数据与状态，渲染留给 Phase 8 UIManager
// Phase 7 暂时用控制台日志输出 3 个选项，按 1/2/3 键选择
// ============================================================================

#include <array>
#include <SFML/System/Vector2.hpp>
#include "gameplay/UpgradeSystem.h"

namespace cu {

// ---- 升级选择项（含 UI 位置与悬停状态）----
struct UpgradeChoice {
    UpgradeOption option;
    sf::Vector2f uiPosition{0.f, 0.f};
    bool hovered = false;
};

// ============================================================================
// UpgradeUI  升级选择 UI（数据层）
// ============================================================================
class UpgradeUI {
public:
    UpgradeUI();
    ~UpgradeUI() = default;

    // 显示升级选择界面
    // options: 3 个升级选项
    void Show(const std::array<UpgradeOption, 3>& options);

    // 获取选中的索引（-1 表示未选）
    [[nodiscard]] int GetSelectedIndex() const noexcept { return selectedIndex_; }

    // 设置悬停的选项索引（-1 表示无悬停）
    void SetHovered(int index);

    // 确认选择
    // 返回选中的索引（-1 表示未选择任何项）
    int ConfirmSelection();

    // 是否正在显示升级选择
    [[nodiscard]] bool IsVisible() const noexcept { return visible_; }

    // 隐藏 UI
    void Hide() { visible_ = false; selectedIndex_ = -1; }

    // 获取当前选择项（只读）
    [[nodiscard]] const std::array<UpgradeChoice, 3>& GetChoices() const noexcept {
        return choices_;
    }

    // 处理按键输入（1/2/3 选择，Enter 确认）
    // 返回 true 表示已做出选择
    bool HandleKeyInput(int key);

private:
    std::array<UpgradeChoice, 3> choices_;
    bool visible_ = false;
    int selectedIndex_ = -1;
    int hoveredIndex_ = -1;
};

} // namespace cu