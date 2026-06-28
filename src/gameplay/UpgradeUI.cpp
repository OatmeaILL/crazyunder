#include "gameplay/UpgradeUI.h"
#include "utils/Logger.h"

namespace cu {

// ============================================================================
// UpgradeUI 构造
// ============================================================================
UpgradeUI::UpgradeUI() {
    for (auto& choice : choices_) {
        choice.hovered = false;
    }
}

// ============================================================================
// Show  显示升级选择界面
// ----------------------------------------------------------------------------
// 接收 3 个升级选项，初始化 UI 状态
// Phase 7 暂时用控制台日志输出
// ============================================================================
void UpgradeUI::Show(const std::array<UpgradeOption, 3>& options) {
    visible_ = true;
    selectedIndex_ = -1;
    hoveredIndex_ = -1;

    // 设置 3 个选项的 UI 位置（屏幕中央水平排列，Phase 8 替换为实际像素位置）
    float startX = 200.f;
    float spacing = 300.f;
    for (int i = 0; i < 3; ++i) {
        choices_[i].option = options[i];
        choices_[i].uiPosition = sf::Vector2f(startX + i * spacing, 360.f);
        choices_[i].hovered = false;
    }

    // 控制台输出 3 个选项
    LOG_INFO("======== 升级选择 ========");
    for (int i = 0; i < 3; ++i) {
        if (options[i].type == UpgradeType::Count) {
            LOG_INFO("  [%d] (无可用升级)", i + 1);
        } else {
            LOG_INFO("  [%d] %s (Lv.%d/%d): %s",
                     i + 1,
                     options[i].name.c_str(),
                     options[i].currentLevel,
                     options[i].maxLevel,
                     options[i].description.c_str());
        }
    }
    LOG_INFO("按 1/2/3 键选择升级");
    LOG_INFO("==========================");
}

// ============================================================================
// SetHovered  设置悬停的选项索引
// ============================================================================
void UpgradeUI::SetHovered(int index) {
    if (index < -1 || index > 2) return;
    // 清除所有悬停标志
    for (auto& choice : choices_) {
        choice.hovered = false;
    }
    hoveredIndex_ = index;
    if (index >= 0 && index < 3) {
        choices_[index].hovered = true;
    }
}

// ============================================================================
// ConfirmSelection  确认选择
// ============================================================================
int UpgradeUI::ConfirmSelection() {
    if (hoveredIndex_ >= 0 && hoveredIndex_ < 3) {
        selectedIndex_ = hoveredIndex_;
        visible_ = false;
        LOG_INFO("已选择升级: [%d] %s",
                 selectedIndex_ + 1,
                 choices_[selectedIndex_].option.name.c_str());
        return selectedIndex_;
    }
    return -1;
}

// ============================================================================
// HandleKeyInput  处理按键输入（1/2/3 选择）
// ----------------------------------------------------------------------------
// key: SFML 按键代码（sf::Keyboard::Num1 = 27, Num2 = 28, Num3 = 29）
// 返回 true 表示已做出选择
// ============================================================================
bool UpgradeUI::HandleKeyInput(int key) {
    if (!visible_) return false;

    // SFML 按键代码：Num1=27, Num2=28, Num3=29
    // 也支持键盘顶部的 1/2/3 键
    int index = -1;
    if (key == 27 || key == 4) index = 0;      // Num1 or Key1
    else if (key == 28 || key == 5) index = 1;  // Num2 or Key2
    else if (key == 29 || key == 6) index = 2;  // Num3 or Key3

    if (index >= 0 && index < 3) {
        // 检查选项是否有效
        if (choices_[index].option.type == UpgradeType::Count) {
            LOG_WARN("选项 %d 无效，请选择有效选项", index + 1);
            return false;
        }
        selectedIndex_ = index;
        visible_ = false;
        LOG_INFO("已选择升级: [%d] %s",
                 selectedIndex_ + 1,
                 choices_[selectedIndex_].option.name.c_str());
        return true;
    }

    return false;
}

} // namespace cu