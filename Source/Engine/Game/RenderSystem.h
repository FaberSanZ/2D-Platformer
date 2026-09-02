#pragma once
#include <cstdint>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

class RenderSystem
{
public:
	RenderSystem() = default;	

	void Initialize(HWND handle, uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;
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


		ID3DBlob* vsBlob = nullptr;
		CompileShaderFromFile(L"../Assets/Shaders/Vertex.hlsl", "VS", "vs_5_0", &vsBlob);
		m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);

		ID3DBlob* psBlob = nullptr;
		CompileShaderFromFile(L"../Assets/Shaders/Pixel.hlsl", "PS", "ps_5_0", &psBlob);
		m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
	}

	void BeginFrame()
	{
		float clearColor[4] = { 0.2f, 0.3f, 0.3f, 1.0f };
		m_cmd->ClearRenderTargetView(m_renderTargetView, clearColor);
		m_cmd->OMSetRenderTargets(1, &m_renderTargetView, nullptr);

		D3D11_VIEWPORT viewport = {};
		viewport.Width = m_width;
		viewport.Height = m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		m_cmd->RSSetViewports(1, &viewport);
	}

	void Render()
	{
		m_cmd->VSSetShader(m_vertexShader, nullptr, 0);
		m_cmd->PSSetShader(m_pixelShader, nullptr, 0);
		// Draw call
		m_cmd->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_cmd->Draw(3, 0);
	}

	void EndFrame()
	{
		m_swapChain->Present(1, 0);
	}


	void Update()
	{

	}

	void Destroy()
	{
		if (m_renderTargetView) m_renderTargetView->Release();
		if (m_backBuffer) m_backBuffer->Release();
		if (m_swapChain) m_swapChain->Release();
		if (m_cmd) m_cmd->Release();
		if (m_device) m_device->Release();
	}


private:
	uint32_t m_width;
	uint32_t m_height;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_cmd = nullptr;
	ID3D11RenderTargetView* m_renderTargetView = nullptr;

	IDXGISwapChain* m_swapChain = nullptr;
	ID3D11Resource* m_backBuffer = nullptr;

	ID3D11VertexShader* m_vertexShader = nullptr;
	ID3D11PixelShader* m_pixelShader = nullptr;


	void CompileShaderFromFile(const wchar_t* filePath, const char* entryPoint, const char* shaderModel, ID3DBlob** blob)
	{
		D3DCompileFromFile(filePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, shaderModel, 0, 0, blob, nullptr);
	}

};
