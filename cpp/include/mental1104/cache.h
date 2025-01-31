/*
 * @Date: 2025-01-29 21:02:07
 * @Author: mental1104 mental1104@gmail.com
 * @LastEditors: mental1104 mental1104@gmail.com
 * @LastEditTime: 2025-01-31 14:40:59
 */
#ifndef __MENTAL1104_CACHE
#define __MENTAL1104_CACHE

#include <iostream>
#include <unordered_map>
#include <list>
#include <tuple>
#include <functional>
// Hash function for tuple arguments
namespace std {
    template <typename... Args>
    struct hash<std::tuple<Args...>> {
        size_t operator()(const std::tuple<Args...>& t) const {
            return std::apply([](auto&&... args) {
                return (std::hash<std::decay_t<decltype(args)>>{}(args) ^ ...);
            }, t);
        }
    };
}


// Forward declaration of ContainerPrinter in the mental1104 namespace
namespace mental1104 {
    class ContainerPrinter;
}

template <typename Ret, typename... Args>
class LRUCache {
private:
    using KeyType = std::tuple<Args...>;
    using ListIt = typename std::list<std::pair<KeyType, Ret>>::iterator;
    
    size_t capacity;
    std::unordered_map<KeyType, ListIt> cache;
    std::list<std::pair<KeyType, Ret>> lru_list;
    std::function<Ret(Args...)> func;

public:
    explicit LRUCache(size_t cap, std::function<Ret(Args...)> f) : capacity(cap), func(f) {}

    Ret operator()(Args... args) {
        KeyType key = std::make_tuple(args...);
        
        auto it = cache.find(key);
        if (it != cache.end()) {
            lru_list.splice(lru_list.begin(), lru_list, it->second);
            return it->second->second;
        }
        
        Ret result = func(args...);
        
        lru_list.emplace_front(key, result);
        cache[key] = lru_list.begin();
        
        if (cache.size() > capacity) {
            cache.erase(lru_list.back().first);
            lru_list.pop_back();
        }
        
        return result;
    }

    // 声明ContainerPrinter为友元类
    friend class mental1104::ContainerPrinter;
};


// 使用完美转发来推导类型，支持 lambda 表达式
template <typename Ret, typename... Args, typename F>
auto make_lru_cache(size_t capacity, F&& func) {
    return LRUCache<Ret, Args...>(capacity, std::forward<F>(func));
}

// 新增的无限容量缓存装饰器
template <typename Ret, typename... Args, typename F>
auto make_cache(F&& func) {
    // 调用原来的 make_lru_cache 函数并传递一个超大容量
    const size_t unlimited_capacity = std::numeric_limits<size_t>::max(); // 最大容量
    return make_lru_cache<Ret, Args...>(unlimited_capacity, func);
}

#endif