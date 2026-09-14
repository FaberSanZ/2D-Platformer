#pragma once

#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <entt/entt.hpp>
#include <yaml-cpp/yaml.h>
#include "Components.h"
#include "SceneSystem.h"
#include "AssetSystem.h"

class SceneSerializer
{
public:
    template<typename T>
    void RegisterTag(const std::string& name)
    {
        for (const TagBinding& tag : m_tags)
        {
            if (tag.name == name)
                return;
        }

        TagBinding tag{};

        tag.name = name;

        tag.has = [](const entt::registry& registry, entt::entity entity)
            {
                return registry.any_of<T>(entity);
            };

        tag.add = [](entt::registry& registry, entt::entity entity)
            {
                if (!registry.any_of<T>(entity))
                    registry.emplace<T>(entity);
            };

        tag.remove = [](entt::registry& registry, entt::entity entity)
            {
                if (registry.any_of<T>(entity))
                    registry.remove<T>(entity);
            };

        m_tags.push_back(std::move(tag));
    }


    size_t GetTagCount() const
    {
        return m_tags.size();
    }

    const std::string& GetTagName(size_t index) const
    {
        return m_tags[index].name;
    }

    bool HasTag(size_t index, const entt::registry& registry, entt::entity entity) const
    {
        if (index >= m_tags.size())
            return false;

        return m_tags[index].has(registry, entity);
    }

    void SetTag(size_t index, entt::registry& registry, entt::entity entity, bool enabled)
    {
        if (index >= m_tags.size())
            return;

        if (enabled)
            m_tags[index].add(registry, entity);
        else
            m_tags[index].remove(registry, entity);
    }

