#include "stdafx.h"
#include <stdio.h>

#include "ER_Illumination.h"
#include "ER_CoreTime.h"
#include "ER_Camera.h"
#include "ER_DirectionalLight.h"
#include "ER_CoreException.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "ER_Core.h"
#include "ER_MatrixHelper.h"
#include "ER_MaterialHelper.h"
#include "ER_Utility.h"
#include "ER_VertexDeclarations.h"
#include "ER_VoxelizationMaterial.h"
#include "ER_RenderToLightProbeMaterial.h"
#include "ER_DebugLightProbeMaterial.h"
#include "ER_Scene.h"
#include "ER_GBuffer.h"
#include "ER_ShadowMapper.h"
#include "ER_FoliageManager.h"
#include "ER_RenderableAABB.h"
#include "ER_LightProbe.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_RenderingObject.h"
#include "ER_Skybox.h"
#include "ER_VolumetricFog.h"

static float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

static const std::string voxelizationPSONames[NUM_VOXEL_GI_CASCADES] =
{
	"ER_RHI_GPUPipelineStateObject: VoxelizationMaterial Pass (cascade 0)",
	"ER_RHI_GPUPipelineStateObject: VoxelizationMaterial Pass (cascade 1)",
};

#define VCT_MAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define VCT_MAIN_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 1
#define VCT_MAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

#define VCT_DEBUG_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define VCT_DEBUG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define UPSAMPLE_BLUR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define UPSAMPLE_BLUR_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 1
#define UPSAMPLE_BLUR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

#define DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 1
#define DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

#define FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 1
#define COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

namespace EveryRay_Core {

	const float voxelCascadesSizes[NUM_VOXEL_GI_CASCADES] = { 256.0f, 256.0f };

	ER_Illumination::ER_Illumination(ER_Core& game, ER_Camera& camera, const ER_DirectionalLight& light, const ER_ShadowMapper& shadowMapper, const ER_Scene* scene)
		: 
		ER_CoreComponent(game),
		mCamera(camera),
		mDirectionalLight(light),
		mShadowMapper(shadowMapper)
	{
		Initialize(scene);
	}

	ER_Illumination::~ER_Illumination()
	{
		DeleteObject(mVCTMainCS);
		DeleteObject(mUpsampleBlurCS);
		DeleteObject(mCompositeIlluminationCS);
		DeleteObject(mVCTVoxelizationDebugVS);
		DeleteObject(mVCTVoxelizationDebugGS);
		DeleteObject(mVCTVoxelizationDebugPS);
		DeleteObject(mDeferredLightingCS);
		DeleteObject(mForwardLightingVS);
		DeleteObject(mForwardLightingVS_Instancing);
		DeleteObject(mForwardLightingPS);
		DeleteObject(mForwardLightingDiffuseProbesPS);
		DeleteObject(mForwardLightingSpecularProbesPS);
		DeleteObject(mForwardLightingRenderingObjectInputLayout);
		DeleteObject(mForwardLightingRenderingObjectInputLayout_Instancing);
		DeletePointerCollection(mVCTVoxelCascades3DRTs);
		DeletePointerCollection(mDebugVoxelZonesGizmos);
		DeleteObject(mVCTVoxelizationDebugRT);
		DeleteObject(mVCTMainRT);
		DeleteObject(mVCTUpsampleAndBlurRT);
		DeleteObject(mLocalIlluminationRT);
		DeleteObject(mFinalIlluminationRT);
		DeleteObject(mDepthBuffer);
		DeleteObject(mVCTRS);
		DeleteObject(mUpsampleAndBlurRS);
		DeleteObject(mCompositeIlluminationRS);
		DeleteObject(mDeferredLightingRS);
		DeleteObject(mVoxelizationRS);
		DeleteObject(mVoxelizationDebugRS);
		DeleteObject(mForwardLightingRS);
		DeleteObject(mDebugProbesRenderRS);

		mVoxelizationDebugConstantBuffer.Release();
		mVoxelConeTracingMainConstantBuffer.Release();
		mUpsampleBlurConstantBuffer.Release();
		mDeferredLightingConstantBuffer.Release();
		mForwardLightingConstantBuffer.Release();
		mLightProbesConstantBuffer.Release();
	}

