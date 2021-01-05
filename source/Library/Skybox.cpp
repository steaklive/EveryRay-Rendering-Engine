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
		mWorldMatrix(MatrixHelper::Identity), mScaleMatrix(MatrixHelper::Identity)
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
		DeleteObject(mSunRenderTarget);
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

		mSunConstantBuffer.Initialize(mGame->Direct3DDevice());
		mSunRenderTarget = new FullScreenRenderTarget(*mGame);

		// add to game components
		//mGame->components.push_back(this);
	}

	void Skybox::Update(const GameTime& gameTime)
	{
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
		const XMFLOAT3& position = mCamera->Position();

		if (mIsMovable) 
			XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * XMMatrixTranslation(position.x, position.y - 50.0f, position.z) * XMMatrixRotationAxis(XMVECTOR{ 0,1,0 }, gameTime.TotalGameTime()*0.003f));
		else XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * XMMatrixTranslation(position.x, position.y, position.z));

		mSunConstantBuffer.Data.InvProj = XMMatrixInverse(nullptr, mCamera->ProjectionMatrix());
		mSunConstantBuffer.Data.InvView = XMMatrixInverse(nullptr, mCamera->ViewMatrix());
		mSunConstantBuffer.Data.SunDir = mSunDir;
		mSunConstantBuffer.Data.SunColor = mSunColor;
		mSunConstantBuffer.Data.SunBrightness = mSunBrightness;
		mSunConstantBuffer.Data.SunExponent = mSunExponent;
		mSunConstantBuffer.ApplyChanges(context);
	}

	void Skybox::Draw(const GameTime& gametime)
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

		XMMATRIX wvp = XMLoadFloat4x4(&mWorldMatrix) * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		mMaterial->WorldViewProjection() << wvp;
		mMaterial->SkyboxTexture() << mCubeMapShaderResourceView;

		float useCustomColor = (mUseCustomColor) ? 1.0f : 0.0f;
		mMaterial->UseCustomColor() << useCustomColor;
		mMaterial->BottomColor() << XMVECTOR{ mBottomColor.x,mBottomColor.y,mBottomColor.z,mBottomColor.w };
		mMaterial->TopColor() << XMVECTOR{ mTopColor.x,mTopColor.y,mTopColor.z,mTopColor.w };

		pass->Apply(0, direct3DDeviceContext);

		direct3DDeviceContext->DrawIndexed(mIndexCount, 0, 0);
		mGame->UnbindPixelShaderResources(0, 3);
	}

	void Skybox::DrawSun(const GameTime& gametime, Rendering::PostProcessingStack* postprocess) {
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		postprocess->BeginRenderingToExtraRT(true);
		Draw(gametime);
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
		}
	}

	ID3D11ShaderResourceView* Skybox::GetSunOutputTexture()
	{
		return mSunRenderTarget->OutputColorTexture();
	}

}