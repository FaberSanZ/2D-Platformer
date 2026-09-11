#pragma once

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>
#include <entt/entt.hpp>
#include "Components.h"
#include "Model.h"

struct AnimationComponent
{
    uint32_t animationIndex = 0;

    float time = 0.0f;

    bool loop = true;
    bool playing = false;
    bool finished = false;

    float blendTime = 0.0f;
    float blendDuration = 0.0f;

    std::vector<NodePose> pose;
    std::vector<NodePose> blendFromPose;
};

class AnimationSystem
{
public:
    bool Play(entt::registry& registry, entt::entity entity, std::string_view animationName, bool loop = true, float blendDuration = 0.15f, bool restart = false)
    {
        ModelComponent* modelComponent = registry.try_get<ModelComponent>(entity);

        if (!modelComponent || !modelComponent->model)
            return false;

        Model& model = *modelComponent->model;

        for (size_t i = 0; i < model.animations.size(); ++i)
        {
            if (model.animations[i].name != animationName)
                continue;

            AnimationComponent& animation = registry.get_or_emplace<AnimationComponent>(entity);

            if (!restart && animation.playing && animation.animationIndex == i)
                return true;

            if (animation.pose.empty())
                ResetPose(model, animation.pose);

            animation.blendFromPose = animation.pose;

            animation.animationIndex = static_cast<uint32_t>(i);
            animation.time = 0.0f;

            animation.loop = loop;
            animation.playing = true;
            animation.finished = false;

            animation.blendTime = 0.0f;
            animation.blendDuration = blendDuration;

            return true;
        }

        return false;
    }

    void Stop(entt::registry& registry, entt::entity entity)
    {
        AnimationComponent* animation = registry.try_get<AnimationComponent>(entity);

        if (!animation)
            return;

        animation->playing = false;
    }

    bool IsPlaying(entt::registry& registry, entt::entity entity) const
    {
        const AnimationComponent* animation = registry.try_get<AnimationComponent>(entity);

        return animation && animation->playing;
    }

    bool Finished(entt::registry& registry, entt::entity entity) const
    {
        const AnimationComponent* animation = registry.try_get<AnimationComponent>(entity);

        return animation && animation->finished;
    }

    bool IsCurrent(entt::registry& registry, entt::entity entity, std::string_view animationName) const
    {
        const ModelComponent* modelComponent = registry.try_get<ModelComponent>(entity);
        const AnimationComponent* animation = registry.try_get<AnimationComponent>(entity);

        if (!modelComponent || !modelComponent->model || !animation)
            return false;

        const Model& model = *modelComponent->model;

        if (animation->animationIndex >= model.animations.size())
            return false;

        return model.animations[animation->animationIndex].name == animationName;
    }

    void Update(entt::registry& registry, float deltaTime)
    {
        auto view = registry.view<ModelComponent, AnimationComponent>();

        for (auto [entity, modelComponent, animation] : view.each())
        {
            if (!modelComponent.model)
                continue;

            Model& model = *modelComponent.model;

            if (animation.animationIndex >= model.animations.size())
                continue;

            const AnimationClip& clip = model.animations[animation.animationIndex];

            if (clip.duration <= 0.0f)
                continue;

            if (animation.playing)
            {
                animation.time += deltaTime;

                if (animation.loop)
                {
                    animation.time = std::fmod(animation.time, clip.duration);
                }
                else if (animation.time >= clip.duration)
                {
                    animation.time = clip.duration;
                    animation.playing = false;
                    animation.finished = true;
                }
            }

            std::vector<NodePose> targetPose;

            ResetPose(model, targetPose);
            Evaluate(model, targetPose, clip, animation.time);

            if (animation.blendDuration > 0.0f && animation.blendTime < animation.blendDuration && animation.blendFromPose.size() == targetPose.size())
            {
                animation.blendTime += deltaTime;

                float factor = std::clamp(animation.blendTime / animation.blendDuration, 0.0f, 1.0f);

                BlendPoses(animation.blendFromPose, targetPose, factor, animation.pose);
            }
            else
            {
                animation.pose = std::move(targetPose);
            }
        }
    }

private:
    void ResetPose(const Model& model, std::vector<NodePose>& pose)
    {
        pose.resize(model.nodes.size());

        for (size_t i = 0; i < model.nodes.size(); ++i)
        {
            pose[i].translation = model.nodes[i].translation;
            pose[i].rotation = model.nodes[i].rotation;
            pose[i].scale = model.nodes[i].scale;
        }
    }

