#include "ER_GBuffer.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_Camera.h"
#include "ER_GBufferMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_RenderingObject.h"
#include "ER_Utility.h"
#include "ER_Scene.h"

namespace Library {

	ER_GBuffer::ER_GBuffer(ER_Core& game, ER_Camera& camera, int width, int height):
		ER_CoreComponent(game), mWidth(width), mHeight(height)
	{
	}

	ER_GBuffer::~ER_GBuffer()
	{
		DeleteObject(mAlbedoBuffer);
		DeleteObject(mNormalBuffer);
		DeleteObject(mPositionsBuffer);
		DeleteObject(mExtraBuffer);
		DeleteObject(mExtra2Buffer);
		DeleteObject(mDepthBuffer);
		ReleaseObject(mRS);
	}

	void ER_GBuffer::Initialize()
	{
		mAlbedoBuffer = new ER_GPUTexture(mCore->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		mNormalBuffer = new ER_GPUTexture(mCore->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
		mPositionsBuffer = new ER_GPUTexture(mCore->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
		mExtraBuffer = new ER_GPUTexture(mCore->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		mExtra2Buffer = new ER_GPUTexture(mCore->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
		mDepthBuffer = new ER_GPUTexture(mCore->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL);

		D3D11_RASTERIZER_DESC rasterizerStateDesc;
		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.CullMode = D3D11_CULL_NONE;
		rasterizerStateDesc.DepthClipEnable = true;
		HRESULT hr = mCore->Direct3DDevice()->CreateRasterizerState(&rasterizerStateDesc, &mRS);
		if (FAILED(hr))
		{
			throw ER_CoreException("ID3D11Device::CreateRasterizerState() in GBuffer failed.", hr);
		}
	}

	void ER_GBuffer::Update(const ER_CoreTime& time)
	{
	}

	void ER_GBuffer::Start()
	{
		float color[4] = { 0,0,0,0 };

		ID3D11RenderTargetView* rtvs[] = { mAlbedoBuffer->GetRTV(), mNormalBuffer->GetRTV(), mPositionsBuffer->GetRTV(),
			mExtraBuffer->GetRTV(), mExtra2Buffer->GetRTV() };
		mCore->Direct3DDeviceContext()->OMSetRenderTargets(5, rtvs, mDepthBuffer->GetDSV());

		mCore->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[0], color);
		mCore->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[1], color);
		mCore->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[2], color);
		mCore->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[3], color);
		mCore->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[4], color);
		mCore->Direct3DDeviceContext()->ClearDepthStencilView(mDepthBuffer->GetDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		mCore->Direct3DDeviceContext()->RSSetState(mRS);

		// Set the viewport.
		//mCore->Direct3DDeviceContext()->RSSetViewports(1, &m_viewport);

	}

	void ER_GBuffer::End()
	{
		mCore->ResetRenderTargets();
	}

	void ER_GBuffer::Draw(const ER_Scene* scene)
	{
		ER_MaterialSystems materialSystems;

		for (auto it = scene->objects.begin(); it != scene->objects.end(); it++)
		{
			auto materialInfo = it->second->GetMaterials().find(ER_MaterialHelper::gbufferMaterialName);
			if (materialInfo != it->second->GetMaterials().end())
			{
				for (int meshIndex = 0; meshIndex < it->second->GetMeshCount(); meshIndex++)
				{
					static_cast<ER_GBufferMaterial*>(materialInfo->second)->PrepareForRendering(materialSystems, it->second, meshIndex);
					it->second->Draw(ER_MaterialHelper::gbufferMaterialName, true, meshIndex);
				}
			}
		}
	}

}
