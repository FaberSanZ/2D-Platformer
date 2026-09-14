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

class EditorSystem
{
public:
    void Initialize(HWND window, ID3D12Device* device, ID3D12CommandQueue* commandQueue)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsClassic();

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

    void Draw(AnimationSystem& animations, AssetSystem& assets, SceneSystem& sceneSystem, SceneSerializer& serializer)
    {
        HandleActiveSceneChange(sceneSystem);

        ImGui::SetNextWindowSize(ImVec2(1100.0f, 620.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Soulslike Editor");

        DrawSceneToolbar(assets, sceneSystem, serializer);
        HandleActiveSceneChange(sceneSystem);

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
            DrawSceneBrowser(assets, sceneSystem, serializer);
            HandleActiveSceneChange(sceneSystem);

            Scene* activeScene = sceneSystem.GetActiveScene();

            ImGui::TableNextColumn();

            if (activeScene)
                DrawHierarchy(activeScene->registry);
            else
                ImGui::TextDisabled("No active scene.");

            ImGui::TableNextColumn();

            if (activeScene)
                DrawInspector(activeScene->registry, animations, assets, serializer);
            else
                ImGui::TextDisabled("No active scene.");

            ImGui::EndTable();
        }

        DrawNewScenePopup(sceneSystem, serializer);

        ImGui::End();
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
    void DrawSceneToolbar(AssetSystem& assets, SceneSystem& sceneSystem, SceneSerializer& serializer)
    {
        if (ImGui::Button("New Scene"))
        {
            m_newSceneNameBuffer[0] = '\0';
            ImGui::OpenPopup("New Scene");
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Scene"))
            LoadSelectedScene(assets, sceneSystem, serializer);

        ImGui::SameLine();

        if (ImGui::Button("Save Scene"))
        {
            Scene* activeScene = sceneSystem.GetActiveScene();

            if (!activeScene)
            {
                m_sceneStatus = "No active scene";
            }
            else
            {
                std::filesystem::path scenePath = std::filesystem::path(m_sceneDirectory) / (activeScene->name + ".yaml");

                if (serializer.Save(*activeScene, scenePath.string()))
                {
                    m_selectedScenePath = scenePath.string();
                    m_sceneStatus = "Scene saved";
                }
                else
                {
                    m_sceneStatus = "Save failed";
                }
            }
        }

        ImGui::SameLine();

        if (Scene* activeScene = sceneSystem.GetActiveScene())
            ImGui::TextDisabled("%s", activeScene->name.c_str());
        else
            ImGui::TextDisabled("No active scene");

        if (!m_sceneStatus.empty())
        {
            ImGui::SameLine();
            ImGui::Text("%s", m_sceneStatus.c_str());
        }

        ImGui::Separator();
    }

    void DrawSceneBrowser(AssetSystem& assets, SceneSystem& sceneSystem, SceneSerializer& serializer)
    {
        ImGui::Text("Scenes");
        ImGui::Separator();

        std::vector<std::filesystem::path> scenes = GetSceneFiles();

        if (scenes.empty())
        {
            ImGui::TextDisabled("No scenes.");
            return;
        }

        for (const std::filesystem::path& path : scenes)
        {
            std::string fileName = path.filename().string();
            bool selected = std::filesystem::path(m_selectedScenePath).lexically_normal() == path.lexically_normal();

            ImGui::PushID(fileName.c_str());

            if (ImGui::Selectable(fileName.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                m_selectedScenePath = path.string();
                m_sceneStatus = "Scene selected";

                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    ImGui::PopID();
                    LoadScene(path, assets, sceneSystem, serializer);
                    return;
                }
            }

            ImGui::PopID();
        }
    }

    void DrawNewScenePopup(SceneSystem& sceneSystem, SceneSerializer& serializer)
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
            std::string sceneName = m_newSceneNameBuffer;
            std::filesystem::path scenePath = std::filesystem::path(m_sceneDirectory) / (sceneName + ".yaml");

            if (sceneSystem.GetScene(sceneName) || std::filesystem::exists(scenePath))
            {
                m_sceneStatus = "Scene already exists";
            }
            else
            {
                Scene& scene = sceneSystem.CreateScene(sceneName);
                sceneSystem.SetActiveScene(sceneName);

                ClearSelection();

                if (serializer.Save(scene, scenePath.string()))
                {
                    m_selectedScenePath = scenePath.string();
                    m_sceneStatus = "Scene created";
                }
                else
                {
                    m_sceneStatus = "Scene created, save failed";
                }

                ImGui::CloseCurrentPopup();
            }
        }

        if (!validName)
            ImGui::EndDisabled();

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    bool LoadSelectedScene(AssetSystem& assets, SceneSystem& sceneSystem, SceneSerializer& serializer)
    {
        if (m_selectedScenePath.empty())
        {
            m_sceneStatus = "Select a scene";
            return false;
        }

        return LoadScene(m_selectedScenePath, assets, sceneSystem, serializer);
    }

    bool LoadScene(const std::filesystem::path& path, AssetSystem& assets, SceneSystem& sceneSystem, SceneSerializer& serializer)
    {
        std::string sceneName = path.stem().string();
        Scene* scene = sceneSystem.GetScene(sceneName);
        bool created = false;

        if (!scene)
        {
            scene = &sceneSystem.CreateScene(sceneName);
            created = true;
        }

        if (!serializer.Load(*scene, assets, path.string()))
        {
            if (created)
                sceneSystem.DestroyScene(sceneName);

            m_sceneStatus = "Load failed";
            return false;
        }

        sceneSystem.SetActiveScene(sceneName);

        m_selectedScenePath = path.string();
        m_sceneStatus = "Scene loaded";

        ClearSelection();

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

    void DrawHierarchy(entt::registry& registry)
    {
        ImGui::Text("Hierarchy");
        ImGui::SameLine();

        if (ImGui::Button("+ Entity"))
        {
            entt::entity entity = registry.create();

            registry.emplace<IDComponent>(entity, IDComponent{ GenerateEntityID() });
            registry.emplace<NameComponent>(entity, NameComponent{ "Entity" });
            registry.emplace<TransformComponent>(entity);

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

    void DrawInspector(entt::registry& registry, AnimationSystem& animations, AssetSystem& assets, SceneSerializer& serializer)
    {
        ImGui::Text("Inspector");
        ImGui::Separator();

        if (m_selectedEntity == entt::null || !registry.valid(m_selectedEntity))
        {
            ImGui::TextDisabled("Select an entity.");
            return;
        }

        DrawEntity(registry);

        if (m_selectedEntity == entt::null || !registry.valid(m_selectedEntity))
            return;

        DrawAddComponent(registry, serializer);
        DrawTransform(registry);
        DrawModel(registry, assets);
        DrawCamera(registry);
        DrawPrimaryCamera(registry);
        DrawRigidBody(registry);
        DrawAnimation(registry, animations);
        DrawGameComponents(registry, serializer);
    }


    void DrawGameComponents(entt::registry& registry, SceneSerializer& serializer)
    {
        ImGui::PushID("RegisteredComponents");

        for (size_t i = 0; i < serializer.GetTagCount(); ++i)
        {
            if (!serializer.HasTag(i, registry, m_selectedEntity))
                continue;

            const std::string& name = serializer.GetTagName(i);
            ImGui::PushID(name.c_str());
            ImGui::SeparatorText(name.c_str());

            if (ImGui::Button("Remove Component"))
                serializer.SetTag(i, registry, m_selectedEntity, false);

            ImGui::PopID();
        }

        ImGui::PopID();
    }

    void DrawEntity(entt::registry& registry)
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
            if (registry.valid(m_selectedEntity))
                registry.destroy(m_selectedEntity);

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

    bool DrawComponentMenuItem(const char* name, bool present, bool allowed = true, const char* requirement = nullptr)
    {
        bool selected = ImGui::MenuItem(name, nullptr, false, !present && allowed);

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            if (present)
                ImGui::SetTooltip("Already added.");
            else if (!allowed && requirement)
                ImGui::SetTooltip("%s", requirement);
        }

        return selected;
    }

    void DrawAddComponent(entt::registry& registry, SceneSerializer& serializer)
    {
        ImGui::Spacing();

        if (ImGui::Button("+ Add Component"))
            ImGui::OpenPopup("AddComponentPopup");

        if (!ImGui::BeginPopup("AddComponentPopup"))
            return;

        const bool hasID = registry.any_of<IDComponent>(m_selectedEntity);
        const bool hasName = registry.any_of<NameComponent>(m_selectedEntity);
        const bool hasTransform = registry.any_of<TransformComponent>(m_selectedEntity);
        const bool hasModel = registry.any_of<ModelComponent>(m_selectedEntity);
        const bool hasCamera = registry.any_of<CameraComponent>(m_selectedEntity);
        const bool hasPrimaryCamera = registry.any_of<PrimaryCameraComponent>(m_selectedEntity);
        const bool hasRigidBody = registry.any_of<RigidBodyComponent>(m_selectedEntity);
        const bool hasAnimation = registry.any_of<AnimationComponent>(m_selectedEntity);

        bool available = !hasID || !hasName || !hasTransform || !hasModel || !hasCamera || !hasRigidBody || !hasAnimation || (!hasPrimaryCamera && hasCamera);

        // Core scene components stay visible, even when already present.
        // ID, Name and Transform are retained because scene saving requires them.
        if (DrawComponentMenuItem("ID", hasID))
            registry.emplace<IDComponent>(m_selectedEntity, IDComponent{ GenerateEntityID() });

        if (DrawComponentMenuItem("Name", hasName))
        {
            registry.emplace<NameComponent>(m_selectedEntity, NameComponent{ "Entity" });
            std::snprintf(m_nameBuffer, sizeof(m_nameBuffer), "%s", "Entity");
        }

        if (DrawComponentMenuItem("Transform", hasTransform))
            registry.emplace<TransformComponent>(m_selectedEntity);

        if (DrawComponentMenuItem("Model", hasModel))
        {
            registry.emplace<ModelComponent>(m_selectedEntity);
            m_modelPathBuffer[0] = '\0';
            m_modelStatus.clear();
        }

        if (DrawComponentMenuItem("Camera", hasCamera))
            registry.emplace<CameraComponent>(m_selectedEntity);

        if (DrawComponentMenuItem("Primary Camera", hasPrimaryCamera, hasCamera, "Add a Camera component first."))
        {
            // Only one camera is primary in a scene.
            registry.clear<PrimaryCameraComponent>();
            registry.emplace<PrimaryCameraComponent>(m_selectedEntity);
        }

        if (DrawComponentMenuItem("Rigid Body", hasRigidBody))
            registry.emplace<RigidBodyComponent>(m_selectedEntity);

        if (DrawComponentMenuItem("Animation", hasAnimation))
        {
            registry.emplace<AnimationComponent>(m_selectedEntity);
            m_selectedClip = 0;
        }

        ImGui::PushID("RegisteredComponents");

        for (size_t i = 0; i < serializer.GetTagCount(); ++i)
        {
            const bool present = serializer.HasTag(i, registry, m_selectedEntity);
            const std::string& name = serializer.GetTagName(i);

            available = available || !present;

            ImGui::PushID(name.c_str());

            if (DrawComponentMenuItem(name.c_str(), present))
                serializer.SetTag(i, registry, m_selectedEntity, true);

            ImGui::PopID();
        }

        ImGui::PopID();

        if (!available)
        {
            ImGui::Separator();
            ImGui::TextDisabled("All components are already added.");
        }

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

        if (ImGui::Button("Remove Camera"))
        {
            registry.remove<CameraComponent>(m_selectedEntity);

            if (registry.any_of<PrimaryCameraComponent>(m_selectedEntity))
                registry.remove<PrimaryCameraComponent>(m_selectedEntity);
        }
    }

    void DrawPrimaryCamera(entt::registry& registry)
    {
        if (!registry.any_of<PrimaryCameraComponent>(m_selectedEntity))
            return;

        ImGui::SeparatorText("Primary Camera");
        ImGui::TextDisabled("Used to render this scene.");

        if (ImGui::Button("Remove Primary Camera"))
            registry.remove<PrimaryCameraComponent>(m_selectedEntity);
    }

    void DrawRigidBody(entt::registry& registry)
    {
        RigidBodyComponent* body = registry.try_get<RigidBodyComponent>(m_selectedEntity);

        if (!body)
            return;

        ImGui::SeparatorText("Rigid Body");

        const char* types[] = { "Static", "Kinematic", "Dynamic" };
        int type = static_cast<int>(body->type);

        if (ImGui::Combo("Body Type", &type, types, IM_ARRAYSIZE(types)))
            body->type = static_cast<BodyType>(type);

        ImGui::DragFloat3("Linear Velocity", &body->linearVelocity.x, 0.05f);
        ImGui::DragFloat3("Linear Acceleration", &body->linearAcceleration.x, 0.05f);

        if (ImGui::Button("Remove Rigid Body"))
            registry.remove<RigidBodyComponent>(m_selectedEntity);
    }

    void DrawAnimation(entt::registry& registry, AnimationSystem& animations)
    {
        AnimationComponent* animation = registry.try_get<AnimationComponent>(m_selectedEntity);

        if (!animation)
            return;

        ImGui::SeparatorText("Animation");

        if (ImGui::Button("Remove Animation"))
        {
            registry.remove<AnimationComponent>(m_selectedEntity);
            m_selectedClip = 0;
            return;
        }

        ModelComponent* modelComponent = registry.try_get<ModelComponent>(m_selectedEntity);

        if (!modelComponent || !modelComponent->model)
        {
            ImGui::TextDisabled("Add and load a Model to select an animation clip.");
            return;
        }

        Model& model = *modelComponent->model;

        if (model.animations.empty())
        {
            ImGui::TextDisabled("This model has no animation clips.");
            return;
        }

        if (m_selectedClip >= model.animations.size())
            m_selectedClip = 0;

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

    void HandleActiveSceneChange(SceneSystem& sceneSystem)
    {
        Scene* activeScene = sceneSystem.GetActiveScene();

        if (activeScene == m_lastActiveScene)
            return;

        m_lastActiveScene = activeScene;
        ClearSelection();
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

    Scene* m_lastActiveScene = nullptr;
    entt::entity m_selectedEntity = entt::null;

    uint32_t m_selectedClip = 0;
    float m_blendDuration = 0.15f;
    bool m_loop = true;

    char m_nameBuffer[128]{};
    char m_modelPathBuffer[512]{};
    char m_newSceneNameBuffer[128]{};

    std::string m_modelStatus;

    std::string m_sceneDirectory = "../Assets/Scenes";
    std::string m_selectedScenePath;
    std::string m_sceneStatus;
};
