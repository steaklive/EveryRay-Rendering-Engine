#include "ER_VolumetricFog.h"
#include "ER_ShadowMapper.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "DirectionalLight.h"
#include "ER_Utility.h"
#include "ER_Camera.h"
#include "ER_QuadRenderer.h"

#define VOXEL_SIZE_X 160
#define VOXEL_SIZE_Y 90
#define VOXEL_SIZE_Z 128

static float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

namespace Library {
	ER_VolumetricFog::ER_VolumetricFog(ER_Core& game, const DirectionalLight& aLight, const ER_ShadowMapper& aShadowMapper)
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

		mCompositePS = rhi->CreateGPUShader();
		mCompositePS->CompileShader(rhi, "content\\shaders\\VolumetricFog\\VolumetricFogComposite.hlsl", "PSComposite", ER_PIXEL);

		mMainConstantBuffer.Initialize(rhi);
		mCompositeConstantBuffer.Initialize(rhi);
	}

	void ER_VolumetricFog::Draw()
	{
		if (!mEnabled)
			return;

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

		rhi->SetShader(mInjectionCS);
		rhi->SetUnorderedAccessResources(ER_COMPUTE, { mTempVoxelInjectionTexture3D[writeIndex] });
		rhi->SetConstantBuffers(ER_COMPUTE, { mMainConstantBuffer.Buffer() });
		rhi->SetShaderResources(ER_COMPUTE, { mShadowMapper.GetShadowTexture(0), mBlueNoiseTexture, mTempVoxelInjectionTexture3D[readIndex] });
		rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS });
		rhi->Dispatch(INT_CEIL(VOXEL_SIZE_X, 8), INT_CEIL(VOXEL_SIZE_Y, 8), INT_CEIL(VOXEL_SIZE_Z, 1));

		rhi->UnbindResourcesFromShader(ER_COMPUTE);

		mCurrentTexture3DRead = !mCurrentTexture3DRead;
	}

	void ER_VolumetricFog::ComputeAccumulation()
	{
		auto rhi = GetCore()->GetRHI();

		int readIndex = mCurrentTexture3DRead;

		rhi->SetShader(mAccumulationCS);
		rhi->SetUnorderedAccessResources(ER_COMPUTE, { mFinalVoxelAccumulationTexture3D });
		rhi->SetConstantBuffers(ER_COMPUTE, { mMainConstantBuffer.Buffer() });
		rhi->SetShaderResources(ER_COMPUTE, { mShadowMapper.GetShadowTexture(0), mBlueNoiseTexture, mTempVoxelInjectionTexture3D[readIndex] });
		rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS });
		rhi->Dispatch(INT_CEIL(VOXEL_SIZE_X, 8), INT_CEIL(VOXEL_SIZE_Y, 8), 1);

		rhi->UnbindResourcesFromShader(ER_COMPUTE);
	}

	void ER_VolumetricFog::Composite(ER_RHI_GPUTexture* aInputColorTexture, ER_RHI_GPUTexture* aGbufferWorldPos)
	{
		assert(aGbufferWorldPos && aInputColorTexture);

		auto rhi = GetCore()->GetRHI();

		rhi->SetShader(mCompositePS);
		rhi->SetConstantBuffers(ER_PIXEL, { mCompositeConstantBuffer.Buffer() });
		rhi->SetShaderResources(ER_PIXEL, { aInputColorTexture, aGbufferWorldPos, mFinalVoxelAccumulationTexture3D });
		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		
		ER_QuadRenderer* quadRenderer = (ER_QuadRenderer*)mCore->GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(quadRenderer);
		quadRenderer->Draw(rhi);

		rhi->UnbindResourcesFromShader(ER_PIXEL);
	}
}