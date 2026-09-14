#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <entt/entt.hpp>

struct Scene
{
    std::string name;
    entt::registry registry;
};

class SceneSystem
{
public:
    Scene& CreateScene(const std::string& name)
    {
        auto it = m_scenes.find(name);

        if (it != m_scenes.end())
            return *it->second;

        auto scene = std::make_unique<Scene>();
        scene->name = name;

        Scene* result = scene.get();

        m_scenes.emplace(name, std::move(scene));

        if (!m_activeScene)
            m_activeScene = result;

        return *result;
    }

    bool DestroyScene(const std::string& name)
    {
        auto it = m_scenes.find(name);

        if (it == m_scenes.end())
            return false;

        bool wasActive = m_activeScene == it->second.get();

        m_scenes.erase(it);

        if (wasActive)
            m_activeScene = m_scenes.empty() ? nullptr : m_scenes.begin()->second.get();

        return true;
    }

    Scene* GetScene(const std::string& name)
    {
        auto it = m_scenes.find(name);
        return it != m_scenes.end() ? it->second.get() : nullptr;
    }

    const Scene* GetScene(const std::string& name) const
    {
        auto it = m_scenes.find(name);
        return it != m_scenes.end() ? it->second.get() : nullptr;
    }

    bool SetActiveScene(const std::string& name)
    {
        Scene* scene = GetScene(name);

        if (!scene)
            return false;

        m_activeScene = scene;
        return true;
    }

    Scene* GetActiveScene()
    {
        return m_activeScene;
    }

    const Scene* GetActiveScene() const
    {
        return m_activeScene;
    }

private:
    std::unordered_map<std::string, std::unique_ptr<Scene>> m_scenes;
    Scene* m_activeScene = nullptr;
};
