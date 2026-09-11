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

    bool blending = false;

    uint32_t sourceAnimationIndex = 0;
    float sourceTime = 0.0f;
    bool sourceLoop = true;

    float blendTime = 0.0f;
    float blendDuration = 0.0f;

    std::vector<NodePose> pose;
};

class AnimationSystem
{
public:
    bool Play(entt::registry& registry, entt::entity entity, std::string_view animationName, bool loop = true, float blendDuration = 0.20f, bool restart = false)
    {
        ModelComponent* modelComponent = registry.try_get<ModelComponent>(entity);

        if (!modelComponent || !modelComponent->model)
            return false;

        Model& model = *modelComponent->model;

        for (size_t i = 0; i < model.animations.size(); ++i)
        {
            if (model.animations[i].name != animationName)
                continue;

            AnimationComponent* existing = registry.try_get<AnimationComponent>(entity);

            if (existing && !restart && existing->animationIndex == i && existing->playing)
                return true;

            AnimationComponent& animation = registry.get_or_emplace<AnimationComponent>(entity);

            const uint32_t newAnimationIndex = static_cast<uint32_t>(i);
            const AnimationClip& targetClip = model.animations[newAnimationIndex];

            float normalizedTime = 0.0f;

            if (animation.animationIndex < model.animations.size())
            {
                const AnimationClip& currentClip = model.animations[animation.animationIndex];

                if (animation.loop && loop && currentClip.duration > 0.0f)
                    normalizedTime = animation.time / currentClip.duration;
            }

            const bool canBlend = !animation.pose.empty() && animation.animationIndex < model.animations.size() && blendDuration > 0.0f;

            if (canBlend)
            {
                animation.sourceAnimationIndex = animation.animationIndex;
                animation.sourceTime = animation.time;
                animation.sourceLoop = animation.loop;

                animation.blending = true;
                animation.blendTime = 0.0f;
                animation.blendDuration = blendDuration;
            }
            else
            {
                animation.blending = false;
                animation.blendTime = 0.0f;
                animation.blendDuration = 0.0f;
            }

            animation.animationIndex = newAnimationIndex;
            animation.time = loop && targetClip.duration > 0.0f ? normalizedTime * targetClip.duration : 0.0f;
            animation.loop = loop;
            animation.playing = true;
            animation.finished = false;

            if (animation.pose.empty())
            {
                ResetPose(model, animation.pose);
                Evaluate(animation.pose, targetClip, animation.time);
            }

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
        animation->blending = false;
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

            const AnimationClip& targetClip = model.animations[animation.animationIndex];

            UpdateTargetTime(animation, targetClip, deltaTime);

            if (!animation.blending)
            {
                ResetPose(model, animation.pose);
                Evaluate(animation.pose, targetClip, animation.time);
                continue;
            }

            if (animation.sourceAnimationIndex >= model.animations.size())
            {
                animation.blending = false;

                ResetPose(model, animation.pose);
                Evaluate(animation.pose, targetClip, animation.time);

                continue;
            }

            const AnimationClip& sourceClip = model.animations[animation.sourceAnimationIndex];

            UpdateSourceTime(animation, sourceClip, deltaTime);

            std::vector<NodePose> sourcePose;
            std::vector<NodePose> targetPose;

            ResetPose(model, sourcePose);
            ResetPose(model, targetPose);

            Evaluate(sourcePose, sourceClip, animation.sourceTime);
            Evaluate(targetPose, targetClip, animation.time);

            animation.blendTime += deltaTime;

            float factor = animation.blendDuration > 0.0f ? animation.blendTime / animation.blendDuration : 1.0f;
            factor = std::clamp(factor, 0.0f, 1.0f);

            const float smoothFactor = factor * factor * (3.0f - 2.0f * factor);

            BlendPoses(sourcePose, targetPose, smoothFactor, animation.pose);

            if (factor >= 1.0f)
            {
                animation.blending = false;
                animation.blendTime = 0.0f;
                animation.blendDuration = 0.0f;
            }
        }
    }

private:
    void UpdateTargetTime(AnimationComponent& animation, const AnimationClip& clip, float deltaTime)
    {
        if (!animation.playing || clip.duration <= 0.0f)
            return;

        animation.time += deltaTime;

        if (animation.loop)
        {
            animation.time = std::fmod(animation.time, clip.duration);
            return;
        }

        if (animation.time >= clip.duration)
        {
            animation.time = clip.duration;
            animation.playing = false;
            animation.finished = true;
        }
    }

