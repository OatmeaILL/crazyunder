#pragma once

// ============================================================================
// Registry —— ECS 组件注册表
// ----------------------------------------------------------------------------
// 核心职责：
//   1. 实体生命周期管理：创建、销毁、存活检测（含版本号防悬挂引用）。
//   2. 组件存储：每种组件类型一个 ComponentPool<T>（稀疏集合 Sparse Set）。
//   3. 查询接口：View<T...>() 返回拥有指定组件集合的实体列表。
//   4. 批量销毁：DestroyEntitiesInRange() 帧末批量清理，避免迭代中修改。
//
// ============================================================================
// 稀疏集合（Sparse Set）原理与性能优势：
// ============================================================================
//   每种组件类型 T 维护三个数组：
//     - dense_entities_ : vector<EntityId>  —— 拥有此组件的实体 ID（紧凑）
//     - dense_data_     : vector<T>         —— 对应的组件数据（与上面平行）
//     - sparse_         : unordered_map<EntityId, size_t> —— 实体→dense 下标映射
//
//   操作复杂度：
//     - AddComponent<T>：O(1) 均摊（push_back + map 插入）
//     - RemoveComponent<T>：O(1)（swap-remove：末尾元素填补删除位置）
//     - GetComponent<T>：O(1)（map 查找下标）
//     - 遍历 View<T>：O(N)，N=拥有 T 的实体数，紧凑数组缓存友好
//
//   为什么比 unordered_map<EntityId, T> 快？
//     - 遍历 unordered_map 时节点分散在堆上，cache miss 严重。
//     - 稀疏集合的 dense_data_ 是连续内存，顺序访问时 CPU 预取器能高效填充
//       缓存行，10000 实体遍历可 < 0.1ms。
//
//   View<T...>() 多组件查询：
//     选取拥有实体数最少的组件池（最小集合），遍历其实体并检查是否同时
//     拥有其他组件。例如 View<Transform, Sprite> 中若 Sprite 池较小，则
//     遍历 Sprite 池的实体，检查每个是否也有 Transform。
// ============================================================================

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <algorithm>
#include "ecs/Entity.h"

namespace cu {

// ----------------------------------------------------------------------------
// IComponentPool —— 组件池类型擦除基类
//   不同 T 的 ComponentPool<T> 需要统一存储在 Registry 的 map 中，
//   通过虚函数提供类型无关的 Remove/Has/Size 接口。
// ----------------------------------------------------------------------------
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void Remove(EntityId id) = 0;
    [[nodiscard]] virtual bool Has(EntityId id) const = 0;
    [[nodiscard]] virtual std::size_t Size() const noexcept = 0;
    [[nodiscard]] virtual const std::vector<EntityId>& GetEntities() const noexcept = 0;
};

// ----------------------------------------------------------------------------
// ComponentPool<T> —— 稀疏集合组件池（模板派生类）
// ----------------------------------------------------------------------------
template <typename T>
class ComponentPool : public IComponentPool {
public:
    // 添加/替换组件，返回引用
    T& Add(EntityId id, T component) {
        auto it = sparse_.find(id);
        if (it != sparse_.end()) {
            // 已存在：替换数据
            dense_data_[it->second] = std::move(component);
            return dense_data_[it->second];
        }
        // 新增：追加到 dense 数组，记录 sparse 映射
        std::size_t idx = dense_entities_.size();
        dense_entities_.push_back(id);
        dense_data_.push_back(std::move(component));
        sparse_[id] = idx;
        return dense_data_.back();
    }

    // 移除组件（swap-remove，O(1)）
    void Remove(EntityId id) override {
        auto it = sparse_.find(id);
        if (it == sparse_.end()) return;
        std::size_t idx = it->second;
        std::size_t last = dense_entities_.size() - 1;
        if (idx != last) {
            // 将末尾元素移到被删位置，更新其 sparse 映射
            dense_entities_[idx] = dense_entities_[last];
            dense_data_[idx] = std::move(dense_data_[last]);
            sparse_[dense_entities_[idx]] = idx;
        }
        dense_entities_.pop_back();
        dense_data_.pop_back();
        sparse_.erase(it);
    }

    [[nodiscard]] bool Has(EntityId id) const override {
        return sparse_.find(id) != sparse_.end();
    }

    [[nodiscard]] T* Get(EntityId id) {
        auto it = sparse_.find(id);
        if (it == sparse_.end()) return nullptr;
        return &dense_data_[it->second];
    }

    [[nodiscard]] const T* Get(EntityId id) const {
        auto it = sparse_.find(id);
        if (it == sparse_.end()) return nullptr;
        return &dense_data_[it->second];
    }

    [[nodiscard]] std::size_t Size() const noexcept override { return dense_data_.size(); }
    [[nodiscard]] const std::vector<EntityId>& GetEntities() const noexcept override { return dense_entities_; }

    // 直接访问 dense 数据数组（用于系统批量遍历）
    [[nodiscard]] std::vector<T>& GetData() noexcept { return dense_data_; }
    [[nodiscard]] const std::vector<T>& GetData() const noexcept { return dense_data_; }

private:
    std::vector<EntityId> dense_entities_;             // 紧凑实体 ID 数组
    std::vector<T> dense_data_;                        // 紧凑组件数据数组（与上面平行）
    std::unordered_map<EntityId, std::size_t> sparse_; // 实体→dense 下标
};

// ============================================================================
// Registry —— 组件注册表
// ============================================================================
class Registry {
public:
    Registry() = default;
    ~Registry() = default;