	void ER_Illumination::Initialize(const ER_Scene* scene)
	{
		if (!scene)
			return;
	
		auto rhi = GetCore()->GetRHI();

		//shaders
		{
			mVCTVoxelizationDebugVS = rhi->CreateGPUShader();
			mVCTVoxelizationDebugVS->CompileShader(rhi, "content\\shaders\\GI\\VoxelConeTracingVoxelizationDebug.hlsl", "VSMain", ER_VERTEX);

			mVCTVoxelizationDebugGS = rhi->CreateGPUShader();
			mVCTVoxelizationDebugGS->CompileShader(rhi, "content\\shaders\\GI\\VoxelConeTracingVoxelizationDebug.hlsl", "GSMain", ER_GEOMETRY);
			
			mVCTVoxelizationDebugPS = rhi->CreateGPUShader();
			mVCTVoxelizationDebugPS->CompileShader(rhi, "content\\shaders\\GI\\VoxelConeTracingVoxelizationDebug.hlsl", "PSMain", ER_PIXEL);
			
			mVCTMainCS = rhi->CreateGPUShader();
			mVCTMainCS->CompileShader(rhi, "content\\shaders\\GI\\VoxelConeTracingMain.hlsl", "CSMain", ER_COMPUTE);
			
			mUpsampleBlurCS = rhi->CreateGPUShader();
			mUpsampleBlurCS->CompileShader(rhi, "content\\shaders\\UpsampleBlur.hlsl", "CSMain", ER_COMPUTE);	
			
			mCompositeIlluminationCS = rhi->CreateGPUShader();
			mCompositeIlluminationCS->CompileShader(rhi, "content\\shaders\\CompositeIllumination.hlsl", "CSMain", ER_COMPUTE);

			mDeferredLightingCS = rhi->CreateGPUShader();
			mDeferredLightingCS->CompileShader(rhi, "content\\shaders\\DeferredLighting.hlsl", "CSMain", ER_COMPUTE);

			ER_RHI_INPUT_ELEMENT_DESC inputElementDescriptions[] =
			{
				{ "POSITION", 0, ER_FORMAT_R32G32B32A32_FLOAT, 0, 0, true, 0 },
				{ "TEXCOORD", 0, ER_FORMAT_R32G32_FLOAT, 0, 0xffffffff, true, 0 },
				{ "NORMAL", 0, ER_FORMAT_R32G32B32_FLOAT, 0, 0xffffffff, true, 0 },
				{ "TANGENT", 0, ER_FORMAT_R32G32B32_FLOAT, 0, 0xffffffff, true, 0 }
			};
			mForwardLightingRenderingObjectInputLayout = rhi->CreateInputLayout(inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
			mForwardLightingVS = rhi->CreateGPUShader();
			mForwardLightingVS->CompileShader(rhi, "content\\shaders\\ForwardLighting.hlsl", "VSMain", ER_VERTEX, mForwardLightingRenderingObjectInputLayout);

			ER_RHI_INPUT_ELEMENT_DESC inputElementDescriptionsInstancing[] =
			{
				{ "POSITION", 0, ER_FORMAT_R32G32B32A32_FLOAT, 0, 0, true, 0 },
				{ "TEXCOORD", 0, ER_FORMAT_R32G32_FLOAT, 0, 0xffffffff, true, 0 },
				{ "NORMAL", 0, ER_FORMAT_R32G32B32_FLOAT, 0, 0xffffffff, true, 0 },
				{ "TANGENT", 0, ER_FORMAT_R32G32B32_FLOAT, 0, 0xffffffff, true, 0 },
				{ "WORLD", 0, ER_FORMAT_R32G32B32A32_FLOAT, 1, 0, false, 1 },
				{ "WORLD", 1, ER_FORMAT_R32G32B32A32_FLOAT, 1, 16,false, 1 },
				{ "WORLD", 2, ER_FORMAT_R32G32B32A32_FLOAT, 1, 32,false, 1 },
				{ "WORLD", 3, ER_FORMAT_R32G32B32A32_FLOAT, 1, 48,false, 1 }
			};
			mForwardLightingRenderingObjectInputLayout_Instancing = rhi->CreateInputLayout(inputElementDescriptionsInstancing, ARRAYSIZE(inputElementDescriptionsInstancing));
			mForwardLightingVS_Instancing = rhi->CreateGPUShader();
			mForwardLightingVS_Instancing->CompileShader(rhi, "content\\shaders\\ForwardLighting.hlsl", "VSMain_instancing", ER_VERTEX, mForwardLightingRenderingObjectInputLayout_Instancing);

			mForwardLightingPS = rhi->CreateGPUShader();
			mForwardLightingPS->CompileShader(rhi, "content\\shaders\\ForwardLighting.hlsl", "PSMain", ER_PIXEL);

			mForwardLightingDiffuseProbesPS = rhi->CreateGPUShader();
			mForwardLightingDiffuseProbesPS->CompileShader(rhi, "content\\shaders\\ForwardLighting.hlsl", "PSMain_DiffuseProbes", ER_PIXEL);

			mForwardLightingSpecularProbesPS = rhi->CreateGPUShader();
			mForwardLightingSpecularProbesPS->CompileShader(rhi, "content\\shaders\\ForwardLighting.hlsl", "PSMain_SpecularProbes", ER_PIXEL);
		}
		
		//cbuffers
		{
			mVoxelizationDebugConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Voxelization Debug CB");
			mVoxelConeTracingMainConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Voxel Cone Tracing Main CB");
			mCompositeTotalIlluminationConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Composite Total Illumination CB");
			mUpsampleBlurConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Upsample+Blur CB");
			mDeferredLightingConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Deferred Lighting CB");
			mForwardLightingConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Forward Lighting CB");
			mLightProbesConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Light Probes CB");
		}

		//RTs and gizmos
		{
			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
			{
				mVCTVoxelCascades3DRTs.push_back(rhi->CreateGPUTexture("ER_RHI_GPUTexture: Voxel Cone Tracing 3D Cascade #" + std::to_string(i)));
				mVCTVoxelCascades3DRTs[i]->CreateGPUTextureResource(rhi, voxelCascadesSizes[i], voxelCascadesSizes[i], 1u,
					ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET | ER_BIND_UNORDERED_ACCESS, 6, voxelCascadesSizes[i]);
				
				mVoxelCameraPositions[i] = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);
				
				mDebugVoxelZonesGizmos.push_back(new ER_RenderableAABB(*mCore, XMFLOAT4(0.1f, 0.34f, 0.1f, 1.0f)));
				float maxBB = voxelCascadesSizes[i] / mWorldVoxelScales[i] * 0.5f;
				mVoxelCascadesAABBs[i].first = XMFLOAT3(-maxBB, -maxBB, -maxBB);
				mVoxelCascadesAABBs[i].second = XMFLOAT3(maxBB, maxBB, maxBB);
				mDebugVoxelZonesGizmos[i]->InitializeGeometry({ mVoxelCascadesAABBs[i].first, mVoxelCascadesAABBs[i].second });
			}
			mVCTMainRT = rhi->CreateGPUTexture("ER_RHI_GPUTexture: Voxel Cone Tracing Main RT");
			mVCTMainRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()) * VCT_GI_MAIN_PASS_DOWNSCALE, static_cast<UINT>(mCore->ScreenHeight()) * VCT_GI_MAIN_PASS_DOWNSCALE, 1u, 
				ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS, 1);
			