    bool Save(const Scene& scene, const std::string& filePath)
    {
        const entt::registry& registry = scene.registry;

        YAML::Emitter out;

        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << scene.name;
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        auto view = registry.view<const IDComponent, const NameComponent, const TransformComponent>();

        for (auto [entity, id, name, transform] : view.each())
        {
            out << YAML::BeginMap;

            out << YAML::Key << "Entity" << YAML::Value << id.id;
            out << YAML::Key << "Name" << YAML::Value << name.name;

            out << YAML::Key << "Transform" << YAML::Value << YAML::BeginMap;
            WriteFloat3(out, "Position", transform.position);
            WriteFloat4(out, "Rotation", transform.rotation);
            WriteFloat3(out, "Scale", transform.scale);
            out << YAML::EndMap;

            if (const ModelComponent* model = registry.try_get<ModelComponent>(entity))
            {
                out << YAML::Key << "Model" << YAML::Value << YAML::BeginMap;
                out << YAML::Key << "Asset" << YAML::Value << model->assetPath;
                WriteFloat4(out, "Color", model->color);
                out << YAML::EndMap;
            }

            if (const CameraComponent* camera = registry.try_get<CameraComponent>(entity))
            {
                out << YAML::Key << "Camera" << YAML::Value << YAML::BeginMap;
                out << YAML::Key << "FieldOfView" << YAML::Value << camera->fieldOfView;
                out << YAML::Key << "NearPlane" << YAML::Value << camera->nearPlane;
                out << YAML::Key << "FarPlane" << YAML::Value << camera->farPlane;
                out << YAML::Key << "Primary" << YAML::Value << registry.any_of<PrimaryCameraComponent>(entity);
                out << YAML::EndMap;
            }

            for (const TagBinding& tag : m_tags)
            {
                if (tag.has(registry, entity))
                    out << YAML::Key << tag.name << YAML::Value << true;
            }

            out << YAML::EndMap;
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream file(filePath);

        if (!file.is_open())
            return false;

        file << out.c_str();

        return true;
    }

    bool Load(Scene& scene, AssetSystem& assetSystem, const std::string& filePath)
    {
        YAML::Node data;

        try
        {
            data = YAML::LoadFile(filePath);
        }
        catch (const YAML::Exception&)
        {
            return false;
        }

        if (!data["Scene"] || !data["Entities"] || !data["Entities"].IsSequence())
            return false;

        entt::registry& registry = scene.registry;
        registry.clear();

        try
        {
            for (const YAML::Node& entityNode : data["Entities"])
            {
                if (!entityNode["Entity"] || !entityNode["Name"])
                    continue;

                EntityID id = entityNode["Entity"].as<EntityID>();
                std::string name = entityNode["Name"].as<std::string>();

                entt::entity entity = registry.create();

                registry.emplace<IDComponent>(entity, IDComponent{ id });
                registry.emplace<NameComponent>(entity, NameComponent{ name });
                registry.emplace<TransformComponent>(entity);

                if (YAML::Node transformNode = entityNode["Transform"])
                {
                    TransformComponent& transform = registry.get<TransformComponent>(entity);

                    ReadFloat3(transformNode["Position"], transform.position);
                    ReadFloat4(transformNode["Rotation"], transform.rotation);
                    ReadFloat3(transformNode["Scale"], transform.scale);
                }

                if (YAML::Node modelNode = entityNode["Model"])
                {
                    auto& model = registry.emplace<ModelComponent>(entity);

                    if (modelNode["Asset"])
                    {
                        model.assetPath = modelNode["Asset"].as<std::string>();
                        model.model = assetSystem.LoadModel(model.assetPath);
                    }

                    if (modelNode["Color"])
                        ReadFloat4(modelNode["Color"], model.color);
                }

                if (YAML::Node cameraNode = entityNode["Camera"])
                {
                    auto& camera = registry.emplace<CameraComponent>(entity);

                    if (cameraNode["FieldOfView"])
                        camera.fieldOfView = cameraNode["FieldOfView"].as<float>();

                    if (cameraNode["NearPlane"])
                        camera.nearPlane = cameraNode["NearPlane"].as<float>();

                    if (cameraNode["FarPlane"])
                        camera.farPlane = cameraNode["FarPlane"].as<float>();

                    if (cameraNode["Primary"] && cameraNode["Primary"].as<bool>())
                        registry.emplace<PrimaryCameraComponent>(entity);
                }

                for (const TagBinding& tag : m_tags)
                {
                    YAML::Node tagNode = entityNode[tag.name];

                    if (tagNode && tagNode.as<bool>())
                        tag.add(registry, entity);
                }
            }
        }
        catch (const YAML::Exception&)
        {
            registry.clear();
            return false;
        }

        return true;
    }

private:
    struct TagBinding
    {
        std::string name;
        std::function<bool(const entt::registry&, entt::entity)> has;
        std::function<void(entt::registry&, entt::entity)> add;
        std::function<void(entt::registry&, entt::entity)> remove;
    };

    std::vector<TagBinding> m_tags;

    static void WriteFloat3(YAML::Emitter& out, const char* name, const DirectX::XMFLOAT3& value)
    {
        out << YAML::Key << name << YAML::Value << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
    }

    static void WriteFloat4(YAML::Emitter& out, const char* name, const DirectX::XMFLOAT4& value)
    {
        out << YAML::Key << name << YAML::Value << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << value.w << YAML::EndSeq;
    }

    static bool ReadFloat3(const YAML::Node& node, DirectX::XMFLOAT3& value)
    {
        if (!node || !node.IsSequence() || node.size() != 3)
            return false;

        value.x = node[0].as<float>();
        value.y = node[1].as<float>();
        value.z = node[2].as<float>();

        return true;
    }

    static bool ReadFloat4(const YAML::Node& node, DirectX::XMFLOAT4& value)
    {
        if (!node || !node.IsSequence() || node.size() != 4)
            return false;

        value.x = node[0].as<float>();
        value.y = node[1].as<float>();
        value.z = node[2].as<float>();
        value.w = node[3].as<float>();

        return true;
    }
};
