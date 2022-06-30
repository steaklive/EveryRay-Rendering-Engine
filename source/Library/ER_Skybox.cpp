#include "stdafx.h"

#include "ER_Skybox.h"
#include "Game.h"
#include "ER_CoreException.h"
#include "ER_CoreTime.h"
#include "ER_Camera.h"
#include "MatrixHelper.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "Utility.h"
#include "ShaderCompiler.h"
#include "ER_QuadRenderer.h"
#include "VertexDeclarations.h"

namespace Library
{
	ER_Skybox::ER_Skybox(Game& game, ER_Camera& camera, float scale)
		: ER_CoreComponent(game), mGame(game), mCamera(camera),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mIndexCount(0),
		mWorldMatrix(MatrixHelper::Identity), mScaleMatrix(MatrixHelper::Identity), mScale(scale)
	{
		XMStoreFloat4x4(&mScaleMatrix, XMMatrixScaling(scale, scale, scale));
	}

	ER_Skybox::~ER_Skybox()
	{
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mIndexBuffer);
		ReleaseObject(mInputLayout);
		ReleaseObject(mSkyboxVS);
		ReleaseObject(mSkyboxPS);
		ReleaseObject(mSunPS);
		ReleaseObject(mSunOcclusionPS);
		mSunConstantBuffer.Release();
		mSkyboxConstantBuffer.Release();
	}

	void ER_Skybox::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		auto device = mGame.Direct3DDevice();
		auto context = mGame.Direct3DDeviceContext();

		std::unique_ptr<ER_Model> model(new ER_Model(mGame, Utility::GetFilePath("content\\models\\sphere_lowpoly.obj"), true));

		auto& meshes = model->Meshes();
		meshes[0].CreateVertexBuffer_Position(&mVertexBuffer);
		meshes[0].CreateIndexBuffer(&mIndexBuffer);
		mIndexCount = meshes[0].Indices().size();

		//skybox
		{
			ID3DBlob* blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Skybox.hlsl").c_str(), "VSMain", "vs_5_0", &blob)))
				throw ER_CoreException("Failed to load VSMain from shader: Skybox.hlsl!");
			if (FAILED(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mSkyboxVS)))
				throw ER_CoreException("Failed to create vertex shader from Skybox.hlsl!");
			
			D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};
			HRESULT hr = device->CreateInputLayout(inputElementDescriptions, ARRAYSIZE(inputElementDescriptions), blob->GetBufferPointer(), blob->GetBufferSize(), &mInputLayout);
			if (FAILED(hr))
				throw ER_CoreException("CreateInputLayout() failed when creating skybox's vertex shader.", hr);
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Skybox.hlsl").c_str(), "PSMain", "ps_5_0", &blob)))
				throw ER_CoreException("Failed to load PSMain from shader: Skybox.hlsl!");
			if (FAILED(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mSkyboxPS)))
				throw ER_CoreException("Failed to create pixel shader from Skybox.hlsl!");
			blob->Release();
			
			mSkyboxConstantBuffer.Initialize(device);
		}

		//sun
		{
			ID3DBlob* blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Sun.hlsl").c_str(), "main", "ps_5_0", &blob)))
				throw ER_CoreException("Failed to load main pass from shader: Sun.hlsl!");
			if (FAILED(mGame.Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mSunPS)))
				throw ER_CoreException("Failed to create shader from Sun.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Sun.hlsl").c_str(), "occlusion", "ps_5_0", &blob)))
				throw ER_CoreException("Failed to load occlusion pass from shader: Sun.hlsl!");
			if (FAILED(mGame.Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mSunOcclusionPS)))
				throw ER_CoreException("Failed to create shader from Sun.hlsl!");
			blob->Release();

			mSunConstantBuffer.Initialize(device);
		}
	}

	void ER_Skybox::Update(ER_Camera* aCustomCamera)
	{
		ID3D11DeviceContext* context = mGame.Direct3DDeviceContext();
		XMFLOAT3 position;
		if (aCustomCamera)
			position = aCustomCamera->Position();
		else
			position = mCamera.Position();

		XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * XMMatrixTranslation(position.x, position.y, position.z));
	}

	void ER_Skybox::UpdateSun(const ER_CoreTime& gameTime, ER_Camera* aCustomCamera)
	{
		ID3D11DeviceContext* context = mGame.Direct3DDeviceContext();

		if (aCustomCamera)
		{
			mSunConstantBuffer.Data.InvProj = XMMatrixInverse(nullptr, aCustomCamera->ProjectionMatrix());
			mSunConstantBuffer.Data.InvView = XMMatrixInverse(nullptr, aCustomCamera->ViewMatrix());
		}
		else
		{
			mSunConstantBuffer.Data.InvProj = XMMatrixInverse(nullptr, mCamera.ProjectionMatrix());
			mSunConstantBuffer.Data.InvView = XMMatrixInverse(nullptr, mCamera.ViewMatrix());
		}
		mSunConstantBuffer.Data.SunDir = mSunDir;
		mSunConstantBuffer.Data.SunColor = mSunColor;
		mSunConstantBuffer.Data.SunBrightness = mSunBrightness;
		mSunConstantBuffer.Data.SunExponent = mSunExponent;
		mSunConstantBuffer.ApplyChanges(context);
	}

	void ER_Skybox::Draw(ER_Camera* aCustomCamera)
	{
		auto context = mGame.Direct3DDeviceContext();
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(mInputLayout);

		UINT stride = sizeof(VertexPosition);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		context->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		XMMATRIX wvp = XMMatrixIdentity();
		if (aCustomCamera)
			wvp = XMLoadFloat4x4(&mWorldMatrix) * aCustomCamera->ViewMatrix() * aCustomCamera->ProjectionMatrix();
		else
			wvp = XMLoadFloat4x4(&mWorldMatrix) * mCamera.ViewMatrix() * mCamera.ProjectionMatrix();

		mSkyboxConstantBuffer.Data.WorldViewProjection = XMMatrixTranspose(wvp);
		mSkyboxConstantBuffer.Data.SunColor = mSunColor;
		mSkyboxConstantBuffer.Data.BottomColor = mBottomColor;
		mSkyboxConstantBuffer.Data.TopColor = mTopColor;
		mSkyboxConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mSkyboxConstantBuffer.Buffer() };

		context->VSSetShader(mSkyboxVS, NULL, NULL);
		context->VSSetConstantBuffers(0, 1, CBs);

		context->PSSetShader(mSkyboxPS, NULL, NULL);
		context->PSSetConstantBuffers(0, 1, CBs);

		context->DrawIndexed(mIndexCount, 0, 0);
	}

	void ER_Skybox::DrawSun(ER_Camera* aCustomCamera, ER_GPUTexture* aSky, ER_GPUTexture* aSceneDepth)
	{
		ID3D11DeviceContext* context = mGame.Direct3DDeviceContext();
		assert(aSceneDepth);
		auto quadRenderer = (ER_QuadRenderer*)mGame.Services().GetService(ER_QuadRenderer::TypeIdClass());
		assert(quadRenderer);

		if (mDrawSun) {
			ID3D11Buffer* CBs[1] = { mSunConstantBuffer.Buffer() };
			context->PSSetConstantBuffers(0, 1, CBs);

			ID3D11ShaderResourceView* SR[2] = { aSky->GetSRV(), aSceneDepth->GetSRV() };
			context->PSSetShaderResources(0, 2, SR);

			context->PSSetShader(mSunPS, NULL, NULL);

			quadRenderer->Draw(context);
		}
	}

	XMFLOAT4 ER_Skybox::CalculateSunPositionOnSkybox(XMFLOAT3 dir, ER_Camera* aCustomCamera)
	{
		float t = 0.0f;

		XMFLOAT3 sphereCenter;
		if (aCustomCamera )
			sphereCenter = aCustomCamera->Position();
		else 
			sphereCenter = mCamera.Position();

		float radius2 = mScale * mScale;

		XMFLOAT3 L = XMFLOAT3(-sphereCenter.x,-sphereCenter.y,-sphereCenter.z);
		float a = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
		float b = 2.0f * (dir.x * L.x + dir.y * L.y + dir.z * L.z);
		float c = L.x * L.x + L.y * L.y + L.z * L.z - radius2;

		float discr = b * b - 4.0 * a * c;
		t = (-b + sqrt(discr)) / 2;

		return XMFLOAT4(dir.x * t, dir.y * t, dir.z * t, 1.0f);
	}

}