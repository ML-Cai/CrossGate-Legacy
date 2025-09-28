// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include <any>

namespace cgl {

using Entity = uint32_t;

constexpr Entity FIRST_ENTITY_ID = 1;

class ECSCore final {
public:
    ECSCore() : nextEntityId_{cgl::FIRST_ENTITY_ID} {

    }

    ~ECSCore() = default;

    Entity createEntity() {
        Entity e = nextEntityId_++;
        entities_.insert(e);
        return e;
    }

    void destroyEntity(Entity e) {
        entities_.erase(e);
        for (auto& [type, storage] : componentStores_) {
            storage.erase(e);
        }
    }

    template<typename T, typename... Args>
    void addComponent(Entity e, Args&&... args) {
        auto& storage = componentStores_[typeid(T)];
        storage[e] = std::make_any<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    void addComponent(Entity e, T value) {
        auto& storage = componentStores_[typeid(T)];
        storage[e] = std::make_any<T>(std::move(value));
    }

    template<typename T>
    T* getComponent(Entity e = cgl::FIRST_ENTITY_ID) {
        auto& storage = componentStores_[typeid(T)];
        return std::any_cast<T>(&storage.at(e));
    }

    template<typename... Components, typename Func>
    void gorEach(Func func) {
        for (auto e : entities_) {
            if ((hasComponent<Components>(e) && ...)) {
                func(e, getComponent<Components>(e)...);
            }
        }
    }

    template<typename T, typename... Args>
    Entity addSingleton(Args&&... args) {
        // 1. Check for existing singleton and return if found
        auto type_index = std::type_index(typeid(T));
        if (singletonEntityMap_.count(type_index)) {
            return singletonEntityMap_.at(type_index);
        }

        // 2. CREATE the dedicated entity *internally*
        Entity singleton_entity = createEntity();
        
        // 3. Add the component, passing only the constructor arguments
        // This calls the correct addComponent(Entity e, Args&&... args)
        addComponent<T>(singleton_entity, std::forward<Args>(args)...); 

        // 4. Record the mapping
        singletonEntityMap_[type_index] = singleton_entity;
        
        return singleton_entity;
    }

    template<typename T>
    T* getSingleton() {
        auto type_index = std::type_index(typeid(T));

        auto it = singletonEntityMap_.find(type_index);
        if (it == singletonEntityMap_.end()) {
            return nullptr;
        }

        Entity singleton_id = it->second;
        return getComponent<T>(singleton_id);
    }

private:
    template<typename T>
    bool hasComponent(Entity e) {
        auto it = componentStores_.find(typeid(T));
        if (it == componentStores_.end()) return false;
        return it->second.find(e) != it->second.end();
    }

    Entity nextEntityId_;
    std::unordered_set<Entity> entities_;
    std::unordered_map<std::type_index,
        std::unordered_map<Entity, std::any> > componentStores_;

    std::unordered_map<std::type_index, Entity> singletonEntityMap_;
};

}   // namespace cgl
