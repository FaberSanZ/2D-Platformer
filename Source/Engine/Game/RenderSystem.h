#pragma once
#include <cstdint>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include "Resource.h"
#include "Model.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")


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

		m_device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, m_msaaSamples, &m_msaaQuality);

		if (m_msaaQuality == 0)
			m_msaaSamples = 1;

		m_swapChain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&m_backBuffer);	
		m_device->CreateRenderTargetView(m_backBuffer, nullptr, &m_renderTargetView);


		D3D11_TEXTURE2D_DESC depthDesc{};
		depthDesc.Width = width;
		depthDesc.Height = height;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = m_msaaSamples;
		depthDesc.SampleDesc.Quality = m_msaaSamples > 1 ? m_msaaQuality - 1 : 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthStencil);
		m_device->CreateDepthStencilView(m_depthStencil, nullptr, &m_depthStencilView);


		D3D11_TEXTURE2D_DESC msaaDesc{};
		msaaDesc.Width = width;
		msaaDesc.Height = height;
		msaaDesc.MipLevels = 1;
		msaaDesc.ArraySize = 1;
		msaaDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		msaaDesc.SampleDesc.Count = m_msaaSamples;
		msaaDesc.SampleDesc.Quality = m_msaaSamples > 1 ? m_msaaQuality - 1 : 0;
		msaaDesc.Usage = D3D11_USAGE_DEFAULT;
		msaaDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

		m_device->CreateTexture2D(&msaaDesc, nullptr, &m_msaaRenderTarget);
		m_device->CreateRenderTargetView(m_msaaRenderTarget, nullptr, &m_msaaRenderTargetView);


		ID3DBlob* vsBlob = nullptr;
		CompileShaderFromFile(L"../Assets/Shaders/Vertex.hlsl", "VS", "vs_5_0", &vsBlob);
		m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);

		ID3DBlob* psBlob = nullptr;
		CompileShaderFromFile(L"../Assets/Shaders/Pixel.hlsl", "PS", "ps_5_0", &psBlob);
		m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);

		// camera like
		m_camera = CreateConstantBuffer(sizeof(DirectX::XMMATRIX), 1);

		D3D11_SAMPLER_DESC samplerDesc{};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

		m_device->CreateSamplerState(&samplerDesc, &m_sampler);


		m_instances = CreateStructuredBuffer(sizeof(DirectX::XMMATRIX), 16 * 16);
		DirectX::XMMATRIX model = DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity());
		UpdateGpuData(m_instances, &model, 1);
	}
	struct TestVertex
	{
		DirectX::XMFLOAT4 position;
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT2 uv;
	};

	Resource m_instances;


	//void DrawModel(const Model& model, const DirectX::XMMATRIX& transform)
	//{
	//	DirectX::XMMATRIX matrix = DirectX::XMMatrixTranspose(transform);
	//	UpdateGpuData(m_instances, &matrix, 1);

	//	for (const Mesh& mesh : model.meshes)
	//	{
	//		for (const MeshPart& part : mesh.parts)
	//		{
	//			m_cmd->VSSetShaderResources(0, 1, &part.vertex.srv);
	//			m_cmd->VSSetShaderResources(1, 1, &m_instances.srv);
	//			m_cmd->IASetIndexBuffer((ID3D11Buffer*)part.index.resource, DXGI_FORMAT_R32_UINT, 0);
	//			m_cmd->DrawIndexedInstanced(part.index.count, 1, 0, 0, 0);
	//		}
	//	}
	//}


	void DrawModel(const Model& model, const DirectX::XMFLOAT4X4* transforms, uint32_t instanceCount)
	{
		if (instanceCount == 0)
			return;

		UpdateGpuData(m_instances, transforms, instanceCount);

		for (const Mesh& mesh : model.meshes)
		{
			for (const MeshPart& part : mesh.parts)
			{
				m_cmd->VSSetShaderResources(0, 1, &part.vertex.srv);
				m_cmd->VSSetShaderResources(1, 1, &m_instances.srv);
				m_cmd->IASetIndexBuffer((ID3D11Buffer*)part.index.resource, DXGI_FORMAT_R32_UINT, 0);
				m_cmd->DrawIndexedInstanced(part.index.count, instanceCount, 0, 0, 0);
			}
		}
	}

	void DrawTestTriangle()
	{

	}

	void BeginFrame()
	{
		float clearColor[4] = { 0.9294f,0.8824f,0.8275f,1.0f };
		m_cmd->ClearRenderTargetView(m_msaaRenderTargetView, clearColor);
		m_cmd->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		m_cmd->OMSetRenderTargets(1, &m_msaaRenderTargetView, m_depthStencilView);

		D3D11_VIEWPORT viewport = {};
		viewport.Width = m_width;
		viewport.Height = m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		m_cmd->RSSetViewports(1, &viewport);

		m_cmd->VSSetShader(m_vertexShader, nullptr, 0);
		m_cmd->PSSetShader(m_pixelShader, nullptr, 0);
		m_cmd->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_cmd->VSSetConstantBuffers(0, 1, (ID3D11Buffer**)&m_camera.resource);
	}

	void Render()
	{
		DrawTestTriangle();
	}

	void EndFrame()
	{
		ID3D11RenderTargetView* nullRenderTarget = nullptr;
		m_cmd->OMSetRenderTargets(1, &nullRenderTarget, nullptr);
		m_cmd->ResolveSubresource(m_backBuffer, 0, m_msaaRenderTarget, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		m_swapChain->Present(1, 0);
	}


	void Update(const DirectX::XMMATRIX& viewProjection)
	{
		DirectX::XMMATRIX vp = DirectX::XMMatrixTranspose(viewProjection);
		UpdateGpuData(m_camera, &vp, 1);
	}

	void Update()
	{
		float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);

		DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f),DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), aspectRatio, 0.1f, 1000.0f);
		DirectX::XMMATRIX vp = DirectX::XMMatrixTranspose(view * projection);

		UpdateGpuData(m_camera, &vp, 1);
	}

	void UpdateGpuData(const Resource& resource, const void* data, uint32_t count, uint32_t offset = 0)
	{
		if (resource.type == ResourceType::Constant)
		{
			m_cmd->UpdateSubresource(resource.resource, 0, nullptr, data, 0, 0);
			return;
		}

		D3D11_BOX box{};
		box.left = offset * resource.stride;
		box.right = (offset + count) * resource.stride;
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;

		m_cmd->UpdateSubresource(resource.resource, 0, &box, data, 0, 0);
	}


	Resource CreateStructuredBuffer(uint32_t stride, uint32_t count)
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
		m_device->CreateBuffer(&desc, nullptr, (ID3D11Buffer**)&resource.resource);

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
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA initialData{};
		initialData.pSysMem = data;
		ID3D11Buffer* buffer = nullptr;
		m_device->CreateBuffer(&desc, data ? &initialData : nullptr, (ID3D11Buffer**)&resource.resource);

		return resource;
	}


	void Destroy()
	{
		if (m_camera.resource) m_camera.resource->Release();
		if (m_sampler) m_sampler->Release();
		if (m_vertexShader) m_vertexShader->Release();
		if (m_pixelShader) m_pixelShader->Release();
		if (m_renderTargetView) m_renderTargetView->Release();
		if (m_backBuffer) m_backBuffer->Release();
		if (m_swapChain) m_swapChain->Release();
		if (m_cmd) m_cmd->Release();
		if (m_device) m_device->Release();
	}