			mVCTUpsampleAndBlurRT = rhi->CreateGPUTexture("ER_RHI_GPUTexture: Voxel Cone Tracing Upsample+Blur RT");
			mVCTUpsampleAndBlurRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u,
				ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS, 1);
			
			mVCTVoxelizationDebugRT = rhi->CreateGPUTexture("ER_RHI_GPUTexture: Voxel Cone Tracing Voxelization Debug RT");
			mVCTVoxelizationDebugRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u,
				ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);
			
			mFinalIlluminationRT = rhi->CreateGPUTexture("ER_RHI_GPUTexture: Final Illumination RT");
			mFinalIlluminationRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET | ER_BIND_UNORDERED_ACCESS, 1);
			
			mLocalIlluminationRT = rhi->CreateGPUTexture("ER_RHI_GPUTexture: Local Illumination RT");
			mLocalIlluminationRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET | ER_BIND_UNORDERED_ACCESS, 1);
			
			mDepthBuffer = rhi->CreateGPUTexture("ER_RHI_GPUTexture: Depth Buffer (ER_Illumination)");
			mDepthBuffer->CreateGPUTextureResource(rhi, mCore->ScreenWidth(), mCore->ScreenHeight(), 1u, ER_FORMAT_D24_UNORM_S8_UINT, ER_BIND_SHADER_RESOURCE | ER_BIND_DEPTH_STENCIL);
		}

		// Root-signatures
		{
			mVoxelizationRS = rhi->CreateRootSignature(2, 2);
			if (mVoxelizationRS)
			{
				mVoxelizationRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_ALL);
				mVoxelizationRS->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SHADER_VISIBILITY_ALL);
				mVoxelizationRS->InitDescriptorTable(rhi, VOXELIZATION_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mVoxelizationRS->InitDescriptorTable(rhi, VOXELIZATION_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mVoxelizationRS->Finalize(rhi, "ER_RHI_GPURootSignature: VoxelizationMaterial Pass", true);
			}

			mVoxelizationDebugRS = rhi->CreateRootSignature(2, 0);
			if (mVoxelizationDebugRS)
			{
				mVoxelizationDebugRS->InitDescriptorTable(rhi, VCT_DEBUG_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mVoxelizationDebugRS->InitDescriptorTable(rhi, VCT_DEBUG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mVoxelizationDebugRS->Finalize(rhi, "ER_RHI_GPURootSignature: Voxelization Debug Pass", true);
			}

			mVCTRS = rhi->CreateRootSignature(3, 1);
			if (mVCTRS)
			{
				mVCTRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_ALL);
				mVCTRS->InitDescriptorTable(rhi, VCT_MAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 4 + NUM_VOXEL_GI_CASCADES }, ER_RHI_SHADER_VISIBILITY_ALL);
				mVCTRS->InitDescriptorTable(rhi, VCT_MAIN_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mVCTRS->InitDescriptorTable(rhi, VCT_MAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mVCTRS->Finalize(rhi, "ER_RHI_GPURootSignature: Voxel Cone Tracing Main Pass");
			}

			mUpsampleAndBlurRS = rhi->CreateRootSignature(3, 1);
			if (mUpsampleAndBlurRS)
			{
				mUpsampleAndBlurRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_ALL);
				mUpsampleAndBlurRS->InitDescriptorTable(rhi, UPSAMPLE_BLUR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mUpsampleAndBlurRS->InitDescriptorTable(rhi, UPSAMPLE_BLUR_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mUpsampleAndBlurRS->InitDescriptorTable(rhi, UPSAMPLE_BLUR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mUpsampleAndBlurRS->Finalize(rhi, "ER_RHI_GPURootSignature: Upsample & Blur Pass");
			}

			mDeferredLightingRS = rhi->CreateRootSignature(3, 2);
			if (mDeferredLightingRS)
			{
				mDeferredLightingRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->InitDescriptorTable(rhi, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 18 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->InitDescriptorTable(rhi, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->InitDescriptorTable(rhi, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->Finalize(rhi, "ER_RHI_GPURootSignature: Deferred Lighting Pass");
			}

			mForwardLightingRS = rhi->CreateRootSignature(2, 2);
			if (mForwardLightingRS)
			{
				mForwardLightingRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mForwardLightingRS->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mForwardLightingRS->InitDescriptorTable(rhi, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 18 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mForwardLightingRS->InitDescriptorTable(rhi, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mForwardLightingRS->Finalize(rhi, "ER_RHI_GPURootSignature: Forward Lighting Pass", true);
			}

			mCompositeIlluminationRS = rhi->CreateRootSignature(3, 1);
			if (mCompositeIlluminationRS)
			{
				mCompositeIlluminationRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_ALL);
				mCompositeIlluminationRS->InitDescriptorTable(rhi, COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mCompositeIlluminationRS->InitDescriptorTable(rhi, COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mCompositeIlluminationRS->InitDescriptorTable(rhi, COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mCompositeIlluminationRS->Finalize(rhi, "ER_RHI_GPURootSignature: Composite Pass");
			}

			mDebugProbesRenderRS = rhi->CreateRootSignature(2, 1);
			if (mDebugProbesRenderRS)
			{
				mDebugProbesRenderRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mDebugProbesRenderRS->InitDescriptorTable(rhi, DEBUGLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mDebugProbesRenderRS->InitDescriptorTable(rhi, DEBUGLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mDebugProbesRenderRS->Finalize(rhi, "ER_RHI_GPURootSignature: Debug light probes Pass", true);
			}
		}

		//callbacks for materials updates
		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = &mCamera;
		materialSystems.mDirectionalLight = &mDirectionalLight;
		materialSystems.mShadowMapper = &mShadowMapper;
		materialSystems.mProbesManager = mProbesManager;

		for (auto& obj : scene->objects)
		{
			if (obj.second->IsForwardShading())
				mForwardPassObjects.emplace(obj);
		}
	}

	void ER_Illumination::DrawLocalIllumination(ER_GBuffer* gbuffer, ER_Skybox* skybox)
	{	
		assert(gbuffer);
		mGbuffer = gbuffer;

		ER_RHI* rhi = GetCore()->GetRHI();
		rhi->SetRenderTargets({ mLocalIlluminationRT }, gbuffer->GetDepth());
		rhi->ClearRenderTarget(mLocalIlluminationRT, clearColorBlack);

		if (skybox)
		{
			skybox->Draw(mLocalIlluminationRT, nullptr, gbuffer->GetDepth());
			//skybox->DrawSun(mLocalIlluminationRT, nullptr, gbuffer->GetDepth());
		}

		DrawDeferredLighting(gbuffer, mLocalIlluminationRT);
		DrawForwardLighting(gbuffer, mLocalIlluminationRT);
	}

	// Dynamic GI based on "Interactive Indirect Illumination Using Voxel Cone Tracing" by C.Crassin et al.
	// https://research.nvidia.com/sites/default/files/pubs/2011-09_Interactive-Indirect-Illumination/GIVoxels-pg2011-authors.pdf
	// Note: static GI uses light probes (if the scene has them) and that is already built in Deferred/Forward lighting
	void ER_Illumination::DrawDynamicGlobalIllumination(ER_GBuffer* gbuffer, const ER_CoreTime& gameTime)
	{
		ER_RHI* rhi = GetCore()->GetRHI();

		if (!mEnabled)
		{
			rhi->ClearUAV(mVCTMainRT, clearColorBlack);
			rhi->ClearUAV(mVCTUpsampleAndBlurRT, clearColorBlack);
			return;
		}

		ER_RHI_Rect currentRect = { 0.0f, 0.0f, mCore->ScreenWidth(), mCore->ScreenHeight() };
		ER_RHI_Viewport currentViewport = rhi->GetCurrentViewport();
		ER_RHI_RASTERIZER_STATE currentRS = rhi->GetCurrentRasterizerState();
		
		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = &mCamera;
		materialSystems.mDirectionalLight = &mDirectionalLight;
		materialSystems.mShadowMapper = &mShadowMapper;

		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//voxelization
		{
			rhi->SetRootSignature(mVoxelizationRS);
			for (int cascade = 0; cascade < NUM_VOXEL_GI_CASCADES; cascade++)
			{
				ER_RHI_Viewport vctViewport = { 0.0f, 0.0f, voxelCascadesSizes[cascade], voxelCascadesSizes[cascade] };
				rhi->SetViewport(vctViewport);

				ER_RHI_Rect vctRect = { 0.0f, 0.0f, voxelCascadesSizes[cascade], voxelCascadesSizes[cascade] };
				rhi->SetRect(vctRect);

				rhi->SetRenderTargets({}, nullptr, mVCTVoxelCascades3DRTs[cascade]);
				rhi->ClearUAV(mVCTVoxelCascades3DRTs[cascade], clearColorBlack);

				std::string materialName = ER_MaterialHelper::voxelizationMaterialName + "_" + std::to_string(cascade);
				const std::string& psoName = voxelizationPSONames[cascade];

				for (auto& obj : mVoxelizationObjects[cascade])
				{
					if (!obj.second->IsInVoxelization())
						continue;

					ER_RenderingObject* renderingObject = obj.second;
					auto materialInfo = renderingObject->GetMaterials().find(materialName);
					if (materialInfo != renderingObject->GetMaterials().end())
					{
						ER_Material* material = materialInfo->second;
						for (int meshIndex = 0; meshIndex < obj.second->GetMeshCount(); meshIndex++)
						{
							if (!rhi->IsPSOReady(psoName))
							{
								rhi->InitializePSO(psoName);
								material->PrepareShaders();
								rhi->SetRasterizerState(ER_RHI_RASTERIZER_STATE::ER_NO_CULLING_NO_DEPTH_SCISSOR_ENABLED);
								rhi->SetRootSignatureToPSO(psoName, mVoxelizationRS);
								rhi->SetTopologyTypeToPSO(psoName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
								rhi->SetRenderTargetFormats({});
								rhi->FinalizePSO(psoName);
							}
							rhi->SetPSO(psoName);
							static_cast<ER_VoxelizationMaterial*>(material)->PrepareForRendering(materialSystems, renderingObject, meshIndex,
								mWorldVoxelScales[cascade], voxelCascadesSizes[cascade], mVoxelCameraPositions[cascade], mVoxelizationRS);
							renderingObject->Draw(materialName, true, meshIndex);
							rhi->UnsetPSO();
						}
					}
				}

				//voxelize extra objects
				{
					if (cascade == 0 && mFoliageSystem)
						mFoliageSystem->Draw(gameTime, &mShadowMapper, FoliageRenderingPass::FOLIAGE_VOXELIZATION, {});
				}

				//reset back
				rhi->UnbindResourcesFromShader(ER_VERTEX);
				rhi->UnbindResourcesFromShader(ER_GEOMETRY);
				rhi->UnbindResourcesFromShader(ER_PIXEL);
				rhi->UnbindRenderTargets(); //TODO maybe unset UAV, too
				rhi->SetViewport(currentViewport);
				rhi->SetRect(currentRect);
				rhi->SetRasterizerState(ER_RHI_RASTERIZER_STATE::ER_BACK_CULLING);
			}
		}
		
		//voxelization debug
		if (mShowVCTVoxelizationOnly) 
		{
			rhi->SetRootSignature(mVoxelizationDebugRS);
			rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_POINTLIST);

			int cascade = 0; //TODO fix for multiple cascades
			{
				float sizeTranslateShift = -voxelCascadesSizes[cascade] / mWorldVoxelScales[cascade] * 0.5f;
				mVoxelizationDebugConstantBuffer.Data.WorldVoxelCube =
					XMMatrixTranslation(
						sizeTranslateShift + mVoxelCameraPositions[cascade].x,
						sizeTranslateShift - mVoxelCameraPositions[cascade].y,
						sizeTranslateShift + mVoxelCameraPositions[cascade].z);
				mVoxelizationDebugConstantBuffer.Data.ViewProjection = XMMatrixTranspose(mCamera.ViewMatrix() * mCamera.ProjectionMatrix());
				mVoxelizationDebugConstantBuffer.ApplyChanges(rhi);

				rhi->ClearRenderTarget(mVCTVoxelizationDebugRT, clearColorBlack);
				rhi->ClearDepthStencilTarget(mDepthBuffer, 1.0f, 0);

				if (!rhi->IsPSOReady(mVoxelizationDebugPSOName))
				{
					rhi->InitializePSO(mVoxelizationDebugPSOName);
					rhi->SetShader(mVCTVoxelizationDebugVS);
					rhi->SetShader(mVCTVoxelizationDebugGS);
					rhi->SetShader(mVCTVoxelizationDebugPS);
					rhi->SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE::ER_DISABLED);
					rhi->SetRasterizerState(ER_RHI_RASTERIZER_STATE::ER_BACK_CULLING);
					rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_POINTLIST);
					rhi->SetEmptyInputLayout();
					rhi->SetRenderTargetFormats({ mVCTVoxelizationDebugRT }, mDepthBuffer);
					rhi->SetRootSignatureToPSO(mVoxelizationDebugPSOName, mVoxelizationDebugRS);
					rhi->FinalizePSO(mVoxelizationDebugPSOName);
				}
				rhi->SetPSO(mVoxelizationDebugPSOName);
				rhi->SetShaderResources(ER_VERTEX, { mVCTVoxelCascades3DRTs[cascade] }, 0, mVoxelizationDebugRS, VCT_DEBUG_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
				rhi->SetConstantBuffers(ER_VERTEX,   { mVoxelizationDebugConstantBuffer.Buffer() }, 0, mVoxelizationDebugRS, VCT_DEBUG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
				rhi->SetConstantBuffers(ER_GEOMETRY, { mVoxelizationDebugConstantBuffer.Buffer() }, 0, mVoxelizationDebugRS, VCT_DEBUG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
				rhi->SetConstantBuffers(ER_PIXEL,    { mVoxelizationDebugConstantBuffer.Buffer() }, 0, mVoxelizationDebugRS, VCT_DEBUG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
				rhi->DrawInstanced(voxelCascadesSizes[cascade] * voxelCascadesSizes[cascade] * voxelCascadesSizes[cascade], 1, 0, 0);
				rhi->UnsetPSO();

				rhi->UnbindRenderTargets();
				rhi->UnbindResourcesFromShader(ER_VERTEX);
				rhi->UnbindResourcesFromShader(ER_GEOMETRY);
				rhi->UnbindResourcesFromShader(ER_PIXEL);
				rhi->SetDepthStencilState(ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
			}
		}
		else // main pass
		{
			rhi->UnbindRenderTargets();

			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
			{
				rhi->GenerateMips(mVCTVoxelCascades3DRTs[i]);
				mVoxelConeTracingMainConstantBuffer.Data.VoxelCameraPositions[i] = mVoxelCameraPositions[i];
				mVoxelConeTracingMainConstantBuffer.Data.WorldVoxelScales[i] = XMFLOAT4(mWorldVoxelScales[i], 0.0, 0.0, 0.0);
			}

			rhi->SetRootSignature(mVCTRS, true);

			mVoxelConeTracingMainConstantBuffer.Data.CameraPos = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1);
			mVoxelConeTracingMainConstantBuffer.Data.UpsampleRatio = XMFLOAT2(1.0f / VCT_GI_MAIN_PASS_DOWNSCALE, 1.0f / VCT_GI_MAIN_PASS_DOWNSCALE);
			mVoxelConeTracingMainConstantBuffer.Data.IndirectDiffuseStrength = mVCTIndirectDiffuseStrength;
			mVoxelConeTracingMainConstantBuffer.Data.IndirectSpecularStrength = mVCTIndirectSpecularStrength;
			mVoxelConeTracingMainConstantBuffer.Data.MaxConeTraceDistance = mVCTMaxConeTraceDistance;
			mVoxelConeTracingMainConstantBuffer.Data.AOFalloff = mVCTAoFalloff;
			mVoxelConeTracingMainConstantBuffer.Data.SamplingFactor = mVCTSamplingFactor;
			mVoxelConeTracingMainConstantBuffer.Data.VoxelSampleOffset = mVCTVoxelSampleOffset;
			mVoxelConeTracingMainConstantBuffer.Data.GIPower = mVCTGIPower;
			mVoxelConeTracingMainConstantBuffer.Data.pad0 = XMFLOAT3(0, 0, 0);
			mVoxelConeTracingMainConstantBuffer.ApplyChanges(rhi);

			if (!rhi->IsPSOReady(mVCTMainPSOName, true))
			{
				rhi->InitializePSO(mVCTMainPSOName, true);
				rhi->SetShader(mVCTMainCS);
				rhi->SetRenderTargetFormats({});
				rhi->SetRootSignatureToPSO(mVCTMainPSOName, mVCTRS, true);
				rhi->FinalizePSO(mVCTMainPSOName, true);
			}
			rhi->SetPSO(mVCTMainPSOName, true);
			rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
			std::vector<ER_RHI_GPUResource*> resources(4 + NUM_VOXEL_GI_CASCADES);
			resources[0] = gbuffer->GetAlbedo();
			resources[1] = gbuffer->GetNormals();
			resources[2] = gbuffer->GetPositions();
			resources[3] = gbuffer->GetExtraBuffer();
			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
				resources[4 + i] = mVCTVoxelCascades3DRTs[i];
			rhi->SetShaderResources(ER_COMPUTE, resources, 0, mVCTRS, VCT_MAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
			rhi->SetUnorderedAccessResources(ER_COMPUTE, { mVCTMainRT }, 0, mVCTRS, VCT_MAIN_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);
			rhi->SetConstantBuffers(ER_COMPUTE, { mVoxelConeTracingMainConstantBuffer.Buffer() }, 0, mVCTRS, VCT_MAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);
			rhi->Dispatch(ER_DivideByMultiple(static_cast<UINT>(mVCTMainRT->GetWidth()), 8u), ER_DivideByMultiple(static_cast<UINT>(mVCTMainRT->GetHeight()), 8u), 1u);
			rhi->UnsetPSO();
			rhi->UnbindResourcesFromShader(ER_COMPUTE);
		}

		//upsample & blur
		{
			mUpsampleBlurConstantBuffer.Data.Upsample = true;
			mUpsampleBlurConstantBuffer.ApplyChanges(rhi);

			rhi->SetRootSignature(mUpsampleAndBlurRS, true);

			if (!rhi->IsPSOReady(mUpsampleBlurPSOName, true))
			{
				rhi->InitializePSO(mUpsampleBlurPSOName, true);
				rhi->SetShader(mUpsampleBlurCS);
				rhi->SetRenderTargetFormats({});
				rhi->SetRootSignatureToPSO(mUpsampleBlurPSOName, mUpsampleAndBlurRS, true);
				rhi->FinalizePSO(mUpsampleBlurPSOName, true);
			}
			rhi->SetPSO(mUpsampleBlurPSOName, true);
			rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
			rhi->SetShaderResources(ER_COMPUTE, { mVCTMainRT }, 0, mUpsampleAndBlurRS, UPSAMPLE_BLUR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
			rhi->SetUnorderedAccessResources(ER_COMPUTE, { mVCTUpsampleAndBlurRT }, 0, mUpsampleAndBlurRS, UPSAMPLE_BLUR_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);
			rhi->SetConstantBuffers(ER_COMPUTE, { mUpsampleBlurConstantBuffer.Buffer() }, 0, mUpsampleAndBlurRS, UPSAMPLE_BLUR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);
			rhi->Dispatch(ER_DivideByMultiple(static_cast<UINT>(mVCTUpsampleAndBlurRT->GetWidth()), 8u), ER_DivideByMultiple(static_cast<UINT>(mVCTUpsampleAndBlurRT->GetHeight()), 8u), 1u);
			rhi->UnsetPSO();
			rhi->UnbindResourcesFromShader(ER_COMPUTE);
		}
	}

	// Combine GI output (Voxel Cone Tracing) with local illumination output
	void ER_Illumination::CompositeTotalIllumination()
	{
		auto rhi = GetCore()->GetRHI();

		mCompositeTotalIlluminationConstantBuffer.Data.DebugVoxelAO = XMFLOAT4(mShowVCTVoxelizationOnly ? 1.0f : -1.0f, mShowVCTAmbientOcclusionOnly ? 1.0f : -1.0f, 0.0, 0.0);
		mCompositeTotalIlluminationConstantBuffer.ApplyChanges(rhi);

		// mLocalIllumination might be bound as RTV before this pass
		rhi->UnbindRenderTargets();
		rhi->SetRootSignature(mCompositeIlluminationRS, true);
		if (!rhi->IsPSOReady(mCompositeIlluminationPSOName, true))
		{
			rhi->InitializePSO(mCompositeIlluminationPSOName, true);
			rhi->SetShader(mCompositeIlluminationCS);
			rhi->SetRootSignatureToPSO(mCompositeIlluminationPSOName, mCompositeIlluminationRS, true);
			rhi->FinalizePSO(mCompositeIlluminationPSOName, true);
		}
		rhi->SetPSO(mCompositeIlluminationPSOName, true);
		rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_COMPUTE, { mShowVCTVoxelizationOnly ? mVCTVoxelizationDebugRT : mVCTUpsampleAndBlurRT, mLocalIlluminationRT }, 0, 
			mCompositeIlluminationRS, COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
		rhi->SetUnorderedAccessResources(ER_COMPUTE, { mFinalIlluminationRT }, 0, mCompositeIlluminationRS, COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);
		rhi->SetConstantBuffers(ER_COMPUTE, { mCompositeTotalIlluminationConstantBuffer.Buffer() }, 0, mCompositeIlluminationRS, COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);
		rhi->Dispatch(ER_DivideByMultiple(static_cast<UINT>(mFinalIlluminationRT->GetWidth()), 8u), ER_DivideByMultiple(static_cast<UINT>(mFinalIlluminationRT->GetHeight()), 8u), 1u);
		rhi->UnsetPSO();
		
		rhi->UnbindResourcesFromShader(ER_COMPUTE);
	}

	void ER_Illumination::DrawDebugGizmos(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPURootSignature* rs)
	{
		//voxel GI
		if (mDrawVCTVoxelZonesGizmos) 
		{
			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
			{
				mDebugVoxelZonesGizmos[i]->Draw(aRenderTarget, rs);
			}
		}

	}

	void ER_Illumination::DrawDebugProbes(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth)
	{
		//light probe system
		if (mProbesManager) {
			auto rhi = GetCore()->GetRHI();
			rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			rhi->SetRootSignature(mDebugProbesRenderRS);

			if (mDrawDiffuseProbes)
				mProbesManager->DrawDebugProbes(rhi, aRenderTarget, aDepth, DIFFUSE_PROBE, mDebugProbesRenderRS);
			if (mDrawSpecularProbes)
				mProbesManager->DrawDebugProbes(rhi, aRenderTarget, aDepth, SPECULAR_PROBE, mDebugProbesRenderRS);
		}
	}

	void ER_Illumination::Update(const ER_CoreTime& gameTime, const ER_Scene* scene)
	{
		//check SSS culling
		{
			for (auto& objectInfo : scene->objects)
			{
				if (!objectInfo.second->IsCulled() && objectInfo.second->IsRendered() && objectInfo.second->IsSeparableSubsurfaceScattering())
				{
					mIsSSSCulled = false;
					break;
				}
				else
					mIsSSSCulled = true;
			}
		}

		CPUCullObjectsAgainstVoxelCascades(scene);
		UpdateVoxelCameraPosition();
		UpdateImGui();
	}

	void ER_Illumination::UpdateImGui()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("Illumination System");
		if (ImGui::CollapsingHeader("Global Illumination"))
		{
			if (ImGui::CollapsingHeader("Dynamic - Voxel Cone Tracing"))
			{
				ImGui::Checkbox("VCT GI Enabled", &mEnabled);
				ImGui::SliderFloat("VCT GI Intensity", &mVCTGIPower, 0.0f, 5.0f);
				ImGui::SliderFloat("VCT Diffuse Strength", &mVCTIndirectDiffuseStrength, 0.0f, 1.0f);
				ImGui::SliderFloat("VCT Specular Strength", &mVCTIndirectSpecularStrength, 0.0f, 1.0f);
				ImGui::SliderFloat("VCT Max Cone Trace Dist", &mVCTMaxConeTraceDistance, 0.0f, 2500.0f);
				ImGui::SliderFloat("VCT AO Falloff", &mVCTAoFalloff, 0.0f, 2.0f);
				ImGui::SliderFloat("VCT Sampling Factor", &mVCTSamplingFactor, 0.01f, 3.0f);
				ImGui::SliderFloat("VCT Sample Offset", &mVCTVoxelSampleOffset, -0.1f, 0.1f);
				for (int cascade = 0; cascade < NUM_VOXEL_GI_CASCADES; cascade++)
				{
					std::string name = "VCT Voxel Scale Cascade " + std::to_string(cascade);
					ImGui::SliderFloat(name.c_str(), &mWorldVoxelScales[cascade], 0.1f, 10.0f);
				}
				ImGui::Separator();
				ImGui::Checkbox("DEBUG - Ambient Occlusion", &mShowVCTAmbientOcclusionOnly);
				ImGui::Checkbox("DEBUG - Voxel Texture", &mShowVCTVoxelizationOnly);
				ImGui::Checkbox("DEBUG - Voxel Cascades Gizmos (Editor)", &mDrawVCTVoxelZonesGizmos);
			}
			if (ImGui::CollapsingHeader("Static - Light Probes"))
			{
				ImGui::Checkbox("DEBUG - Skip indirect lighting", &mDebugSkipIndirectProbeLighting);
				ImGui::Separator();
				ImGui::Checkbox("DEBUG - Hide culled probes", &mProbesManager->mDebugDiscardCulledProbes);
				ImGui::Checkbox("DEBUG - Diffuse probes", &mDrawDiffuseProbes);
				ImGui::Checkbox("DEBUG - Specular probes", &mDrawSpecularProbes);
			}
		}
		ImGui::End();
	}

	void ER_Illumination::SetFoliageSystemForGI(ER_FoliageManager* foliageSystem)
	{
		mFoliageSystem = foliageSystem;
		if (mFoliageSystem)
		{
			//only first cascade due to performance
			mFoliageSystem->SetVoxelizationParams(&mWorldVoxelScales[0], &voxelCascadesSizes[0], &mVoxelCameraPositions[0]);
		}
	}

	void ER_Illumination::UpdateVoxelCameraPosition()
	{
		for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
		{
			float halfCascadeBox = 0.5f * (voxelCascadesSizes[i] / mWorldVoxelScales[i] * 0.5f);
			XMFLOAT3 voxelGridBoundsMax = XMFLOAT3{ mVoxelCameraPositions[i].x + halfCascadeBox, mVoxelCameraPositions[i].y + halfCascadeBox, mVoxelCameraPositions[i].z + halfCascadeBox };
			XMFLOAT3 voxelGridBoundsMin = XMFLOAT3{ mVoxelCameraPositions[i].x - halfCascadeBox, mVoxelCameraPositions[i].y - halfCascadeBox, mVoxelCameraPositions[i].z - halfCascadeBox };
			
			if (mCamera.Position().x < voxelGridBoundsMin.x || mCamera.Position().y < voxelGridBoundsMin.y || mCamera.Position().z < voxelGridBoundsMin.z ||
				mCamera.Position().x > voxelGridBoundsMax.x || mCamera.Position().y > voxelGridBoundsMax.y || mCamera.Position().z > voxelGridBoundsMax.z)
			{
				mVoxelCameraPositions[i] = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);
				mIsVCTVoxelCameraPositionsUpdated = true;
			}
			else
				mIsVCTVoxelCameraPositionsUpdated = false;
		}
	}

	void ER_Illumination::DrawDeferredLighting(ER_GBuffer* gbuffer, ER_RHI_GPUTexture* aRenderTarget)
	{
		static const float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		auto rhi = mCore->GetRHI();

		//compute pass
		if (aRenderTarget)
		{
			for (size_t i = 0; i < NUM_SHADOW_CASCADES; i++)
				mDeferredLightingConstantBuffer.Data.ShadowMatrices[i] = mShadowMapper.GetViewMatrix(i) * mShadowMapper.GetProjectionMatrix(i) /** XMLoadFloat4x4(&ER_MatrixHelper::GetProjectionShadowMatrix())*/;
			mDeferredLightingConstantBuffer.Data.ViewProj = XMMatrixTranspose(mCamera.ViewMatrix() * mCamera.ProjectionMatrix());
			mDeferredLightingConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ mCamera.GetCameraFarShadowCascadeDistance(0), mCamera.GetCameraFarShadowCascadeDistance(1), mCamera.GetCameraFarShadowCascadeDistance(2), 1.0f };
			mDeferredLightingConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
			mDeferredLightingConstantBuffer.Data.SunDirection = XMFLOAT4{ -mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f };
			mDeferredLightingConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, mDirectionalLight.GetDirectionalLightIntensity() };
			mDeferredLightingConstantBuffer.Data.CameraPosition = XMFLOAT4{ mCamera.Position().x,mCamera.Position().y,mCamera.Position().z, 1.0f };
			mDeferredLightingConstantBuffer.Data.CameraNearFarPlanes = XMFLOAT4{ mCamera.GetCameraNearShadowCascadeDistance(0), mCamera.GetCameraFarShadowCascadeDistance(0), 0.0f, 0.0f };
			mDeferredLightingConstantBuffer.Data.UseGlobalProbe = !mProbesManager->IsEnabled() && mProbesManager->AreGlobalProbesReady();
			mDeferredLightingConstantBuffer.Data.SkipIndirectProbeLighting = mDebugSkipIndirectProbeLighting;
			mDeferredLightingConstantBuffer.Data.SSSTranslucency = mSSSTranslucency;
			mDeferredLightingConstantBuffer.Data.SSSWidth = mSSSWidth;
			mDeferredLightingConstantBuffer.Data.SSSDirectionLightMaxPlane = mSSSDirectionalLightPlaneScale;
			mDeferredLightingConstantBuffer.Data.SSSAvailable = (mIsSSS && !mIsSSSCulled) ? 1.0f : -1.0f;
			mDeferredLightingConstantBuffer.ApplyChanges(rhi);

			if (mProbesManager->IsEnabled())
			{
				mLightProbesConstantBuffer.Data.DiffuseProbesCellsCount = mProbesManager->GetProbesCellsCount(DIFFUSE_PROBE);
				mLightProbesConstantBuffer.Data.SpecularProbesCellsCount = mProbesManager->GetProbesCellsCount(SPECULAR_PROBE);
				mLightProbesConstantBuffer.Data.SceneLightProbesBounds = XMFLOAT4{ mProbesManager->GetSceneProbesVolumeMin().x, mProbesManager->GetSceneProbesVolumeMin().y, mProbesManager->GetSceneProbesVolumeMin().z, 1.0f };
				mLightProbesConstantBuffer.Data.DistanceBetweenDiffuseProbes = mProbesManager->GetDistanceBetweenDiffuseProbes();
				mLightProbesConstantBuffer.Data.DistanceBetweenSpecularProbes = mProbesManager->GetDistanceBetweenSpecularProbes();
				mLightProbesConstantBuffer.ApplyChanges(rhi);
			}

			rhi->UnbindRenderTargets();
			
			rhi->SetRootSignature(mDeferredLightingRS, true);
			if (!rhi->IsPSOReady(mDeferredLightingPSOName, true))
			{
				rhi->InitializePSO(mDeferredLightingPSOName, true);
				rhi->SetShader(mDeferredLightingCS);
				rhi->SetRootSignatureToPSO(mDeferredLightingPSOName, mDeferredLightingRS, true);
				rhi->FinalizePSO(mDeferredLightingPSOName, true);
			}
			rhi->SetPSO(mDeferredLightingPSOName, true);
			rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS });
			rhi->SetUnorderedAccessResources(ER_COMPUTE, { aRenderTarget }, 0, mDeferredLightingRS, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);
			if (mProbesManager->AreGlobalProbesReady())
			{
				if (mProbesManager->IsEnabled())
					rhi->SetConstantBuffers(ER_COMPUTE, { mDeferredLightingConstantBuffer.Buffer(), mLightProbesConstantBuffer.Buffer() }, 0, mDeferredLightingRS, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);
				else
					rhi->SetConstantBuffers(ER_COMPUTE, { mDeferredLightingConstantBuffer.Buffer() }, 0, mDeferredLightingRS, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);

				std::vector<ER_RHI_GPUResource*> resources(18);
				resources[0] = gbuffer->GetAlbedo();
				resources[1] = gbuffer->GetNormals();
				resources[2] = gbuffer->GetPositions();
				resources[3] = gbuffer->GetExtraBuffer();
				resources[4] = gbuffer->GetExtra2Buffer();
				for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
					resources[5 + i] = mShadowMapper.GetShadowTexture(i);
				resources[8] = mProbesManager->GetGlobalDiffuseProbe()->GetCubemapTexture();
				resources[9] = mProbesManager->IsEnabled() ? mProbesManager->GetDiffuseProbesCellsIndicesBuffer() : nullptr;
				resources[10] = mProbesManager->IsEnabled() ? mProbesManager->GetDiffuseProbesSphericalHarmonicsCoefficientsBuffer() : nullptr;
				resources[11] = mProbesManager->IsEnabled() ? mProbesManager->GetDiffuseProbesPositionsBuffer() : nullptr;
				resources[12] = mProbesManager->GetGlobalSpecularProbe()->GetCubemapTexture();
				resources[13] = mProbesManager->IsEnabled() ? mProbesManager->GetCulledSpecularProbesTextureArray() : nullptr;
				resources[14] = mProbesManager->IsEnabled() ? mProbesManager->GetSpecularProbesCellsIndicesBuffer() : nullptr;
				resources[15] = mProbesManager->IsEnabled() ? mProbesManager->GetSpecularProbesTexArrayIndicesBuffer() : nullptr;
				resources[16] = mProbesManager->IsEnabled() ? mProbesManager->GetSpecularProbesPositionsBuffer() : nullptr;
				resources[17] = mProbesManager->GetIntegrationMap();
				rhi->SetShaderResources(ER_COMPUTE, resources, 0, mDeferredLightingRS, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
			}

			rhi->Dispatch(ER_DivideByMultiple(static_cast<UINT>(aRenderTarget->GetWidth()), 8u), ER_DivideByMultiple(static_cast<UINT>(aRenderTarget->GetHeight()), 8u), 1u);
			rhi->UnbindResourcesFromShader(ER_COMPUTE);
			rhi->UnsetPSO();
		}
	}

	void ER_Illumination::DrawForwardLighting(ER_GBuffer* gbuffer, ER_RHI_GPUTexture* aRenderTarget)
	{
		auto rhi = mCore->GetRHI();
		rhi->SetRenderTargets({ aRenderTarget }, gbuffer->GetDepth());
		rhi->SetRootSignature(mForwardLightingRS);
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		for (auto& obj : mForwardPassObjects)
			obj.second->Draw(ER_MaterialHelper::forwardLightingNonMaterialName);

		rhi->UnsetPSO();

		auto scene = mCore->GetLevel()->mScene;
		assert(scene);
		// Passes for all other materials (which are called "standard") that are rendered in "Forward" way into local illumination RT.
		// This can be used for all kinds of materials that are layered onto each other (transparent ones can also be rendered here).
		// TODO: We'd better render objects in batches per material in order to reduce SetRootSignature()/SetPSO() calls etc. Code below is not optimal
		for (auto& it = scene->objects.begin(); it != scene->objects.end(); it++)
		{
			for (auto& mat : it->second->GetMaterials())
			{
				if (mat.second->IsStandard())
				{
					it->second->Draw(mat.first);
					rhi->UnsetPSO();
				}
			}
		}
	}

	void ER_Illumination::PreparePipelineForForwardLighting(ER_RenderingObject* aObj)
	{
		auto rhi = mCore->GetRHI();

		std::string& psoName = aObj->IsInstanced() ? mForwardLightingInstancingPSOName : mForwardLightingPSOName;
		if (!rhi->IsPSOReady(psoName))
		{
			rhi->InitializePSO(psoName);
			rhi->SetInputLayout(aObj->IsInstanced() ? mForwardLightingRenderingObjectInputLayout_Instancing : mForwardLightingRenderingObjectInputLayout);
			rhi->SetShader(aObj->IsInstanced() ? mForwardLightingVS_Instancing : mForwardLightingVS);
			rhi->SetShader(mForwardLightingPS);
			rhi->SetRasterizerState(ER_NO_CULLING);
			rhi->SetBlendState(ER_NO_BLEND);
			rhi->SetDepthStencilState(ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
			rhi->SetRenderTargetFormats({ mLocalIlluminationRT }, mGbuffer->GetDepth());
			rhi->SetTopologyTypeToPSO(psoName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			rhi->SetRootSignatureToPSO(psoName, mForwardLightingRS);
			rhi->FinalizePSO(psoName);
		}
		rhi->SetPSO(psoName);
		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS });
	}

	void ER_Illumination::PrepareResourcesForForwardLighting(ER_RenderingObject* aObj, int meshIndex)
	{
		assert(mGbuffer);

		auto rhi = mCore->GetRHI();

		if (aObj && aObj->IsForwardShading())
		{
			for (size_t i = 0; i < NUM_SHADOW_CASCADES; i++)
				mForwardLightingConstantBuffer.Data.ShadowMatrices[i] = XMMatrixTranspose(mShadowMapper.GetViewMatrix(i) * mShadowMapper.GetProjectionMatrix(i) * XMLoadFloat4x4(&ER_MatrixHelper::GetProjectionShadowMatrix()));
			mForwardLightingConstantBuffer.Data.ViewProjection = XMMatrixTranspose(mCamera.ViewMatrix() * mCamera.ProjectionMatrix());
			mForwardLightingConstantBuffer.Data.World = XMMatrixTranspose(aObj->GetTransformationMatrix());
			mForwardLightingConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
			mForwardLightingConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ mCamera.GetCameraFarShadowCascadeDistance(0), mCamera.GetCameraFarShadowCascadeDistance(1), mCamera.GetCameraFarShadowCascadeDistance(2), 1.0f };
			mForwardLightingConstantBuffer.Data.SunDirection = XMFLOAT4{ -mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f };
			mForwardLightingConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, mDirectionalLight.GetDirectionalLightIntensity() };
			mForwardLightingConstantBuffer.Data.CameraPosition = XMFLOAT4{ mCamera.Position().x,mCamera.Position().y,mCamera.Position().z, 1.0f };
			mForwardLightingConstantBuffer.Data.UseGlobalProbe = (!mProbesManager->IsEnabled() && mProbesManager->AreGlobalProbesReady()) || aObj->GetUseIndirectGlobalLightProbeMask();
			mForwardLightingConstantBuffer.Data.SkipIndirectProbeLighting = mDebugSkipIndirectProbeLighting;
			mForwardLightingConstantBuffer.ApplyChanges(rhi);

			if (mProbesManager->IsEnabled())
			{
				mLightProbesConstantBuffer.Data.DiffuseProbesCellsCount = mProbesManager->GetProbesCellsCount(DIFFUSE_PROBE);
				mLightProbesConstantBuffer.Data.SpecularProbesCellsCount = mProbesManager->GetProbesCellsCount(SPECULAR_PROBE);
				mLightProbesConstantBuffer.Data.SceneLightProbesBounds = XMFLOAT4{ mProbesManager->GetSceneProbesVolumeMin().x, mProbesManager->GetSceneProbesVolumeMin().y, mProbesManager->GetSceneProbesVolumeMin().z, 1.0f };
				mLightProbesConstantBuffer.Data.DistanceBetweenDiffuseProbes = mProbesManager->GetDistanceBetweenDiffuseProbes();
				mLightProbesConstantBuffer.Data.DistanceBetweenSpecularProbes = mProbesManager->GetDistanceBetweenSpecularProbes();
				mLightProbesConstantBuffer.ApplyChanges(rhi);

				rhi->SetConstantBuffers(ER_VERTEX, { mForwardLightingConstantBuffer.Buffer(), mLightProbesConstantBuffer.Buffer() }, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
				rhi->SetConstantBuffers(ER_PIXEL, { mForwardLightingConstantBuffer.Buffer(), mLightProbesConstantBuffer.Buffer() }, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
			}
			else
			{
				rhi->SetConstantBuffers(ER_VERTEX, { mForwardLightingConstantBuffer.Buffer() }, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
				rhi->SetConstantBuffers(ER_PIXEL, { mForwardLightingConstantBuffer.Buffer() }, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
			}

			if (mProbesManager->AreGlobalProbesReady())
			{
				std::vector<ER_RHI_GPUResource*> resources(18);
				resources[0] = aObj->GetTextureData(meshIndex).AlbedoMap;
				resources[1] = aObj->GetTextureData(meshIndex).NormalMap;
				resources[2] = aObj->GetTextureData(meshIndex).MetallicMap;
				resources[3] = aObj->GetTextureData(meshIndex).RoughnessMap;
				resources[4] = aObj->GetTextureData(meshIndex).HeightMap;
				for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
					resources[5 + i] = mShadowMapper.GetShadowTexture(i);
				resources[8] = mProbesManager->GetGlobalDiffuseProbe()->GetCubemapTexture();
				resources[9] = mProbesManager->IsEnabled() ? mProbesManager->GetDiffuseProbesCellsIndicesBuffer() : nullptr;
				resources[10] = mProbesManager->IsEnabled() ? mProbesManager->GetDiffuseProbesSphericalHarmonicsCoefficientsBuffer() : nullptr;
				resources[11] = mProbesManager->IsEnabled() ? mProbesManager->GetDiffuseProbesPositionsBuffer() : nullptr;
				resources[12] = mProbesManager->GetGlobalSpecularProbe()->GetCubemapTexture();
				resources[13] = mProbesManager->IsEnabled() ? mProbesManager->GetCulledSpecularProbesTextureArray() : nullptr;
				resources[14] = mProbesManager->IsEnabled() ? mProbesManager->GetSpecularProbesCellsIndicesBuffer() : nullptr;
				resources[15] = mProbesManager->IsEnabled() ? mProbesManager->GetSpecularProbesTexArrayIndicesBuffer() : nullptr;
				resources[16] = mProbesManager->IsEnabled() ? mProbesManager->GetSpecularProbesPositionsBuffer() : nullptr;
				resources[17] = mProbesManager->GetIntegrationMap();
				rhi->SetShaderResources(ER_PIXEL, resources, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
			}

			// we unset PSO after all objects are rendered
		}
	}

	void ER_Illumination::CPUCullObjectsAgainstVoxelCascades(const ER_Scene* scene)
	{
		//TODO add instancing support
		//TODO fix repetition checks when the object AABB is bigger than the lower cascade (i.e. sponza)
		//TODO add optimization for culling objects by checking its volume size in second+ cascades
		//TODO add indirect drawing support (GPU cull)
		//TODO add multithreading per cascade
		for (int cascade = 0; cascade < NUM_VOXEL_GI_CASCADES; cascade++)
		{
			for (auto& objectInfo : scene->objects)
			{
				auto aabbObj = objectInfo.second->GetLocalAABB();
				XMFLOAT3 position;
				ER_MatrixHelper::GetTranslation(objectInfo.second->GetTransformationMatrix(), position);
				aabbObj.first.x += position.x;
				aabbObj.first.y += position.y;
				aabbObj.first.z += position.z;
				aabbObj.second.x += position.x;
				aabbObj.second.y += position.y;
				aabbObj.second.z += position.z;

				//check if exists in previous cascade container
				if (cascade > 0)
				{
					auto it = mVoxelizationObjects[cascade - 1].find(objectInfo.first);
					if (it != mVoxelizationObjects[cascade - 1].end())
					{
						//check if should be removed from current cascade container
						auto it2 = mVoxelizationObjects[cascade].find(objectInfo.first);
						if (it2 != mVoxelizationObjects[cascade].end())
							mVoxelizationObjects[cascade].erase(it2);
						
						continue;
					}
				}

				auto aabbCascade = mVoxelCascadesAABBs[cascade];
				aabbCascade.first.x += mVoxelCameraPositions[cascade].x;
				aabbCascade.first.y += mVoxelCameraPositions[cascade].y;
				aabbCascade.first.z += mVoxelCameraPositions[cascade].z;		
				aabbCascade.second.x += mVoxelCameraPositions[cascade].x;
				aabbCascade.second.y += mVoxelCameraPositions[cascade].y;
				aabbCascade.second.z += mVoxelCameraPositions[cascade].z;

				bool isColliding =
					(aabbObj.first.x <= aabbCascade.second.x && aabbObj.second.x >= aabbCascade.first.x) &&
					(aabbObj.first.y <= aabbCascade.second.y && aabbObj.second.y >= aabbCascade.first.y) &&
					(aabbObj.first.z <= aabbCascade.second.z && aabbObj.second.z >= aabbCascade.first.z);

				auto it = mVoxelizationObjects[cascade].find(objectInfo.first);
				if (isColliding && (it == mVoxelizationObjects[cascade].end()))
					mVoxelizationObjects[cascade].insert(objectInfo);
				else if (!isColliding && (it != mVoxelizationObjects[cascade].end()))
					mVoxelizationObjects[cascade].erase(it);
			}
		}
	}
}