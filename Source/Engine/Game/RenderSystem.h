#pragma once
#include <cstdint>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

class RenderSystem
{
public:
	RenderSystem() = default;	

	void Initialize(HWND handle, uint32_t width, uint32_t height)
	{
		uint32_t frameCount = 2;

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferCount = frameCount;
		swapChainDesc.BufferDesc.Width = width;
		swapChainDesc.BufferDesc.Height = height;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.OutputWindow = handle;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.Windowed = true;

		D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION ,&swapChainDesc, &m_swapChain, &m_device, nullptr, &m_cmd);

		m_swapChain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&m_backBuffer);	
		m_device->CreateRenderTargetView(m_backBuffer, nullptr, &m_renderTargetView);

	}

	void BeginFrame()
	{
		float clearColor[4] = { 0.2f, 0.3f, 0.3f, 1.0f };
		m_cmd->ClearRenderTargetView(m_renderTargetView, clearColor);
	}

	void Render()
	{
	}

	void EndFrame()
	{
		m_swapChain->Present(1, 0);
	}


	void Update()
	{

	}


	private:
		ID3D11Device* m_device = nullptr;
		ID3D11DeviceContext* m_cmd = nullptr;
		ID3D11RenderTargetView* m_renderTargetView = nullptr;

		IDXGISwapChain* m_swapChain = nullptr;
		ID3D11Resource* m_backBuffer = nullptr;
};
