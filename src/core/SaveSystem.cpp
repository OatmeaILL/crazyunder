// ============================================================================
// SaveSystem —— 3 槽位手动存档系统实现
// ============================================================================
#include "core/SaveSystem.h"
#include "utils/Logger.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <sys/stat.h>

namespace cu {

// ---- 路径与目录管理 ----

std::string SaveSystem::getSlotPath(int slot) {
    // saves/save_slot_N.dat（相对工作目录，与 settings.ini 一致）
    char buf[64];
    std::snprintf(buf, sizeof(buf), "saves/save_slot_%d.dat", slot);
    return std::string(buf);
}

void SaveSystem::ensureSavesDirectory() {
    std::error_code ec;
    std::filesystem::create_directories("saves", ec);
    // 忽略错误：目录已存在或创建失败（保存时会再次检查）
}

// ---- 基础二进制读写 ----

void SaveSystem::writePOD(std::ofstream& ofs, const void* data, std::streamsize size) {
    ofs.write(static_cast<const char*>(data), size);
}

bool SaveSystem::readPOD(std::ifstream& ifs, void* data, std::streamsize size) {
    ifs.read(static_cast<char*>(data), size);
    return static_cast<bool>(ifs);
}

// ---- 字符串：长度前缀 + 数据 ----

void SaveSystem::writeString(std::ofstream& ofs, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    writePOD(ofs, &len, sizeof(len));
    if (len > 0) ofs.write(s.data(), len);
}

bool SaveSystem::readString(std::ifstream& ifs, std::string& s) {
    uint32_t len = 0;
    if (!readPOD(ifs, &len, sizeof(len))) return false;
    if (len > 4096) return false; // 防止异常长度导致内存爆炸
    s.resize(len);
    if (len > 0) {
        if (!readPOD(ifs, s.data(), len)) return false;
    }
    return true;
}

// ---- 词缀：type(1B) + value(4B) + isPercent(1B) ----

void SaveSystem::writeAffix(std::ofstream& ofs, const Affix& affix) {
    uint8_t type = static_cast<uint8_t>(affix.type);
    writePOD(ofs, &type, sizeof(type));
    writePOD(ofs, &affix.value, sizeof(affix.value));
    uint8_t isPercent = affix.isPercent ? 1 : 0;
    writePOD(ofs, &isPercent, sizeof(isPercent));
}

bool SaveSystem::readAffix(std::ifstream& ifs, Affix& affix) {
    uint8_t type = 0;
    if (!readPOD(ifs, &type, sizeof(type))) return false;
    affix.type = static_cast<AffixType>(type);
    if (!readPOD(ifs, &affix.value, sizeof(affix.value))) return false;
    uint8_t isPercent = 0;
    if (!readPOD(ifs, &isPercent, sizeof(isPercent))) return false;
    affix.isPercent = (isPercent != 0);
    return true;
}

// ---- 物品：1B has_value 标记 + 完整 Item ----

void SaveSystem::writeItem(std::ofstream& ofs, const std::optional<Item>& item) {
    uint8_t hasValue = item.has_value() ? 1 : 0;
    writePOD(ofs, &hasValue, sizeof(hasValue));
    if (!item.has_value()) return;
    const Item& it = *item;
    uint8_t slot = static_cast<uint8_t>(it.slot);
    uint8_t quality = static_cast<uint8_t>(it.quality);
    writePOD(ofs, &slot, sizeof(slot));
    writePOD(ofs, &quality, sizeof(quality));
    writePOD(ofs, &it.ilvl, sizeof(it.ilvl));
    writeString(ofs, it.name);
    uint32_t affixCount = static_cast<uint32_t>(it.affixes.size());
    writePOD(ofs, &affixCount, sizeof(affixCount));
    for (const Affix& a : it.affixes) {
        writeAffix(ofs, a);
    }
    // 第二十三轮新增：写入套装 ID（1 字节）
    uint8_t setId = static_cast<uint8_t>(it.setId);
    writePOD(ofs, &setId, sizeof(setId));
}

bool SaveSystem::readItem(std::ifstream& ifs, std::optional<Item>& item) {
    uint8_t hasValue = 0;
    if (!readPOD(ifs, &hasValue, sizeof(hasValue))) return false;
    if (hasValue == 0) { item.reset(); return true; }
    Item it;
    uint8_t slot = 0, quality = 0;
    if (!readPOD(ifs, &slot, sizeof(slot))) return false;
    if (!readPOD(ifs, &quality, sizeof(quality))) return false;
    if (!readPOD(ifs, &it.ilvl, sizeof(it.ilvl))) return false;
    it.slot = static_cast<ItemSlot>(slot);
    it.quality = static_cast<ItemQuality>(quality);
    if (!readString(ifs, it.name)) return false;
    uint32_t affixCount = 0;
    if (!readPOD(ifs, &affixCount, sizeof(affixCount))) return false;
    if (affixCount > 16) return false; // 防止异常长度
    it.affixes.resize(affixCount);
    for (uint32_t i = 0; i < affixCount; ++i) {
        if (!readAffix(ifs, it.affixes[i])) return false;
    }
    // 第二十三轮新增：读取套装 ID（1 字节）
    // 防御：值 > Guardian 视为损坏，回退为 None
    uint8_t setId = 0;
    if (!readPOD(ifs, &setId, sizeof(setId))) return false;
    if (setId > static_cast<uint8_t>(EquipmentSet::Guardian)) {
        setId = 0; // None
    }
    it.setId = static_cast<EquipmentSet>(setId);
    item = std::move(it);
    return true;
}

// ---- 完整 SaveData 序列化 ----

void SaveSystem::writeSaveData(std::ofstream& ofs, const SaveData& data) const {
    // ---- 文件头：魔数 + 版本号 ----
    uint32_t magic = kMagicNumber;
    uint8_t version = kSaveVersion;
    writePOD(ofs, &magic, sizeof(magic));
    writePOD(ofs, &version, sizeof(version));

    // ---- 元信息 ----
    writePOD(ofs, &data.level, sizeof(data.level));
    writePOD(ofs, &data.kills, sizeof(data.kills));
    writePOD(ofs, &data.survivalTime, sizeof(data.survivalTime));

    // ---- 玩家状态 ----
    writePOD(ofs, &data.playerHp, sizeof(data.playerHp));
    writePOD(ofs, &data.coins, sizeof(data.coins));

    // ---- 升级系统 ----
    writePOD(ofs, &data.playerLevel, sizeof(data.playerLevel));
    writePOD(ofs, &data.exp, sizeof(data.exp));
    writePOD(ofs, &data.expToNext, sizeof(data.expToNext));
    writePOD(ofs, &data.skillPoints, sizeof(data.skillPoints));
    // 升级等级数组：写入元素个数（冗余但便于版本迁移）+ 数据
    uint32_t upgradeCount = static_cast<uint32_t>(data.upgradeLevels.size());
    writePOD(ofs, &upgradeCount, sizeof(upgradeCount));
    writePOD(ofs, data.upgradeLevels.data(),
             static_cast<std::streamsize>(upgradeCount * sizeof(int)));

    // ---- 装备系统 ----
    // 6 装备槽：slot(1B) + optional<Item>
    for (const EquipmentSlot& es : data.equipped) {
        uint8_t slot = static_cast<uint8_t>(es.slot);
        writePOD(ofs, &slot, sizeof(slot));
        writeItem(ofs, es.item);
    }
    // 25 格大背包：连续 25 个 optional<Item>
    for (const std::optional<Item>& it : data.backpack) {
        writeItem(ofs, it);
    }

    // ---- 技能系统 ----
    // 4 技能槽：type(1B) + cooldownRemain(4B) + level(4B)
    for (const SkillInstance& sk : data.skillSlots) {
        uint8_t type = static_cast<uint8_t>(sk.type);
        writePOD(ofs, &type, sizeof(type));
        writePOD(ofs, &sk.cooldownRemain, sizeof(sk.cooldownRemain));
        writePOD(ofs, &sk.level, sizeof(sk.level));
    }
    // 5 格技能背包：连续 5 个 type(1B)
    for (SkillType st : data.skillBackpack) {
        uint8_t type = static_cast<uint8_t>(st);
        writePOD(ofs, &type, sizeof(type));
    }

    // ---- 任务系统状态（5 个任务）----
    // 每个任务保存 state(1B) + currentProgress(4B) + timeAccumulator(4B)
    for (const auto& q : data.questStates) {
        writePOD(ofs, &q.state, sizeof(q.state));
        writePOD(ofs, &q.currentProgress, sizeof(q.currentProgress));
        writePOD(ofs, &q.timeAccumulator, sizeof(q.timeAccumulator));
    }

    // ---- 圣物系统（6 个圣物槽位，连续 6 字节）----
    writePOD(ofs, data.relicIds.data(),
             static_cast<std::streamsize>(data.relicIds.size() * sizeof(uint8_t)));

    // ---- 地牢变异系统（第十七轮新增，2 个修饰符槽位，连续 2 字节）----
    writePOD(ofs, data.floorModifierIds.data(),
             static_cast<std::streamsize>(data.floorModifierIds.size() * sizeof(uint8_t)));

    // ---- 时间戳 ----
    writePOD(ofs, &data.timestamp, sizeof(data.timestamp));
}

bool SaveSystem::readSaveData(std::ifstream& ifs, SaveData& data) const {
    // ---- 文件头 ----
    uint32_t magic = 0;
    if (!readPOD(ifs, &magic, sizeof(magic))) return false;
    if (magic != kMagicNumber) {
        LOG_WARN("存档文件魔数不匹配 (0x%08X)，可能已损坏", magic);
        return false;
    }
    uint8_t version = 0;
    if (!readPOD(ifs, &version, sizeof(version))) return false;
    if (version != kSaveVersion) {
        LOG_WARN("存档版本不兼容: 期望 %d 实际 %d", (int)kSaveVersion, (int)version);
        return false;
    }

    // ---- 元信息 ----
    if (!readPOD(ifs, &data.level, sizeof(data.level))) return false;
    if (!readPOD(ifs, &data.kills, sizeof(data.kills))) return false;
    if (!readPOD(ifs, &data.survivalTime, sizeof(data.survivalTime))) return false;

    // ---- 玩家状态 ----
    if (!readPOD(ifs, &data.playerHp, sizeof(data.playerHp))) return false;
    if (!readPOD(ifs, &data.coins, sizeof(data.coins))) return false;

    // ---- 升级系统 ----
    if (!readPOD(ifs, &data.playerLevel, sizeof(data.playerLevel))) return false;
    if (!readPOD(ifs, &data.exp, sizeof(data.exp))) return false;
    if (!readPOD(ifs, &data.expToNext, sizeof(data.expToNext))) return false;
    if (!readPOD(ifs, &data.skillPoints, sizeof(data.skillPoints))) return false;
    uint32_t upgradeCount = 0;
    if (!readPOD(ifs, &upgradeCount, sizeof(upgradeCount))) return false;
    if (upgradeCount != data.upgradeLevels.size()) {
        LOG_WARN("存档升级数组长度不匹配: 期望 %u 实际 %u",
                 (unsigned)data.upgradeLevels.size(), upgradeCount);
        return false;
    }
    if (!readPOD(ifs, data.upgradeLevels.data(),
                 static_cast<std::streamsize>(upgradeCount * sizeof(int)))) return false;

    // ---- 装备系统 ----
    for (EquipmentSlot& es : data.equipped) {
        uint8_t slot = 0;
        if (!readPOD(ifs, &slot, sizeof(slot))) return false;
        es.slot = static_cast<ItemSlot>(slot);
        if (!readItem(ifs, es.item)) return false;
    }
    for (std::optional<Item>& it : data.backpack) {
        if (!readItem(ifs, it)) return false;
    }

    // ---- 技能系统 ----
    for (SkillInstance& sk : data.skillSlots) {
        uint8_t type = 0;
        if (!readPOD(ifs, &type, sizeof(type))) return false;
        sk.type = static_cast<SkillType>(type);
        if (!readPOD(ifs, &sk.cooldownRemain, sizeof(sk.cooldownRemain))) return false;
        if (!readPOD(ifs, &sk.level, sizeof(sk.level))) return false;
    }
    for (SkillType& st : data.skillBackpack) {
        uint8_t type = 0;
        if (!readPOD(ifs, &type, sizeof(type))) return false;
        st = static_cast<SkillType>(type);
    }

    // ---- 任务系统状态（5 个任务）----
    for (auto& q : data.questStates) {
        if (!readPOD(ifs, &q.state, sizeof(q.state))) return false;
        if (!readPOD(ifs, &q.currentProgress, sizeof(q.currentProgress))) return false;
        if (!readPOD(ifs, &q.timeAccumulator, sizeof(q.timeAccumulator))) return false;
    }

    // ---- 圣物系统（6 个圣物槽位，连续 6 字节）----
    if (!readPOD(ifs, data.relicIds.data(),
                 static_cast<std::streamsize>(data.relicIds.size() * sizeof(uint8_t)))) return false;

    // ---- 地牢变异系统（第十七轮新增，2 个修饰符槽位，连续 2 字节）----
    if (!readPOD(ifs, data.floorModifierIds.data(),
                 static_cast<std::streamsize>(data.floorModifierIds.size() * sizeof(uint8_t)))) return false;

    // ---- 时间戳 ----
    if (!readPOD(ifs, &data.timestamp, sizeof(data.timestamp))) return false;

    // 全部字段读取成功
    return true;
}

// ---- 对外接口 ----

bool SaveSystem::SaveToSlot(int slot, const SaveData& data) {
    if (!IsValidSlot(slot)) {
        LOG_WARN("存档槽位无效: %d", slot);
        return false;
    }
    ensureSavesDirectory();
    const std::string path = getSlotPath(slot);
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        LOG_ERROR("无法打开存档文件写入: %s", path.c_str());
        return false;
    }
    // 强制 C locale，避免二进制浮点写入受 locale 影响
    ofs.imbue(std::locale::classic());
    writeSaveData(ofs, data);
    ofs.flush();
    if (!ofs.good()) {
        LOG_ERROR("存档写入失败: %s", path.c_str());
        return false;
    }
    LOG_INFO("存档已保存到槽位 %d (层数=%d 击杀=%d 时长=%.1fs)",
             slot, data.level, data.kills, data.survivalTime);
    return true;
}

