#include "ER_GBuffer.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_Camera.h"
#include "ER_GBufferMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_RenderingObject.h"
#include "ER_Utility.h"
#include "ER_Scene.h"

namespace EveryRay_Core {

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
	}

	void ER_GBuffer::Initialize()
	{
		auto rhi = GetCore()->GetRHI();

		mAlbedoBuffer = rhi->CreateGPUTexture();
		mAlbedoBuffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mNormalBuffer = rhi->CreateGPUTexture();
		mNormalBuffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_R16G16B16A16_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mPositionsBuffer = rhi->CreateGPUTexture();
		mPositionsBuffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_R32G32B32A32_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mExtraBuffer = rhi->CreateGPUTexture();
		mExtraBuffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mExtra2Buffer = rhi->CreateGPUTexture();
		mExtra2Buffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_R16G16B16A16_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mDepthBuffer = rhi->CreateGPUTexture();
		mDepthBuffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_D24_UNORM_S8_UINT, ER_BIND_SHADER_RESOURCE | ER_BIND_DEPTH_STENCIL);
	}

	void ER_GBuffer::Update(const ER_CoreTime& time)
	{
	}

	void ER_GBuffer::Start()
	{
		auto rhi = GetCore()->GetRHI();

		float color[4] = { 0,0,0,0 };

		rhi->SetRenderTargets({ mAlbedoBuffer, mNormalBuffer, mPositionsBuffer,	mExtraBuffer, mExtra2Buffer }, mDepthBuffer);
		rhi->ClearRenderTarget(mAlbedoBuffer, color);
		rhi->ClearRenderTarget(mNormalBuffer, color);
		rhi->ClearRenderTarget(mPositionsBuffer, color);
		rhi->ClearRenderTarget(mExtraBuffer, color);
		rhi->ClearRenderTarget(mExtra2Buffer, color);
		rhi->ClearDepthStencilTarget(mDepthBuffer, 1.0f, 0);
		rhi->SetRasterizerState(ER_NO_CULLING);
	}

	void ER_GBuffer::End()
	{
		auto rhi = GetCore()->GetRHI();
		rhi->UnbindResourcesFromShader(ER_VERTEX);
		rhi->UnbindResourcesFromShader(ER_PIXEL);
		rhi->UnbindRenderTargets();
	}

	void ER_GBuffer::Draw(const ER_Scene* scene)
	{
		auto rhi = GetCore()->GetRHI();

		ER_MaterialSystems materialSystems;
		const std::string psoName = ER_MaterialHelper::gbufferMaterialName + " PSO";

		for (auto renderingObjectInfo = scene->objects.begin(); renderingObjectInfo != scene->objects.end(); renderingObjectInfo++)
		{
			ER_RenderingObject* renderingObject = renderingObjectInfo->second;
			auto materialInfo = renderingObject->GetMaterials().find(ER_MaterialHelper::gbufferMaterialName);
			if (materialInfo != renderingObject->GetMaterials().end())
			{
				ER_Material* material = materialInfo->second;
				for (int meshIndex = 0; meshIndex < renderingObject->GetMeshCount(); meshIndex++)
				{
					if (!rhi->IsPSOReady(psoName))
					{
						rhi->InitializePSO(psoName);
						material->PrepareResources();
						rhi->SetRasterizerState(ER_NO_CULLING);
						rhi->SetRenderTargetFormats({ mAlbedoBuffer, mNormalBuffer, mPositionsBuffer,	mExtraBuffer, mExtra2Buffer }, mDepthBuffer);
						rhi->FinalizePSO(psoName);
					}
					rhi->SetPSO(psoName);
					static_cast<ER_GBufferMaterial*>(material)->PrepareForRendering(materialSystems, renderingObject, meshIndex);
					renderingObject->Draw(ER_MaterialHelper::gbufferMaterialName, true, meshIndex);
					rhi->UnsetPSO();
				}
			}
		}
	}
}
