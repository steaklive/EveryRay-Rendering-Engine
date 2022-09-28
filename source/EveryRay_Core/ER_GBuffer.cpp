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

	static const std::string psoNameNonInstanced = "GBufferMaterial PSO";
	static const std::string psoNameInstanced = "GBufferMaterial w/ Instancing PSO";

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
		DeleteObject(mRootSignature);
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

		mRootSignature = rhi->CreateRootSignature(2, 1);
		if (mRootSignature)
		{
			mRootSignature->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mRootSignature->InitDescriptorTable(rhi, GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 6 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mRootSignature->InitDescriptorTable(rhi, GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
			mRootSignature->Finalize(rhi, "GBufferMaterial Pass Root Signature", true);
		}
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

		rhi->SetRootSignature(mRootSignature);
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ER_MaterialSystems materialSystems;
		for (auto renderingObjectInfo = scene->objects.begin(); renderingObjectInfo != scene->objects.end(); renderingObjectInfo++)
		{
			ER_RenderingObject* renderingObject = renderingObjectInfo->second;
			if (renderingObject->IsCulled())
				continue;

			const std::string& psoName = renderingObject->IsInstanced() ? psoNameInstanced : psoNameNonInstanced;
			auto materialInfo = renderingObject->GetMaterials().find(ER_MaterialHelper::gbufferMaterialName);
			if (materialInfo != renderingObject->GetMaterials().end())
			{
				ER_GBufferMaterial* material = static_cast<ER_GBufferMaterial*>(materialInfo->second);
				for (int meshIndex = 0; meshIndex < renderingObject->GetMeshCount(); meshIndex++)
				{
					if (!rhi->IsPSOReady(psoName))
					{
						rhi->InitializePSO(psoName);
						material->PrepareShaders();
						rhi->SetRasterizerState(ER_NO_CULLING);
						rhi->SetBlendState(ER_NO_BLEND);
						rhi->SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
						rhi->SetRenderTargetFormats({ mAlbedoBuffer, mNormalBuffer, mPositionsBuffer, mExtraBuffer, mExtra2Buffer }, mDepthBuffer);
						rhi->SetRootSignatureToPSO(psoName, mRootSignature);
						rhi->SetTopologyTypeToPSO(psoName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
						rhi->FinalizePSO(psoName);
					}
					rhi->SetPSO(psoName);
					material->PrepareForRendering(materialSystems, renderingObject, meshIndex, mRootSignature);
					renderingObject->Draw(ER_MaterialHelper::gbufferMaterialName, true, meshIndex);
					rhi->UnsetPSO();
				}
			}
		}
	}
}
