#pragma once

// ============================================================================
// ObjectPool —— 模板对象池（仅头文件）
// ----------------------------------------------------------------------------
// 原理：
//   1. 预分配一大块连续内存（capacity 个槽位），避免运行期频繁 new/delete。
//   2. 自由链表（free_ 栈）记录可复用槽位下标；acquire() O(1) 弹出，
//      release() O(1) 压入。
//   3. 紧凑数组（active_）记录当前活跃对象的槽位下标，保证遍历时缓存友好，
//      活跃对象在内存中紧凑排列，减少 cache miss。
//   4. release() 通过在槽位中保存其在 active_ 中的下标（activeIndex），
//      实现 O(1) 的 swap-remove：将末尾元素移到被删位置，并更新其 activeIndex。
//
// 性能优势：
//   - 零堆分配（游戏循环内）：所有内存在构造时一次性分配。
//   - O(1) acquire / release。
//   - 紧凑遍历：活跃对象连续，对 CPU 缓存友好，适合每帧遍历大量子弹/粒子。
//
// 约束：
//   - 模板参数 T 要求默认可构造（acquire 时 placement new 默认构造）。
//   - Game Loop 内禁止扩容：容量耗尽时 acquire() 返回 nullptr。
// ============================================================================

#include <cstddef>
#include <vector>
#include <cstdint>
#include <cassert>
#include <new>
#include <iterator>

namespace cu {

template <typename T>
class ObjectPool {
public:
    // 显式构造：预分配 capacity 个槽位
    explicit ObjectPool(std::size_t capacity) {
        slots_.reserve(capacity);
        active_.reserve(capacity);
        free_.reserve(capacity);
        // 预分配槽位并按 LIFO 顺序入栈，acquire 时优先取末尾
        for (std::size_t i = 0; i < capacity; ++i) {
            slots_.push_back(Slot{});
            free_.push_back(capacity - 1 - i); // 0..capacity-1，末尾为 0
        }
    }

    ~ObjectPool() {
        // 析构所有活跃对象
        for (std::size_t idx : active_) {
            reinterpret_cast<T*>(slots_[idx].storage)->~T();
        }
    }

    // 禁止拷贝/移动：对象池持有大量资源，不应被意外复制
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;

    // 从自由链表获取一个槽位，默认构造对象。
    // 容量耗尽返回 nullptr（Game Loop 内禁止扩容）。
    [[nodiscard]] T* acquire() {
        if (free_.empty()) return nullptr;
        std::size_t idx = free_.back();
        free_.pop_back();
        // placement new：在预分配内存上默认构造
        T* obj = new (slots_[idx].storage) T{};
        slots_[idx].activeIndex = active_.size();
        active_.push_back(idx);
        return obj;
    }

    // 归还对象到自由链表。O(1) swap-remove。
    void release(T* obj) {
        if (obj == nullptr) return;
        // 通过字节偏移计算槽位下标（storage 位于 Slot 起始处）
        auto byteOffset = reinterpret_cast<unsigned char*>(obj) -
                          reinterpret_cast<unsigned char*>(slots_.data());
        std::size_t idx = static_cast<std::size_t>(byteOffset) / sizeof(Slot);
        assert(idx < slots_.size() && "release: 指针不属于本对象池");

        obj->~T(); // 析构对象

        // swap-remove：用 active_ 末尾元素填补被删位置
        std::size_t aIdx = slots_[idx].activeIndex;
        std::size_t lastIdx = active_.back();
        active_[aIdx] = lastIdx;
        slots_[lastIdx].activeIndex = aIdx;
        active_.pop_back();

        free_.push_back(idx); // 槽位回归自由链表
    }

    [[nodiscard]] std::size_t size() const noexcept { return active_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return slots_.size(); }
    [[nodiscard]] bool empty() const noexcept { return active_.empty(); }

    // ---- 迭代器：遍历活跃对象 ----
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        Iterator(ObjectPool* pool, std::size_t pos) : pool_(pool), pos_(pos) {}

