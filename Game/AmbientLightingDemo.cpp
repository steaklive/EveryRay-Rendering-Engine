#include "stdafx.h"

#include "AmbientLightingDemo.h"
#include "..\Library\AmbientLightingMaterial.h"
#include "..\Library\Game.h"
#include "..\Library\GameException.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\ColorHelper.h"
#include "..\Library\Camera.h"
#include "..\Library\Model.h"
#include "..\Library\Mesh.h"
#include "..\Library\Utility.h"
#include "..\Library\DirectionalLight.h"
#include "..\Library\Keyboard.h"
#include "..\Library\Light.h"
#include "..\Library\RenderStateHelper.h"

#include <WICTextureLoader.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <sstream>

namespace Rendering
{
	RTTI_DEFINITIONS(AmbientLightingDemo)

	const float AmbientLightingDemo::AmbientModulationRate = UCHAR_MAX;

	AmbientLightingDemo::AmbientLightingDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera), mEffect(nullptr), mMaterial(nullptr), mTextureShaderResourceView(nullptr),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mIndexCount(0),
		mKeyboard(nullptr), mAmbientLight(), mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(nullptr), mSpriteBatch(nullptr), mSpriteFont(nullptr), mTextPosition(0.0f, 40.0f)
	{
	}

	AmbientLightingDemo::~AmbientLightingDemo()
	{
		DeleteObject(mSpriteFont);
		DeleteObject(mSpriteBatch);
		DeleteObject(mRenderStateHelper);
		DeleteObject(mAmbientLight);
		ReleaseObject(mTextureShaderResourceView);
		DeleteObject(mMaterial);
		DeleteObject(mEffect);
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mIndexBuffer);
	}

	void AmbientLightingDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		// Load the model
		std::unique_ptr<Model> model(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\Head_Level2.obj", true));

		// Initialize the material
		mEffect = new Effect(*mGame);
		mEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\AmbientLighting.fx");
		mMaterial = new AmbientLightingMaterial();
		mMaterial->Initialize(mEffect);

		Mesh* mesh = model->Meshes().at(0);
		mMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh, &mVertexBuffer);
		mesh->CreateIndexBuffer(&mIndexBuffer);
		mIndexCount = mesh->Indices().size();

		std::wstring textureName = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\HeadColour.jpg";
		HRESULT hr = DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName.c_str(), nullptr, &mTextureShaderResourceView);
		if (FAILED(hr))
		{
			throw GameException("CreateWICTextureFromFile() failed.", hr);
		}

		mAmbientLight = new Light(*mGame);
		mAmbientLight->SetColor(ColorHelper::White);

		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);

		mRenderStateHelper = new RenderStateHelper(*mGame);

		//mSpriteBatch = new SpriteBatch(mGame->Direct3DDeviceContext());
		//mSpriteFont = new SpriteFont(mGame->Direct3DDevice(), L"Content\\Fonts\\Arial_14_Regular.spritefont");
	}

	void AmbientLightingDemo::Update(const GameTime& gameTime)
	{
		UpdateAmbientLight(gameTime);
	}

	void AmbientLightingDemo::Draw(const GameTime& gameTime)
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

		XMMATRIX worldMatrix = XMLoadFloat4x4(&mWorldMatrix);
		XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();

		mMaterial->WorldViewProjection() << wvp;
		mMaterial->ColorTexture() << mTextureShaderResourceView;
		mMaterial->AmbientColor() << mAmbientLight->ColorVector();

		pass->Apply(0, direct3DDeviceContext);

		direct3DDeviceContext->DrawIndexed(mIndexCount, 0, 0);

		mRenderStateHelper->SaveAll();

		/*mSpriteBatch->Begin();

		std::wostringstream helpLabel;
		helpLabel << L"Ambient Intensity (+PgUp/-PgDn): " << mAmbientLight->Color().a << "\n";

		mSpriteFont->DrawString(mSpriteBatch, helpLabel.str().c_str(), mTextPosition);

		mSpriteBatch->End();*/

		mRenderStateHelper->RestoreAll();
	}

	void AmbientLightingDemo::UpdateAmbientLight(const GameTime& gameTime)
	{
		static float ambientIntensity = mAmbientLight->Color().a;

		if (mKeyboard != nullptr)
		{
			if (mKeyboard->IsKeyDown(DIK_NUMPADPLUS) && ambientIntensity < UCHAR_MAX)
			{
				ambientIntensity += AmbientModulationRate * (float)gameTime.ElapsedGameTime();

				XMCOLOR ambientColor = mAmbientLight->Color();
				ambientColor.a = (UCHAR)XMMin<float>(ambientIntensity, UCHAR_MAX);
				mAmbientLight->SetColor(ambientColor);
			}

			if (mKeyboard->IsKeyDown(DIK_NUMPADMINUS) && ambientIntensity > 0)
			{
				ambientIntensity -= AmbientModulationRate * (float)gameTime.ElapsedGameTime();

				XMCOLOR ambientColor = mAmbientLight->Color();
				ambientColor.a = (UCHAR)XMMax<float>(ambientIntensity, 0);
				mAmbientLight->SetColor(ambientColor);
			}
		}
	}
}