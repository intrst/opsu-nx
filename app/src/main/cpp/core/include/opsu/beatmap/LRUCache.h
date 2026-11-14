#pragma once
#include <cstddef>
#include <iterator>
#include <list>
#include <unordered_map>
#include <utility>

namespace opsu {

/**
 * Least-recently-used cache with a fixed capacity.
 * Port of LRUCache.java (a LinkedHashMap in access order): both get() and put() mark
 * an entry as most recently used; contains() does not.
 */
template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}
    virtual ~LRUCache() = default;

    /** Returns a pointer to the value (marking it most recently used), or nullptr. */
    V* get(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return nullptr;
        order_.splice(order_.end(), order_, it->second);
        return &it->second->second;
    }

    /** Inserts or replaces a value, evicting the eldest entry if over capacity. */
    void put(const K& key, V value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = std::move(value);
            order_.splice(order_.end(), order_, it->second);
            return;
        }
        order_.emplace_back(key, std::move(value));
        map_[key] = std::prev(order_.end());
        if (order_.size() > capacity_) {
            auto eldest = order_.begin();
            eldestRemoved(eldest->first, eldest->second);
            map_.erase(eldest->first);
            order_.erase(eldest);
        }
    }

    bool contains(const K& key) const { return map_.count(key) != 0; }
    bool erase(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        order_.erase(it->second);
        map_.erase(it);
        return true;
    }
    void clear() { map_.clear(); order_.clear(); }
    size_t size() const { return order_.size(); }
    size_t capacity() const { return capacity_; }

protected:
    /** Called just before the eldest entry is dropped; override to release resources. */
    virtual void eldestRemoved(const K& /*key*/, V& /*value*/) {}

private:
    using List = std::list<std::pair<K, V>>;
    size_t capacity_;
    List order_;  // front = eldest, back = most recently used
    std::unordered_map<K, typename List::iterator> map_;
};

}  // namespace opsu