        reference operator*() const {
            return *reinterpret_cast<T*>(pool_->slots_[pool_->active_[pos_]].storage);
        }
        pointer operator->() const {
            return reinterpret_cast<T*>(pool_->slots_[pool_->active_[pos_]].storage);
        }
        Iterator& operator++() { ++pos_; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++pos_; return tmp; }
        bool operator==(const Iterator& o) const { return pos_ == o.pos_; }
        bool operator!=(const Iterator& o) const { return pos_ != o.pos_; }

    private:
        ObjectPool* pool_;
        std::size_t pos_;
    };

    Iterator begin() { return Iterator(this, 0); }
    Iterator end()   { return Iterator(this, active_.size()); }

private:
    struct Slot {
        alignas(T) unsigned char storage[sizeof(T)]; // 对齐的原始内存
        std::size_t activeIndex = 0;                  // 在 active_ 中的下标，用于 O(1) release
    };

    std::vector<Slot> slots_;        // 预分配槽位
    std::vector<std::size_t> active_; // 活跃槽位下标（紧凑数组）
    std::vector<std::size_t> free_;   // 空闲槽位下标（自由链表栈）
};

} // namespace cu


// ============================================================================
// 单元测试（可选）：在 main.cpp 中定义 CU_OBJECTPOOL_ENABLE_TESTS 编译宏，
// 或通过 --test-objectpool 命令行参数调用 cu::RunObjectPoolTests()。
// ============================================================================
#ifdef CU_OBJECTPOOL_ENABLE_TESTS
#include <cstdio>
#include <vector>

namespace cu {

// 返回 true 表示全部通过
inline bool RunObjectPoolTests() {
    struct Particle { float x = 0, y = 0; int life = 0; };
    ObjectPool<Particle> pool(10000);

    // 1) 全部 acquire
    std::vector<Particle*> ptrs;
    ptrs.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        Particle* p = pool.acquire();
        if (!p) { std::fprintf(stderr, "[FAIL] acquire 返回 nullptr @ %d\n", i); return false; }
        p->x = float(i); p->y = float(i); p->life = i;
        ptrs.push_back(p);
    }
    if (pool.size() != 10000) { std::fprintf(stderr, "[FAIL] size=%zu != 10000\n", pool.size()); return false; }

    // 2) 容量耗尽应返回 nullptr
    if (pool.acquire() != nullptr) { std::fprintf(stderr, "[FAIL] 超容量未返回 nullptr\n"); return false; }

    // 3) 隔一个释放一个，验证 swap-remove 正确性
    for (int i = 0; i < 10000; i += 2) {
        pool.release(ptrs[i]);
        ptrs[i] = nullptr;
    }
    if (pool.size() != 5000) { std::fprintf(stderr, "[FAIL] 释放后 size=%zu != 5000\n", pool.size()); return false; }

    // 4) 遍历活跃对象，确保数量正确且可访问
    int count = 0;
    for (auto& p : pool) { (void)p; ++count; }
    if (count != 5000) { std::fprintf(stderr, "[FAIL] 遍历计数=%d != 5000\n", count); return false; }

    // 5) 重新 acquire 5000 个，应复用已释放槽位
    for (int i = 0; i < 5000; ++i) {
        Particle* p = pool.acquire();
        if (!p) { std::fprintf(stderr, "[FAIL] 复用 acquire 返回 nullptr\n"); return false; }
        ptrs[i * 2] = p;
    }
    if (pool.size() != 10000) { std::fprintf(stderr, "[FAIL] 复用后 size=%zu != 10000\n", pool.size()); return false; }

    // 6) 全部释放
    for (Particle* p : ptrs) {
        if (p) pool.release(p);
    }
    if (pool.size() != 0) { std::fprintf(stderr, "[FAIL] 全释放后 size=%zu != 0\n", pool.size()); return false; }

    std::printf("[OK] ObjectPool 单元测试通过（10000 次 acquire/release 无泄漏）\n");
    return true;
}

} // namespace cu
#endif // CU_OBJECTPOOL_ENABLE_TESTS
