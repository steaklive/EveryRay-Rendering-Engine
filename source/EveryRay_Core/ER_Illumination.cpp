
#include <stdio.h>

#include "ER_Illumination.h"
#include "ER_CoreTime.h"
#include "ER_Camera.h"
#include "ER_DirectionalLight.h"
#include "ER_PointLight.h"
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

#define FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_PIXEL_SRV_INDEX 0
#define FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_VERTEX_SRV_INDEX 1
#define FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2
#define FORWARD_LIGHTING_PASS_ROOT_CONSTANT_INDEX 3

#define COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 1
#define COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

namespace EveryRay_Core {

	const float voxelCascadesSizes[NUM_VOXEL_GI_CASCADES] = { 128.0, 128.0 };

	ER_Illumination::ER_Illumination(ER_Core& game, ER_Camera& camera, const ER_DirectionalLight& light, const ER_ShadowMapper& shadowMapper, const ER_Scene* scene, GIQuality quality)
		: 
		ER_CoreComponent(game),
		mCamera(camera),
		mDirectionalLight(light),
		mShadowMapper(shadowMapper),
		mCurrentGIQuality(quality),
		mLastPointLightsDataCPUHash(0)
	{
		switch (quality)
		{
		case GIQuality::GI_LOW:
			mVCTDownscaleFactor = 0.25; // we are not using VCT on this config anyway, just a placeholder
			break;
		case GIQuality::GI_MEDIUM:
			mVCTDownscaleFactor = 0.5;
			break;
		case GIQuality::GI_HIGH:
			mVCTDownscaleFactor = 0.75;
			break;
		}
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
		DeleteObject(mForwardLightingPS_Transparent);
		DeleteObject(mForwardLightingDiffuseProbesPS);
		DeleteObject(mForwardLightingSpecularProbesPS);
		DeleteObject(mForwardLightingRenderingObjectInputLayout);
		DeleteObject(mForwardLightingRenderingObjectInputLayout_Instancing);
		for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
		{
			DeleteObject(mVCTVoxelCascades3DRTs[i]);
			DeleteObject(mDebugVoxelZonesGizmos[i]);
		}
		DeleteObject(mVCTVoxelizationDebugRT);
		DeleteObject(mVCTMainRT);
		DeleteObject(mVCTUpsampleAndBlurRT);
		DeleteObject(mLocalIlluminationRT);
		DeleteObject(mFinalIlluminationRT);
		DeleteObject(mDepthBuffer);
		DeleteObject(mPointLightsBuffer);
		DeleteObject(mVCTRS);
		DeleteObject(mUpsampleAndBlurRS);
		DeleteObject(mCompositeIlluminationRS);
		DeleteObject(mDeferredLightingRS);
		DeleteObject(mVoxelizationRS);
		DeleteObject(mVoxelizationDebugRS);
		DeleteObject(mForwardLightingRS);
		DeleteObject(mDebugProbesRenderRS);

		if (mCurrentGIQuality != GIQuality::GI_LOW)
		{
			mVoxelizationDebugConstantBuffer.Release();
			mVoxelConeTracingMainConstantBuffer.Release();
		}

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
			if (mCurrentGIQuality != GIQuality::GI_LOW)
			{
				mVCTVoxelizationDebugVS = rhi->CreateGPUShader();
				mVCTVoxelizationDebugVS->CompileShader(rhi, "content\\shaders\\GI\\VoxelConeTracingVoxelizationDebug.hlsl", "VSMain", ER_VERTEX);

				mVCTVoxelizationDebugGS = rhi->CreateGPUShader();
				mVCTVoxelizationDebugGS->CompileShader(rhi, "content\\shaders\\GI\\VoxelConeTracingVoxelizationDebug.hlsl", "GSMain", ER_GEOMETRY);

				mVCTVoxelizationDebugPS = rhi->CreateGPUShader();
				mVCTVoxelizationDebugPS->CompileShader(rhi, "content\\shaders\\GI\\VoxelConeTracingVoxelizationDebug.hlsl", "PSMain", ER_PIXEL);

				mVCTMainCS = rhi->CreateGPUShader();
				mVCTMainCS->CompileShader(rhi, "content\\shaders\\GI\\VoxelConeTracingMain.hlsl", "CSMain", ER_COMPUTE);
			}

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

			mForwardLightingPS_Transparent = rhi->CreateGPUShader();
			mForwardLightingPS_Transparent->CompileShader(rhi, "content\\shaders\\ForwardLighting.hlsl", "PSMain_Transparent", ER_PIXEL);

			//mForwardLightingDiffuseProbesPS = rhi->CreateGPUShader();
			//mForwardLightingDiffuseProbesPS->CompileShader(rhi, "content\\shaders\\ForwardLighting.hlsl", "PSMain_DiffuseProbes", ER_PIXEL);
			//
			//mForwardLightingSpecularProbesPS = rhi->CreateGPUShader();
			//mForwardLightingSpecularProbesPS->CompileShader(rhi, "content\\shaders\\ForwardLighting.hlsl", "PSMain_SpecularProbes", ER_PIXEL);
		}
		
		//cbuffers
		{
			if (mCurrentGIQuality != GIQuality::GI_LOW)
			{
				mVoxelizationDebugConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Voxelization Debug CB");
				mVoxelConeTracingMainConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Voxel Cone Tracing Main CB");
			}
			mCompositeTotalIlluminationConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Composite Total Illumination CB");
			mUpsampleBlurConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Upsample+Blur CB");
			mDeferredLightingConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Deferred Lighting CB");
			mForwardLightingConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Forward Lighting CB");
			mLightProbesConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Light Probes CB");
		}

