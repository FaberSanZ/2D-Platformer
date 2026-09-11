#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <cstdint>
#include <entt/entt.hpp>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include "Components.h"
#include "AnimationSystem.h"

class EditorSystem
{
public:
    void Initialize(HWND window, ID3D11Device* device, ID3D11DeviceContext* context)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX11_Init(device, context);
    }

    void BeginFrame()
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void Draw(entt::registry& registry, AnimationSystem& animations)
    {
        ImGui::SetNextWindowSize(ImVec2(760.0f, 500.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Soulslike Editor");

        ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("%.3f ms", 1000.0f / ImGui::GetIO().Framerate);

        ImGui::Separator();

        if (ImGui::BeginTable("EditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Hierarchy", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextColumn();
            DrawHierarchy(registry);

            ImGui::TableNextColumn();
            DrawInspector(registry, animations);

            ImGui::EndTable();
        }

        ImGui::End();
    }

    void EndFrame()
    {
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void Destroy()
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
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
    void DrawHierarchy(entt::registry& registry)
    {
        ImGui::Text("Hierarchy");
        ImGui::Separator();

        auto view = registry.view<IDComponent, NameComponent>();

        for (auto [entity, id, name] : view.each())
        {
            bool selected = entity == m_selectedEntity;
            uint64_t entityID = id.id;

            ImGui::PushID(static_cast<int>(entt::to_integral(entity)));

            if (ImGui::Selectable(name.name.c_str(), selected))
            {
                m_selectedEntity = entity;

                if (AnimationComponent* animation = registry.try_get<AnimationComponent>(entity))
                    m_selectedClip = animation->animationIndex;
                else
                    m_selectedClip = 0;
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("ID: %llu", static_cast<unsigned long long>(entityID));

            ImGui::PopID();
        }
    }

    void DrawInspector(entt::registry& registry, AnimationSystem& animations)
    {
        ImGui::Text("Inspector");
        ImGui::Separator();

        if (m_selectedEntity == entt::null || !registry.valid(m_selectedEntity))
        {
            ImGui::TextDisabled("Select an entity.");
            return;
        }

        NameComponent* name = registry.try_get<NameComponent>(m_selectedEntity);
        IDComponent* id = registry.try_get<IDComponent>(m_selectedEntity);

        if (name)
            ImGui::Text("%s", name->name.c_str());

        if (id)
            ImGui::TextDisabled("ID: %llu", static_cast<unsigned long long>(id->id));

        ImGui::Spacing();

        DrawTransform(registry);
        DrawAnimation(registry, animations);
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

private:
    entt::entity m_selectedEntity = entt::null;
    uint32_t m_selectedClip = 0;
    float m_blendDuration = 0.15f;
    bool m_loop = true;
};