    void UpdateSourceTime(AnimationComponent& animation, const AnimationClip& clip, float deltaTime)
    {
        if (clip.duration <= 0.0f)
            return;

        animation.sourceTime += deltaTime;

        if (animation.sourceLoop)
        {
            animation.sourceTime = std::fmod(animation.sourceTime, clip.duration);
            return;
        }

        animation.sourceTime = std::min(animation.sourceTime, clip.duration);
    }

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

    void Evaluate(std::vector<NodePose>& pose, const AnimationClip& clip, float time)
    {
        for (const AnimationChannel& channel : clip.channels)
        {
            if (channel.nodeIndex >= pose.size() || channel.samplerIndex >= clip.samplers.size())
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

    void BlendPoses(const std::vector<NodePose>& source, const std::vector<NodePose>& target, float factor, std::vector<NodePose>& result)
    {
        result.resize(target.size());

        for (size_t i = 0; i < target.size(); ++i)
        {
            DirectX::XMVECTOR sourceTranslation = DirectX::XMLoadFloat3(&source[i].translation);
            DirectX::XMVECTOR targetTranslation = DirectX::XMLoadFloat3(&target[i].translation);

            DirectX::XMVECTOR sourceRotation = DirectX::XMLoadFloat4(&source[i].rotation);
            DirectX::XMVECTOR targetRotation = DirectX::XMLoadFloat4(&target[i].rotation);

            DirectX::XMVECTOR sourceScale = DirectX::XMLoadFloat3(&source[i].scale);
            DirectX::XMVECTOR targetScale = DirectX::XMLoadFloat3(&target[i].scale);

            DirectX::XMStoreFloat3(&result[i].translation, DirectX::XMVectorLerp(sourceTranslation, targetTranslation, factor));
            DirectX::XMStoreFloat4(&result[i].rotation, DirectX::XMQuaternionSlerp(sourceRotation, targetRotation, factor));
            DirectX::XMStoreFloat3(&result[i].scale, DirectX::XMVectorLerp(sourceScale, targetScale, factor));
        }
    }

    DirectX::XMFLOAT4 Sample(const AnimationSampler& sampler, AnimationPath path, float time)
    {
        if (sampler.times.size() == 1 || time <= sampler.times.front())
            return sampler.values.front();

        if (time >= sampler.times.back())
            return sampler.values.back();

        auto upper = std::upper_bound(sampler.times.begin(), sampler.times.end(), time);

        const size_t nextIndex = static_cast<size_t>(upper - sampler.times.begin());
        const size_t previousIndex = nextIndex - 1;

        if (sampler.interpolation == AnimationInterpolation::Step)
            return sampler.values[previousIndex];

        const float previousTime = sampler.times[previousIndex];
        const float nextTime = sampler.times[nextIndex];
        const float factor = (time - previousTime) / (nextTime - previousTime);

        DirectX::XMVECTOR previous = DirectX::XMLoadFloat4(&sampler.values[previousIndex]);
        DirectX::XMVECTOR next = DirectX::XMLoadFloat4(&sampler.values[nextIndex]);

        DirectX::XMVECTOR result = path == AnimationPath::Rotation ? DirectX::XMQuaternionSlerp(previous, next, factor) : DirectX::XMVectorLerp(previous, next, factor);

        DirectX::XMFLOAT4 value{};
        DirectX::XMStoreFloat4(&value, result);

        return value;
    }
};