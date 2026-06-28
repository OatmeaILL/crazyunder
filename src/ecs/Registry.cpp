#include "ecs/Registry.h"
#include "utils/Logger.h"

namespace cu {

EntityId Registry::CreateEntity() {
    EntityId id;
    if (!freeIds_.empty()) {
        // 复用已回收的 ID
        id = freeIds_.back();
        freeIds_.pop_back();
    } else {
        // 分配新 ID
        id = static_cast<EntityId>(versions_.size());
        versions_.push_back(0);
        alive_.push_back(false);
    }
    alive_[id] = true;
    aliveEntities_.push_back(id);
    return id;
}

void Registry::DestroyEntity(EntityId id) {
    if (!IsAlive(id)) return;

    // 移除该实体的所有组件
    removeAllComponents(id);

    // 标记为不存活，递增版本号（使旧 Entity 引用失效）
    alive_[id] = false;
    ++versions_[id];

    // 从存活列表中移除（swap-remove，O(1)）
    auto it = std::find(aliveEntities_.begin(), aliveEntities_.end(), id);
    if (it != aliveEntities_.end()) {
        *it = aliveEntities_.back();
        aliveEntities_.pop_back();
    }

    // 回收 ID 供后续复用
    freeIds_.push_back(id);
}

bool Registry::IsAlive(EntityId id) const noexcept {
    if (id >= alive_.size()) return false;
    return alive_[id];
}

uint32_t Registry::GetVersion(EntityId id) const {
    if (id >= versions_.size()) return 0;
    return versions_[id];
}

Entity Registry::GetEntity(EntityId id) const {
    if (id >= versions_.size()) return Entity{};
    return Entity{ id, versions_[id] };
}

void Registry::DestroyEntitiesInRange(EntityId first, EntityId last) {
    // 批量销毁 [first, last) 范围内的存活实体
    // 用于帧末清理：系统在更新中收集待销毁实体，帧末统一调用
    for (EntityId id = first; id < last; ++id) {
        if (IsAlive(id)) {
            DestroyEntity(id);
        }
    }
}

void Registry::Clear() {
    versions_.clear();
    alive_.clear();
    freeIds_.clear();
    aliveEntities_.clear();
    pools_.clear();
    LOG_INFO("Registry 已清空：所有实体与组件已释放");
}

void Registry::removeAllComponents(EntityId id) {
    // 遍历所有组件池，移除该实体的组件
    for (auto& [ti, pool] : pools_) {
        pool->Remove(id);
    }
}

} // namespace cu