    void Evaluate(const Model& model, std::vector<NodePose>& pose, const AnimationClip& clip, float time)
    {
        for (const AnimationChannel& channel : clip.channels)
        {
            if (channel.nodeIndex >= pose.size())
                continue;

            if (channel.samplerIndex >= clip.samplers.size())
                continue;

            const AnimationSampler& sampler = clip.samplers[channel.samplerIndex];

            if (sampler.times.empty() || sampler.values.empty())
                continue;

            NodePose& node = pose[channel.nodeIndex];

            DirectX::XMFLOAT4 value = Sample(sampler, channel.path, time);

            switch (channel.path)
            {
            case AnimationPath::Translation:
                node.translation = { value.x, value.y, value.z };
                break;

            case AnimationPath::Rotation:
                node.rotation = value;
                break;

            case AnimationPath::Scale:
                node.scale = { value.x, value.y, value.z };
                break;
            }
        }
    }

    void BlendPoses(const std::vector<NodePose>& from, const std::vector<NodePose>& to, float factor, std::vector<NodePose>& result)
    {
        result.resize(to.size());

        for (size_t i = 0; i < to.size(); ++i)
        {
            DirectX::XMVECTOR fromTranslation = DirectX::XMLoadFloat3(&from[i].translation);
            DirectX::XMVECTOR toTranslation = DirectX::XMLoadFloat3(&to[i].translation);

            DirectX::XMVECTOR fromRotation = DirectX::XMLoadFloat4(&from[i].rotation);
            DirectX::XMVECTOR toRotation = DirectX::XMLoadFloat4(&to[i].rotation);

            DirectX::XMVECTOR fromScale = DirectX::XMLoadFloat3(&from[i].scale);
            DirectX::XMVECTOR toScale = DirectX::XMLoadFloat3(&to[i].scale);

            DirectX::XMStoreFloat3(&result[i].translation, DirectX::XMVectorLerp(fromTranslation, toTranslation, factor));
            DirectX::XMStoreFloat4(&result[i].rotation, DirectX::XMQuaternionSlerp(fromRotation, toRotation, factor));
            DirectX::XMStoreFloat3(&result[i].scale, DirectX::XMVectorLerp(fromScale, toScale, factor));
        }
    }

    DirectX::XMFLOAT4 Sample(const AnimationSampler& sampler, AnimationPath path, float time)
    {
        if (sampler.times.size() == 1 || time <= sampler.times.front())
            return sampler.values.front();

        if (time >= sampler.times.back())
            return sampler.values.back();

        auto upper = std::upper_bound(sampler.times.begin(), sampler.times.end(), time);

        size_t nextIndex = static_cast<size_t>(upper - sampler.times.begin());
        size_t previousIndex = nextIndex - 1;

        if (sampler.interpolation == AnimationInterpolation::Step)
            return sampler.values[previousIndex];

        float previousTime = sampler.times[previousIndex];
        float nextTime = sampler.times[nextIndex];

        float factor = (time - previousTime) / (nextTime - previousTime);

        DirectX::XMVECTOR previous = DirectX::XMLoadFloat4(&sampler.values[previousIndex]);
        DirectX::XMVECTOR next = DirectX::XMLoadFloat4(&sampler.values[nextIndex]);

        DirectX::XMVECTOR result;

        if (path == AnimationPath::Rotation)
            result = DirectX::XMQuaternionSlerp(previous, next, factor);
        else
            result = DirectX::XMVectorLerp(previous, next, factor);

        DirectX::XMFLOAT4 value{};
        DirectX::XMStoreFloat4(&value, result);

        return value;
    }
};