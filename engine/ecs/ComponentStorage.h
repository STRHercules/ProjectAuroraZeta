// Type-erased component storage with per-type pools.
#pragma once

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include "Entity.h"

namespace Engine::ECS {

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void remove(Entity e) = 0;
    virtual bool contains(Entity e) const = 0;
};

template <typename T>
class ComponentPool : public IComponentPool {
public:
    template <typename... Args>
    T& emplace(Entity e, Args&&... args) {
        auto [it, inserted] = components_.try_emplace(e, std::forward<Args>(args)...);
        if (!inserted) {
            it->second = T{std::forward<Args>(args)...};
        }
        return it->second;
    }

    T* get(Entity e) {
        auto it = components_.find(e);
        return it != components_.end() ? &it->second : nullptr;
    }

    const T* get(Entity e) const {
        auto it = components_.find(e);
        return it != components_.end() ? &it->second : nullptr;
    }

    void remove(Entity e) override { components_.erase(e); }

    bool contains(Entity e) const override { return components_.count(e) > 0; }

    auto begin() { return components_.begin(); }
    auto end() { return components_.end(); }
    auto begin() const { return components_.begin(); }
    auto end() const { return components_.end(); }

private:
    std::unordered_map<Entity, T> components_;
};

class ComponentStorage {
public:
    template <typename T>
    ComponentPool<T>& pool() {
        const std::type_index key = typeid(T);
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            auto poolPtr = std::make_unique<ComponentPool<T>>();
            auto* raw = poolPtr.get();
            pools_.emplace(key, std::move(poolPtr));
            return *raw;
        }
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    const ComponentPool<T>* pool() const {
        const std::type_index key = typeid(T);
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            return nullptr;
        }
        return static_cast<const ComponentPool<T>*>(it->second.get());
    }

    void removeAll(Entity e) {
        for (auto& [_, pool] : pools_) {
            pool->remove(e);
        }
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools_;
};

}  // namespace Engine::ECS
