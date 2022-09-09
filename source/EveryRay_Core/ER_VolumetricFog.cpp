#include "ER_VolumetricFog.h"
#include "ER_ShadowMapper.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_DirectionalLight.h"
#include "ER_Utility.h"
#include "ER_Camera.h"
#include "ER_QuadRenderer.h"

#define VOXEL_SIZE_X 160
#define VOXEL_SIZE_Y 90
#define VOXEL_SIZE_Z 128

static float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

namespace EveryRay_Core {
	ER_VolumetricFog::ER_VolumetricFog(ER_Core& game, const ER_DirectionalLight& aLight, const ER_ShadowMapper& aShadowMapper)
	    : ER_CoreComponent(game), mShadowMapper(aShadowMapper), mDirectionalLight(aLight)
	{	
		mPrevViewProj = XMMatrixIdentity();
	}
	
	ER_VolumetricFog::~ER_VolumetricFog()
	{
		DeleteObject(mTempVoxelInjectionTexture3D[0]);
		DeleteObject(mTempVoxelInjectionTexture3D[1]);
		DeleteObject(mFinalVoxelAccumulationTexture3D);
		DeleteObject(mBlueNoiseTexture);
	
		DeleteObject(mInjectionCS);
		DeleteObject(mAccumulationCS);
		DeleteObject(mCompositePS);

		DeleteObject(mInjectionAccumulationPassesRootSignature);
		DeleteObject(mCompositePassRootSignature);

		mMainConstantBuffer.Release();
	}
    
