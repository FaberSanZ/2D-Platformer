#pragma once
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include "Model.h"
#include "RenderSystem.h"

class AssetSystem
{
public:
    void Initialize(RenderSystem* renderSystem)
    {
        m_renderSystem = renderSystem;
    }

    Model* LoadModel(const std::string& filePath)
    {
        if (auto it = m_models.find(filePath); it != m_models.end())
            return it->second.get();

        std::filesystem::path path = filePath;

        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None)
            return nullptr;

        auto asset = m_parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::None);
        if (asset.error() != fastgltf::Error::None)
            return nullptr;

        auto model = std::make_unique<Model>();

        // Temporary: load only the first mesh and first primitive.
        if (asset->meshes.empty() || asset->meshes[0].primitives.empty())
            return nullptr;

        auto& gltfMesh = asset->meshes[0];
        auto& primitive = gltfMesh.primitives[0];

        std::vector<Vertex> vertices;

        auto positionIt = primitive.findAttribute("POSITION");
        if (positionIt == primitive.attributes.end())
            return nullptr;

        auto& positionAccessor = asset->accessors[positionIt->accessorIndex];
        vertices.resize(positionAccessor.count);

        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset.get(), positionAccessor, [&](fastgltf::math::fvec3 position, std::size_t index)
            {
                vertices[index].position = { position.x(), position.y(), position.z() };
                vertices[index].normal = { 0.0f, 0.0f, 0.0f };
                vertices[index].uv = { 0.0f, 0.0f };
            });

        std::vector<uint32_t> indices;

        if (!primitive.indicesAccessor.has_value())
            return nullptr;

        auto& indexAccessor = asset->accessors[primitive.indicesAccessor.value()];
        indices.resize(indexAccessor.count);

        fastgltf::iterateAccessorWithIndex<uint32_t>(asset.get(), indexAccessor, [&](uint32_t index, std::size_t i)
            {
                indices[i] = index;
            });

        MeshPart part{};
        part.vertex = m_renderSystem->CreateStructuredBuffer(sizeof(Vertex), static_cast<uint32_t>(vertices.size()));
        part.index = m_renderSystem->CreateIndexBuffer(indices.data(), sizeof(uint32_t), static_cast<uint32_t>(indices.size()));

        m_renderSystem->UpdateGpuData(part.vertex, vertices.data(), static_cast<uint32_t>(vertices.size()));

        Mesh mesh{};
        mesh.parts.push_back(std::move(part));

        model->meshes.push_back(std::move(mesh));

        Model* result = model.get();
        m_models.emplace(filePath, std::move(model));
        return result;
    }

private:
    fastgltf::Parser m_parser;
    RenderSystem* m_renderSystem = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Model>> m_models;
};