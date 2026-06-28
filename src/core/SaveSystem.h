#pragma once

// ============================================================================
// SaveSystem —— 3 槽位手动存档系统
// ----------------------------------------------------------------------------
// 设计要点：
//   1. 二进制格式（紧凑、抗 locale 污染），文件头放魔数+版本号便于迁移
//   2. 强制 C locale（imbue classic），避免中文 Windows 千位分隔符污染
//   3. Item 含 std::string 与 std::vector<Affix>，逐字段写入长度前缀
//   4. std::optional 用 1 字节标记 has_value
//
// 存档时机：
//   - 主菜单"读取存档"：显示 3 槽位概要，选择载入
//   - 新游戏：选择槽位（覆盖已有存档）
//   - 暂停菜单"保存进度"：保存到当前槽位
//   - nextLevel 进入下一层：自动保存到当前槽位
//   - 死亡：存档保留（玩家可从主菜单读档重玩该层）
//
// 存档内容（保留存档时状态）：
//   - 元信息：层数、击杀数、存活时间
//   - 玩家状态：当前 HP、金币
//   - 升级系统：等级/经验/技能点/17 项升级等级
//   - 装备系统：6 装备槽 + 25 格大背包
//   - 技能系统：4 技能槽 + 5 格技能背包
//
// 不存档的内容（运行时状态）：
//   - 地牢种子（读档重新生成，更 Roguelike）
//   - 技能冷却/buff 计时器（瞬时战斗状态）
//   - 玩家实体 ID、Boss 实体 ID（重建时会变化）
//   - 当前房间清理状态、敌人列表
// ============================================================================

#include <array>
#include <string>
#include <cstdint>
#include <optional>
#include <fstream>
#include "gameplay/Player.h"
#include "gameplay/UpgradeSystem.h"
#include "gameplay/InventorySystem.h"
#include "gameplay/SkillSystem.h"
#include "gameplay/LootSystem.h"
#include "gameplay/RelicSystem.h"
#include "gameplay/FloorModifier.h"

namespace cu {

// ---- 存档槽位概要信息（用于菜单显示，轻量读取）----
struct SaveSlotInfo {
    bool exists = false;          // 槽位是否有存档
    int level = 1;                // 当前层数
    int kills = 0;                // 累计击杀数
    float survivalTime = 0.f;     // 存活时间（秒）
    int coins = 0;                // 金币数
    int playerLevel = 1;          // 玩家等级
    int64_t timestamp = 0;        // 存档时间戳（Unix 秒）
};

// ---- 完整存档数据 ----
struct SaveData {
    // ---- 元信息 ----
    int level = 1;                // 当前层数
    int kills = 0;                // 累计击杀数
    float survivalTime = 0.f;     // 存活时间（秒）

    // ---- 玩家状态 ----
    float playerHp = 100.f;       // 当前 HP（maxHp 由 recomputePlayerStats 重算）
    int coins = 0;                // 金币

    // ---- 升级系统（决定 PlayerStats 派生属性的核心源数据）----
    int playerLevel = 1;          // 玩家等级
    int exp = 0;                  // 当前经验
    int expToNext = 150;          // 升级所需经验
    int skillPoints = 0;          // 未使用技能点
    std::array<int, static_cast<size_t>(UpgradeType::Count)> upgradeLevels{};

    // ---- 装备系统 ----
    std::array<EquipmentSlot, 6> equipped{};
    std::array<std::optional<Item>, InventorySystem::kBackpackSize> backpack{};

    // ---- 技能系统 ----
    std::array<SkillInstance, kSkillSlotCount> skillSlots{};
    std::array<SkillType, kSkillBackpackSize> skillBackpack{};

    // ---- 任务系统状态（5 个主线任务的运行时进度）----
    // 简化序列化：每个任务保存 {state, currentProgress}，ID 由数组索引隐式确定
    // 数组大小固定为 5（与 QuestSystem 中预定义任务数一致）
    struct QuestSaveEntry {
        uint8_t state = 0;        // QuestState 枚举值
        int32_t currentProgress = 0;
        float timeAccumulator = 0.f;
    };
    std::array<QuestSaveEntry, 5> questStates{};

