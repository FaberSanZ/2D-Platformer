#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include "Resource.h"
#include "Model.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct InstanceData
{
    DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
    DirectX::XMFLOAT4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
};

struct DirectionalLight
{
    DirectX::XMFLOAT4 direction{ 0.35f, -0.75f, 0.55f, 0.0f };
    DirectX::XMFLOAT4 colorIntensity{ 1.0f, 0.98f, 0.94f, 0.9f };
};
class RenderSystem
{
public:
    RenderSystem() = default;

    void Initialize(HWND handle, uint32_t width, uint32_t height)
    {
        m_width = width;
        m_height = height;

        IDXGIFactory6* factory = nullptr;
        CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

        D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        swapChainDesc.Width = width;
        swapChainDesc.Height = height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = FrameCount;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        IDXGISwapChain1* swapChain = nullptr;
        factory->CreateSwapChainForHwnd(m_commandQueue, handle, &swapChainDesc, nullptr, nullptr, &swapChain);
        factory->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER);
        swapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain));
        swapChain->Release();
        factory->Release();

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = FrameCount + 1;
        m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (uint32_t i = 0; i < FrameCount; ++i)
        {
            m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
            m_device->CreateRenderTargetView(m_backBuffers[i], nullptr, rtvHandle);
            rtvHandle.ptr += m_rtvDescriptorSize;
        }

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NumDescriptors = 1;
        m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));
        m_dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS colorMsaa{};
        colorMsaa.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        colorMsaa.SampleCount = m_msaaSamples;
        colorMsaa.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS depthMsaa{};
        depthMsaa.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthMsaa.SampleCount = m_msaaSamples;
        depthMsaa.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        bool colorSupported = SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &colorMsaa, sizeof(colorMsaa))) && colorMsaa.NumQualityLevels > 0;
        bool depthSupported = SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &depthMsaa, sizeof(depthMsaa))) && depthMsaa.NumQualityLevels > 0;

        if (!colorSupported || !depthSupported)
        {
            m_msaaSamples = 1;
            m_msaaQuality = 0;
        }
        else
        {
            m_msaaQuality = std::min(colorMsaa.NumQualityLevels, depthMsaa.NumQualityLevels) - 1;
        }

        CreateRenderTargets();

        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator, nullptr, IID_PPV_ARGS(&m_cmd));
        m_cmd->Close();

        CreateRootSignature();
        CreatePipeline();

        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        m_camera = CreateCBV(sizeof(DirectX::XMMATRIX));
        m_light = CreateCBV(sizeof(DirectionalLight));
        m_dynamicUpload = CreateStructuredBuffer(1, DynamicUploadSize);

        D3D12_RANGE readRange{ 0, 0 };
        m_dynamicUpload.resource->Map(0, &readRange, reinterpret_cast<void**>(&m_dynamicMappedData));

        UpdateGpuData(m_light, &m_sun, 1);
    }

    ID3D12Device* Device() const { return m_device; }
    ID3D12CommandQueue* CommandQueue() const { return m_commandQueue; }
    ID3D12GraphicsCommandList* CommandList() const { return m_cmd; }

    Resource CreateCBV(uint32_t size)
    {
        Resource resource{};
        resource.stride = size;
        resource.count = 1;

        uint64_t alignedSize = AlignUp(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        resource.resource = CreateUploadResource(alignedSize);
        resource.gpuAddress = resource.resource->GetGPUVirtualAddress();
        return resource;
    }

    Resource CreateStructuredBuffer(uint32_t stride, uint32_t count)
    {
        Resource resource{};
        resource.stride = stride;
        resource.count = count;

        uint64_t size = static_cast<uint64_t>(stride) * count;
        if (size == 0)
            size = 1;

        resource.resource = CreateUploadResource(size);
        resource.gpuAddress = resource.resource->GetGPUVirtualAddress();
        return resource;
    }

    void UpdateGpuData(const Resource& resource, const void* data, uint32_t count, uint32_t offset = 0)
    {
        if (!resource.resource || !data || count == 0)
            return;

        uint64_t byteOffset = static_cast<uint64_t>(offset) * resource.stride;
        uint64_t byteSize = static_cast<uint64_t>(count) * resource.stride;
        uint64_t capacity = resource.resource->GetDesc().Width;

        if (byteOffset + byteSize > capacity)
            return;

        uint8_t* mappedData = nullptr;
        D3D12_RANGE readRange{ 0, 0 };

        resource.resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
        std::memcpy(mappedData + byteOffset, data, static_cast<size_t>(byteSize));

        D3D12_RANGE writtenRange{ static_cast<SIZE_T>(byteOffset), static_cast<SIZE_T>(byteOffset + byteSize) };
        resource.resource->Unmap(0, &writtenRange);
    }

    D3D12_INDEX_BUFFER_VIEW GetView(const Resource& resource) const
    {
        D3D12_INDEX_BUFFER_VIEW view{};
        view.BufferLocation = resource.gpuAddress;
        view.SizeInBytes = resource.stride * resource.count;
        view.Format = DXGI_FORMAT_R32_UINT;
        return view;
    }

    NodePose GetNodePose(const Model& model, uint32_t nodeIndex, const std::vector<NodePose>* pose)
    {
        if (pose && nodeIndex < pose->size())
            return (*pose)[nodeIndex];

        NodePose result{};

        if (nodeIndex >= model.nodes.size())
            return result;

        const Node& node = model.nodes[nodeIndex];
        result.translation = node.translation;
        result.rotation = node.rotation;
        result.scale = node.scale;
        return result;
    }

    DirectX::XMMATRIX GetLocalNodeTransform(const NodePose& node)
    {
        DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&node.rotation);

        return DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z) *
            DirectX::XMMatrixRotationQuaternion(rotation) *
            DirectX::XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z);
    }

    DirectX::XMMATRIX GetNodeTransform(const Model& model, uint32_t nodeIndex, const std::vector<NodePose>* pose = nullptr)
    {
        if (nodeIndex >= model.nodes.size())
            return DirectX::XMMatrixIdentity();

        NodePose nodePose = GetNodePose(model, nodeIndex, pose);
        DirectX::XMMATRIX transform = GetLocalNodeTransform(nodePose);
        int32_t parent = model.nodes[nodeIndex].parent;

        while (parent >= 0)
        {
            NodePose parentPose = GetNodePose(model, static_cast<uint32_t>(parent), pose);
            transform = transform * GetLocalNodeTransform(parentPose);
            parent = model.nodes[parent].parent;
        }

        return transform;
    }

    void BuildSkinMatrices(const Model& model, uint32_t meshNodeIndex, const Skin& skin, const std::vector<NodePose>* pose, std::vector<DirectX::XMMATRIX>& jointMatrices)
    {
        jointMatrices.resize(skin.joints.size());

        DirectX::XMMATRIX meshTransform = GetNodeTransform(model, meshNodeIndex, pose);
        DirectX::XMMATRIX inverseMeshTransform = DirectX::XMMatrixInverse(nullptr, meshTransform);

        for (size_t i = 0; i < skin.joints.size(); ++i)
        {
            uint32_t jointNodeIndex = skin.joints[i];

            if (jointNodeIndex >= model.nodes.size() || i >= skin.inverseBindMatrices.size())
            {
                jointMatrices[i] = DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity());
                continue;
            }

            DirectX::XMMATRIX inverseBind = DirectX::XMLoadFloat4x4(&skin.inverseBindMatrices[i]);
            DirectX::XMMATRIX jointTransform = GetNodeTransform(model, jointNodeIndex, pose);
            DirectX::XMMATRIX skinMatrix = inverseBind * jointTransform * inverseMeshTransform;
            jointMatrices[i] = DirectX::XMMatrixTranspose(skinMatrix);
        }
    }

    void DrawModel(const Model& model, const InstanceData* instances, uint32_t instanceCount, const std::vector<NodePose>* pose = nullptr)
    {
        if (instanceCount == 0)
            return;

        for (const Mesh& mesh : model.meshes)
        {
            for (const MeshPart& part : mesh.parts)
            {
                DirectX::XMFLOAT4 materialColor{ 1.0f, 1.0f, 1.0f, 1.0f };

                if (part.material)
                    materialColor = part.material->baseColor;

                std::vector<InstanceData> materialInstances(instanceCount);

                for (uint32_t i = 0; i < instanceCount; ++i)
                {
                    materialInstances[i] = instances[i];
                    materialInstances[i].baseColor =
                    {
                        instances[i].baseColor.x * materialColor.x,
                        instances[i].baseColor.y * materialColor.y,
                        instances[i].baseColor.z * materialColor.z,
                        instances[i].baseColor.w * materialColor.w
                    };
                }

                D3D12_GPU_VIRTUAL_ADDRESS instanceAddress = UploadDynamicData(materialInstances.data(), sizeof(InstanceData) * instanceCount, 256);

                uint32_t nodeIndex = 0;

                if (part.node)
                    nodeIndex = static_cast<uint32_t>(part.node - model.nodes.data());

                DirectX::XMMATRIX nodeTransform = GetNodeTransform(model, nodeIndex, pose);
                DirectX::XMMATRIX gpuNodeTransform = DirectX::XMMatrixTranspose(nodeTransform);
                D3D12_GPU_VIRTUAL_ADDRESS nodeAddress = UploadDynamicData(&gpuNodeTransform, sizeof(gpuNodeTransform), 256);

                bool hasSkin = false;
                D3D12_GPU_VIRTUAL_ADDRESS jointAddress = 0;

                if (part.node && part.node->skinIndex >= 0)
                {
                    uint32_t skinIndex = static_cast<uint32_t>(part.node->skinIndex);

                    if (skinIndex < model.skins.size())
                    {
                        const Skin& skin = model.skins[skinIndex];

                        if (!skin.joints.empty())
                        {
                            std::vector<DirectX::XMMATRIX> jointMatrices;
                            BuildSkinMatrices(model, nodeIndex, skin, pose, jointMatrices);
                            jointAddress = UploadDynamicData(jointMatrices.data(), static_cast<uint32_t>(sizeof(DirectX::XMMATRIX) * jointMatrices.size()), 256);
                            hasSkin = true;
                        }
                    }
                }

                if (!hasSkin)
                {
                    DirectX::XMMATRIX identity = DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity());
                    jointAddress = UploadDynamicData(&identity, sizeof(identity), 256);
                }

                if (instanceAddress == 0 || nodeAddress == 0 || jointAddress == 0)
                    return;

                m_cmd->SetGraphicsRootConstantBufferView(2, nodeAddress);
                m_cmd->SetGraphicsRootShaderResourceView(3, part.vertex.gpuAddress);
                m_cmd->SetGraphicsRootShaderResourceView(4, instanceAddress);
                m_cmd->SetGraphicsRootShaderResourceView(5, jointAddress);

                D3D12_INDEX_BUFFER_VIEW indexView = GetView(part.index);
                m_cmd->IASetIndexBuffer(&indexView);
                m_cmd->DrawIndexedInstanced(part.index.count, instanceCount, 0, 0, 0);
            }
        }
    }

    void Update(const DirectX::XMMATRIX& viewProjection)
    {
        DirectX::XMMATRIX vp = DirectX::XMMatrixTranspose(viewProjection);
        UpdateGpuData(m_camera, &vp, 1);
    }

    void BeginFrame()
    {
        m_dynamicOffset = 0;

        m_commandAllocator->Reset();
        m_cmd->Reset(m_commandAllocator, m_pipelineState);

        m_cmd->SetGraphicsRootSignature(m_rootSignature);
        m_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        D3D12_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(m_width);
        viewport.Height = static_cast<float>(m_height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        D3D12_RECT scissorRect{ 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };

        m_cmd->RSSetViewports(1, &viewport);
        m_cmd->RSSetScissorRects(1, &scissorRect);

        D3D12_CPU_DESCRIPTOR_HANDLE renderTarget = GetBackBufferRTV(m_frameIndex);

        if (m_msaaSamples > 1)
        {
            renderTarget = m_msaaRtvHandle;
        }
        else
        {
            Transition(m_backBuffers[m_frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        float clearColor[4] = { 0.9294f, 0.8824f, 0.8275f, 1.0f };

        m_cmd->ClearRenderTargetView(renderTarget, clearColor, 0, nullptr);
        m_cmd->ClearDepthStencilView(m_dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        m_cmd->OMSetRenderTargets(1, &renderTarget, FALSE, &m_dsvHandle);

        m_cmd->SetGraphicsRootConstantBufferView(0, m_camera.gpuAddress);
        m_cmd->SetGraphicsRootConstantBufferView(1, m_light.gpuAddress);
    }

    void BeginEditor()
    {
        D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = GetBackBufferRTV(m_frameIndex);

        if (m_msaaSamples > 1)
        {
            Transition(m_msaaRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
            Transition(m_backBuffers[m_frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST);

            m_cmd->ResolveSubresource(m_backBuffers[m_frameIndex], 0, m_msaaRenderTarget, 0, DXGI_FORMAT_R8G8B8A8_UNORM);

            Transition(m_msaaRenderTarget, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            Transition(m_backBuffers[m_frameIndex], D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        m_cmd->OMSetRenderTargets(1, &backBufferRTV, FALSE, nullptr);
    }

    void EndFrame()
    {
        Transition(m_backBuffers[m_frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        m_cmd->Close();

        ID3D12CommandList* commandLists[] = { m_cmd };
        m_commandQueue->ExecuteCommandLists(1, commandLists);

        m_swapChain->Present(1, 0);

        WaitForGpu();
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    }

    void Destroy()
    {
        if (m_commandQueue && m_fence)
            WaitForGpu();

        if (m_dynamicUpload.resource)
        {
            m_dynamicUpload.resource->Unmap(0, nullptr);
            m_dynamicUpload.resource->Release();
        }

        if (m_camera.resource) m_camera.resource->Release();
        if (m_light.resource) m_light.resource->Release();

        if (m_pipelineState) m_pipelineState->Release();
        if (m_rootSignature) m_rootSignature->Release();

        if (m_depthStencil) m_depthStencil->Release();
        if (m_msaaRenderTarget) m_msaaRenderTarget->Release();

        for (uint32_t i = 0; i < FrameCount; ++i)
        {
            if (m_backBuffers[i])
                m_backBuffers[i]->Release();
        }

        if (m_dsvHeap) m_dsvHeap->Release();
        if (m_rtvHeap) m_rtvHeap->Release();

        if (m_cmd) m_cmd->Release();
        if (m_commandAllocator) m_commandAllocator->Release();

        if (m_fence) m_fence->Release();

        if (m_fenceEvent)
        {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }

        if (m_swapChain) m_swapChain->Release();
        if (m_commandQueue) m_commandQueue->Release();
        if (m_device) m_device->Release();
    }

private:
    static constexpr uint32_t FrameCount = 2;
    static constexpr uint32_t DynamicUploadSize = 16 * 1024 * 1024;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    uint32_t m_frameIndex = 0;
    uint32_t m_rtvDescriptorSize = 0;

    uint32_t m_msaaSamples = 4;
    uint32_t m_msaaQuality = 0;

    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    IDXGISwapChain3* m_swapChain = nullptr;

    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    ID3D12DescriptorHeap* m_dsvHeap = nullptr;

    ID3D12Resource* m_backBuffers[FrameCount]{};
    ID3D12Resource* m_msaaRenderTarget = nullptr;
    ID3D12Resource* m_depthStencil = nullptr;

    D3D12_CPU_DESCRIPTOR_HANDLE m_msaaRtvHandle{};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle{};

    ID3D12CommandAllocator* m_commandAllocator = nullptr;
    ID3D12GraphicsCommandList* m_cmd = nullptr;

    ID3D12RootSignature* m_rootSignature = nullptr;
    ID3D12PipelineState* m_pipelineState = nullptr;

    ID3D12Fence* m_fence = nullptr;
    HANDLE m_fenceEvent = nullptr;
    uint64_t m_fenceValue = 0;

    Resource m_camera;
    Resource m_light;
    Resource m_dynamicUpload;

    uint8_t* m_dynamicMappedData = nullptr;
    uint32_t m_dynamicOffset = 0;

    DirectionalLight m_sun;

    static uint64_t AlignUp(uint64_t value, uint64_t alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    ID3D12Resource* CreateUploadResource(uint64_t size)
    {
        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ID3D12Resource* resource = nullptr;
        m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
        return resource;
    }

    D3D12_GPU_VIRTUAL_ADDRESS UploadDynamicData(const void* data, uint32_t size, uint32_t alignment)
    {
        if (!data || size == 0)
            return 0;

        uint32_t alignedOffset = static_cast<uint32_t>(AlignUp(m_dynamicOffset, alignment));
        uint64_t capacity = m_dynamicUpload.resource->GetDesc().Width;

        if (static_cast<uint64_t>(alignedOffset) + size > capacity)
            return 0;

        std::memcpy(m_dynamicMappedData + alignedOffset, data, size);

        D3D12_GPU_VIRTUAL_ADDRESS address = m_dynamicUpload.gpuAddress + alignedOffset;
        m_dynamicOffset = alignedOffset + size;
        return address;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index) * m_rtvDescriptorSize;
        return handle;
    }

    void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_cmd->ResourceBarrier(1, &barrier);
    }

    void CreateRenderTargets()
    {
        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        if (m_msaaSamples > 1)
        {
            D3D12_RESOURCE_DESC colorDesc{};
            colorDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            colorDesc.Width = m_width;
            colorDesc.Height = m_height;
            colorDesc.DepthOrArraySize = 1;
            colorDesc.MipLevels = 1;
            colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            colorDesc.SampleDesc.Count = m_msaaSamples;
            colorDesc.SampleDesc.Quality = m_msaaQuality;
            colorDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            D3D12_CLEAR_VALUE colorClear{};
            colorClear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            colorClear.Color[0] = 0.9294f;
            colorClear.Color[1] = 0.8824f;
            colorClear.Color[2] = 0.8275f;
            colorClear.Color[3] = 1.0f;

            m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &colorDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &colorClear, IID_PPV_ARGS(&m_msaaRenderTarget));

            m_msaaRtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
            m_msaaRtvHandle.ptr += static_cast<SIZE_T>(FrameCount) * m_rtvDescriptorSize;
            m_device->CreateRenderTargetView(m_msaaRenderTarget, nullptr, m_msaaRtvHandle);
        }

        D3D12_RESOURCE_DESC depthDesc{};
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = m_width;
        depthDesc.Height = m_height;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = m_msaaSamples;
        depthDesc.SampleDesc.Quality = m_msaaQuality;
        depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE depthClear{};
        depthClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthClear.DepthStencil.Depth = 1.0f;
        depthClear.DepthStencil.Stencil = 0;

        m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear, IID_PPV_ARGS(&m_depthStencil));
        m_device->CreateDepthStencilView(m_depthStencil, nullptr, m_dsvHandle);
    }

    void CreateRootSignature()
    {
        D3D12_ROOT_PARAMETER parameters[6]{};

        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[0].Descriptor.ShaderRegister = 0;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[1].Descriptor.ShaderRegister = 1;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[2].Descriptor.ShaderRegister = 2;
        parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[3].Descriptor.ShaderRegister = 0;
        parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[4].Descriptor.ShaderRegister = 1;
        parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[5].Descriptor.ShaderRegister = 2;
        parameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = 6;
        desc.pParameters = parameters;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* signatureBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;

        HRESULT result = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);

        if (FAILED(result))
        {
            if (errorBlob)
            {
                OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
                errorBlob->Release();
            }
            return;
        }

        m_device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));

        if (errorBlob) errorBlob->Release();
        signatureBlob->Release();
    }

    void CreatePipeline()
    {
        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;

        CompileShaderFromFile(L"../Assets/Shaders/Vertex.hlsl", "VS", "vs_5_1", &vsBlob);
        CompileShaderFromFile(L"../Assets/Shaders/Pixel.hlsl", "PS", "ps_5_1", &psBlob);

        if (!vsBlob || !psBlob)
        {
            if (vsBlob) vsBlob->Release();
            if (psBlob) psBlob->Release();
            return;
        }

        D3D12_RASTERIZER_DESC rasterizer{};
        rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizer.CullMode = D3D12_CULL_MODE_BACK;
        rasterizer.FrontCounterClockwise = FALSE;
        rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizer.DepthClipEnable = TRUE;
        rasterizer.MultisampleEnable = m_msaaSamples > 1;
        rasterizer.AntialiasedLineEnable = FALSE;
        rasterizer.ForcedSampleCount = 0;
        rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_BLEND_DESC blend{};
        blend.AlphaToCoverageEnable = FALSE;
        blend.IndependentBlendEnable = FALSE;

        D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlend{};
        renderTargetBlend.BlendEnable = FALSE;
        renderTargetBlend.LogicOpEnable = FALSE;
        renderTargetBlend.SrcBlend = D3D12_BLEND_ONE;
        renderTargetBlend.DestBlend = D3D12_BLEND_ZERO;
        renderTargetBlend.BlendOp = D3D12_BLEND_OP_ADD;
        renderTargetBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        renderTargetBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        renderTargetBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        renderTargetBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
        renderTargetBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        blend.RenderTarget[0] = renderTargetBlend;

        D3D12_DEPTH_STENCIL_DESC depth{};
        depth.DepthEnable = TRUE;
        depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        depth.StencilEnable = FALSE;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = m_rootSignature;
        pso.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        pso.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        pso.BlendState = blend;
        pso.SampleMask = 0xFFFFFFFF;
        pso.RasterizerState = rasterizer;
        pso.DepthStencilState = depth;
        pso.InputLayout = { nullptr, 0 };
        pso.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        pso.SampleDesc.Count = m_msaaSamples;
        pso.SampleDesc.Quality = m_msaaQuality;

        m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pipelineState));

        vsBlob->Release();
        psBlob->Release();
    }

    void CompileShaderFromFile(const wchar_t* filePath, const char* entryPoint, const char* shaderModel, ID3DBlob** blob)
    {
        ID3DBlob* errorBlob = nullptr;

        HRESULT result = D3DCompileFromFile(filePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, shaderModel, 0, 0, blob, &errorBlob);

        if (FAILED(result) && errorBlob)
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));

        if (errorBlob)
            errorBlob->Release();
    }

    void WaitForGpu()
    {
        uint64_t fenceValue = ++m_fenceValue;
        m_commandQueue->Signal(m_fence, fenceValue);

        if (m_fence->GetCompletedValue() < fenceValue)
        {
            m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }
};
