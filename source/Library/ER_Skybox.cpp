#include "stdafx.h"

#include "ER_Skybox.h"
#include "Game.h"
#include "GameException.h"
#include "GameTime.h"
#include "Camera.h"
#include "MatrixHelper.h"
#include "Model.h"
#include "Mesh.h"
#include "Utility.h"
#include "ShaderCompiler.h"
#include "FullScreenRenderTarget.h"
#include "ER_QuadRenderer.h"
#include "VertexDeclarations.h"

namespace Library
{
	ER_Skybox::ER_Skybox(Game& game, Camera& camera, float scale)
		: GameComponent(game), mGame(game), mCamera(camera),
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
		DeleteObject(mSunRenderTarget);
		DeleteObject(mSunOcclusionRenderTarget);
		mSunConstantBuffer.Release();
		mSkyboxConstantBuffer.Release();
	}

	void ER_Skybox::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		std::unique_ptr<Model> model(new Model(mGame, Utility::GetFilePath("content\\models\\sphere_lowpoly.obj"), true));

		auto& meshes = model->Meshes();
		meshes[0].CreateVertexBuffer_Position(&mVertexBuffer);
		meshes[0].CreateIndexBuffer(&mIndexBuffer);
		mIndexCount = meshes[0].Indices().size();

		//skybox
		{
			ID3DBlob* blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Skybox.hlsl").c_str(), "VSMain", "vs_5_0", &blob)))
				throw GameException("Failed to load VSMain from shader: Skybox.hlsl!");
			if (FAILED(mGame.Direct3DDevice()->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mSkyboxVS)))
				throw GameException("Failed to create vertex shader from Skybox.hlsl!");
			
			D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};
			HRESULT hr = mGame.Direct3DDevice()->CreateInputLayout(inputElementDescriptions, ARRAYSIZE(inputElementDescriptions), blob->GetBufferPointer(), blob->GetBufferSize(), &mInputLayout);
			if (FAILED(hr))
				throw GameException("CreateInputLayout() failed when creating skybox's vertex shader.", hr);
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Skybox.hlsl").c_str(), "PSMain", "ps_5_0", &blob)))
				throw GameException("Failed to load PSMain from shader: Skybox.hlsl!");
			if (FAILED(mGame.Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mSkyboxPS)))
				throw GameException("Failed to create pixel shader from Skybox.hlsl!");
			blob->Release();
			
			mSkyboxConstantBuffer.Initialize(mGame.Direct3DDevice());
		}

		//sun
		{
			ID3DBlob* blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Sun.hlsl").c_str(), "main", "ps_5_0", &blob)))
				throw GameException("Failed to load main pass from shader: Sun.hlsl!");
			if (FAILED(mGame.Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mSunPS)))
				throw GameException("Failed to create shader from Sun.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Sun.hlsl").c_str(), "occlusion", "ps_5_0", &blob)))
				throw GameException("Failed to load occlusion pass from shader: Sun.hlsl!");
			if (FAILED(mGame.Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mSunOcclusionPS)))
				throw GameException("Failed to create shader from Sun.hlsl!");
			blob->Release();

			mSunConstantBuffer.Initialize(mGame.Direct3DDevice());
			mSunRenderTarget = new FullScreenRenderTarget(mGame);
			mSunOcclusionRenderTarget = new FullScreenRenderTarget(mGame);
		}
	}

	void ER_Skybox::Update(Camera* aCustomCamera)
	{
		ID3D11DeviceContext* context = mGame.Direct3DDeviceContext();
		XMFLOAT3 position;
		if (aCustomCamera)
			position = aCustomCamera->Position();
		else
			position = mCamera.Position();

		XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * XMMatrixTranslation(position.x, position.y, position.z));
	}

	void ER_Skybox::UpdateSun(const GameTime& gameTime, Camera* aCustomCamera)
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

	void ER_Skybox::Draw(Camera* aCustomCamera)
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

	void ER_Skybox::DrawSun(Camera* aCustomCamera, ER_GPUTexture* aSky, DepthTarget* aSceneDepth)
	{
		ID3D11DeviceContext* context = mGame.Direct3DDeviceContext();
		assert(aSceneDepth);
		auto quadRenderer = (ER_QuadRenderer*)mGame.Services().GetService(ER_QuadRenderer::TypeIdClass());
		assert(quadRenderer);

		if (mDrawSun) {
			ID3D11Buffer* CBs[1] = { mSunConstantBuffer.Buffer() };
			context->PSSetConstantBuffers(0, 1, CBs);

			ID3D11ShaderResourceView* SR[2] = { aSky->GetSRV(), aSceneDepth->getSRV() };
			context->PSSetShaderResources(0, 2, SR);

			context->PSSetShader(mSunPS, NULL, NULL);

			quadRenderer->Draw(context);
			
			return;// TODO fix for light shafts
			//draw occlusion texture for light shafts
			{
				mSunOcclusionRenderTarget->Begin();
				context->PSSetConstantBuffers(0, 1, CBs);
				context->PSSetShaderResources(0, 2, SR);
				//context->PSSetSamplers(0, 2, SS);
				context->PSSetShader(mSunOcclusionPS, NULL, NULL);
				quadRenderer->Draw(context);
				mSunOcclusionRenderTarget->End();

				XMFLOAT4 posWorld = CalculateSunPositionOnSkybox(XMFLOAT3(-mSunDir.x, -mSunDir.y, -mSunDir.z), aCustomCamera);
				XMVECTOR ndc;
				if (aCustomCamera)
					ndc = XMVector3Transform(XMLoadFloat4(&posWorld), aCustomCamera->ViewProjectionMatrix());
				else
					ndc = XMVector3Transform(XMLoadFloat4(&posWorld), mCamera.ViewProjectionMatrix());

				XMFLOAT4 ndcF;
				XMStoreFloat4(&ndcF, ndc);
				ndcF = XMFLOAT4(ndcF.x / ndcF.w, ndcF.y / ndcF.w, ndcF.z / ndcF.w, ndcF.w / ndcF.w);
				XMFLOAT2 ndcPos = XMFLOAT2(ndcF.x * 0.5f + 0.5f, -ndcF.y * 0.5f + 0.5f);
				//XMFLOAT2 screenPos = XMFLOAT2(ndcPos.x * mGame->ScreenWidth(), (1 - ndcPos.y) * mGame->ScreenHeight());
				//TODO postprocess->SetSunNDCPos(ndcPos);
			}
		}
	}

	ID3D11ShaderResourceView* ER_Skybox::GetSunOcclusionOutputTexture() const
	{
		return mSunOcclusionRenderTarget->OutputColorTexture();
	}

	XMFLOAT4 ER_Skybox::CalculateSunPositionOnSkybox(XMFLOAT3 dir, Camera* aCustomCamera)
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