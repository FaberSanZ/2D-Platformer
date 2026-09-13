#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <entt/entt.hpp>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include "Components.h"
#include "AnimationSystem.h"
#include "AssetSystem.h"
#include "SceneSystem.h"
#include "SceneSerializer.h"

enum class EditorSceneAction
{
    None,
    New,
    Load
};

class EditorSystem
{
public:
    void Initialize(HWND window, ID3D12Device* device, ID3D12CommandQueue* commandQueue)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap));

        ImGui_ImplWin32_Init(window);

        ImGui_ImplDX12_InitInfo initInfo{};
        initInfo.Device = device;
        initInfo.CommandQueue = commandQueue;
        initInfo.NumFramesInFlight = 2;
        initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
        initInfo.UserData = this;
        initInfo.SrvDescriptorHeap = m_srvDescriptorHeap;
        initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle)
            {
                EditorSystem* editor = static_cast<EditorSystem*>(info->UserData);
                *cpuHandle = editor->m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
                *gpuHandle = editor->m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
            };
        initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};

        ImGui_ImplDX12_Init(&initInfo);

        std::error_code error;
        std::filesystem::create_directories(m_sceneDirectory, error);
    }

    void BeginFrame()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    EditorSceneAction Draw(entt::registry& registry, AnimationSystem& animations, AssetSystem& assets, SceneSystem& sceneSystem, SceneSerializer& serializer)
    {
        ImGui::SetNextWindowSize(ImVec2(1100.0f, 620.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Soulslike Editor");

        EditorSceneAction sceneAction = DrawSceneToolbar(registry, assets, sceneSystem, serializer);

        ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        float frameRate = ImGui::GetIO().Framerate;
        float frameTime = frameRate > 0.0f ? 1000.0f / frameRate : 0.0f;

        ImGui::Text("%.3f ms", frameTime);
        ImGui::Separator();

        if (ImGui::BeginTable("EditorLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Scenes", ImGuiTableColumnFlags_WidthFixed, 210.0f);
            ImGui::TableSetupColumn("Hierarchy", ImGuiTableColumnFlags_WidthFixed, 230.0f);
            ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextColumn();

            EditorSceneAction browserAction = DrawSceneBrowser(registry, assets, sceneSystem, serializer);

            if (browserAction != EditorSceneAction::None)
                sceneAction = browserAction;

            ImGui::TableNextColumn();
            DrawHierarchy(registry, sceneSystem);

            ImGui::TableNextColumn();
            DrawInspector(registry, animations, assets, sceneSystem);

            ImGui::EndTable();
        }

        DrawNewScenePopup(registry, sceneSystem, serializer, sceneAction);

        ImGui::End();

        return sceneAction;
    }

    void EndFrame(ID3D12GraphicsCommandList* commandList)
    {
        ImGui::Render();

        ID3D12DescriptorHeap* heaps[] = { m_srvDescriptorHeap };
        commandList->SetDescriptorHeaps(1, heaps);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
    }

    void Destroy()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (m_srvDescriptorHeap)
        {
            m_srvDescriptorHeap->Release();
            m_srvDescriptorHeap = nullptr;
        }
    }

    bool WantsMouse() const
    {
        return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
    }

    bool WantsKeyboard() const
    {
        return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard;
    }

private:
    EditorSceneAction DrawSceneToolbar(entt::registry& registry, AssetSystem& assets, SceneSystem& sceneSystem, SceneSerializer& serializer)
    {
        EditorSceneAction action = EditorSceneAction::None;

        if (ImGui::Button("New Scene"))
        {
            m_newSceneNameBuffer[0] = '\0';
            ImGui::OpenPopup("New Scene");
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Scene"))
        {
            if (LoadCurrentScene(registry, assets, sceneSystem, serializer))
                action = EditorSceneAction::Load;
        }

        ImGui::SameLine();

        if (ImGui::Button("Save Scene"))
        {
            if (serializer.Save(registry, m_scenePath, m_sceneName))
                m_sceneStatus = "Scene saved";
            else
                m_sceneStatus = "Save failed";
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_sceneName.c_str());

        if (!m_sceneStatus.empty())
        {
            ImGui::SameLine();
            ImGui::Text("%s", m_sceneStatus.c_str());
        }

        ImGui::Separator();

        return action;
    }

    EditorSceneAction DrawSceneBrowser(entt::registry& registry, AssetSystem& assets, SceneSystem& sceneSystem, SceneSerializer& serializer)
    {
        ImGui::Text("Scenes");
        ImGui::Separator();

        std::vector<std::filesystem::path> scenes = GetSceneFiles();

        if (scenes.empty())
        {
            ImGui::TextDisabled("No scenes.");
            return EditorSceneAction::None;
        }

        for (const std::filesystem::path& path : scenes)
        {
            std::string fileName = path.filename().string();
            bool selected = std::filesystem::path(m_scenePath).lexically_normal() == path.lexically_normal();

            ImGui::PushID(fileName.c_str());

            if (ImGui::Selectable(fileName.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                m_scenePath = path.string();
                m_sceneName = path.stem().string();
                m_sceneStatus = "Scene selected";

                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    ImGui::PopID();

                    if (LoadCurrentScene(registry, assets, sceneSystem, serializer))
                        return EditorSceneAction::Load;

                    return EditorSceneAction::None;
                }
            }

            ImGui::PopID();
        }

        return EditorSceneAction::None;
    }

    void DrawNewScenePopup(entt::registry& registry, SceneSystem& sceneSystem, SceneSerializer& serializer, EditorSceneAction& sceneAction)
    {
        if (!ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        ImGui::Text("Scene Name");
        ImGui::InputText("##SceneName", m_newSceneNameBuffer, sizeof(m_newSceneNameBuffer));

        ImGui::Spacing();

        bool validName = m_newSceneNameBuffer[0] != '\0';

        if (!validName)
            ImGui::BeginDisabled();

        if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
        {
            m_sceneName = m_newSceneNameBuffer;

            std::filesystem::path scenePath = std::filesystem::path(m_sceneDirectory) / (m_sceneName + ".yaml");
            m_scenePath = scenePath.string();

            sceneSystem.Clear(registry);
            ClearSelection();

            if (serializer.Save(registry, m_scenePath, m_sceneName))
                m_sceneStatus = "Scene created";
            else
                m_sceneStatus = "Scene created, save failed";

            sceneAction = EditorSceneAction::New;

            ImGui::CloseCurrentPopup();
        }

        if (!validName)
            ImGui::EndDisabled();

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    bool LoadCurrentScene(entt::registry& registry, AssetSystem& assets, SceneSystem& sceneSystem, SceneSerializer& serializer)
    {
        if (m_scenePath.empty())
        {
            m_sceneStatus = "Select a scene";
            return false;
        }

        if (!serializer.Load(registry, sceneSystem, assets, m_scenePath))
        {
            m_sceneStatus = "Load failed";
            return false;
        }

        ClearSelection();

        m_sceneName = std::filesystem::path(m_scenePath).stem().string();
        m_sceneStatus = "Scene loaded";

        return true;
    }

    std::vector<std::filesystem::path> GetSceneFiles() const
    {
        std::vector<std::filesystem::path> scenes;

        std::error_code error;

        if (!std::filesystem::exists(m_sceneDirectory, error))
            return scenes;

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(m_sceneDirectory, error))
        {
            if (error)
                break;

            if (!entry.is_regular_file())
                continue;

            if (entry.path().extension() != ".yaml")
                continue;

            scenes.push_back(entry.path());
        }

        std::sort(scenes.begin(), scenes.end(), [](const std::filesystem::path& a, const std::filesystem::path& b)
            {
                return a.filename().string() < b.filename().string();
            });

        return scenes;
    }

    void DrawHierarchy(entt::registry& registry, SceneSystem& sceneSystem)
    {
        ImGui::Text("Hierarchy");
        ImGui::SameLine();

        if (ImGui::Button("+ Entity"))
        {
            entt::entity entity = sceneSystem.CreateEntity(registry, "Entity");
            SelectEntity(registry, entity);
        }

        ImGui::Separator();

        auto view = registry.view<IDComponent, NameComponent>();

        for (auto [entity, id, name] : view.each())
        {
            bool selected = entity == m_selectedEntity;

            ImGui::PushID(static_cast<int>(entt::to_integral(entity)));

            if (ImGui::Selectable(name.name.c_str(), selected))
                SelectEntity(registry, entity);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("ID: %llu", static_cast<unsigned long long>(id.id));

            ImGui::PopID();
        }
    }

    void DrawInspector(entt::registry& registry, AnimationSystem& animations, AssetSystem& assets, SceneSystem& sceneSystem)
    {
        ImGui::Text("Inspector");
        ImGui::Separator();

        if (m_selectedEntity == entt::null || !registry.valid(m_selectedEntity))
        {
            ImGui::TextDisabled("Select an entity.");
            return;
        }

        DrawEntity(registry, sceneSystem);

        if (m_selectedEntity == entt::null || !registry.valid(m_selectedEntity))
            return;

        DrawTransform(registry);
        DrawAddComponent(registry);
        DrawModel(registry, assets);
        DrawCamera(registry);
        DrawAnimation(registry, animations);
    }

    void DrawEntity(entt::registry& registry, SceneSystem& sceneSystem)
    {
        NameComponent* name = registry.try_get<NameComponent>(m_selectedEntity);
        IDComponent* id = registry.try_get<IDComponent>(m_selectedEntity);

        ImGui::SeparatorText("Entity");

        if (name && ImGui::InputText("Name", m_nameBuffer, sizeof(m_nameBuffer)))
            name->name = m_nameBuffer;

        if (id)
            ImGui::TextDisabled("ID: %llu", static_cast<unsigned long long>(id->id));

        ImGui::Spacing();

        if (ImGui::Button("Delete Entity"))
        {
            sceneSystem.DestroyEntity(registry, m_selectedEntity);
            ClearSelection();
        }
    }

    void DrawTransform(entt::registry& registry)
    {
        TransformComponent* transform = registry.try_get<TransformComponent>(m_selectedEntity);

        if (!transform)
            return;

        ImGui::SeparatorText("Transform");

        ImGui::DragFloat3("Position", &transform->position.x, 0.05f);
        ImGui::DragFloat4("Rotation", &transform->rotation.x, 0.01f);
        ImGui::DragFloat3("Scale", &transform->scale.x, 0.01f, 0.001f, 100.0f);
    }

    void DrawAddComponent(entt::registry& registry)
    {
        ImGui::Spacing();

        if (ImGui::Button("+ Add Component"))
            ImGui::OpenPopup("AddComponentPopup");

        if (!ImGui::BeginPopup("AddComponentPopup"))
            return;

        bool hasModel = registry.any_of<ModelComponent>(m_selectedEntity);
        bool hasCamera = registry.any_of<CameraComponent>(m_selectedEntity);

        if (!hasModel && ImGui::MenuItem("Model"))
        {
            registry.emplace<ModelComponent>(m_selectedEntity);
            m_modelPathBuffer[0] = '\0';
            m_modelStatus.clear();
        }

        if (!hasCamera && ImGui::MenuItem("Camera"))
            registry.emplace<CameraComponent>(m_selectedEntity);

        if (hasModel && hasCamera)
            ImGui::TextDisabled("No components available.");

        ImGui::EndPopup();
    }

    void DrawModel(entt::registry& registry, AssetSystem& assets)
    {
        ModelComponent* model = registry.try_get<ModelComponent>(m_selectedEntity);

        if (!model)
            return;

        ImGui::SeparatorText("Model");

        if (ImGui::InputText("Asset Path", m_modelPathBuffer, sizeof(m_modelPathBuffer)))
            model->assetPath = m_modelPathBuffer;

        if (ImGui::Button("Load Model"))
        {
            if (model->assetPath.empty())
            {
                m_modelStatus = "Asset path is empty";
            }
            else
            {
                model->model = assets.LoadModel(model->assetPath);
                m_modelStatus = model->model ? "Model loaded" : "Failed to load model";
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Remove Model"))
        {
            registry.remove<ModelComponent>(m_selectedEntity);
            m_modelPathBuffer[0] = '\0';
            m_modelStatus.clear();
            return;
        }

        if (!m_modelStatus.empty())
            ImGui::TextDisabled("%s", m_modelStatus.c_str());

        ImGui::ColorEdit4("Color", &model->color.x);
    }

    void DrawCamera(entt::registry& registry)
    {
        CameraComponent* camera = registry.try_get<CameraComponent>(m_selectedEntity);

        if (!camera)
            return;

        ImGui::SeparatorText("Camera");

        ImGui::DragFloat("Field Of View", &camera->fieldOfView, 0.25f, 1.0f, 179.0f);
        ImGui::DragFloat("Near Plane", &camera->nearPlane, 0.01f, 0.001f, 1000.0f);
        ImGui::DragFloat("Far Plane", &camera->farPlane, 1.0f, 0.01f, 100000.0f);

        bool primary = registry.any_of<PrimaryCameraComponent>(m_selectedEntity);

        if (ImGui::Checkbox("Primary Camera", &primary))
        {
            if (primary)
            {
                auto view = registry.view<PrimaryCameraComponent>();

                for (entt::entity entity : view)
                {
                    if (entity != m_selectedEntity)
                        registry.remove<PrimaryCameraComponent>(entity);
                }

                if (!registry.any_of<PrimaryCameraComponent>(m_selectedEntity))
                    registry.emplace<PrimaryCameraComponent>(m_selectedEntity);
            }
            else
            {
                registry.remove<PrimaryCameraComponent>(m_selectedEntity);
            }
        }

        if (ImGui::Button("Remove Camera"))
        {
            registry.remove<CameraComponent>(m_selectedEntity);

            if (registry.any_of<PrimaryCameraComponent>(m_selectedEntity))
                registry.remove<PrimaryCameraComponent>(m_selectedEntity);
        }
    }

    void DrawAnimation(entt::registry& registry, AnimationSystem& animations)
    {
        ModelComponent* modelComponent = registry.try_get<ModelComponent>(m_selectedEntity);

        if (!modelComponent || !modelComponent->model)
            return;

        Model& model = *modelComponent->model;

        if (model.animations.empty())
            return;

        ImGui::SeparatorText("Animation");

        if (m_selectedClip >= model.animations.size())
            m_selectedClip = 0;

        AnimationComponent* animation = registry.try_get<AnimationComponent>(m_selectedEntity);

        ImGui::Text("Clips: %u", static_cast<uint32_t>(model.animations.size()));

        if (animation && animation->animationIndex < model.animations.size())
            ImGui::Text("Current: %s", model.animations[animation->animationIndex].name.c_str());
        else
            ImGui::Text("Current: None");

        const char* preview = model.animations[m_selectedClip].name.c_str();

        if (ImGui::BeginCombo("Clip", preview))
        {
            for (uint32_t i = 0; i < model.animations.size(); ++i)
            {
                bool selected = i == m_selectedClip;

                if (ImGui::Selectable(model.animations[i].name.c_str(), selected))
                {
                    m_selectedClip = i;
                    animations.Play(registry, m_selectedEntity, model.animations[i].name, m_loop, m_blendDuration, true);
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        ImGui::Checkbox("Loop", &m_loop);
        ImGui::SliderFloat("Blend Duration", &m_blendDuration, 0.0f, 1.0f, "%.2f s");

        if (ImGui::Button("Play"))
            animations.Play(registry, m_selectedEntity, model.animations[m_selectedClip].name, m_loop, m_blendDuration, true);

        ImGui::SameLine();

        if (ImGui::Button("Stop"))
            animations.Stop(registry, m_selectedEntity);

        animation = registry.try_get<AnimationComponent>(m_selectedEntity);

        if (!animation || animation->animationIndex >= model.animations.size())
            return;

        const AnimationClip& clip = model.animations[animation->animationIndex];

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::Text("Playing: %s", animation->playing ? "Yes" : "No");
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("Finished: %s", animation->finished ? "Yes" : "No");

        ImGui::Text("Duration: %.3f s", clip.duration);

        float time = animation->time;

        if (ImGui::SliderFloat("Timeline", &time, 0.0f, clip.duration, "%.3f s"))
        {
            animation->time = time;
            animation->playing = false;
            animation->finished = false;
        }

        ImGui::Text("Pose Nodes: %u", static_cast<uint32_t>(animation->pose.size()));
    }

    void SelectEntity(entt::registry& registry, entt::entity entity)
    {
        m_selectedEntity = entity;
        m_selectedClip = 0;
        m_modelStatus.clear();

        if (NameComponent* name = registry.try_get<NameComponent>(entity))
            std::snprintf(m_nameBuffer, sizeof(m_nameBuffer), "%s", name->name.c_str());
        else
            m_nameBuffer[0] = '\0';

        if (ModelComponent* model = registry.try_get<ModelComponent>(entity))
            std::snprintf(m_modelPathBuffer, sizeof(m_modelPathBuffer), "%s", model->assetPath.c_str());
        else
            m_modelPathBuffer[0] = '\0';

        if (AnimationComponent* animation = registry.try_get<AnimationComponent>(entity))
            m_selectedClip = animation->animationIndex;
    }

    void ClearSelection()
    {
        m_selectedEntity = entt::null;
        m_selectedClip = 0;

        m_nameBuffer[0] = '\0';
        m_modelPathBuffer[0] = '\0';

        m_modelStatus.clear();
    }

private:
    ID3D12DescriptorHeap* m_srvDescriptorHeap = nullptr;

    entt::entity m_selectedEntity = entt::null;

    uint32_t m_selectedClip = 0;
    float m_blendDuration = 0.15f;
    bool m_loop = true;

    char m_nameBuffer[128]{};
    char m_modelPathBuffer[512]{};
    char m_newSceneNameBuffer[128]{};

    std::string m_modelStatus;

    std::string m_sceneDirectory = "../Assets/Scenes";
    std::string m_sceneName = "TestScene";
    std::string m_scenePath = "../Assets/Scenes/TestScene.yaml";
    std::string m_sceneStatus;
};
