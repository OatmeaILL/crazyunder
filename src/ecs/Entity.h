#pragma once

// ============================================================================
// Entity —— 实体 ID 与版本号系统
// ----------------------------------------------------------------------------
// 为什么需要版本号（Generation/Version）？
//   实体销毁后，其 EntityId 会被回收复用。若某处仍持有旧的 EntityId 引用，
//   不带版本号的话会误认为引用仍有效（指向了新实体）。版本号在每次销毁时
//   递增，Entity{id, version} 只有在 version 与 Registry 当前版本一致时才有效，
//   从而可靠检测悬挂引用。
//
// EntityId vs Entity：
//   - EntityId：纯 uint32_t 索引，用于 Registry 的快速查找（O(1) 数组下标）。
//   - Entity：{id, version} 二元组，用于长期持有引用（如 AI 的 target 字段）。
// ============================================================================

#include <cstdint>
#include <limits>
#include <functional>

namespace cu {

// 实体 ID 类型：uint32_t，足够索引 40 亿实体（实际远不需要）
using EntityId = uint32_t;

// 无效实体 ID：取 uint32_t 最大值作为哨兵
inline constexpr EntityId kInvalidEntity = std::numeric_limits<EntityId>::max();

// 实体引用：id + version，用于检测悬挂引用
struct Entity {
    EntityId id = kInvalidEntity;
    uint32_t version = 0; // 版本号，Registry 销毁实体时递增

    [[nodiscard]] bool IsValid() const noexcept { return id != kInvalidEntity; }

    [[nodiscard]] bool operator==(const Entity& o) const noexcept {
        return id == o.id && version == o.version;
    }
    [[nodiscard]] bool operator!=(const Entity& o) const noexcept {
        return !(*this == o);
    }
};

} // namespace cu

// ----------------------------------------------------------------------------
// Entity 哈希函数：用于 std::unordered_map<Entity, T>
// ----------------------------------------------------------------------------
namespace std {
template <>
struct hash<cu::Entity> {
    size_t operator()(const cu::Entity& e) const noexcept {
        // 组合 id 与 version 的哈希（boost::hash_combine 风格）
        size_t h1 = hash<cu::EntityId>{}(e.id);
        size_t h2 = hash<uint32_t>{}(e.version);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
} // namespace std