bool SaveSystem::LoadFromSlot(int slot, SaveData& data) const {
    if (!IsValidSlot(slot)) return false;
    const std::string path = getSlotPath(slot);
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return false; // 文件不存在
    ifs.imbue(std::locale::classic());
    data = SaveData{}; // 重置为默认值
    if (!readSaveData(ifs, data)) {
        LOG_WARN("存档读取失败（格式损坏）: %s", path.c_str());
        return false;
    }
    LOG_INFO("存档已从槽位 %d 载入 (层数=%d 击杀=%d)",
             slot, data.level, data.kills);
    return true;
}

bool SaveSystem::DeleteSlot(int slot) const {
    if (!IsValidSlot(slot)) return false;
    const std::string path = getSlotPath(slot);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return true; // 不存在视为已删除
    return std::filesystem::remove(path, ec);
}

SaveSlotInfo SaveSystem::GetSlotInfo(int slot) const {
    SaveSlotInfo info;
    if (!IsValidSlot(slot)) return info;
    const std::string path = getSlotPath(slot);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return info; // 无存档

    // 完整读取（存档很小，开销可忽略）
    SaveData data;
    if (!LoadFromSlot(slot, data)) return info;
    info.exists = true;
    info.level = data.level;
    info.kills = data.kills;
    info.survivalTime = data.survivalTime;
    info.coins = data.coins;
    info.playerLevel = data.playerLevel;
    info.timestamp = data.timestamp;
    return info;
}

std::array<SaveSlotInfo, SaveSystem::kSlotCount> SaveSystem::GetAllSlotInfo() const {
    std::array<SaveSlotInfo, kSlotCount> result;
    for (int i = 0; i < kSlotCount; ++i) {
        result[i] = GetSlotInfo(i + 1);
    }
    return result;
}

} // namespace cu
