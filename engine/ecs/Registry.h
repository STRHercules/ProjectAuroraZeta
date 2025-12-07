// Minimal ECS registry: entity lifecycle + component management.
#pragma once

#include <cstdint>
#include <queue>
#include <tuple>

#include "ComponentStorage.h"
#include "Entity.h"

namespace Engine::ECS {

class Registry {
public:
    Entity create() {
        Entity id;
        if (!freeList_.empty()) {
            id = freeList_.front();
            freeList_.pop();
        } else {
            id = ++lastIssued_;
        }
        return id;
    }

    void destroy(Entity e) {
        storage_.removeAll(e);
        freeList_.push(e);
    }

    template <typename T, typename... Args>
    T& emplace(Entity e, Args&&... args) {
        return storage_.template pool<T>().emplace(e, std::forward<Args>(args)...);
    }

    template <typename T>
    T* get(Entity e) {
        return storage_.template pool<T>().get(e);
    }

    template <typename T>
    const T* get(Entity e) const {
        const auto* p = storage_.template pool<T>();
        return p ? p->get(e) : nullptr;
    }

    template <typename T>
    bool has(Entity e) const {
        const auto* p = storage_.template pool<T>();
        return p && p->contains(e);
    }

    template <typename Primary, typename... Rest, typename Func>
    void view(Func&& func) {
        auto& primaryPool = storage_.template pool<Primary>();
        for (auto& [entity, primary] : primaryPool) {
            if ((storage_.template pool<Rest>().contains(entity) && ...)) {
                func(entity, primary, *storage_.template pool<Rest>().get(entity)...);
            }
        }
    }

    template <typename Primary, typename... Rest, typename Func>
    void view(Func&& func) const {
        const auto* primaryPool = storage_.template pool<Primary>();
        if (!primaryPool) {
            return;
        }
        for (const auto& [entity, primary] : *primaryPool) {
            bool allHave = ((storage_.template pool<Rest>() && storage_.template pool<Rest>()->contains(entity)) && ...);
            if (allHave) {
                func(entity, primary, *storage_.template pool<Rest>()->get(entity)...);
            }
        }
    }

    Entity lastIssued_{kInvalidEntity};
    std::queue<Entity> freeList_;
    ComponentStorage storage_;
};

}  // namespace Engine::ECS
