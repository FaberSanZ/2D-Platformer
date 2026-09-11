#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
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

        auto asset = m_parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::DecomposeNodeMatrices);

        if (asset.error() != fastgltf::Error::None)
            return nullptr;

        auto model = std::make_unique<Model>();

        // Nodes.
        model->nodes.resize(asset->nodes.size());

        for (size_t i = 0; i < asset->nodes.size(); ++i)
        {
            const auto& gltfNode = asset->nodes[i];
            Node& node = model->nodes[i];

            const auto& trs = std::get<fastgltf::TRS>(gltfNode.transform);

            node.translation = { trs.translation.x(), trs.translation.y(), trs.translation.z() };
            node.rotation = { trs.rotation.x(), trs.rotation.y(), trs.rotation.z(), trs.rotation.w() };
            node.scale = { trs.scale.x(), trs.scale.y(), trs.scale.z() };

            node.baseTranslation = node.translation;
            node.baseRotation = node.rotation;
            node.baseScale = node.scale;

            if (gltfNode.meshIndex.has_value())
                node.meshIndex = static_cast<int32_t>(gltfNode.meshIndex.value());

            if (gltfNode.skinIndex.has_value())
                node.skinIndex = static_cast<int32_t>(gltfNode.skinIndex.value());

            for (size_t childIndex : gltfNode.children)
            {
                node.children.push_back(static_cast<uint32_t>(childIndex));
                model->nodes[childIndex].parent = static_cast<int32_t>(i);
            }
        }

        // Materials.
        model->materials.resize(asset->materials.size());

        for (size_t i = 0; i < asset->materials.size(); ++i)
        {
            const auto& gltfMaterial = asset->materials[i];
            const auto& color = gltfMaterial.pbrData.baseColorFactor;

            Material& material = model->materials[i];

            material.baseColor =
            {
                static_cast<float>(color[0]),
                static_cast<float>(color[1]),
                static_cast<float>(color[2]),
                static_cast<float>(color[3])
            };
        }

        // Skins.
        model->skins.resize(asset->skins.size());

        for (size_t i = 0; i < asset->skins.size(); ++i)
        {
            const auto& gltfSkin = asset->skins[i];
            Skin& skin = model->skins[i];

            skin.joints.reserve(gltfSkin.joints.size());

            for (size_t jointIndex : gltfSkin.joints)
                skin.joints.push_back(static_cast<uint32_t>(jointIndex));

            skin.inverseBindMatrices.resize(skin.joints.size());

            for (DirectX::XMFLOAT4X4& matrix : skin.inverseBindMatrices)
                DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixIdentity());

            if (gltfSkin.inverseBindMatrices.has_value())
            {
                const auto& accessor = asset->accessors[gltfSkin.inverseBindMatrices.value()];

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fmat4x4>(asset.get(), accessor, [&](fastgltf::math::fmat4x4 matrix, std::size_t index)
                    {
                        if (index >= skin.inverseBindMatrices.size())
                            return;

                        skin.inverseBindMatrices[index] =
                        {
                            matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3],
                            matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3],
                            matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3],
                            matrix[3][0], matrix[3][1], matrix[3][2], matrix[3][3]
                        };
                    });
            }
        }

        // Animations.
        model->animations.reserve(asset->animations.size());

        for (const auto& gltfAnimation : asset->animations)
        {
            AnimationClip clip{};

            clip.name = std::string(gltfAnimation.name);
            clip.samplers.resize(gltfAnimation.samplers.size());

            for (size_t samplerIndex = 0; samplerIndex < gltfAnimation.samplers.size(); ++samplerIndex)
            {
                const auto& gltfSampler = gltfAnimation.samplers[samplerIndex];
                AnimationSampler& sampler = clip.samplers[samplerIndex];

                if (gltfSampler.interpolation == fastgltf::AnimationInterpolation::Step)
                    sampler.interpolation = AnimationInterpolation::Step;
                else
                    sampler.interpolation = AnimationInterpolation::Linear;

                const auto& timeAccessor = asset->accessors[gltfSampler.inputAccessor];

                sampler.times.resize(timeAccessor.count);

                fastgltf::iterateAccessorWithIndex<float>(asset.get(), timeAccessor, [&](float time, std::size_t index)
                    {
                        sampler.times[index] = time;

                        if (time > clip.duration)
                            clip.duration = time;
                    });

                fastgltf::AnimationPath pathType = fastgltf::AnimationPath::Translation;

                for (const auto& channel : gltfAnimation.channels)
                {
                    if (channel.samplerIndex == samplerIndex)
                    {
                        pathType = channel.path;
                        break;
                    }
                }

                const auto& outputAccessor = asset->accessors[gltfSampler.outputAccessor];

                if (pathType == fastgltf::AnimationPath::Rotation)
                {
                    sampler.values.resize(outputAccessor.count);

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(asset.get(), outputAccessor, [&](fastgltf::math::fvec4 value, std::size_t index)
                        {
                            sampler.values[index] = { value.x(), value.y(), value.z(), value.w() };
                        });
                }
                else
                {
                    sampler.values.resize(outputAccessor.count);

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset.get(), outputAccessor, [&](fastgltf::math::fvec3 value, std::size_t index)
                        {
                            sampler.values[index] = { value.x(), value.y(), value.z(), 0.0f };
                        });
                }
            }

            for (const auto& gltfChannel : gltfAnimation.channels)
            {
                if (!gltfChannel.nodeIndex.has_value())
                    continue;

                if (gltfChannel.path == fastgltf::AnimationPath::Weights)
                    continue;

                AnimationChannel channel{};

                channel.samplerIndex = static_cast<uint32_t>(gltfChannel.samplerIndex);
                channel.nodeIndex = static_cast<uint32_t>(gltfChannel.nodeIndex.value());

                switch (gltfChannel.path)
                {
                case fastgltf::AnimationPath::Translation:
                    channel.path = AnimationPath::Translation;
                    break;

                case fastgltf::AnimationPath::Rotation:
                    channel.path = AnimationPath::Rotation;
                    break;

                case fastgltf::AnimationPath::Scale:
                    channel.path = AnimationPath::Scale;
                    break;

                default:
                    continue;
                }

                clip.channels.push_back(channel);
            }

            model->animations.push_back(std::move(clip));
        }

        // Associate glTF meshes with their nodes.
        std::vector<Node*> meshNodes(asset->meshes.size(), nullptr);

        for (size_t i = 0; i < asset->nodes.size(); ++i)
        {
            if (!asset->nodes[i].meshIndex.has_value())
                continue;

            size_t meshIndex = asset->nodes[i].meshIndex.value();

            if (meshIndex < meshNodes.size())
                meshNodes[meshIndex] = &model->nodes[i];
        }

        // Meshes.
        for (size_t meshIndex = 0; meshIndex < asset->meshes.size(); ++meshIndex)
        {
            const auto& gltfMesh = asset->meshes[meshIndex];

            Mesh mesh{};

            for (const auto& primitive : gltfMesh.primitives)
            {
                auto positionIt = primitive.findAttribute("POSITION");

                if (positionIt == primitive.attributes.end())
                    continue;

                const auto& positionAccessor = asset->accessors[positionIt->accessorIndex];

                std::vector<Vertex> vertices(positionAccessor.count);

                for (Vertex& vertex : vertices)
                {
                    vertex.position = { 0.0f, 0.0f, 0.0f };
                    vertex.normal = { 0.0f, 1.0f, 0.0f };
                    vertex.uv = { 0.0f, 0.0f };
                    vertex.joints = { 0, 0, 0, 0 };
                    vertex.weights = { 1.0f, 0.0f, 0.0f, 0.0f };
                }

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset.get(), positionAccessor, [&](fastgltf::math::fvec3 position, std::size_t index)
                    {
                        vertices[index].position = { position.x(), position.y(), position.z() };
                    });

                auto normalIt = primitive.findAttribute("NORMAL");

                if (normalIt != primitive.attributes.end())
                {
                    const auto& normalAccessor = asset->accessors[normalIt->accessorIndex];

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset.get(), normalAccessor, [&](fastgltf::math::fvec3 normal, std::size_t index)
                        {
                            vertices[index].normal = { normal.x(), normal.y(), normal.z() };
                        });
                }

                auto uvIt = primitive.findAttribute("TEXCOORD_0");

                if (uvIt != primitive.attributes.end())
                {
                    const auto& uvAccessor = asset->accessors[uvIt->accessorIndex];

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset.get(), uvAccessor, [&](fastgltf::math::fvec2 uv, std::size_t index)
                        {
                            vertices[index].uv = { uv.x(), uv.y() };
                        });
                }

                auto jointsIt = primitive.findAttribute("JOINTS_0");

                if (jointsIt != primitive.attributes.end())
                {
                    const auto& jointsAccessor = asset->accessors[jointsIt->accessorIndex];

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::uvec4>(asset.get(), jointsAccessor, [&](fastgltf::math::uvec4 joints, std::size_t index)
                        {
                            vertices[index].joints = { joints.x(), joints.y(), joints.z(), joints.w() };
                        });
                }

                auto weightsIt = primitive.findAttribute("WEIGHTS_0");

                if (weightsIt != primitive.attributes.end())
                {
                    const auto& weightsAccessor = asset->accessors[weightsIt->accessorIndex];

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(asset.get(), weightsAccessor, [&](fastgltf::math::fvec4 weights, std::size_t index)
                        {
                            vertices[index].weights = { weights.x(), weights.y(), weights.z(), weights.w() };
                        });
                }

                if (!primitive.indicesAccessor.has_value())
                    continue;

                const auto& indexAccessor = asset->accessors[primitive.indicesAccessor.value()];

                std::vector<uint32_t> indices(indexAccessor.count);

                fastgltf::iterateAccessorWithIndex<uint32_t>(asset.get(), indexAccessor, [&](uint32_t index, std::size_t indexPosition)
                    {
                        indices[indexPosition] = index;
                    });

                MeshPart part{};

                part.vertex = m_renderSystem->CreateStructuredBuffer(sizeof(Vertex), static_cast<uint32_t>(vertices.size()));
                part.index = m_renderSystem->CreateIndexBuffer(indices.data(), sizeof(uint32_t), static_cast<uint32_t>(indices.size()));

                if (meshIndex < meshNodes.size())
                    part.node = meshNodes[meshIndex];

                if (primitive.materialIndex.has_value())
                {
                    size_t materialIndex = primitive.materialIndex.value();

                    if (materialIndex < model->materials.size())
                        part.material = &model->materials[materialIndex];
                }

                m_renderSystem->UpdateGpuData(part.vertex, vertices.data(), static_cast<uint32_t>(vertices.size()));

                mesh.parts.push_back(std::move(part));
            }

            if (!mesh.parts.empty())
                model->meshes.push_back(std::move(mesh));
        }

        if (model->meshes.empty())
            return nullptr;

        std::cout << "Nodes: " << model->nodes.size() << std::endl;
        std::cout << "Materials: " << model->materials.size() << std::endl;
        std::cout << "Skins: " << model->skins.size() << std::endl;
        std::cout << "Animations: " << model->animations.size() << std::endl;

        for (const AnimationClip& animation : model->animations)
            std::cout << animation.name << " - " << animation.duration << " seconds" << std::endl;

        Model* result = model.get();

        m_models.emplace(filePath, std::move(model));

        return result;
    }

private:
    fastgltf::Parser m_parser;
    RenderSystem* m_renderSystem = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Model>> m_models;
};