    // ---- 圣物系统（第十五轮新增）----
    // 6 个圣物槽位，每个保存 RelicType 枚举值（0=None 表示空槽）
    // 容器大小固定为 kRelicMaxCount=6，前 ownedCount 个有效
    std::array<uint8_t, kRelicMaxCount> relicIds{};

    // ---- 地牢变异系统（第十七轮新增）----
    // 当前层的修饰符 ID 列表（最多 2 个，0=None 表示空槽）
    // 跨层不保留——读档时重新生成地牢但应用保存时的修饰符
    std::array<uint8_t, kFloorModifierSlotCount> floorModifierIds{};

    // ---- 存档时间戳 ----
    int64_t timestamp = 0;
};

// ============================================================================
// SaveSystem  存档系统
// ============================================================================
class SaveSystem {
public:
    static constexpr int kSlotCount = 3;
    // 文件魔数 "CRAZ"（4 字节，便于识别与版本迁移）
    static constexpr uint32_t kMagicNumber = 0x4352415A; // 'C''R''A''Z'
    // 版本 3：新增地牢变异系统字段（floorModifierIds 数组，2 字节）
    // 版本 2：新增圣物系统字段（relicIds 数组，6 字节）
    // 旧版本 1 存档因格式不兼容将无法读取，需开始新游戏
    static constexpr uint8_t kSaveVersion = 4; // 第二十三轮升级：v3→v4，新增 Item.setId 字段

    // 保存到指定槽位（slot 为 1-based 索引：1, 2, 3）
    // 返回 true 表示保存成功
    [[nodiscard]] bool SaveToSlot(int slot, const SaveData& data);

    // 从指定槽位加载完整存档
    // 返回 true 表示加载成功（文件存在且格式正确）
    [[nodiscard]] bool LoadFromSlot(int slot, SaveData& data) const;

    // 删除指定槽位存档
    // 返回 true 表示删除成功（或文件本就不存在）
    bool DeleteSlot(int slot) const;

    // 获取槽位概要信息（用于菜单显示，仅读取头部元信息，轻量）
    [[nodiscard]] SaveSlotInfo GetSlotInfo(int slot) const;

    // 获取所有槽位概要
    [[nodiscard]] std::array<SaveSlotInfo, kSlotCount> GetAllSlotInfo() const;

    // 校验槽位索引有效性（1..kSlotCount）
    [[nodiscard]] static bool IsValidSlot(int slot) noexcept {
        return slot >= 1 && slot <= kSlotCount;
    }

private:
    // 获取槽位文件路径：saves/save_slot_N.dat
    [[nodiscard]] static std::string getSlotPath(int slot);

    // 确保 saves 目录存在
    static void ensureSavesDirectory();

    // ---- 序列化辅助（二进制写入）----
    static void writePOD(std::ofstream& ofs, const void* data, std::streamsize size);
    static bool readPOD(std::ifstream& ifs, void* data, std::streamsize size);

    // 字符串：先写长度（uint32），再写字符
    static void writeString(std::ofstream& ofs, const std::string& s);
    static bool readString(std::ifstream& ifs, std::string& s);

    // 词缀：type(1B) + value(4B) + isPercent(1B)
    static void writeAffix(std::ofstream& ofs, const Affix& affix);
    static bool readAffix(std::ifstream& ifs, Affix& affix);

    // 物品：1B has_value 标记，若存在则写完整 Item
    //   Item: slot(1B) + quality(1B) + ilvl(4B) + nameLen(4B) + name + affixCount(4B) + N×Affix
    static void writeItem(std::ofstream& ofs, const std::optional<Item>& item);
    static bool readItem(std::ifstream& ifs, std::optional<Item>& item);

    // 写入完整 SaveData 到已打开的流
    void writeSaveData(std::ofstream& ofs, const SaveData& data) const;
    // 从已打开的流读取完整 SaveData
    [[nodiscard]] bool readSaveData(std::ifstream& ifs, SaveData& data) const;
};

} // namespace cu
