#pragma once

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <string_view>
#include <unordered_map>
#include "Model.h"

class AnimationSystem
{
public:

    void Play(Model* model, std::string_view animationName, bool loop = true)
    {
        if (!model)
            return;

        for (size_t i = 0; i < model->animations.size(); ++i)
        {
            if (model->animations[i].name == animationName)
            {
                AnimationState& state = m_states[model];

                state.animationIndex = static_cast<uint32_t>(i);
                state.time = 0.0f;
                state.loop = loop;
                state.playing = true;

                return;
            }
        }
    }

    void Update(float deltaTime)
    {
        for (auto& [model, state] : m_states)
        {
            if (!model || !state.playing)
                continue;

            if (state.animationIndex >= model->animations.size())
                continue;

            AnimationClip& clip = model->animations[state.animationIndex];

            if (clip.duration <= 0.0f)
                continue;

            state.time += deltaTime;

            if (state.loop)
                state.time = std::fmod(state.time, clip.duration);
            else if (state.time >= clip.duration)
            {
                state.time = clip.duration;
                state.playing = false;
            }

            Evaluate(*model, clip, state.time);
        }
    }

private:

    struct AnimationState
    {
        uint32_t animationIndex = 0;

        float time = 0.0f;

        bool loop = true;
        bool playing = false;
    };

    std::unordered_map<Model*, AnimationState> m_states;

    void Evaluate(Model& model, const AnimationClip& clip, float time)
    {
        // Reset every node to its original transform.
        for (Node& node : model.nodes)
        {
            node.translation = node.baseTranslation;
            node.rotation = node.baseRotation;
            node.scale = node.baseScale;
        }

        for (const AnimationChannel& channel : clip.channels)
        {
            if (channel.nodeIndex >= model.nodes.size())
                continue;

            if (channel.samplerIndex >= clip.samplers.size())
                continue;

            const AnimationSampler& sampler = clip.samplers[channel.samplerIndex];

            if (sampler.times.empty() || sampler.values.empty())
                continue;

            Node& node = model.nodes[channel.nodeIndex];

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