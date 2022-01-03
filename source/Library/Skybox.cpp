#include "stdafx.h"

#include "Skybox.h"
#include "Game.h"
#include "GameException.h"
#include "SkyboxMaterial.h"
#include "Camera.h"
#include "MatrixHelper.h"
#include "Model.h"
#include "Mesh.h"
#include "Utility.h"
#include "ShaderCompiler.h"
#include "FullScreenRenderTarget.h"


namespace Library
{
	RTTI_DEFINITIONS(Skybox)

	Skybox::Skybox(Game& game, Camera& camera, const std::wstring& cubeMapFileName, float scale)
		: DrawableGameComponent(game, camera),
		mCubeMapFileName(cubeMapFileName), mEffect(nullptr), mMaterial(nullptr),
		mCubeMapShaderResourceView(nullptr), mVertexBuffer(nullptr), mIndexBuffer(nullptr), mIndexCount(0),
		mWorldMatrix(MatrixHelper::Identity), mScaleMatrix(MatrixHelper::Identity), mScale(scale)
	{
		XMStoreFloat4x4(&mScaleMatrix, XMMatrixScaling(scale, scale, scale));
	}

	Skybox::~Skybox()
	{
		//delete skybox from components
		std::pair<bool, int> isSkyboxComponent = mGame->FindInGameComponents<Skybox*>(mGame->components, this);
		if (isSkyboxComponent.first)
		{
			mGame->components.erase(mGame->components.begin() + isSkyboxComponent.second);
		}

		ReleaseObject(mCubeMapShaderResourceView);
		DeleteObject(mMaterial);
		DeleteObject(mEffect);
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mIndexBuffer);
		ReleaseObject(mSunPS);
		ReleaseObject(mSunOcclusionPS);
		DeleteObject(mSunRenderTarget);
		DeleteObject(mSunOcclusionRenderTarget);
		mSunConstantBuffer.Release();
	}

	void Skybox::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		std::unique_ptr<Model> model(new Model(*mGame, Utility::GetFilePath("content\\models\\SphereSkybox.obj"), true));

		mEffect = new Effect(*mGame);
		mEffect->CompileFromFile(Utility::GetFilePath(L"content\\effects\\Skybox.fx"));

		mMaterial = new SkyboxMaterial();
		mMaterial->Initialize(mEffect);

		Mesh* mesh = model->Meshes().at(0);
		mMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh, &mVertexBuffer);
		mesh->CreateIndexBuffer(&mIndexBuffer);
		mIndexCount = mesh->Indices().size();
		

		if (mCubeMapFileName.find(std::wstring(L".dds")) != std::string::npos) {
			
			HRESULT hr = DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mCubeMapFileName.c_str(), nullptr, &mCubeMapShaderResourceView);
			if (FAILED(hr))
			{
				throw GameException("Failed to load a Skybox texture!", hr);
			}
		
		}
		else
		{
			HRESULT hr = DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mCubeMapFileName.c_str(), nullptr, &mCubeMapShaderResourceView);
			if (FAILED(hr))
			{
				throw GameException("Failed to load a Skybox texture!", hr);
			}


		}

		//init sun shader
		ID3DBlob* blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Sun.hlsl").c_str(), "main", "ps_5_0", &blob)))
			throw GameException("Failed to load main pass from shader: Sun.hlsl!");
		if (FAILED(mGame->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mSunPS)))
			throw GameException("Failed to create shader from Sun.hlsl!");
		blob->Release();

		blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Sun.hlsl").c_str(), "occlusion", "ps_5_0", &blob)))
			throw GameException("Failed to load occlusion pass from shader: Sun.hlsl!");
		if (FAILED(mGame->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mSunOcclusionPS)))
			throw GameException("Failed to create shader from Sun.hlsl!");
		blob->Release();

		mSunConstantBuffer.Initialize(mGame->Direct3DDevice());
		mSunRenderTarget = new FullScreenRenderTarget(*mGame);
		mSunOcclusionRenderTarget = new FullScreenRenderTarget(*mGame);

		// add to game components
		//mGame->components.push_back(this);
	}

	void Skybox::Update(const GameTime& gameTime, Camera* aCustomCamera)
	{
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
		XMFLOAT3 position;
		if (aCustomCamera)
			position = aCustomCamera->Position();
		else
			position = mCamera->Position();

		if (mIsMovable)
			XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * XMMatrixTranslation(position.x, position.y - 50.0f, position.z) * XMMatrixRotationAxis(XMVECTOR{ 0,1,0 }, gameTime.TotalGameTime() * 0.003f));
		else
			XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * XMMatrixTranslation(position.x, position.y, position.z));

	}

	void Skybox::UpdateSun(const GameTime& gameTime, Camera* aCustomCamera)
	{
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		if (aCustomCamera)
		{
			mSunConstantBuffer.Data.InvProj = XMMatrixInverse(nullptr, aCustomCamera->ProjectionMatrix());
			mSunConstantBuffer.Data.InvView = XMMatrixInverse(nullptr, aCustomCamera->ViewMatrix());
		}
		else
		{
			mSunConstantBuffer.Data.InvProj = XMMatrixInverse(nullptr, mCamera->ProjectionMatrix());
			mSunConstantBuffer.Data.InvView = XMMatrixInverse(nullptr, mCamera->ViewMatrix());
		}
		mSunConstantBuffer.Data.SunDir = mSunDir;
		mSunConstantBuffer.Data.SunColor = mSunColor;
		mSunConstantBuffer.Data.SunBrightness = mSunBrightness;
		mSunConstantBuffer.Data.SunExponent = mSunExponent;
		mSunConstantBuffer.ApplyChanges(context);
	}

	void Skybox::Draw(Camera* aCustomCamera)
	{
		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		Pass* pass = mMaterial->CurrentTechnique()->Passes().at(0);
		ID3D11InputLayout* inputLayout = mMaterial->InputLayouts().at(pass);
		direct3DDeviceContext->IASetInputLayout(inputLayout);

		UINT stride = mMaterial->VertexSize();
		UINT offset = 0;
		direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		direct3DDeviceContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		XMMATRIX wvp;
		if (aCustomCamera)
			wvp = XMLoadFloat4x4(&mWorldMatrix) * aCustomCamera->ViewMatrix() * aCustomCamera->ProjectionMatrix();
		else
			wvp = XMLoadFloat4x4(&mWorldMatrix) * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		mMaterial->WorldViewProjection() << wvp;
		mMaterial->SkyboxTexture() << mCubeMapShaderResourceView;

		float useCustomColor = (mUseCustomColor) ? 1.0f : 0.0f;
		mMaterial->SunColor() << mSunColor;
		mMaterial->UseCustomColor() << useCustomColor;
		mMaterial->BottomColor() << XMVECTOR{ mBottomColor.x,mBottomColor.y,mBottomColor.z,mBottomColor.w };
		mMaterial->TopColor() << XMVECTOR{ mTopColor.x,mTopColor.y,mTopColor.z,mTopColor.w };

		pass->Apply(0, direct3DDeviceContext);

		direct3DDeviceContext->DrawIndexed(mIndexCount, 0, 0);
		mGame->UnbindPixelShaderResources(0, 3);
	}

	void Skybox::DrawSun(Camera* aCustomCamera, Rendering::PostProcessingStack* postprocess) {
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		postprocess->BeginRenderingToExtraRT(true);
		Draw(aCustomCamera);
		postprocess->EndRenderingToExtraRT();

		if (mDrawSun && postprocess) {
			ID3D11Buffer* CBs[1] = {
				mSunConstantBuffer.Buffer()
			};

			ID3D11ShaderResourceView* SR[3] = {
				postprocess->GetExtraColorOutputTexture(),
				postprocess->GetPrepassColorOutputTexture(),
				postprocess->GetDepthOutputTexture()
			};

			mSunRenderTarget->Begin();
			context->PSSetConstantBuffers(0, 1, CBs);
			context->PSSetShaderResources(0, 3, SR);
			//context->PSSetSamplers(0, 2, SS);
			context->PSSetShader(mSunPS, NULL, NULL);
			postprocess->DrawFullscreenQuad(context);
			mSunRenderTarget->End();
			//reset main RT 
			postprocess->SetMainRT(mSunRenderTarget->OutputColorTexture());
		
			//draw occlusion texture for light shafts
			mSunOcclusionRenderTarget->Begin();
			context->PSSetConstantBuffers(0, 1, CBs);
			context->PSSetShaderResources(0, 3, SR);
			//context->PSSetSamplers(0, 2, SS);
			context->PSSetShader(mSunOcclusionPS, NULL, NULL);
			postprocess->DrawFullscreenQuad(context);
			mSunOcclusionRenderTarget->End();

			XMFLOAT3 mSunDirection;
			XMStoreFloat3(&mSunDirection, mSunDir);
			XMFLOAT4 posWorld = CalculateSunPositionOnSkybox(XMFLOAT3(-mSunDirection.x,-mSunDirection.y,-mSunDirection.z));
			XMVECTOR ndc;
			if (aCustomCamera)
				ndc = XMVector3Transform(XMLoadFloat4(&posWorld), aCustomCamera->ViewProjectionMatrix());
			else
				ndc = XMVector3Transform(XMLoadFloat4(&posWorld), mCamera->ViewProjectionMatrix());

			XMFLOAT4 ndcF;
			XMStoreFloat4(&ndcF, ndc);
			ndcF = XMFLOAT4(ndcF.x / ndcF.w, ndcF.y / ndcF.w, ndcF.z / ndcF.w, ndcF.w / ndcF.w);
			XMFLOAT2 ndcPos = XMFLOAT2(ndcF.x * 0.5f + 0.5f, -ndcF.y * 0.5f + 0.5f);
			//XMFLOAT2 screenPos = XMFLOAT2(ndcPos.x * mGame->ScreenWidth(), (1 - ndcPos.y) * mGame->ScreenHeight());
			postprocess->SetSunNDCPos(ndcPos);
		}
	}

	ID3D11ShaderResourceView* Skybox::GetSunOutputTexture()
	{
		return mSunRenderTarget->OutputColorTexture();
	}

	ID3D11ShaderResourceView* Skybox::GetSunOcclusionOutputTexture()
	{
		return mSunOcclusionRenderTarget->OutputColorTexture();
	}

	XMFLOAT4 Skybox::CalculateSunPositionOnSkybox(XMFLOAT3 dir)
	{
		float t = 0.0f;

		XMFLOAT3 sphereCenter = mCamera->Position();
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