private:
	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_msaaSamples = 4;
	uint32_t m_msaaQuality = 0;
	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_cmd = nullptr;
	ID3D11RenderTargetView* m_renderTargetView = nullptr;
	IDXGISwapChain* m_swapChain = nullptr;
	ID3D11Resource* m_backBuffer = nullptr;
	ID3D11Texture2D* m_msaaRenderTarget = nullptr;
	ID3D11RenderTargetView* m_msaaRenderTargetView = nullptr;
	ID3D11VertexShader* m_vertexShader = nullptr;
	ID3D11PixelShader* m_pixelShader = nullptr;
	ID3D11SamplerState* m_sampler = nullptr;

	ID3D11Texture2D* m_depthStencil = nullptr;
	ID3D11DepthStencilView* m_depthStencilView = nullptr;

	Resource m_camera;

	void CompileShaderFromFile(const wchar_t* filePath, const char* entryPoint, const char* shaderModel, ID3DBlob** blob)
	{
		D3DCompileFromFile(filePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, shaderModel, 0, 0, blob, nullptr);
	}



	Resource CreateConstantBuffer(uint32_t stride, uint32_t count)
	{
		Resource resource{};
		resource.type = ResourceType::Constant;
		resource.stride = stride;
		resource.count = count;

		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = stride * count;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		m_device->CreateBuffer(&desc, nullptr, (ID3D11Buffer**)&resource.resource);
		return resource;
	}

	Resource CreateTextureWIC(const wchar_t* filePath)
	{
		Resource resource{};
		resource.type = ResourceType::Texture;

		IWICImagingFactory* factory = nullptr;
		IWICBitmapDecoder* decoder = nullptr;
		IWICBitmapFrameDecode* frame = nullptr;
		IWICFormatConverter* converter = nullptr;

		CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
		factory->CreateDecoderFromFilename(filePath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
		decoder->GetFrame(0, &frame);

		UINT width = 0;
		UINT height = 0;
		frame->GetSize(&width, &height);

		factory->CreateFormatConverter(&converter);
		converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

		UINT stride = width * 4;
		UINT imageSize = stride * height;

		std::vector<uint8_t> pixels(imageSize);
		converter->CopyPixels(nullptr, stride, imageSize, pixels.data());

		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA data{};
		data.pSysMem = pixels.data();
		data.SysMemPitch = stride;

		ID3D11Texture2D* texture = nullptr;
		m_device->CreateTexture2D(&desc, &data, &texture);

		resource.resource = texture;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		m_device->CreateShaderResourceView(resource.resource, &srvDesc, &resource.srv);

		if (converter) converter->Release();
		if (frame) frame->Release();
		if (decoder) decoder->Release();
		if (factory) factory->Release();

		return resource;
	}
};
