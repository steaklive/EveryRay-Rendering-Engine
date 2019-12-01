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
#include "..\Library\DemoLevel.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\RenderingObject.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <WICTextureLoader.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <sstream>

namespace Rendering
{
	RTTI_DEFINITIONS(AmbientLightingDemo)

	const float AmbientLightingDemo::AmbientModulationRate = UCHAR_MAX;

	AmbientLightingDemo::AmbientLightingDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),
		//mEffect(nullptr), mMaterial(nullptr), mTextureShaderResourceView(nullptr),
		//mVertexBuffer(nullptr), mIndexBuffer(nullptr), mIndexCount(0),
		mKeyboard(nullptr), mAmbientLight(), mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(nullptr), mSpriteBatch(nullptr), mSpriteFont(nullptr), mTextPosition(0.0f, 40.0f)
	{
	}

	AmbientLightingDemo::~AmbientLightingDemo()
	{
		mObject->MeshMaterialVariablesUpdateEvent->RemoverAllListeners();

		DeleteObject(mSpriteFont);
		DeleteObject(mSpriteBatch);
		DeleteObject(mRenderStateHelper);
		DeleteObject(mAmbientLight);
		//ReleaseObject(mTextureShaderResourceView);
		//DeleteObject(mMaterial);
		//DeleteObject(mEffect);
		//ReleaseObject(mVertexBuffer);
		//ReleaseObject(mIndexBuffer);
	}
#pragma region COMPONENT_METHODS
	/////////////////////////////////////////////////////////////
	// 'DemoLevel' ugly methods...
	bool AmbientLightingDemo::IsComponent()
	{
		return mGame->IsInGameComponents<AmbientLightingDemo*>(mGame->components, this);
	}
	void AmbientLightingDemo::Create()
	{
		Initialize();
		mGame->components.push_back(this);
	}
	void AmbientLightingDemo::Destroy()
	{
		std::pair<bool, int> res = mGame->FindInGameComponents<AmbientLightingDemo*>(mGame->components, this);

		if (res.first)
		{
			mGame->components.erase(mGame->components.begin() + res.second);

			//very provocative...
			delete this;
		}

	}
	/////////////////////////////////////////////////////////////  
#pragma endregion
	void AmbientLightingDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		std::string currentdir = Utility::CurrentDirectory();
		std::wstring exedir = Utility::ExecutableDirectory();
		std::string dir;
		Utility::GetDirectory(currentdir, dir);
		mObject = new RenderingObject("Statue", *mGame, std::unique_ptr<Model>(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\bunny.fbx", true)));

		// Initialize the material
		Effect* mEffect = new Effect(*mGame);
		mEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\AmbientLighting.fx");

		mObject->LoadMaterial(new AmbientLightingMaterial(), mEffect);
		mObject->LoadRenderBuffers();
		mObject->LoadAssignedMeshTextures();
		//for (size_t i = 0; i < mObject->GetMeshCount(); i++)
		//{
		//	mObject->LoadCustomMeshTextures(i,
		//		L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\statue\\statue_d.dds",
		//		L"",
		//		L"",
		//		L"",
		//		L"",
		//		L"",
		//		L""
		//	);
		//}

		mAmbientLight = new Light(*mGame);
		mAmbientLight->SetColor(ColorHelper::White);

		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);

		mRenderStateHelper = new RenderStateHelper(*mGame);

		mObject->MeshMaterialVariablesUpdateEvent->AddListener("Ambient Material Update", [&](int meshIndex) { UpdateAmbientMaterialVariables(meshIndex); });
	}

	void AmbientLightingDemo::Update(const GameTime& gameTime)
	{
		UpdateAmbientLight(gameTime);
	}

	void AmbientLightingDemo::Draw(const GameTime& gameTime)
	{
		mRenderStateHelper->SaveRasterizerState();

		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		mObject->Draw();

		mRenderStateHelper->SaveAll();

#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion
	}

	void AmbientLightingDemo::UpdateAmbientMaterialVariables(int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&mWorldMatrix);
		XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		static_cast<AmbientLightingMaterial*>(mObject->GetMeshMaterial(meshIndex))->WorldViewProjection() << wvp;
		static_cast<AmbientLightingMaterial*>(mObject->GetMeshMaterial(meshIndex))->ColorTexture() << mObject->GetTextureData(meshIndex).AlbedoMap;
		static_cast<AmbientLightingMaterial*>(mObject->GetMeshMaterial(meshIndex))->AmbientColor() << mAmbientLight->ColorVector();
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