	void ER_VolumetricFog::Initialize()
	{
		auto rhi = GetCore()->GetRHI();
		
		mTempVoxelInjectionTexture3D[0] = rhi->CreateGPUTexture();
		mTempVoxelInjectionTexture3D[0]->CreateGPUTextureResource(rhi, VOXEL_SIZE_X, VOXEL_SIZE_Y, 1, ER_FORMAT_R16G16B16A16_FLOAT,
			ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS, 1, VOXEL_SIZE_Z);

		mTempVoxelInjectionTexture3D[1] = rhi->CreateGPUTexture();
		mTempVoxelInjectionTexture3D[1]->CreateGPUTextureResource(rhi, VOXEL_SIZE_X, VOXEL_SIZE_Y, 1, ER_FORMAT_R16G16B16A16_FLOAT,
			ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS, 1, VOXEL_SIZE_Z);

		mFinalVoxelAccumulationTexture3D = rhi->CreateGPUTexture();
		mFinalVoxelAccumulationTexture3D->CreateGPUTextureResource(rhi, VOXEL_SIZE_X, VOXEL_SIZE_Y, 1, ER_FORMAT_R16G16B16A16_FLOAT,
			ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS, 1, VOXEL_SIZE_Z);

		mBlueNoiseTexture = rhi->CreateGPUTexture();
		mBlueNoiseTexture->CreateGPUTextureResource(rhi, "content\\textures\\blueNoise.dds");

		mInjectionCS = rhi->CreateGPUShader();
		mInjectionCS->CompileShader(rhi, "content\\shaders\\VolumetricFog\\VolumetricFogMain.hlsl", "CSInjection", ER_COMPUTE);
		mAccumulationCS = rhi->CreateGPUShader();
		mAccumulationCS->CompileShader(rhi, "content\\shaders\\VolumetricFog\\VolumetricFogMain.hlsl", "CSAccumulation", ER_COMPUTE);

		mInjectionAccumulationPassesRootSignature = rhi->CreateRootSignature(3, 2);
		if (!mInjectionAccumulationPassesRootSignature)
		{
			mInjectionAccumulationPassesRootSignature->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP);
			mInjectionAccumulationPassesRootSignature->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS);
			mInjectionAccumulationPassesRootSignature->InitDescriptorTable(rhi, 0, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 3 });
			mInjectionAccumulationPassesRootSignature->InitDescriptorTable(rhi, 1, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 });
			mInjectionAccumulationPassesRootSignature->InitDescriptorTable(rhi, 2, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 });
			mInjectionAccumulationPassesRootSignature->Finalize(rhi, "Volumetric Fog: Injection + Accumulation Passes Root Signature");
		}
		
		mCompositePS = rhi->CreateGPUShader();
		mCompositePS->CompileShader(rhi, "content\\shaders\\VolumetricFog\\VolumetricFogComposite.hlsl", "PSComposite", ER_PIXEL);

		mCompositePassRootSignature = rhi->CreateRootSignature(2, 1);
		if (!mCompositePassRootSignature)
		{
			mCompositePassRootSignature->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mCompositePassRootSignature->InitDescriptorTable(rhi, 0, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 3 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mCompositePassRootSignature->InitDescriptorTable(rhi, 2, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mCompositePassRootSignature->Finalize(rhi, "Volumetric Fog: Composite Pass Root Signature");
		}

		mMainConstantBuffer.Initialize(rhi);
		mCompositeConstantBuffer.Initialize(rhi);
	}

	void ER_VolumetricFog::Draw()
	{
		if (!mEnabled)
			return;

		auto rhi = GetCore()->GetRHI();
		rhi->SetRootSignature(mInjectionAccumulationPassesRootSignature, true);

		ComputeInjection();
		ComputeAccumulation();
	}

	void ER_VolumetricFog::Update(const ER_CoreTime& gameTime)
	{
		ER_Camera* camera = (ER_Camera*)GetCore()->GetServices().FindService(ER_Camera::TypeIdClass());
		assert(camera);

		UpdateImGui();
		auto rhi = GetCore()->GetRHI();

		if (!mEnabled)
			return;

		mMainConstantBuffer.Data.InvViewProj = XMMatrixTranspose(XMMatrixInverse(nullptr, camera->ViewMatrix() * camera->ProjectionMatrix()));
		mMainConstantBuffer.Data.PrevViewProj = mPrevViewProj;
		mMainConstantBuffer.Data.ShadowMatrix = mShadowMapper.GetViewMatrix(0) * mShadowMapper.GetProjectionMatrix(0) /** XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix())*/;
		mMainConstantBuffer.Data.SunDirection = XMFLOAT4{ -mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f };
		mMainConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, mDirectionalLight.GetDirectionalLightIntensity() };
		mMainConstantBuffer.Data.CameraPosition = XMFLOAT4{ camera->Position().x, camera->Position().y, camera->Position().z, 1.0f };
		mMainConstantBuffer.Data.CameraNearFar = XMFLOAT4{ camera->NearPlaneDistance(), camera->FarPlaneDistance(), 0.0f, 0.0f };
		mMainConstantBuffer.Data.Anisotropy = mAnisotropy;
		mMainConstantBuffer.Data.Density = mDensity;
		mMainConstantBuffer.Data.Strength = mStrength;
		mMainConstantBuffer.Data.ThicknessFactor = mThicknessFactor;
		mMainConstantBuffer.Data.AmbientIntensity = mAmbientIntensity;
		mMainConstantBuffer.Data.PreviousFrameBlend = mPreviousFrameBlendFactor;
		mMainConstantBuffer.ApplyChanges(rhi);

		mCompositeConstantBuffer.Data.ViewProj = XMMatrixTranspose(camera->ViewMatrix() * camera->ProjectionMatrix());
		mCompositeConstantBuffer.Data.CameraNearFar = XMFLOAT4{ camera->NearPlaneDistance(), camera->FarPlaneDistance(), 0.0f, 0.0f };
		mCompositeConstantBuffer.Data.BlendingWithSceneColorFactor = mBlendingWithSceneColorFactor;
		mCompositeConstantBuffer.ApplyChanges(rhi);
		
		mPrevViewProj = mCompositeConstantBuffer.Data.ViewProj;
	}

	void ER_VolumetricFog::UpdateImGui()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("Volumetric Fog System");
		ImGui::Checkbox("Enabled", &mEnabled);
		ImGui::SliderFloat("Anisotropy", &mAnisotropy, 0.0f, 1.0f);
		ImGui::SliderFloat("Density", &mDensity, 0.1f, 10.0f);
		ImGui::SliderFloat("Strength", &mStrength, 0.0f, 50.0f);
		ImGui::SliderFloat("Thickness", &mThicknessFactor, 0.0f, 0.1f);
		ImGui::SliderFloat("Ambient Intensity", &mAmbientIntensity, 0.0f, 1.0f);
		ImGui::SliderFloat("Blending with scene", &mBlendingWithSceneColorFactor, 0.0f, 1.0f);
		ImGui::SliderFloat("Blending with previous frame", &mPreviousFrameBlendFactor, 0.0f, 0.1f);
		ImGui::End();
	}

	void ER_VolumetricFog::ComputeInjection()
	{
		auto rhi = GetCore()->GetRHI();

		int readIndex = mCurrentTexture3DRead;
		int writeIndex = !mCurrentTexture3DRead;

		if (rhi->IsPSOReady(mInjectionPassPSOName, true))
		{
			rhi->InitializePSO(mInjectionPassPSOName, true);
			rhi->SetRootSignatureToPSO(mInjectionPassPSOName, mInjectionAccumulationPassesRootSignature, true);
			rhi->SetShader(mInjectionCS);
			rhi->FinalizePSO(mInjectionPassPSOName, true);
		}
		rhi->SetPSO(mInjectionPassPSOName, true);
		// we set common injection/accumulation root signature in ER_VolumetricFog::Draw()
		rhi->SetShaderResources(ER_COMPUTE, { mShadowMapper.GetShadowTexture(0), mBlueNoiseTexture, mTempVoxelInjectionTexture3D[readIndex] }, 0, mInjectionAccumulationPassesRootSignature, 0, true);
		rhi->SetUnorderedAccessResources(ER_COMPUTE, { mTempVoxelInjectionTexture3D[writeIndex] }, 0, mInjectionAccumulationPassesRootSignature, 1, true);
		rhi->SetConstantBuffers(ER_COMPUTE, { mMainConstantBuffer.Buffer() }, 0, mInjectionAccumulationPassesRootSignature, 2, true);
		rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS });
		rhi->Dispatch(ER_CEIL(VOXEL_SIZE_X, 8), ER_CEIL(VOXEL_SIZE_Y, 8), ER_CEIL(VOXEL_SIZE_Z, 1));
		rhi->UnsetPSO();

		rhi->UnbindResourcesFromShader(ER_COMPUTE);

		mCurrentTexture3DRead = !mCurrentTexture3DRead;
	}

	void ER_VolumetricFog::ComputeAccumulation()
	{
		auto rhi = GetCore()->GetRHI();

		int readIndex = mCurrentTexture3DRead;

		if (rhi->IsPSOReady(mAccumulationPassPSOName, true))
		{
			rhi->InitializePSO(mAccumulationPassPSOName, true);
			rhi->SetRootSignatureToPSO(mAccumulationPassPSOName, mInjectionAccumulationPassesRootSignature, true);
			rhi->SetShader(mAccumulationCS);
			rhi->FinalizePSO(mAccumulationPassPSOName, true);
		}
		rhi->SetPSO(mAccumulationPassPSOName, true);
		// we set common injection/accumulation root signature in ER_VolumetricFog::Draw()
		rhi->SetShaderResources(ER_COMPUTE, { mShadowMapper.GetShadowTexture(0), mBlueNoiseTexture, mTempVoxelInjectionTexture3D[readIndex] }, 0, mInjectionAccumulationPassesRootSignature, 0, true);
		rhi->SetUnorderedAccessResources(ER_COMPUTE, { mFinalVoxelAccumulationTexture3D }, 0, mInjectionAccumulationPassesRootSignature, 1, true);
		rhi->SetConstantBuffers(ER_COMPUTE, { mMainConstantBuffer.Buffer() }, 0, mInjectionAccumulationPassesRootSignature, 2, true);
		rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS });
		rhi->Dispatch(ER_CEIL(VOXEL_SIZE_X, 8), ER_CEIL(VOXEL_SIZE_Y, 8), 1);
		rhi->UnsetPSO();

		rhi->UnbindResourcesFromShader(ER_COMPUTE);
	}

	void ER_VolumetricFog::Composite(ER_RHI_GPUTexture* aRT, ER_RHI_GPUTexture* aInputColorTexture, ER_RHI_GPUTexture* aGbufferWorldPos)
	{
		assert(aGbufferWorldPos && aInputColorTexture && aRT);

		ER_QuadRenderer* quadRenderer = (ER_QuadRenderer*)mCore->GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(quadRenderer);

		auto rhi = GetCore()->GetRHI();

		if (!rhi->IsPSOReady(mCompositePassPSOName))
		{
			rhi->InitializePSO(mCompositePassPSOName);
			rhi->SetShader(mCompositePS);
			rhi->SetRenderTargetFormats({ aRT });
			rhi->SetRootSignatureToPSO(mCompositePassPSOName, mCompositePassRootSignature);
			quadRenderer->PrepareDraw(rhi);
			rhi->FinalizePSO(mCompositePassPSOName);
		}
		rhi->SetPSO(mCompositePassPSOName);
		rhi->SetRootSignature(mCompositePassRootSignature);
		rhi->SetShaderResources(ER_PIXEL, { aInputColorTexture, aGbufferWorldPos, mFinalVoxelAccumulationTexture3D }, 0, mCompositePassRootSignature, 0);
		rhi->SetConstantBuffers(ER_PIXEL, { mCompositeConstantBuffer.Buffer() }, 0, mCompositePassRootSignature, 1);
		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		quadRenderer->Draw(rhi);
		rhi->UnsetPSO();

		rhi->UnbindResourcesFromShader(ER_PIXEL);
	}
}