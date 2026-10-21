// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <assert.h>
#include <cstdint>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include <any>
#include <vector>
#include <tuple>

namespace cgl {

using Entity = uint32_t;

constexpr Entity FIRST_ENTITY_ID = 1;

struct IComponentStorage {
    virtual ~IComponentStorage() = default;
    virtual void erase(Entity e) = 0;
};

/// ----
/// Generate type id, prevent std::type_index
/// ----
inline size_t GenerateTypeId() noexcept {
    static std::atomic<size_t> counter{0};
    return counter++;
}

template <typename T>
inline size_t TypeId() noexcept {
    static const size_t id = GenerateTypeId();
    return id;
}

/// ----
/// SparseSet-based ComponentStorage
/// ----
template<typename T>
class ComponentStorage : public IComponentStorage {
public:
    T& emplace(Entity e, T&& value) {
        assert(!has(e));
        dense_.push_back(std::move(value));
        entities_.push_back(e);
        sparse_[e] = dense_.size() - 1;
        return dense_.back();
    }

    template<typename... Args>
    T& emplace(Entity e, Args&&... args) {
        assert(!has(e));
        dense_.emplace_back(std::forward<Args>(args)...);
        entities_.push_back(e);
        sparse_[e] = dense_.size() - 1;
        return dense_.back();
    }

    T* get(Entity e) {
        auto it = sparse_.find(e);
        return (it != sparse_.end()) ? &dense_[it->second] : nullptr;
    }

    bool has(Entity e) const {
        return sparse_.find(e) != sparse_.end();
    }

    void erase(Entity e) override {
        auto it = sparse_.find(e);
        if (it == sparse_.end()) return;

        size_t index = it->second;
        size_t last = dense_.size() - 1;

        if (index != last) {
            dense_[index] = std::move(dense_[last]);
            entities_[index] = entities_[last];
            sparse_[entities_[index]] = index;
        }

        dense_.pop_back();
        entities_.pop_back();
        sparse_.erase(it);
    }

    const std::vector<Entity>& entities() const noexcept { return entities_; }

    T& getByIndex(size_t i) noexcept { return dense_[i]; }

private:
    std::unordered_map<Entity, size_t> sparse_; // entity -> index
    std::vector<Entity> entities_;
    std::vector<T> dense_;
};


/// ----
/// ECSCore
/// ----
class ECSCore final {
public:
    Entity createEntity() {
        Entity e = nextEntityId_++;
        entities_.insert(e);
        return e;
    }

    void destroyEntity(Entity e) {
        entities_.erase(e);
        for (auto it = singletonEntityMap_.begin(); it != singletonEntityMap_.end();) {
            if (it->second == e) it = singletonEntityMap_.erase(it);
            else ++it;
        }
        for (auto& [_, storage] : componentStores_) {
            storage->erase(e);
        }
    }

    template<typename T, typename... Args>
    void addComponent(Entity e, Args&&... args) {
        auto& storage = getOrCreateStorage<T>();
        storage.emplace(e, std::forward<Args>(args)...);
    }

    template<typename T>
    T* getComponent(Entity e) {
        auto it = componentStores_.find(cgl::TypeId<T>());
        if (it == componentStores_.end()) return nullptr;
        return static_cast<ComponentStorage<T>*>(it->second.get())->get(e);
    }

    template<typename T>
    bool hasComponent(Entity e) const {
        auto it = componentStores_.find(cgl::TypeId<T>());
        if (it == componentStores_.end()) return false;
        return static_cast<ComponentStorage<T>*>(it->second.get())->has(e);
    }

    // Component values may be updated in callbacks. Defer structural changes
    // (entity/component creation/removal) until after traversal; iteration order
    // is unspecified.
    template<typename... Components, typename Func>
    void forEach(Func&& func) {
        if constexpr (sizeof...(Components) == 1) {
            // single component
            using T = std::tuple_element_t<0, std::tuple<Components...>>;
            auto* storage = getStorage<T>();
            if (!storage) return;
            const auto& ents = storage->entities();
            for (size_t i = 0; i < ents.size(); ++i) {
                func(ents[i], &storage->getByIndex(i));
            }
        } else if constexpr (sizeof...(Components) == 0) {
            for (Entity e : entities_) func(e);
        } else {
            // Cache typed stores once and scan the smallest dense entity list.
            // Resolve each component once, instead of probing the world and then
            // repeating has/get lookups for every entity in unordered order.
            auto stores=std::tuple{getStorage<Components>()...};
            if (!std::apply([](auto*... s){return ((s!=nullptr)&&...);},stores)) return;
            const std::vector<Entity>* candidates=nullptr;
            auto consider=[&](auto* store) {
                if(!candidates || store->entities().size()<candidates->size()) candidates=&store->entities();
            };
            std::apply([&](auto*... s){(consider(s),...);},stores);
            for(Entity e:*candidates) {
                auto values=std::apply([&](auto*... s){return std::tuple{s->get(e)...};},stores);
                std::apply([&](auto*... c){if((c&&...)) func(e,c...);},values);
            }
        }
    }

    // ---- Singleton ----
    template<typename T, typename... Args>
    Entity addSingleton(Args&&... args) {
        if (singletonEntityMap_.count(cgl::TypeId<T>()))
            return singletonEntityMap_[cgl::TypeId<T>()];
        Entity e = createEntity();
        addComponent<T>(e, std::forward<Args>(args)...);
        singletonEntityMap_[cgl::TypeId<T>()] = e;
        return e;
    }

    template<typename T>
    T* getSingleton() {
        auto it = singletonEntityMap_.find(cgl::TypeId<T>());
        if (it == singletonEntityMap_.end()) return nullptr;
        return getComponent<T>(it->second);
    }

    template<typename T>
    void removeSingleton() {
        auto it = singletonEntityMap_.find(cgl::TypeId<T>());
        if (it == singletonEntityMap_.end()) return;

        Entity e = it->second;
        destroyEntity(e);  // clean all component
    }

private:
    template<typename T>
    ComponentStorage<T>& getOrCreateStorage() {
        size_t id = cgl::TypeId<T>();
        auto it = componentStores_.find(id);
        if (it == componentStores_.end()) {
            auto storage = std::make_unique<ComponentStorage<T>>();
            it = componentStores_.emplace(id, std::move(storage)).first;
        }
        return *static_cast<ComponentStorage<T>*>(it->second.get());
    }

    template<typename T>
    ComponentStorage<T>* getStorage() const {
        auto it = componentStores_.find(cgl::TypeId<T>());
        return (it != componentStores_.end())
            ? static_cast<ComponentStorage<T>*>(it->second.get())
            : nullptr;
    }

    Entity nextEntityId_{ FIRST_ENTITY_ID };
    std::unordered_set<Entity> entities_;
    std::unordered_map<size_t, std::unique_ptr<IComponentStorage>> componentStores_;
    std::unordered_map<size_t, Entity> singletonEntityMap_;
};


}   // namespace cgl
