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

	static std::string psoNameNonInstanced = "ER_RHI_GPUPipelineStateObject: GBufferMaterial";
	static std::string psoNameInstanced = "ER_RHI_GPUPipelineStateObject: GBufferMaterial w/ Instancing";
	static std::string psoNameNonInstancedWireframe = "ER_RHI_GPUPipelineStateObject: GBufferMaterial (Wireframe)";
	static std::string psoNameInstancedWireframe = "ER_RHI_GPUPipelineStateObject: GBufferMaterial w/ Instancing (Wireframe)";

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

		mAlbedoBuffer = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: GBuffer Albedo RT");
		mAlbedoBuffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mNormalBuffer = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: GBuffer Normals RT");
		mNormalBuffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_R16G16B16A16_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mPositionsBuffer = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: GBuffer Positions RT");
		mPositionsBuffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_R32G32B32A32_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mExtraBuffer = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: GBuffer Extra RT");
		mExtraBuffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mExtra2Buffer = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: GBuffer Extra2 RT");
		mExtra2Buffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_R32_UINT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mDepthBuffer = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: GBuffer Depth");
		mDepthBuffer->CreateGPUTextureResource(rhi, mWidth, mHeight, 1, ER_FORMAT_D24_UNORM_S8_UINT, ER_BIND_SHADER_RESOURCE | ER_BIND_DEPTH_STENCIL);

		mRootSignature = rhi->CreateRootSignature(4, 1);
		if (mRootSignature)
		{
			mRootSignature->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mRootSignature->InitDescriptorTable(rhi, GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_PIXEL_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 6 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mRootSignature->InitDescriptorTable(rhi, GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_VERTEX_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 6 }, { 1 }, ER_RHI_SHADER_VISIBILITY_VERTEX);
			mRootSignature->InitDescriptorTable(rhi, GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
			mRootSignature->InitConstant(rhi, GBUFFER_MAT_ROOT_CONSTANT_INDEX, 2 /*we already use 2 slots for CBVs*/, 1 /* only 1 constant for LOD index*/, ER_RHI_SHADER_VISIBILITY_ALL);
			mRootSignature->Finalize(rhi, "ER_RHI_GPURootSignature: GBufferMaterial Pass", true);
		}
	}

	void ER_GBuffer::Update(const ER_CoreTime& time)
	{
		UpdateImGui();
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
		rhi->SetRasterizerState(ER_Utility::IsWireframe ? ER_WIREFRAME : ER_NO_CULLING);
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
		if (!mIsEnabled)
			return;

		rhi->SetRootSignature(mRootSignature);
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ER_MaterialSystems materialSystems;
		for (auto renderingObjectInfo = scene->objects.begin(); renderingObjectInfo != scene->objects.end(); renderingObjectInfo++)
		{
			ER_RenderingObject* renderingObject = renderingObjectInfo->second;
			if (renderingObject->IsCulled())
				continue;

			std::string& psoName = ER_Utility::IsWireframe ? psoNameNonInstancedWireframe : psoNameNonInstanced;
			if (renderingObject->IsInstanced())
				psoName = ER_Utility::IsWireframe ? psoNameInstancedWireframe : psoNameInstanced;

			auto materialInfo = renderingObject->GetMaterials().find(ER_MaterialHelper::gbufferMaterialName);
			if (materialInfo != renderingObject->GetMaterials().end())
			{
				ER_GBufferMaterial* material = static_cast<ER_GBufferMaterial*>(materialInfo->second);
				if (!rhi->IsPSOReady(psoName))
				{
					rhi->InitializePSO(psoName);
					material->PrepareShaders();
					rhi->SetRasterizerState(ER_Utility::IsWireframe ? ER_WIREFRAME : ER_NO_CULLING);
					rhi->SetBlendState(ER_NO_BLEND);
					rhi->SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
					rhi->SetRenderTargetFormats({ mAlbedoBuffer, mNormalBuffer, mPositionsBuffer, mExtraBuffer, mExtra2Buffer }, mDepthBuffer);
					rhi->SetRootSignatureToPSO(psoName, mRootSignature);
					rhi->SetTopologyTypeToPSO(psoName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					rhi->FinalizePSO(psoName);
				}
				rhi->SetPSO(psoName);
				for (int meshIndex = 0; meshIndex < renderingObject->GetMeshCount(); meshIndex++)
				{
					material->PrepareForRendering(materialSystems, renderingObject, meshIndex, mRootSignature);
					renderingObject->Draw(ER_MaterialHelper::gbufferMaterialName, true, meshIndex);
				}
			}
		}
		rhi->UnsetPSO();
	}

	void ER_GBuffer::UpdateImGui()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("GBuffer");
		ImGui::Checkbox("Enabled", &mIsEnabled);
		ImGui::End();
	}

}