    // 禁止拷贝（持有大量资源）
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    // ---- 实体管理 ----

    // 创建新实体，返回 EntityId
    [[nodiscard]] EntityId CreateEntity();

    // 销毁实体（递增版本号，回收 ID，移除所有组件）
    void DestroyEntity(EntityId id);

    // 检查实体是否存活
    [[nodiscard]] bool IsAlive(EntityId id) const noexcept;

    // 获取实体的版本号（用于构造 Entity 引用）
    [[nodiscard]] uint32_t GetVersion(EntityId id) const;

    // 获取 Entity 引用（id + version）
    [[nodiscard]] Entity GetEntity(EntityId id) const;

    // 批量销毁 [first, last) 范围内的实体（帧末清理用）
    void DestroyEntitiesInRange(EntityId first, EntityId last);

    // 获取所有存活实体 ID
    [[nodiscard]] const std::vector<EntityId>& GetAliveEntities() const noexcept { return aliveEntities_; }

    // 获取存活实体数量
    [[nodiscard]] std::size_t GetEntityCount() const noexcept { return aliveEntities_.size(); }

    // 清空所有实体与组件（场景重置）
    void Clear();

    // ---- 组件管理 ----

    // 添加组件到实体（返回引用，可链式设置字段）
    template <typename T, typename... Args>
    T& AddComponent(EntityId id, Args&&... args) {
        return getPool<T>().Add(id, T{std::forward<Args>(args)...});
    }

    // 移除组件
    template <typename T>
    void RemoveComponent(EntityId id) {
        auto* pool = findPool<T>();
        if (pool) pool->Remove(id);
    }

    // 获取组件指针（不存在返回 nullptr）
    template <typename T>
    [[nodiscard]] T* GetComponent(EntityId id) {
        auto* pool = findPool<T>();
        return pool ? pool->Get(id) : nullptr;
    }

    template <typename T>
    [[nodiscard]] const T* GetComponent(EntityId id) const {
        auto* pool = findPool<T>();
        return pool ? pool->Get(id) : nullptr;
    }

    // 检查是否拥有组件
    template <typename T>
    [[nodiscard]] bool HasComponent(EntityId id) const {
        auto* pool = findPool<T>();
        return pool ? pool->Has(id) : false;
    }

    // 获取组件池（用于系统直接批量遍历 dense 数据）
    template <typename T>
    [[nodiscard]] ComponentPool<T>* GetPool() {
        return findPool<T>();
    }

    template <typename T>
    [[nodiscard]] const ComponentPool<T>* GetPool() const {
        return findPool<T>();
    }

    // ---- 查询：View<T...>() ----

    // 返回拥有所有指定组件的实体 ID 列表。
    // 选取最小的组件池遍历，检查其他组件是否存在。
    template <typename... Components>
    [[nodiscard]] std::vector<EntityId> View() {
        std::vector<EntityId> result;

        // 找到拥有实体数最少的组件池
        // getPool<Components>() 会自动创建不存在的池
        IComponentPool* smallest = nullptr;
        std::size_t minSize = SIZE_MAX;
        IComponentPool* pools[] = { static_cast<IComponentPool*>(&getPool<Components>())... };
        for (auto* p : pools) {
            if (p && p->Size() < minSize) {
                minSize = p->Size();
                smallest = p;
            }
        }
        if (!smallest) return result;

        // 遍历最小池的实体，检查是否拥有所有其他组件
        result.reserve(smallest->Size());
        for (EntityId id : smallest->GetEntities()) {
            bool hasAll = (poolsMatch<Components>(id) && ...);
            if (hasAll) result.push_back(id);
        }
        return result;
    }

    // 销毁实体时移除其所有组件（内部调用）
    void removeAllComponents(EntityId id);

private:
    // 获取或创建组件池
    template <typename T>
    [[nodiscard]] ComponentPool<T>& getPool() {
        auto ti = std::type_index(typeid(T));
        auto it = pools_.find(ti);
        if (it == pools_.end()) {
            auto pool = std::make_unique<ComponentPool<T>>();
            auto* ptr = pool.get();
            pools_[ti] = std::move(pool);
            return *ptr;
        }
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

    // 查找组件池（不创建）
    template <typename T>
    [[nodiscard]] ComponentPool<T>* findPool() {
        auto ti = std::type_index(typeid(T));
        auto it = pools_.find(ti);
        if (it == pools_.end()) return nullptr;
        return static_cast<ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    [[nodiscard]] const ComponentPool<T>* findPool() const {
        auto ti = std::type_index(typeid(T));
        auto it = pools_.find(ti);
        if (it == pools_.end()) return nullptr;
        return static_cast<const ComponentPool<T>*>(it->second.get());
    }

    // 辅助：检查实体是否拥有组件 T
    template <typename T>
    [[nodiscard]] bool poolsMatch(EntityId id) const {
        auto* pool = findPool<T>();
        return pool ? pool->Has(id) : false;
    }

private:
    // 实体版本号数组（下标 = EntityId）
    std::vector<uint32_t> versions_;
    // 存活标志数组（下标 = EntityId）
    std::vector<bool> alive_;
    // 已回收的空闲 EntityId 栈
    std::vector<EntityId> freeIds_;
    // 当前存活的实体 ID 列表（紧凑）
    std::vector<EntityId> aliveEntities_;

    // 组件池映射：type_index → 类型擦除的池
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools_;
};

} // namespace cu
