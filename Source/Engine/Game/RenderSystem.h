#pragma once
#include <cstdint>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct Vertex
{
	float position[4];
	float color[4];
};

enum class ResourceType
{
	Structured,
	Index,
	Null,
};

struct Resource
{
	ID3D11Resource* resource = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;
	ResourceType type = ResourceType::Null;

	uint32_t stride = 0;
	uint32_t count = 0;
};

struct Mesh2D
{
	Resource vertex;
	Resource index;
};

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

		Vertex vertices[] =
		{
			{  -0.5f,  0.5f, 0.0f, 1.0f, // POSITION
				0.9f, 0.0f, 0.0f, 1.0f},     // COLOR

			{  0.5f, 0.5f, 0.0f, 1.0f, // POSITION
				0.0f, 0.9f, 0.0f, 1.0f,},     // COLOR

			{ 0.5f, -0.5f, 0.0f,1.0f,  // POSITION
				0.0f, 0.0f, 0.9f, 1.0f, } ,     // COLOR

			{  -0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 0.0f, 0.9f, 1.0f,}
		};

		uint32_t indices[] =
		{
			0, 1, 2,
			0, 2, 3
		};

		m_triangleMesh = CreateMesh(vertices, sizeof(vertices), indices, sizeof(indices));
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

		DrawMesh(m_triangleMesh);
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

	Mesh2D m_triangleMesh;

	void CompileShaderFromFile(const wchar_t* filePath, const char* entryPoint, const char* shaderModel, ID3DBlob** blob)
	{
		D3DCompileFromFile(filePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, shaderModel, 0, 0, blob, nullptr);
	}


	Resource CreateStructuredBuffer(const void* data, uint32_t stride, uint32_t count, bool createView = true)
	{
		Resource resource{};

		resource.type = ResourceType::Structured;
		resource.stride = stride;
		resource.count = count;

		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = stride * count;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = stride;

		D3D11_SUBRESOURCE_DATA initialData{};
		initialData.pSysMem = data;

		ID3D11Buffer* buffer = nullptr;

		m_device->CreateBuffer(&desc, data ? &initialData : nullptr, (ID3D11Buffer**)&resource.resource);


		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = count;
		m_device->CreateShaderResourceView(resource.resource, &srvDesc, &resource.srv);
		

		return resource;
	}

	Resource CreateIndexBuffer(const void* data, uint32_t stride, uint32_t count)
	{
		Resource resource{};
		resource.type = ResourceType::Index;
		resource.stride = stride;
		resource.count = count;

		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = stride * count;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_INDEX_BUFFER;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA initialData{};
		initialData.pSysMem = data;
		ID3D11Buffer* buffer = nullptr;
		m_device->CreateBuffer(&desc, data ? &initialData : nullptr, (ID3D11Buffer**)&resource.resource);

		return resource;
	}

	Mesh2D CreateMesh(void* verticesData, uint32_t verticesSize, void* indicesData, uint32_t indicesSize)
	{
		Mesh2D mesh{};

		mesh.vertex = CreateStructuredBuffer(verticesData, sizeof(Vertex), verticesSize / sizeof(Vertex));
		mesh.index = CreateIndexBuffer(indicesData, sizeof(uint32_t), indicesSize / sizeof(uint32_t));

		return mesh;
	}

	void DrawMesh(const Mesh2D& mesh)
	{
		m_cmd->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_cmd->VSSetShaderResources(0, 1, &mesh.vertex.srv);
		m_cmd->IASetIndexBuffer((ID3D11Buffer*)mesh.index.resource, DXGI_FORMAT_R32_UINT, 0);
		m_cmd->DrawIndexed(mesh.index.count, 0, 0);
	}

};
