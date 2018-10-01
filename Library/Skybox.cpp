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
#include <DDSTextureLoader.h>

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
		ReleaseObject(mCubeMapShaderResourceView);
		DeleteObject(mMaterial);
		DeleteObject(mEffect);
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mIndexBuffer);
	}

	void Skybox::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		std::unique_ptr<Model> model(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\Sphere.obj", true));

		mEffect = new Effect(*mGame);
		mEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\Skybox.fx");

		mMaterial = new SkyboxMaterial();
		mMaterial->Initialize(mEffect);

		Mesh* mesh = model->Meshes().at(0);
		mMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh, &mVertexBuffer);
		mesh->CreateIndexBuffer(&mIndexBuffer);
		mIndexCount = mesh->Indices().size();
		
		HRESULT hr = DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mCubeMapFileName.c_str(), nullptr, &mCubeMapShaderResourceView);
		if (FAILED(hr))
		{
			throw GameException("Failed to load a Skybox texture!", hr);
		}
	}

	void Skybox::Update(const GameTime& gameTime)
	{
		const XMFLOAT3& position = mCamera->Position();

		XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * XMMatrixTranslation(position.x, position.y, position.z));
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

		pass->Apply(0, direct3DDeviceContext);

		direct3DDeviceContext->DrawIndexed(mIndexCount, 0, 0);
	}
}