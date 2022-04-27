#include "ER_VolumetricFog.h"
#include "ER_ShadowMapper.h"
#include "ER_GPUTexture.h"
#include "Game.h"
#include "GameException.h"
#include "ShaderCompiler.h"
#include "DirectionalLight.h"
#include "Utility.h"
#include "Camera.h"
#include "SamplerStates.h"
#include "ER_QuadRenderer.h"

#define VOXEL_SIZE_X 160
#define VOXEL_SIZE_Y 90
#define VOXEL_SIZE_Z 128

static const float clearColorBlack[] = { 0.0f, 0.0f, 0.0f, 0.0f };

namespace Library {
	ER_VolumetricFog::ER_VolumetricFog(Game& game, const DirectionalLight& aLight, const ER_ShadowMapper& aShadowMapper)
	    : GameComponent(game), mShadowMapper(aShadowMapper), mDirectionalLight(aLight)
	{	
	}
	
	ER_VolumetricFog::~ER_VolumetricFog()
	{
		DeleteObject(mTempVoxelInjectionTexture3D[0]);
		DeleteObject(mTempVoxelInjectionTexture3D[1]);
		DeleteObject(mFinalVoxelAccumulationTexture3D);
		DeleteObject(mBlueNoiseTexture);
	
		ReleaseObject(mInjectionCS);
		ReleaseObject(mAccumulationCS);
		ReleaseObject(mCompositePS);

		mMainConstantBuffer.Release();
	}
    
	void ER_VolumetricFog::Initialize()
	{
		auto device = GetGame()->Direct3DDevice();
		assert(device);
		
		mTempVoxelInjectionTexture3D[0] = new ER_GPUTexture(device, VOXEL_SIZE_X, VOXEL_SIZE_Y, 1, DXGI_FORMAT_R16G16B16A16_FLOAT,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1, VOXEL_SIZE_Z);
		mTempVoxelInjectionTexture3D[1] = new ER_GPUTexture(device, VOXEL_SIZE_X, VOXEL_SIZE_Y, 1, DXGI_FORMAT_R16G16B16A16_FLOAT,
				D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1, VOXEL_SIZE_Z);
		mFinalVoxelAccumulationTexture3D = new ER_GPUTexture(device, VOXEL_SIZE_X, VOXEL_SIZE_Y, 1, DXGI_FORMAT_R16G16B16A16_FLOAT,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1, VOXEL_SIZE_Z);
		mBlueNoiseTexture = new ER_GPUTexture(device, GetGame()->Direct3DDeviceContext(), "content\\textures\\blueNoise.dds");

		ID3DBlob* blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\VolumetricFog\\VolumetricFogMain.hlsl").c_str(), "CSInjection", "cs_5_0", &blob)))
			throw GameException("Failed to load CSInjection from shader: VolumetricFogMain.hlsl!");
		if (FAILED(mGame->Direct3DDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mInjectionCS)))
			throw GameException("Failed to create shader from VolumetricFogMain.hlsl!");
		blob->Release();
		
		blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\VolumetricFog\\VolumetricFogMain.hlsl").c_str(), "CSAccumulation", "cs_5_0", &blob)))
			throw GameException("Failed to load CSAccumulation from shader: VolumetricFogMain.hlsl!");
		if (FAILED(mGame->Direct3DDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mAccumulationCS)))
			throw GameException("Failed to create shader from VolumetricFogMain.hlsl!");
		blob->Release();

		blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\VolumetricFog\\VolumetricFogComposite.hlsl").c_str(), "PSComposite", "ps_5_0", &blob)))
			throw GameException("Failed to load PSComposite from shader: VolumetricFogComposite.hlsl!");
		if (FAILED(mGame->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mCompositePS)))
			throw GameException("Failed to create shader from VolumetricFogComposite.hlsl!");
		blob->Release();

		mMainConstantBuffer.Initialize(device);
		mCompositeConstantBuffer.Initialize(device);
	}

	void ER_VolumetricFog::Draw()
	{
		if (!mEnabled)
			return;

		ComputeInjection();
		ComputeAccumulation();
	}

	void ER_VolumetricFog::Update(const GameTime& gameTime)
	{
		Camera* camera = (Camera*)GetGame()->Services().GetService(Camera::TypeIdClass());
		assert(camera);

		UpdateImGui();
		auto context = GetGame()->Direct3DDeviceContext();

		mMainConstantBuffer.Data.InvViewProj = XMMatrixTranspose(XMMatrixInverse(nullptr, camera->ViewMatrix() * camera->ProjectionMatrix()));
		mMainConstantBuffer.Data.ShadowMatrix = mShadowMapper.GetViewMatrix(0) * mShadowMapper.GetProjectionMatrix(0) /** XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix())*/;
		mMainConstantBuffer.Data.SunDirection = XMFLOAT4{ -mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f };
		mMainConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, mDirectionalLight.GetDirectionalLightIntensity() };
		mMainConstantBuffer.Data.CameraPosition = XMFLOAT4{ camera->Position().x, camera->Position().y, camera->Position().z, 1.0f };
		mMainConstantBuffer.Data.CameraNearFar = XMFLOAT4{ camera->GetCameraNearCascadeDistance(0), camera->GetCameraFarCascadeDistance(0), 0.0f, 0.0f };
		mMainConstantBuffer.Data.Anisotropy = mAnisotropy;
		mMainConstantBuffer.Data.Density = mDensity;
		mMainConstantBuffer.ApplyChanges(context);

		mCompositeConstantBuffer.Data.ViewProj = XMMatrixTranspose(camera->ViewMatrix() * camera->ProjectionMatrix());
		mCompositeConstantBuffer.Data.CameraNearFar = XMFLOAT4{ camera->GetCameraNearCascadeDistance(0), camera->GetCameraFarCascadeDistance(0), 0.0f, 0.0f };
		mCompositeConstantBuffer.ApplyChanges(context);
	}

	void ER_VolumetricFog::UpdateImGui()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("Volumetric Fog System");
		ImGui::Checkbox("Enabled", &mEnabled);
		ImGui::SliderFloat("Anisotropy", &mAnisotropy, 0.0f, 1.0f);
		ImGui::SliderFloat("Density", &mDensity, 0.1f, 10.0f);
		ImGui::End();
	}

	void ER_VolumetricFog::ComputeInjection()
	{
		auto context = GetGame()->Direct3DDeviceContext();

		int readIndex = mCurrentTexture3DRead;
		int writeIndex = !mCurrentTexture3DRead;

		ID3D11UnorderedAccessView* UAV[1] = { mTempVoxelInjectionTexture3D[writeIndex]->GetUAV() };
		context->CSSetUnorderedAccessViews(0, 1, UAV, NULL);
		
		ID3D11Buffer* CBs[1] = { mMainConstantBuffer.Buffer() };
		context->CSSetConstantBuffers(0, 1, CBs);
		
		ID3D11ShaderResourceView* SR[3] = {
			mShadowMapper.GetShadowTexture(0),
			mBlueNoiseTexture->GetSRV(),
			mTempVoxelInjectionTexture3D[readIndex]->GetSRV()
		};
		context->CSSetShaderResources(0, 3, SR);
		
		ID3D11SamplerState* SS[] = { SamplerStates::TrilinearWrap, SamplerStates::ShadowSamplerState };
		context->CSSetSamplers(0, 2, SS);

		context->CSSetShader(mInjectionCS, NULL, NULL);
		context->Dispatch(INT_CEIL(VOXEL_SIZE_X, 8), INT_CEIL(VOXEL_SIZE_Y, 8), INT_CEIL(VOXEL_SIZE_Z, 1));

		ID3D11ShaderResourceView* nullSRV[] = { NULL };
		context->CSSetShaderResources(0, 1, nullSRV);
		ID3D11UnorderedAccessView* nullUAV[] = { NULL };
		context->CSSetUnorderedAccessViews(0, 1, nullUAV, 0);
		ID3D11Buffer* nullCBs[] = { NULL };
		context->CSSetConstantBuffers(0, 1, nullCBs);
		ID3D11SamplerState* nullSSs[] = { NULL };
		context->CSSetSamplers(0, 1, nullSSs);

		mCurrentTexture3DRead = !mCurrentTexture3DRead;
	}

	void ER_VolumetricFog::ComputeAccumulation()
	{
		auto context = GetGame()->Direct3DDeviceContext();

		int readIndex = mCurrentTexture3DRead;

		context->ClearUnorderedAccessViewFloat(mFinalVoxelAccumulationTexture3D->GetUAV(), clearColorBlack);

		ID3D11UnorderedAccessView* UAV[1] = { mFinalVoxelAccumulationTexture3D->GetUAV() };
		context->CSSetUnorderedAccessViews(0, 1, UAV, NULL);

		ID3D11Buffer* CBs[1] = { mMainConstantBuffer.Buffer() };
		context->CSSetConstantBuffers(0, 1, CBs);

		ID3D11ShaderResourceView* SR[3] = {
			mShadowMapper.GetShadowTexture(0),
			mBlueNoiseTexture->GetSRV(),
			mTempVoxelInjectionTexture3D[readIndex]->GetSRV()
		};
		context->CSSetShaderResources(0, 3, SR);

		ID3D11SamplerState* SS[] = { SamplerStates::TrilinearWrap, SamplerStates::ShadowSamplerState };
		context->CSSetSamplers(0, 2, SS);

		context->CSSetShader(mAccumulationCS, NULL, NULL);
		context->Dispatch(INT_CEIL(VOXEL_SIZE_X, 8), INT_CEIL(VOXEL_SIZE_Y, 8), 1);

		ID3D11ShaderResourceView* nullSRV[] = { NULL };
		context->CSSetShaderResources(0, 1, nullSRV);
		ID3D11UnorderedAccessView* nullUAV[] = { NULL };
		context->CSSetUnorderedAccessViews(0, 1, nullUAV, 0);
		ID3D11Buffer* nullCBs[] = { NULL };
		context->CSSetConstantBuffers(0, 1, nullCBs);
		ID3D11SamplerState* nullSSs[] = { NULL };
		context->CSSetSamplers(0, 1, nullSSs);
	}

	void ER_VolumetricFog::Composite(ER_GPUTexture* aInputColorTexture, ER_GPUTexture* aGbufferWorldPos)
	{
		assert(aGbufferWorldPos && aInputColorTexture);

		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
		
		ID3D11Buffer* CBs[1] = { mCompositeConstantBuffer.Buffer() };
		context->PSSetConstantBuffers(0, 1, CBs);

		ID3D11SamplerState* SS[] = { SamplerStates::TrilinearWrap };
		context->PSSetSamplers(0, 1, SS);

		ID3D11ShaderResourceView* SR[3] = { 
			aInputColorTexture->GetSRV(),
			aGbufferWorldPos->GetSRV(),
			mFinalVoxelAccumulationTexture3D->GetSRV()
		};
		context->PSSetShaderResources(0, 3, SR);

		context->PSSetShader(mCompositePS, NULL, NULL);

		ER_QuadRenderer* quadRenderer = (ER_QuadRenderer*)mGame->Services().GetService(ER_QuadRenderer::TypeIdClass());
		assert(quadRenderer);
		quadRenderer->Draw(context);
	}
}