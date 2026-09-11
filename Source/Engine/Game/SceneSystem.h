#pragma once

#include <string>
#include <entt/entt.hpp>
#include "Components.h"

class SceneSystem
{
public:
    entt::entity CreateEntity(entt::registry& registry, const std::string& name)
    {
        return CreateEntity(registry, name, GenerateEntityID());
    }

    entt::entity CreateEntity(entt::registry& registry, const std::string& name, EntityID id)
    {
        entt::entity entity = registry.create();

        registry.emplace<IDComponent>(entity, IDComponent{ id });
        registry.emplace<NameComponent>(entity, NameComponent{ name });
        registry.emplace<TransformComponent>(entity);

        return entity;
    }

    void DestroyEntity(entt::registry& registry, entt::entity entity)
    {
        if (registry.valid(entity))
            registry.destroy(entity);
    }

    void Clear(entt::registry& registry)
    {
        registry.clear();
    }

    entt::entity FindEntity(entt::registry& registry, EntityID id)
    {
        auto view = registry.view<IDComponent>();

        for (auto [entity, component] : view.each())
        {
            if (component.id == id)
                return entity;
        }

        return entt::null;
    }
};