		//RTs and gizmos
		{
			if (mCurrentGIQuality != GIQuality::GI_LOW)
			{
				for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
				{
					mVCTVoxelCascades3DRTs[i] = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Voxel Cone Tracing 3D Cascade #" + std::to_wstring(i));
					mVCTVoxelCascades3DRTs[i]->CreateGPUTextureResource(rhi, voxelCascadesSizes[i], voxelCascadesSizes[i], 1u,
						ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET | ER_BIND_UNORDERED_ACCESS, 6, voxelCascadesSizes[i]);

					mVoxelCameraPositions[i] = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);

					mDebugVoxelZonesGizmos[i] = new ER_RenderableAABB(*mCore, XMFLOAT4(0.1f, 0.34f, 0.1f, 1.0f));
					float maxBB = voxelCascadesSizes[i] / mWorldVoxelScales[i] * 0.5f;
					mLocalVoxelCascadesAABBs[i].first = XMFLOAT3(-maxBB, -maxBB, -maxBB);
					mLocalVoxelCascadesAABBs[i].second = XMFLOAT3(maxBB, maxBB, maxBB);
					mDebugVoxelZonesGizmos[i]->InitializeGeometry({ mLocalVoxelCascadesAABBs[i].first, mLocalVoxelCascadesAABBs[i].second });
				}
				mVCTMainRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Voxel Cone Tracing Main RT");
				mVCTMainRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()) * mVCTDownscaleFactor, static_cast<UINT>(mCore->ScreenHeight()) * mVCTDownscaleFactor, 1u,
					ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS, 1);

				mVCTUpsampleAndBlurRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Voxel Cone Tracing Upsample+Blur RT");
				mVCTUpsampleAndBlurRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u,
					ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS, 1);

				mVCTVoxelizationDebugRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Voxel Cone Tracing Voxelization Debug RT");
				mVCTVoxelizationDebugRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u,
					ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);
			}

			mFinalIlluminationRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Final Illumination RT");
			mFinalIlluminationRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET | ER_BIND_UNORDERED_ACCESS, 1);
			
			mLocalIlluminationRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Local Illumination RT");
			mLocalIlluminationRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u,
				ER_FORMAT_R10G10B10A2_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET | ER_BIND_UNORDERED_ACCESS, 1);
			
			mDepthBuffer = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Depth Buffer (ER_Illumination)");
			mDepthBuffer->CreateGPUTextureResource(rhi, mCore->ScreenWidth(), mCore->ScreenHeight(), 1u, ER_FORMAT_D24_UNORM_S8_UINT, ER_BIND_SHADER_RESOURCE | ER_BIND_DEPTH_STENCIL);
		}

		//light buffers
		{	
			UpdatePointLightsDataCPU();

			mPointLightsBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: Point Lights Buffer");
			mPointLightsBuffer->CreateGPUBufferResource(rhi, mPointLightsDataCPU, MAX_NUM_POINT_LIGHTS, sizeof(PointLightData), true, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
		
			mLastPointLightsDataCPUHash = ER_Utility::FastHash(mPointLightsDataCPU, sizeof(PointLightData) * MAX_NUM_POINT_LIGHTS);
		}

		// Root-signatures
		{
			if (mCurrentGIQuality != GIQuality::GI_LOW)
			{
				mVoxelizationRS = rhi->CreateRootSignature(3, 2);
				if (mVoxelizationRS)
				{
					mVoxelizationRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_ALL);
					mVoxelizationRS->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SHADER_VISIBILITY_ALL);
					mVoxelizationRS->InitDescriptorTable(rhi, VOXELIZATION_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
					mVoxelizationRS->InitDescriptorTable(rhi, VOXELIZATION_MAT_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
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

			mDeferredLightingRS = rhi->CreateRootSignature(3, 3);
			if (mDeferredLightingRS)
			{
				mDeferredLightingRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->InitStaticSampler(rhi, 2, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->InitDescriptorTable(rhi, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { LIGHTING_SRV_INDEX_MAX }, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->InitDescriptorTable(rhi, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->InitDescriptorTable(rhi, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mDeferredLightingRS->Finalize(rhi, "ER_RHI_GPURootSignature: Deferred Lighting Pass");
			}

			mForwardLightingRS = rhi->CreateRootSignature(4, 3);
			if (mForwardLightingRS)
			{
				mForwardLightingRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mForwardLightingRS->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mForwardLightingRS->InitStaticSampler(rhi, 2, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mForwardLightingRS->InitDescriptorTable(rhi, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_PIXEL_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { LIGHTING_SRV_INDEX_MAX }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mForwardLightingRS->InitDescriptorTable(rhi, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_VERTEX_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { LIGHTING_SRV_INDEX_INDIRECT_INSTANCE_BUFFER }, { 1 }, ER_RHI_SHADER_VISIBILITY_VERTEX);
				mForwardLightingRS->InitDescriptorTable(rhi, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 3 }, ER_RHI_SHADER_VISIBILITY_ALL);
				mForwardLightingRS->InitConstant(rhi, FORWARD_LIGHTING_PASS_ROOT_CONSTANT_INDEX, 3 /*we already use 3 slots for CBVs*/, 1 /* only 1 constant for LOD index*/, ER_RHI_SHADER_VISIBILITY_ALL);
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

		rhi->BeginEventTag("EveryRay: Skybox rendering (main)");
		if (skybox)
		{
			skybox->Draw(mLocalIlluminationRT, nullptr, gbuffer->GetDepth());
			//skybox->DrawSun(mLocalIlluminationRT, nullptr, gbuffer->GetDepth());
		}
		rhi->EndEventTag();

		rhi->BeginEventTag("EveryRay: Deferred Lighting");
		DrawDeferredLighting(gbuffer, mLocalIlluminationRT);
		rhi->EndEventTag();

		rhi->BeginEventTag("EveryRay: Forward Lighting");
		DrawForwardLighting(gbuffer, mLocalIlluminationRT);
		rhi->EndEventTag();
	}

	// Dynamic GI based on "Interactive Indirect Illumination Using Voxel Cone Tracing" by C.Crassin et al.
	// https://research.nvidia.com/sites/default/files/pubs/2011-09_Interactive-Indirect-Illumination/GIVoxels-pg2011-authors.pdf
	// Note: static GI uses light probes (if the scene has them) and that is already built in Deferred/Forward lighting
	void ER_Illumination::DrawDynamicGlobalIllumination(ER_GBuffer* gbuffer, const ER_CoreTime& gameTime)
	{
		ER_RHI* rhi = GetCore()->GetRHI();

		if (mCurrentGIQuality == GIQuality::GI_LOW)
			return;

		if (!mIsVCTEnabled)
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

		rhi->BeginEventTag("EveryRay: Voxel Cone Tracing - Voxelization");
		{
			rhi->SetRootSignature(mVoxelizationRS);
			for (int cascade = 0; cascade < NUM_VOXEL_GI_CASCADES; cascade++)
			{
				ER_RHI_Viewport vctViewport = { 0.0f, 0.0f, voxelCascadesSizes[cascade], voxelCascadesSizes[cascade] };
				rhi->SetViewport(vctViewport);

				ER_RHI_Rect vctRect = { 0.0f, 0.0f, voxelCascadesSizes[cascade], voxelCascadesSizes[cascade] };
				rhi->SetRect(vctRect);

				rhi->ClearUAV(mVCTVoxelCascades3DRTs[cascade], clearColorBlack);

				if (rhi->GetAPI() == ER_GRAPHICS_API::DX11)
					rhi->SetRenderTargets({}, nullptr, mVCTVoxelCascades3DRTs[cascade]);
				else
					rhi->SetUnorderedAccessResources(ER_PIXEL, { mVCTVoxelCascades3DRTs[cascade] }, 0, mVoxelizationRS, VOXELIZATION_MAT_ROOT_DESCRIPTOR_TABLE_UAV_INDEX);

				const std::string materialName = ER_MaterialHelper::voxelizationMaterialName + "_" + std::to_string(cascade);
				const std::string psoName = voxelizationPSONames[cascade];

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
				//{
				//	if (cascade == 0 && mFoliageSystem)
				//		mFoliageSystem->Draw(gameTime, &mShadowMapper, FoliageRenderingPass::FOLIAGE_VOXELIZATION, {});
				//}

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
		rhi->EndEventTag();
		
		if (mVCTDebugMode == VCT_DEBUG_VOXELS)
		{
			if (rhi->GetAPI() == DX12)
				throw ER_CoreException("'Voxel Cone Tracing - Debug Voxels' is not yet supported on DX12!");

			rhi->BeginEventTag("EveryRay: Voxel Cone Tracing - Debug Voxels");
			rhi->SetRootSignature(mVoxelizationDebugRS);
			rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_POINTLIST);

			assert(mVCTVoxelsDebugSelectedCascade < NUM_VOXEL_GI_CASCADES);
			int cascade = mVCTVoxelsDebugSelectedCascade;
			{
				float scale = 1.0f / (mWorldVoxelScales[cascade] * 0.5f);
				float sizeTranslateShift = -voxelCascadesSizes[cascade] / mWorldVoxelScales[cascade] * 0.5f;
				mVoxelizationDebugConstantBuffer.Data.WorldVoxelCube =
					XMMatrixScaling(scale, scale, scale) *
					XMMatrixTranslation(
						sizeTranslateShift + mVoxelCameraPositions[cascade].x,
						sizeTranslateShift - mVoxelCameraPositions[cascade].y,
						sizeTranslateShift + mVoxelCameraPositions[cascade].z);
				mVoxelizationDebugConstantBuffer.Data.ViewProjection = XMMatrixTranspose(mCamera.ViewMatrix() * mCamera.ProjectionMatrix());
				mVoxelizationDebugConstantBuffer.ApplyChanges(rhi);

				rhi->ClearRenderTarget(mVCTVoxelizationDebugRT, clearColorBlack);
				rhi->ClearDepthStencilTarget(mDepthBuffer, 1.0f, 0);
				rhi->SetRenderTargets({ mVCTVoxelizationDebugRT }, mDepthBuffer);

				if (!rhi->IsPSOReady(mVoxelizationDebugPSOName))
				{
					rhi->InitializePSO(mVoxelizationDebugPSOName);
					rhi->SetShader(mVCTVoxelizationDebugVS);
					rhi->SetShader(mVCTVoxelizationDebugGS);
					rhi->SetShader(mVCTVoxelizationDebugPS);
					rhi->SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE::ER_DISABLED);
					rhi->SetRasterizerState(ER_RHI_RASTERIZER_STATE::ER_BACK_CULLING);
					rhi->SetTopologyTypeToPSO(mVoxelizationDebugPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_POINTLIST);
					rhi->SetEmptyInputLayout();
					rhi->SetRenderTargetFormats({ mVCTVoxelizationDebugRT }, mDepthBuffer);
					rhi->SetRootSignatureToPSO(mVoxelizationDebugPSOName, mVoxelizationDebugRS);
					rhi->FinalizePSO(mVoxelizationDebugPSOName);
				}
				rhi->SetPSO(mVoxelizationDebugPSOName);
				rhi->SetShaderResources(ER_VERTEX,	 { mVCTVoxelCascades3DRTs[cascade] }, 0, mVoxelizationDebugRS, VCT_DEBUG_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
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
			rhi->EndEventTag();
		}
		else // main pass
		{
			rhi->BeginEventTag("EveryRay: Voxel Cone Tracing - Main");
			rhi->UnbindRenderTargets();

			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
			{
				rhi->GenerateMips(mVCTVoxelCascades3DRTs[i]);
				mVoxelConeTracingMainConstantBuffer.Data.VoxelCameraPositions[i] = mVoxelCameraPositions[i];
				mVoxelConeTracingMainConstantBuffer.Data.WorldVoxelScales[i] = XMFLOAT4(mWorldVoxelScales[i], 0.0, 0.0, 0.0);
			}

			mVoxelConeTracingMainConstantBuffer.Data.CameraPos = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1);
			mVoxelConeTracingMainConstantBuffer.Data.UpsampleRatio = XMFLOAT2(1.0f / mVCTDownscaleFactor, 1.0f / mVCTDownscaleFactor);
			mVoxelConeTracingMainConstantBuffer.Data.IndirectDiffuseStrength = mVCTIndirectDiffuseStrength;
			mVoxelConeTracingMainConstantBuffer.Data.IndirectSpecularStrength = mVCTIndirectSpecularStrength;
			mVoxelConeTracingMainConstantBuffer.Data.MaxConeTraceDistance = mVCTMaxConeTraceDistance;
			mVoxelConeTracingMainConstantBuffer.Data.AOFalloff = mVCTAoFalloff;
			mVoxelConeTracingMainConstantBuffer.Data.SamplingFactor = mVCTSamplingFactor;
			mVoxelConeTracingMainConstantBuffer.Data.VoxelSampleOffset = mVCTVoxelSampleOffset;
			mVoxelConeTracingMainConstantBuffer.Data.GIPower = mVCTGIPower;
			mVoxelConeTracingMainConstantBuffer.Data.PreviousRadianceDelta = mVCTPreviousRadianceDelta;
			mVoxelConeTracingMainConstantBuffer.Data.pad0 = XMFLOAT2(0.0, 0.0);
			mVoxelConeTracingMainConstantBuffer.ApplyChanges(rhi);

			rhi->SetRootSignature(mVCTRS, true);
			if (!rhi->IsPSOReady(mVCTMainPSOName, true))
			{
				rhi->InitializePSO(mVCTMainPSOName, true);
				rhi->SetShader(mVCTMainCS);
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

			rhi->EndEventTag();
		}

		rhi->BeginEventTag("EveryRay: Voxel Cone Tracing - Upsample/Blur");
		{
			mUpsampleBlurConstantBuffer.Data.Upsample = true;
			mUpsampleBlurConstantBuffer.ApplyChanges(rhi);

			rhi->SetRootSignature(mUpsampleAndBlurRS, true);
			if (!rhi->IsPSOReady(mUpsampleBlurPSOName, true))
			{
				rhi->InitializePSO(mUpsampleBlurPSOName, true);
				rhi->SetShader(mUpsampleBlurCS);
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
		rhi->EndEventTag();
	}

	// Combine GI output (Voxel Cone Tracing) with local illumination output
	void ER_Illumination::CompositeTotalIllumination(ER_GBuffer* gbuffer)
	{
		auto rhi = GetCore()->GetRHI();

		ER_RHI_GPUTexture* currentCustomLocalIllumination = mLocalIlluminationRT;

		mCompositeTotalIlluminationConstantBuffer.Data.CompositeFlags = COMPOSITE_FLAGS_NONE;
		GBufferDebugMode currentDebugMode = gbuffer->GetCurrentDebugMode();
		if (currentDebugMode == GBUFFER_DEBUG_NONE)
		{
			if (mCurrentGIQuality == GIQuality::GI_LOW)
				mCompositeTotalIlluminationConstantBuffer.Data.CompositeFlags = COMPOSITE_FLAGS_NO_GI;
			else
			{
				if (mVCTDebugMode == VCT_DEBUG_AO)
					mCompositeTotalIlluminationConstantBuffer.Data.CompositeFlags = COMPOSITE_FLAGS_DEBUG_GI_AO;
				else if (mVCTDebugMode == VCT_DEBUG_VOXELS || mVCTDebugMode == VCT_DEBUG_IRRADIANCE)
					mCompositeTotalIlluminationConstantBuffer.Data.CompositeFlags = COMPOSITE_FLAGS_DEBUG_GI_IRRADIANCE;
			}
		}
		else
		{
			mVCTDebugMode = VCT_DEBUG_NONE;

			switch (currentDebugMode)
			{
			case GBUFFER_DEBUG_ALBEDO:
				mCompositeTotalIlluminationConstantBuffer.Data.CompositeFlags = COMPOSITE_FLAGS_DEBUG_GBUFFER_ALBEDO;
				currentCustomLocalIllumination = gbuffer->GetAlbedo();
				break;
			case GBUFFER_DEBUG_NORMALS:
				mCompositeTotalIlluminationConstantBuffer.Data.CompositeFlags = COMPOSITE_FLAGS_DEBUG_GBUFFER_NORMALS;
				currentCustomLocalIllumination = gbuffer->GetNormals();
				break;
			case GBUFFER_DEBUG_ROUGHNESS:
				mCompositeTotalIlluminationConstantBuffer.Data.CompositeFlags = COMPOSITE_FLAGS_DEBUG_GBUFFER_ROUGHNESS;
				currentCustomLocalIllumination = gbuffer->GetExtraBuffer();
				break;
			case GBUFFER_DEBUG_METALNESS:
				mCompositeTotalIlluminationConstantBuffer.Data.CompositeFlags = COMPOSITE_FLAGS_DEBUG_GBUFFER_METALNESS;
				currentCustomLocalIllumination = gbuffer->GetExtraBuffer();
				break;
			default:
				mCompositeTotalIlluminationConstantBuffer.Data.CompositeFlags = COMPOSITE_FLAGS_NONE;
			}
		}
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
		if (mCurrentGIQuality == GIQuality::GI_LOW)
		{
			rhi->SetShaderResources(ER_COMPUTE, { nullptr, currentCustomLocalIllumination }, 0,
				mCompositeIlluminationRS, COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
		}
		else
		{
			rhi->SetShaderResources(ER_COMPUTE, { mVCTDebugMode == VCT_DEBUG_VOXELS ? mVCTVoxelizationDebugRT : mVCTUpsampleAndBlurRT, currentCustomLocalIllumination }, 0,
				mCompositeIlluminationRS, COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
		}
		rhi->SetUnorderedAccessResources(ER_COMPUTE, { mFinalIlluminationRT }, 0, mCompositeIlluminationRS, COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);
		rhi->SetConstantBuffers(ER_COMPUTE, { mCompositeTotalIlluminationConstantBuffer.Buffer() }, 0, mCompositeIlluminationRS, COMPOSITE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);
		rhi->Dispatch(ER_DivideByMultiple(static_cast<UINT>(mFinalIlluminationRT->GetWidth()), 8u), ER_DivideByMultiple(static_cast<UINT>(mFinalIlluminationRT->GetHeight()), 8u), 1u);
		rhi->UnsetPSO();
		
		rhi->UnbindResourcesFromShader(ER_COMPUTE);
	}

	void ER_Illumination::DrawDebugGizmos(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs)
	{
		if (mCurrentGIQuality == GIQuality::GI_LOW)
			return;

		//voxel GI
		for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
		{
			if (mShowVCTVoxelZonesGizmos[i])
				mDebugVoxelZonesGizmos[i]->Draw(aRenderTarget, aDepth, rs);
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
		// point lights data
		{
			UpdatePointLightsDataCPU();

			UINT currentPointLightsDataCPUHash = ER_Utility::FastHash(mPointLightsDataCPU, sizeof(PointLightData) * MAX_NUM_POINT_LIGHTS);
			if (mPointLightsBuffer && (mLastPointLightsDataCPUHash != currentPointLightsDataCPUHash))
			{
				auto rhi = GetCore()->GetRHI();
				rhi->UpdateBuffer(mPointLightsBuffer, mPointLightsDataCPU, sizeof(PointLightData) * MAX_NUM_POINT_LIGHTS);

				mLastPointLightsDataCPUHash = currentPointLightsDataCPUHash;
			}
		}

		// SSS flag
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

		// voxel data
		{
			for (int cascade = 0; cascade < NUM_VOXEL_GI_CASCADES; cascade++)
			{
				if (mIsVCTVoxelCameraPositionsUpdated[cascade])
				{
					mWorldVoxelCascadesAABBs[cascade] = mLocalVoxelCascadesAABBs[cascade];
					mWorldVoxelCascadesAABBs[cascade].first.x += mVoxelCameraPositions[cascade].x;
					mWorldVoxelCascadesAABBs[cascade].first.y += mVoxelCameraPositions[cascade].y;
					mWorldVoxelCascadesAABBs[cascade].first.z += mVoxelCameraPositions[cascade].z;
					mWorldVoxelCascadesAABBs[cascade].second.x += mVoxelCameraPositions[cascade].x;
					mWorldVoxelCascadesAABBs[cascade].second.y += mVoxelCameraPositions[cascade].y;
					mWorldVoxelCascadesAABBs[cascade].second.z += mVoxelCameraPositions[cascade].z;

					if (mDebugVoxelZonesGizmos[cascade])
						mDebugVoxelZonesGizmos[cascade]->Update(mWorldVoxelCascadesAABBs[cascade]);
				}
				
				// we need it every frame because objects can be dynamic
				CPUCullObjectsAgainstVoxelCascade(scene, cascade);
			}

			UpdateVoxelCameraPosition();
		}

		UpdateImGui();
	}

	void ER_Illumination::UpdateImGui()
	{
		if (!mShowDebugWindow)
			return;

		ImGui::Begin("Illumination System");
		if (ImGui::CollapsingHeader("Global Illumination"))
		{
			if (ImGui::CollapsingHeader("Dynamic - Voxel Cone Tracing"))
			{
				ImGui::Checkbox("VCT GI Enabled", &mIsVCTEnabled);
				ImGui::SliderFloat("VCT GI Intensity", &mVCTGIPower, 0.0f, 5.0f);
				ImGui::SliderFloat("VCT Min Radiance Margin", &mVCTPreviousRadianceDelta, 0.0f, 0.1f);
				ImGui::SliderFloat("VCT Diffuse Strength", &mVCTIndirectDiffuseStrength, 0.0f, 1.0f);
				ImGui::SliderFloat("VCT Specular Strength", &mVCTIndirectSpecularStrength, 0.0f, 1.0f);
				ImGui::SliderFloat("VCT Max Cone Trace Dist", &mVCTMaxConeTraceDistance, 0.0f, 2500.0f);
				ImGui::SliderFloat("VCT AO Falloff", &mVCTAoFalloff, 0.0f, 2.0f);
				ImGui::SliderFloat("VCT Sampling Factor", &mVCTSamplingFactor, 0.01f, 3.0f);
				ImGui::SliderFloat("VCT Sample Offset", &mVCTVoxelSampleOffset, -0.1f, 0.1f);
				if (ImGui::CollapsingHeader("Voxel Cascades Properties"))
				{
					for (int cascade = 0; cascade < NUM_VOXEL_GI_CASCADES; cascade++)
					{
						std::string objectsInVolumeText = "Num objects in VCT Voxel Cascade " + std::to_string(cascade) + ": " + std::to_string(static_cast<int>(mVoxelizationObjects[cascade].size()));
						ImGui::Text(objectsInVolumeText.c_str());

						std::string name = "Voxel Cascade " + std::to_string(cascade) + " Scale";
						ImGui::SliderFloat(name.c_str(), &mWorldVoxelScales[cascade], 0.1f, 10.0f);

						std::string name2 = "DEBUG - VCT Voxel Cascade " + std::to_string(cascade) + " Gizmo (Editor)";
						ImGui::Checkbox(name2.c_str(), &mShowVCTVoxelZonesGizmos[cascade]);

						ImGui::Separator();
					}
					ImGui::Checkbox("DEBUG - Voxel Cascades Update Always", &mIsVCTAlwaysUpdated);
				}
				ImGui::SliderInt("DEBUG - None, AO, Irradiance, Voxels", &(int)mVCTDebugMode, 0, VCT_DEBUG_COUNT - 1);
				if (mVCTDebugMode == VCT_DEBUG_VOXELS)
					ImGui::SliderInt("DEBUG - Cascade Index", &mVCTVoxelsDebugSelectedCascade, 0, NUM_VOXEL_GI_CASCADES - 1);
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
		if (ImGui::CollapsingHeader("Shadow Properties"))
		{
			ImGui::SliderFloat("Cascade #0 distance", &ER_Utility::ShadowCascadeDistances[0], 0.0f, 300.0f);
			ImGui::SliderFloat("Cascade #1 distance", &ER_Utility::ShadowCascadeDistances[1], ER_Utility::ShadowCascadeDistances[0], 1000.0f);
			ImGui::SliderFloat("Cascade #2 distance", &ER_Utility::ShadowCascadeDistances[2], ER_Utility::ShadowCascadeDistances[1], 5000.0f);
			
			ImGui::Checkbox("DEBUG - Shadow cascades", &mDebugShadowCascades);
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
		if (mCurrentGIQuality == GIQuality::GI_LOW)
			return;

		for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
		{
			float halfCascadeBox = 0.5f * (voxelCascadesSizes[i] / mWorldVoxelScales[i] * 0.5f);
			XMFLOAT3 voxelGridBoundsMax = XMFLOAT3{ mVoxelCameraPositions[i].x + halfCascadeBox, mVoxelCameraPositions[i].y + halfCascadeBox, mVoxelCameraPositions[i].z + halfCascadeBox };
			XMFLOAT3 voxelGridBoundsMin = XMFLOAT3{ mVoxelCameraPositions[i].x - halfCascadeBox, mVoxelCameraPositions[i].y - halfCascadeBox, mVoxelCameraPositions[i].z - halfCascadeBox };
			
			if (mCamera.Position().x < voxelGridBoundsMin.x || mCamera.Position().y < voxelGridBoundsMin.y || mCamera.Position().z < voxelGridBoundsMin.z ||
				mCamera.Position().x > voxelGridBoundsMax.x || mCamera.Position().y > voxelGridBoundsMax.y || mCamera.Position().z > voxelGridBoundsMax.z || mIsVCTAlwaysUpdated)
			{
				mVoxelCameraPositions[i] = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);
				mIsVCTVoxelCameraPositionsUpdated[i] = true;
			}
			else
				mIsVCTVoxelCameraPositionsUpdated[i] = false;
		}
	}

	void ER_Illumination::UpdatePointLightsDataCPU()
	{
		const std::vector<ER_PointLight*>& lights = GetCore()->GetLevel()->mPointLights;
		const UINT sceneLightCount = static_cast<UINT>(lights.size());

		for (UINT i = 0; i < MAX_NUM_POINT_LIGHTS; ++i)
		{
			if (i < sceneLightCount)
			{
				mPointLightsDataCPU[i].PositionRadius = XMFLOAT4(lights[i]->GetPosition().x, lights[i]->GetPosition().y, lights[i]->GetPosition().z, lights[i]->mRadius);
				mPointLightsDataCPU[i].ColorIntensity = XMFLOAT4(lights[i]->GetColor().x, lights[i]->GetColor().y, lights[i]->GetColor().z, lights[i]->GetColor().w);
			}
			else
				mPointLightsDataCPU[i].PositionRadius = XMFLOAT4(0.0, 0.0, 0.0, -1.0);
		}
	}

	// Compute deferred lighting pass 
	void ER_Illumination::DrawDeferredLighting(ER_GBuffer* gbuffer, ER_RHI_GPUTexture* aRenderTarget)
	{
		auto rhi = mCore->GetRHI();
		assert(aRenderTarget);

		if (!aRenderTarget)
			return;

		for (size_t i = 0; i < NUM_SHADOW_CASCADES; i++)
			mDeferredLightingConstantBuffer.Data.ShadowMatrices[i] = mShadowMapper.GetViewMatrix(i) * mShadowMapper.GetProjectionMatrix(i) /** XMLoadFloat4x4(&ER_MatrixHelper::GetProjectionShadowMatrix())*/;
		mDeferredLightingConstantBuffer.Data.ViewProj = XMMatrixTranspose(mCamera.ViewMatrix() * mCamera.ProjectionMatrix());
		mDeferredLightingConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ 
			mShadowMapper.GetCameraFarShadowCascadeDistance(0), mShadowMapper.GetCameraFarShadowCascadeDistance(1), mShadowMapper.GetCameraFarShadowCascadeDistance(2),
			mDebugShadowCascades ? 1.0f : 0.0f };
		mDeferredLightingConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
		mDeferredLightingConstantBuffer.Data.SunDirection = XMFLOAT4{ -mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f };
		mDeferredLightingConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetColor().x, mDirectionalLight.GetColor().y, mDirectionalLight.GetColor().z, mDirectionalLight.mLightIntensity };
		mDeferredLightingConstantBuffer.Data.CameraPosition = XMFLOAT4{ mCamera.Position().x,mCamera.Position().y,mCamera.Position().z, 1.0f };
		mDeferredLightingConstantBuffer.Data.CameraNearFarPlanes = XMFLOAT4{ mCamera.NearPlaneDistance(), mCamera.FarPlaneDistance(), 0.0f, 0.0f };
		mDeferredLightingConstantBuffer.Data.SSSTranslucency = mSSSTranslucency;
		mDeferredLightingConstantBuffer.Data.SSSWidth = mSSSWidth;
		mDeferredLightingConstantBuffer.Data.SSSDirectionLightMaxPlane = mSSSDirectionalLightPlaneScale;
		mDeferredLightingConstantBuffer.Data.SSSAvailable = (mIsSSS && !mIsSSSCulled) ? 1.0f : -1.0f;
		mDeferredLightingConstantBuffer.Data.HasGlobalProbe = !mProbesManager->IsEnabled() && mProbesManager->AreGlobalProbesReady();
		mDeferredLightingConstantBuffer.ApplyChanges(rhi);

		if (mProbesManager->IsEnabled())
		{
			mLightProbesConstantBuffer.Data.DiffuseProbesCellsCount = mProbesManager->GetProbesCellsCount(DIFFUSE_PROBE);
			mLightProbesConstantBuffer.Data.SpecularProbesCellsCount = mProbesManager->GetProbesCellsCount(SPECULAR_PROBE);
			mLightProbesConstantBuffer.Data.SceneLightProbesBounds = XMFLOAT4{ 
				mProbesManager->GetSceneProbesVolumeMin().x,
				mProbesManager->GetSceneProbesVolumeMin().y,
				mProbesManager->GetSceneProbesVolumeMin().z, mProbesManager->Is2DCellGrid() ? -1.0f : 1.0f };
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
		rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP });
		rhi->SetUnorderedAccessResources(ER_COMPUTE, { aRenderTarget }, 0, mDeferredLightingRS, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);

		if (mProbesManager->AreGlobalProbesReady())
		{
			if (mProbesManager->IsEnabled())
				rhi->SetConstantBuffers(ER_COMPUTE, { mDeferredLightingConstantBuffer.Buffer(), mLightProbesConstantBuffer.Buffer() }, 0, mDeferredLightingRS, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);
			else
				rhi->SetConstantBuffers(ER_COMPUTE, { mDeferredLightingConstantBuffer.Buffer() }, 0, mDeferredLightingRS, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);
		}

		std::vector<ER_RHI_GPUResource*> resources(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1);
		resources[0] = gbuffer->GetAlbedo();
		resources[1] = gbuffer->GetNormals();
		resources[2] = gbuffer->GetPositions();
		resources[3] = gbuffer->GetExtraBuffer();
		resources[4] = gbuffer->GetExtra2Buffer();
		
		std::vector<ER_RHI_GPUResource*> commonResources = GetCommonLightingShaderResources();
		resources.insert(resources.end(), commonResources.begin(), commonResources.end());

		rhi->SetShaderResources(ER_COMPUTE, resources, 0, mDeferredLightingRS, DEFERRED_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);

		rhi->Dispatch(ER_DivideByMultiple(static_cast<UINT>(aRenderTarget->GetWidth()), 8u), ER_DivideByMultiple(static_cast<UINT>(aRenderTarget->GetHeight()), 8u), 1u);
		rhi->UnbindResourcesFromShader(ER_COMPUTE);
		rhi->UnsetPSO();
	}

	// Forward lighting passes for rendering objects
	void ER_Illumination::DrawForwardLighting(ER_GBuffer* gbuffer, ER_RHI_GPUTexture* aRenderTarget)
	{
		auto rhi = mCore->GetRHI();
		assert(aRenderTarget);

		if (!aRenderTarget)
			return;

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

	std::vector<ER_RHI_GPUResource*> ER_Illumination::GetCommonLightingShaderResources()
	{
		std::vector<ER_RHI_GPUResource*> resources(LIGHTING_SRV_INDEX_MAX - LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES, nullptr);
		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_CSM_START + i] = mShadowMapper.GetShadowTexture(i);

		if (mProbesManager->AreGlobalProbesReady())
		{
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_GLOBAL_DIFFUSE_PROBE] = mProbesManager->GetGlobalDiffuseProbe()->GetCubemapTexture();
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_DIFFUSE_PROBES_CELLS_INDICES] = mProbesManager->IsEnabled() ? mProbesManager->GetDiffuseProbesCellsIndicesBuffer() : nullptr;
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_DIFFUSE_PROBES_SH_COEFFICIENTS] = mProbesManager->IsEnabled() ? mProbesManager->GetDiffuseProbesSphericalHarmonicsCoefficientsBuffer() : nullptr;
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_DIFFUSE_PROBES_POSITIONS] = mProbesManager->IsEnabled() ? mProbesManager->GetDiffuseProbesPositionsBuffer() : nullptr;
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_GLOBAL_SPECULAR_PROBE] = mProbesManager->GetGlobalSpecularProbe()->GetCubemapTexture();
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_SPECULAR_PROBES_CULLED] = mProbesManager->IsEnabled() ? mProbesManager->GetCulledSpecularProbesTextureArray() : nullptr;
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_SPECULAR_PROBES_CELLS_INDICES] = mProbesManager->IsEnabled() ? mProbesManager->GetSpecularProbesCellsIndicesBuffer() : nullptr;
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_SPECULAR_PROBES_ARRAY_INDICES] = mProbesManager->IsEnabled() ? mProbesManager->GetSpecularProbesTexArrayIndicesBuffer() : nullptr;
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_SPECULAR_PROBES_POSITIONS] = mProbesManager->IsEnabled() ? mProbesManager->GetSpecularProbesPositionsBuffer() : nullptr;
			resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_INTEGRATION_MAP] = mProbesManager->GetIntegrationMap();
		}

		resources[-(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1) + LIGHTING_SRV_INDEX_POINT_LIGHTS] = mPointLightsBuffer;

		return resources;
	}

	void ER_Illumination::PreparePipelineForForwardLighting(ER_RenderingObject* aObj)
	{
		auto rhi = mCore->GetRHI();

		std::string psoName = mForwardLightingPSOName;
		if (!aObj->IsTransparent())
		{
			if (aObj->IsInstanced())
				psoName = !ER_Utility::IsWireframe ? mForwardLightingInstancingPSOName : mForwardLightingInstancingWireframePSOName;
			else
				psoName = !ER_Utility::IsWireframe ? mForwardLightingPSOName : mForwardLightingWireframePSOName;
		}
		else
		{
			if (aObj->IsInstanced())
				psoName = !ER_Utility::IsWireframe ? mForwardLightingTransparentInstancingPSOName : mForwardLightingTransparentInstancingWireframePSOName;
			else
				psoName = !ER_Utility::IsWireframe ? mForwardLightingTransparentPSOName : mForwardLightingTransparentWireframePSOName;
		}

		if (!rhi->IsPSOReady(psoName))
		{
			rhi->InitializePSO(psoName);
			rhi->SetInputLayout(aObj->IsInstanced() ? mForwardLightingRenderingObjectInputLayout_Instancing : mForwardLightingRenderingObjectInputLayout);
			rhi->SetShader(aObj->IsInstanced() ? mForwardLightingVS_Instancing : mForwardLightingVS);
			rhi->SetShader(aObj->IsTransparent() ? mForwardLightingPS_Transparent : mForwardLightingPS);
			rhi->SetRasterizerState(ER_Utility::IsWireframe ? ER_WIREFRAME : ER_NO_CULLING);
			rhi->SetBlendState(aObj->IsTransparent() ? ER_ALPHA_BLEND : ER_NO_BLEND);
			rhi->SetDepthStencilState(ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
			rhi->SetRenderTargetFormats({ mLocalIlluminationRT }, mGbuffer->GetDepth());
			rhi->SetTopologyTypeToPSO(psoName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			rhi->SetRootSignatureToPSO(psoName, mForwardLightingRS);
			rhi->FinalizePSO(psoName);
		}
		rhi->SetPSO(psoName);
		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP });
	}

	void ER_Illumination::PrepareResourcesForForwardLighting(ER_RenderingObject* aObj, int meshIndex, int lod)
	{
		assert(mGbuffer);

		auto rhi = mCore->GetRHI();

		if (aObj && aObj->IsForwardShading())
		{
			for (size_t i = 0; i < NUM_SHADOW_CASCADES; i++)
				mForwardLightingConstantBuffer.Data.ShadowMatrices[i] = XMMatrixTranspose(mShadowMapper.GetViewMatrix(i) * mShadowMapper.GetProjectionMatrix(i) * XMLoadFloat4x4(&ER_MatrixHelper::GetProjectionShadowMatrix()));
			mForwardLightingConstantBuffer.Data.ViewProjection = XMMatrixTranspose(mCamera.ViewMatrix() * mCamera.ProjectionMatrix());
			mForwardLightingConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
			mForwardLightingConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ mShadowMapper.GetCameraFarShadowCascadeDistance(0), mShadowMapper.GetCameraFarShadowCascadeDistance(1), mShadowMapper.GetCameraFarShadowCascadeDistance(2), 1.0f };
			mForwardLightingConstantBuffer.Data.SunDirection = XMFLOAT4{ -mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f };
			mForwardLightingConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetColor().x, mDirectionalLight.GetColor().y, mDirectionalLight.GetColor().z, mDirectionalLight.mLightIntensity };
			mForwardLightingConstantBuffer.Data.CameraPosition = XMFLOAT4{ mCamera.Position().x,mCamera.Position().y,mCamera.Position().z, 1.0f };
			mForwardLightingConstantBuffer.ApplyChanges(rhi);

			if (mProbesManager->IsEnabled())
			{
				mLightProbesConstantBuffer.Data.DiffuseProbesCellsCount = mProbesManager->GetProbesCellsCount(DIFFUSE_PROBE);
				mLightProbesConstantBuffer.Data.SpecularProbesCellsCount = mProbesManager->GetProbesCellsCount(SPECULAR_PROBE);
				mLightProbesConstantBuffer.Data.SceneLightProbesBounds = XMFLOAT4{ 
					mProbesManager->GetSceneProbesVolumeMin().x,
					mProbesManager->GetSceneProbesVolumeMin().y,
					mProbesManager->GetSceneProbesVolumeMin().z, mProbesManager->Is2DCellGrid() ? -1.0f : 1.0f };
				mLightProbesConstantBuffer.Data.DistanceBetweenDiffuseProbes = mProbesManager->GetDistanceBetweenDiffuseProbes();
				mLightProbesConstantBuffer.Data.DistanceBetweenSpecularProbes = mProbesManager->GetDistanceBetweenSpecularProbes();
				mLightProbesConstantBuffer.ApplyChanges(rhi);

				if (rhi->IsRootConstantSupported())
				{
					rhi->SetConstantBuffers(ER_VERTEX, { mForwardLightingConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer(), mLightProbesConstantBuffer.Buffer() }, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
					rhi->SetRootConstant(static_cast<UINT>(lod), FORWARD_LIGHTING_PASS_ROOT_CONSTANT_INDEX);
				}
				else
				{
					rhi->SetConstantBuffers(ER_VERTEX, { 
						mForwardLightingConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer(), mLightProbesConstantBuffer.Buffer(), aObj->GetObjectsFakeRootConstantBuffer().Buffer()
						}, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
				}
				rhi->SetConstantBuffers(ER_PIXEL, { mForwardLightingConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer(), mLightProbesConstantBuffer.Buffer() }, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
			}
			else
			{
				mLightProbesConstantBuffer.Data.DiffuseProbesCellsCount = XMFLOAT4{ 0, 0, 0, 0 };
				mLightProbesConstantBuffer.Data.SpecularProbesCellsCount = XMFLOAT4{ 0, 0, 0, 0 };
				mLightProbesConstantBuffer.ApplyChanges(rhi);

				if (rhi->IsRootConstantSupported())
				{
					rhi->SetConstantBuffers(ER_VERTEX, { mForwardLightingConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer() }, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
					rhi->SetRootConstant(static_cast<UINT>(lod), FORWARD_LIGHTING_PASS_ROOT_CONSTANT_INDEX);
				}
				else
				{
					rhi->SetConstantBuffers(ER_VERTEX, { mForwardLightingConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer() }, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
					rhi->SetConstantBuffers(ER_VERTEX, { aObj->GetObjectsFakeRootConstantBuffer().Buffer() }, 3 /* we used 0-2 slots already*/, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
				}
				rhi->SetConstantBuffers(ER_PIXEL,  { mForwardLightingConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer(), mLightProbesConstantBuffer.Buffer() }, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
			}

			std::vector<ER_RHI_GPUResource*> resources(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + 1);
			resources[0] = aObj->GetTextureData(meshIndex).AlbedoMap;
			resources[1] = aObj->GetTextureData(meshIndex).NormalMap;
			resources[2] = aObj->GetTextureData(meshIndex).MetallicMap;
			resources[3] = aObj->GetTextureData(meshIndex).RoughnessMap;
			resources[4] = aObj->IsTransparent() ? aObj->GetTextureData(meshIndex).ExtraMaskMap : aObj->GetTextureData(meshIndex).HeightMap;

			std::vector<ER_RHI_GPUResource*> commonResources = GetCommonLightingShaderResources();
			resources.insert(resources.end(), commonResources.begin(), commonResources.end());

			rhi->SetShaderResources(ER_PIXEL, resources, 0, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_PIXEL_SRV_INDEX);

			if (aObj->IsGPUIndirectlyRendered())
				rhi->SetShaderResources(ER_VERTEX, { aObj->GetIndirectNewInstanceBuffer() }, LIGHTING_SRV_INDEX_INDIRECT_INSTANCE_BUFFER, mForwardLightingRS, FORWARD_LIGHTING_PASS_ROOT_DESCRIPTOR_TABLE_VERTEX_SRV_INDEX);

			// we unset PSO after all objects are rendered
		}
	}

	ER_RHI_GPUTexture* ER_Illumination::GetGBufferDepth() const
	{
		return mGbuffer->GetDepth();
	}

	void ER_Illumination::CPUCullObjectsAgainstVoxelCascade(const ER_Scene* scene, int cascade)
	{
		if (mCurrentGIQuality == GIQuality::GI_LOW)
			return;

		assert(cascade < NUM_VOXEL_GI_CASCADES);

		//TODO add instancing support
		//TODO fix repetition checks when the object AABB is bigger than the lower cascade (i.e. sponza)
		//TODO add optimization for culling objects by checking its volume size in second+ cascades
		//TODO add indirect drawing support (GPU cull)
		//TODO add multithreading per cascade
		for (auto& objectInfo : scene->objects)
		{
			if (!objectInfo.second->IsInVoxelization())
				continue;

			ER_AABB& aabbObj = objectInfo.second->GetGlobalAABB();

			bool isColliding = objectInfo.second->IsInstanced() ||
				((aabbObj.first.x <= mWorldVoxelCascadesAABBs[cascade].second.x && aabbObj.second.x >= mWorldVoxelCascadesAABBs[cascade].first.x) &&
				 (aabbObj.first.y <= mWorldVoxelCascadesAABBs[cascade].second.y && aabbObj.second.y >= mWorldVoxelCascadesAABBs[cascade].first.y) &&
				 (aabbObj.first.z <= mWorldVoxelCascadesAABBs[cascade].second.z && aabbObj.second.z >= mWorldVoxelCascadesAABBs[cascade].first.z));

			auto it = mVoxelizationObjects[cascade].find(objectInfo.first);
			if (isColliding && (it == mVoxelizationObjects[cascade].end()))
				mVoxelizationObjects[cascade].insert(objectInfo);
			else if (!isColliding && (it != mVoxelizationObjects[cascade].end()))
				mVoxelizationObjects[cascade].erase(it);
		}
	}
}