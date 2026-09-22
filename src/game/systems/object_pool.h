#pragma once

// ObjectPool: 索引槽位对象池 — 高频生成/销毁实体的复用容器
//
// 设计要点:
//   1. 槽位用 deque 存储: push_back 不使既有元素的地址失效, 因此
//      acquire() 返回的 T* 跨扩容依然有效 (旧实现用 vector, 扩容后全部悬垂)
//   2. acquire/release 均 O(1); 回收槽位压入空闲栈复用, 不缩容
//   3. 回收即重置为默认状态, 杜绝上一次使用的残留数据
//   4. 相比 vector + erase(remove_if), 免去了每帧的 O(n) 元素搬移
//
// 用法:
//   ObjectPool<Projectile> pool{512};
//   pool.insert(proj);                                        // 取槽位并写入
//   pool.for_each([](const Projectile& p, int) { ... });      // 遍历存活项
//   pool.release_if([](const Projectile& p) { return !p.alive; });
//
// 约束: 不要在 for_each / release_if 回调内调用 acquire (迭代中不扩容)

#include <deque>
#include <vector>

namespace Game {

template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(int initial_capacity = 64) {
        _free.reserve(initial_capacity);
    }

    // 存活项数
    int  size() const { return _used; }
    bool empty() const { return _used == 0; }
    // 槽位总数 (含空闲): 越大说明复用越充分
    int  capacity() const { return (int)_slots.size(); }
    int  idle() const { return (int)_free.size(); }

    T& at(int index) { return _slots.at(index).value; }

    // 取一个槽位: 优先复用空闲槽位, 否则追加; 槽位值重置为默认状态
    T* acquire() {
        int index = acquire_index();
        return &_slots[index].value;
    }

    // 取槽位并写入值
    T* insert(const T& value) {
        int index = acquire_index();
        _slots[index].value = value;
        return &_slots[index].value;
    }

    // 回收槽位 (幂等; 越界忽略)
    void release(int index) {
        if (!in_range(index) || !_slots[index].in_use) return;
        _slots[index].in_use = false;
        _slots[index].value = T{};
        _free.push_back(index);
        --_used;
    }

    // 回收全部存活项 (不缩容, 槽位继续复用)
    void clear() {
        for (int index = 0; index < (int)_slots.size(); ++index)
            if (_slots[index].in_use) release(index);
    }

    // 遍历存活项, 回调签名为 fn(value, index)
    template <typename Fn>
    void for_each(Fn&& fn) const {
        for (int index = 0; index < (int)_slots.size(); ++index)
            if (_slots[index].in_use) fn(_slots[index].value, index);
    }
    template <typename Fn>
    void for_each(Fn&& fn) {
        for (int index = 0; index < (int)_slots.size(); ++index)
            if (_slots[index].in_use) fn(_slots[index].value, index);
    }

    // 回收所有满足条件的存活项, pred(value) 返回 true 则回收
    template <typename Pred>
    void release_if(Pred&& pred) {
        for (int index = 0; index < (int)_slots.size(); ++index)
            if (_slots[index].in_use && pred(_slots[index].value)) release(index);
    }

private:
    int acquire_index() {
        int index;
        if (_free.empty()) {
            index = (int)_slots.size();
            _slots.push_back(Slot{});
        } else {
            index = _free.back();
            _free.pop_back();
        }
        _slots[index].value = T{};
        _slots[index].in_use = true;
        ++_used;
        return index;
    }

    bool in_range(int index) const {
        return index >= 0 && index < (int)_slots.size();
    }

    struct Slot {
        T    value{};
        bool in_use = false;
    };

    std::deque<Slot> _slots;     // deque: push_back 不失效既有元素地址
    std::vector<int> _free;      // 空闲槽位索引栈
    int               _used = 0;
};

} // namespace Game
