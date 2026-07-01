#include "gameplay/ClassSystem.h"
#include <array>

namespace cu {

// ============================================================================
// 职业数据表
// ----------------------------------------------------------------------------
// 静态数组，索引 = PlayerClass 枚举值。
// 保证 GetClassData 是 O(1) 查表，无分支。
// ============================================================================
static const std::array<ClassData, static_cast<size_t>(PlayerClass::Count)> kClassDataTable = {{
    // ---- 法师 (Mage) ----
    {
        "法师",
        "远程弹幕输出\n高蓝量，均衡攻速\n擅长元素 build",
        "法杖",
        sf::Color(100, 120, 255),       // 蓝色主题
        sf::Color(255, 255, 100, 255),  // 黄白色子弹
        80.f,   // maxHp
        60.f,   // maxMp
        10.f,   // damage
        2.0f,   // attackSpeed
        195.f,  // moveSpeed
        0.12f,  // critChance
        1.4f,   // critDamage
        0.f     // defense
    },
    // ---- 剑士 (Warrior) ----
    {
        "剑士",
        "近战扇形斩击\n高生命，高爆伤\n擅长暴击 build",
        "长剑",
        sf::Color(255, 100, 60),        // 橙红色主题
        sf::Color(255, 180, 50, 255),   // 橙黄色斩击
        120.f,  // maxHp
        25.f,   // maxMp
        14.f,   // damage
        1.5f,   // attackSpeed
        205.f,  // moveSpeed
        0.08f,  // critChance
        1.6f,   // critDamage
        3.f     // defense
    },
}};

const ClassData& GetClassData(PlayerClass cls) noexcept {
    size_t idx = static_cast<size_t>(cls);
    if (idx < kClassDataTable.size()) {
        return kClassDataTable[idx];
    }
    // 回退到法师
    return kClassDataTable[0];
}

const char* GetClassName(PlayerClass cls) noexcept {
    return GetClassData(cls).name;
}

void ApplyClassBaseStats(PlayerStats& stats, PlayerClass cls) {
    const ClassData& cd = GetClassData(cls);
    stats.moveSpeed = cd.moveSpeed;
    stats.attackSpeed = cd.attackSpeed;
    stats.damage = cd.damage;
    stats.maxHp = cd.maxHp;
    stats.maxMp = cd.maxMp;
    stats.critChance = cd.critChance;
    stats.critDamage = cd.critDamage;
    stats.defense = cd.defense;
}

} // namespace cu
