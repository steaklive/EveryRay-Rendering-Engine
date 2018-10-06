#include "stdafx.h"

#include "PhysicallyBasedRenderingDemo.h"
#include "..\Library\PBRMaterial.h"
#include "..\Library\AmbientLightingMaterial.h"
#include "..\Library\Game.h"
#include "..\Library\GameException.h"
#include "..\Library\VectorHelper.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\ColorHelper.h"
#include "..\Library\Camera.h"
#include "..\Library\Model.h"
#include "..\Library\Mesh.h"
#include "..\Library\Utility.h"
#include "..\Library\PointLight.h"
#include "..\Library\Keyboard.h"
#include "..\Library\ProxyModel.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\Skybox.h"
#include "..\Library\IBLRadianceMap.h"

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <sstream>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

namespace Rendering
{
	RTTI_DEFINITIONS(PhysicallyBasedRenderingDemo)

	const float PhysicallyBasedRenderingDemo::LightModulationRate = UCHAR_MAX;
	const float PhysicallyBasedRenderingDemo::LightMovementRate = 10.0f;
	const int NUM_SPHERES = 5;

	PBRDemoLightInfo::LightData lightData;

	ShaderBallWoodMaterial		shaderBallWoodMaterial;
	ShaderBallFloorMaterial		shaderBallFloorMaterial;
	ShaderBallMarbleMaterial    shaderBallMarbleMaterial;
	ShaderBallBathroomMaterial  shaderBallBathroomMaterial;
	ShaderBallBrickMaterial		shaderBallBrickMaterial;
	ShaderBallConcreteMaterial  shaderBallConcreteMaterial;
	ShaderBallGoldMaterial		shaderBallGoldMaterial;
	ShaderBallSilverMaterial	shaderBallSilverMaterial;


	PhysicallyBasedRenderingDemo::PhysicallyBasedRenderingDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),

		mEffectPBR(nullptr),
		mPBRShaderBallOuterMaterial(nullptr),
		mPBRShaderBallInnerMaterial(nullptr),
		
		mIntegrationTextureShaderResourceView(nullptr),
		mIrradianceTextureShaderResourceView(nullptr),
		mRadianceTextureShaderResourceView(nullptr),
		
		mVertexBufferShaderBallOuterPart(nullptr), mIndexBufferShaderBallOuterPart(nullptr), mIndexCountShaderBallOuterPart(0),
		mVertexBufferShaderBallInnerPart(nullptr), mIndexBufferShaderBallInnerPart(nullptr), mIndexCountShaderBallInnerPart(0),
		
		mWorldMatricesShaderBallFirstRow(NUM_SPHERES, MatrixHelper::Identity),
		mWorldMatricesShaderBallSecondRow(NUM_SPHERES, MatrixHelper::Identity),

		mKeyboard(nullptr),
		mSkybox(nullptr), 
		mProxyModel(nullptr),
		mRenderStateHelper(nullptr),

		mShaderBallInnerAlbedoTextureShaderResourceView		(nullptr),
		mShaderBallInnerMetalnessTextureShaderResourceView	(nullptr),
		mShaderBallInnerRoughnessTextureShaderResourceView	(nullptr),
		mShaderBallInnerNormalTextureShaderResourceView		(nullptr)
	{
	}

	PhysicallyBasedRenderingDemo::~PhysicallyBasedRenderingDemo()
	{

		lightData.Destroy();

		shaderBallConcreteMaterial.Destroy();
		shaderBallWoodMaterial.Destroy();
		shaderBallFloorMaterial.Destroy();
		shaderBallMarbleMaterial.Destroy();
		shaderBallBathroomMaterial.Destroy();
		shaderBallBrickMaterial.Destroy();
		shaderBallGoldMaterial.Destroy();
		shaderBallSilverMaterial.Destroy();

		DeleteObject(mEffectPBR);

		ReleaseObject(mShaderBallInnerAlbedoTextureShaderResourceView);
		ReleaseObject(mShaderBallInnerMetalnessTextureShaderResourceView);
		ReleaseObject(mShaderBallInnerRoughnessTextureShaderResourceView);
		ReleaseObject(mShaderBallInnerNormalTextureShaderResourceView);

		DeleteObject(mPBRShaderBallOuterMaterial);
		ReleaseObject(mVertexBufferShaderBallOuterPart);
		ReleaseObject(mIndexBufferShaderBallOuterPart);

		DeleteObject(mPBRShaderBallInnerMaterial);
		ReleaseObject(mVertexBufferShaderBallInnerPart);
		ReleaseObject(mIndexBufferShaderBallInnerPart);
		
		ReleaseObject(mIrradianceTextureShaderResourceView);
		ReleaseObject(mRadianceTextureShaderResourceView);
		ReleaseObject(mIntegrationTextureShaderResourceView);
		
		DeleteObject(mRenderStateHelper);
		DeleteObject(mProxyModel);
		
		//delete skybox from components
		std::pair<bool, int> isSkyboxComponent = mGame->FindInGameComponents<Skybox*>(mGame->components, mSkybox);
		if (isSkyboxComponent.first)
		{
			mGame->components.erase(mGame->components.begin() + isSkyboxComponent.second);
		}

		DeleteObject(mSkybox);
	}

	bool PhysicallyBasedRenderingDemo::IsComponent()
	{
		return mGame->IsInGameComponents<PhysicallyBasedRenderingDemo*>(mGame->components, this);
	}

	void PhysicallyBasedRenderingDemo::Create()
	{
		Initialize();
		mGame->components.push_back(this);

	}
	
	void PhysicallyBasedRenderingDemo::Destroy()
	{
		std::pair<bool, int> res = mGame->FindInGameComponents<PhysicallyBasedRenderingDemo*>(mGame->components, this);
	
		if (res.first)
		{
			mGame->components.erase(mGame->components.begin() + res.second);

			//very provocative...
			delete this;
		}

	}


	void PhysicallyBasedRenderingDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		std::unique_ptr<Model> model(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\MaterialSphere.fbx", true));

		// Initialize the material
		mEffectPBR = new Effect(*mGame);
		mEffectPBR->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\PBR.fx");
		
		//***********************************//
		//			Initialize IBL			 //
		//***********************************//
		//									 //
		// Load Pre-Convoluted Irradiance Map
		std::wstring textureIrradiance =  L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Skyboxes\\milkmill_diffuse_cube_map.dds";
		HRESULT hr5 = DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureIrradiance.c_str(), nullptr, &mIrradianceTextureShaderResourceView);
		if (FAILED(hr5))
		{
			throw GameException("Failed to create Irradiance Map.", hr5);
		}

		// Create an IBL Radiance Map from Environment Map
		mRadianceMap.reset(new IBLRadianceMap(*mGame, L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Skyboxes\\milkmill_cube_map.dds") );
		mRadianceMap->Initialize();
		mRadianceMap->Create(*mGame);
		
		mRadianceTextureShaderResourceView = *mRadianceMap->GetShaderResourceViewAddress();
		if (mRadianceTextureShaderResourceView == nullptr)
		{
			throw GameException("Failed to create Radiance Map.");
		}
		mRadianceMap.release();
		mRadianceMap.reset(nullptr);

		// Load a pre-computed Integration Map
		std::wstring textureIntegration = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Skyboxes\\ibl_brdf_lut.png";
		DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureIntegration.c_str(), nullptr, &mIntegrationTextureShaderResourceView);
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureIntegration.c_str(), nullptr, &mIntegrationTextureShaderResourceView)))
		{
			throw GameException("Failed to create Integration Texture.");
		}
		//									 //
		//***********************************//
		//									 //
		//***********************************//
		
		
		//***********************************//
		// LOADING INNER MATERIAL TEXTURES   //
		std::wstring textureShaderBallInnerMaterialAlbedo = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\PlasticPattern\\plasticpattern1-albedo.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureShaderBallInnerMaterialAlbedo.c_str(), nullptr, &mShaderBallInnerAlbedoTextureShaderResourceView)))
		{
			throw GameException("Failed to create ShaderBall Inner Material Albedo Texture.");
		}
		std::wstring textureShaderBallInnerMaterialMetal = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\PlasticPattern\\plasticpattern1-metalness.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureShaderBallInnerMaterialMetal.c_str(), nullptr, &mShaderBallInnerMetalnessTextureShaderResourceView)))
		{
			throw GameException("Failed to create ShaderBall Inner Material Metalness Texture.");
		}
		std::wstring textureShaderBallInnerMaterialRoughness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\PlasticPattern\\plasticpattern1-roughness2.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureShaderBallInnerMaterialRoughness.c_str(), nullptr, &mShaderBallInnerRoughnessTextureShaderResourceView)))
		{
			throw GameException("Failed to create ShaderBall Inner Material Roughness Texture.");
		}
		std::wstring textureShaderBallInnerMaterialNormal = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\PlasticPattern\\plasticpattern1-normal2b.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureShaderBallInnerMaterialNormal.c_str(), nullptr, &mShaderBallInnerNormalTextureShaderResourceView)))
		{
			throw GameException("Failed to create ShaderBall Inner Material Normal Texture.");
		}
		//									 //
		//***********************************//
		

		//***********************************//
		// LOADING 'BRICK' TEXTURES		     //
		std::wstring textBrickAlbedo = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Brick\\redbricks2b-albedo.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textBrickAlbedo.c_str(), nullptr, &shaderBallBrickMaterial.AlbedoTextureShaderResourceView)))
		{
			throw GameException("Failed to load Brick Albedo Texture.");
		}
		std::wstring textBrickMetalness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Brick\\redbricks2b-metalness.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textBrickMetalness.c_str(), nullptr, &shaderBallBrickMaterial.MetalnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Brick Metalness Texture.");
		}
		std::wstring textBrickRoughness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Brick\\redbricks2b-rough.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textBrickRoughness.c_str(), nullptr, &shaderBallBrickMaterial.RoughnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Brick Roughness Texture.");
		}
		std::wstring textBrickNormal = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Brick\\redbricks2b-normal.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textBrickNormal.c_str(), nullptr, &shaderBallBrickMaterial.NormalTextureShaderResourceView)))
		{
			throw GameException("Failed to load Brick Normal Texture.");
		}
		//									 //
		//***********************************//


		//***********************************//
		// LOADING 'CONCRETE' TEXTURES		 //
		std::wstring textConcreteAlbedo = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Concrete\\concrete2_albedo.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textConcreteAlbedo.c_str(), nullptr, &shaderBallConcreteMaterial.AlbedoTextureShaderResourceView)	))
		{
			throw GameException("Failed to load Concrete Albedo Texture.");
		}
		std::wstring textConcreteMetalness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Concrete\\concrete2_Metallic.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textConcreteMetalness.c_str(), nullptr, &shaderBallConcreteMaterial.MetalnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Concrete Metalness Texture.");
		}
		std::wstring textConcreteRoughness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Concrete\\concrete2_Roughness.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textConcreteRoughness.c_str(), nullptr, &shaderBallConcreteMaterial.RoughnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Concrete Roughness Texture.");
		}
		std::wstring textConcreteNormal = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Concrete\\concrete2_Normal-dx.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textConcreteNormal.c_str(), nullptr, &shaderBallConcreteMaterial.NormalTextureShaderResourceView)))
		{
			throw GameException("Failed to load Concrete Normal Texture.");
		}
		//									 //
		//***********************************//


		//***********************************//
		// LOADING 'BATHROOM' TEXTURES		 //
		std::wstring textBathroomAlbedo = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Bathroom\\bathroomtile1_basecolor.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textBathroomAlbedo.c_str(), nullptr, &shaderBallBathroomMaterial.AlbedoTextureShaderResourceView)))
		{
			throw GameException("Failed to load Bathroom Albedo Texture.");
		}
		std::wstring textBathroomMetalness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Bathroom\\bathroomtile1_metalness.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textBathroomMetalness.c_str(), nullptr, &shaderBallBathroomMaterial.MetalnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Bathroom Metalness Texture.");
		}
		std::wstring textBathroomRoughness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Bathroom\\bathroomtile1_roughness.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textBathroomRoughness.c_str(), nullptr, &shaderBallBathroomMaterial.RoughnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Bathroom Roughness Texture.");
		}
		std::wstring textBathroomNormal = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Bathroom\\bathroomtile1_normal-dx.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textBathroomNormal.c_str(), nullptr, &shaderBallBathroomMaterial.NormalTextureShaderResourceView)))
		{
			throw GameException("Failed to load Bathroom Normal Texture.");
		}
		//									 //
		//***********************************//


		//***********************************//
		// LOADING 'FLOOR' TEXTURES		     //
		std::wstring textFloorAlbedo = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Floor\\Herringbone_marble_tiles_01_Color.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textFloorAlbedo.c_str(), nullptr, &shaderBallFloorMaterial.AlbedoTextureShaderResourceView)))
		{
			throw GameException("Failed to load Floor Albedo Texture.");
		}
		std::wstring textFloorMetalness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Floor\\EmptyMetalMap.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textFloorMetalness.c_str(), nullptr, &shaderBallFloorMaterial.MetalnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Floor Metalness Texture.");
		}
		std::wstring textFloorRoughness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Floor\\Herringbone_marble_tiles_01_Roughness.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textFloorRoughness.c_str(), nullptr, &shaderBallFloorMaterial.RoughnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Floor Roughness Texture.");
		}
		std::wstring textFloorNormal = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Floor\\Herringbone_marble_tiles_01_Normal.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textFloorNormal.c_str(), nullptr, &shaderBallFloorMaterial.NormalTextureShaderResourceView)))
		{
			throw GameException("Failed to load Floor Normal Texture.");
		}
		//									 //
		//***********************************//


		//***********************************//
		// LOADING 'MARBLE' TEXTURES		 //
		std::wstring textMarbleAlbedo = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Marble\\Marble_col.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textMarbleAlbedo.c_str(), nullptr, &shaderBallMarbleMaterial.AlbedoTextureShaderResourceView)))
		{
			throw GameException("Failed to load Marble Albedo Texture.");
		}
		std::wstring textMarbleMetalness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Marble\\EmptyMetalMap.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textMarbleMetalness.c_str(), nullptr, &shaderBallMarbleMaterial.MetalnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Marble Metalness Texture.");
		}
		std::wstring textMarbleRoughness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Marble\\Marble_rgh.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textMarbleRoughness.c_str(), nullptr, &shaderBallMarbleMaterial.RoughnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Marble Roughness Texture.");
		}
		std::wstring textMarbleNormal = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Marble\\Marble_nrm.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textMarbleNormal.c_str(), nullptr, &shaderBallMarbleMaterial.NormalTextureShaderResourceView)))
		{
			throw GameException("Failed to load Marble Normal Texture.");
		}
		//									 //
		//***********************************//


		//***********************************//
		// LOADING 'WOOD' TEXTURES           //
		std::wstring textWoodAlbedo = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Wood\\WoodFloor07_col.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textWoodAlbedo.c_str(), nullptr, &shaderBallWoodMaterial.AlbedoTextureShaderResourceView)))
		{
			throw GameException("Failed to load Peeling Albedo Texture.");
		}
		std::wstring textWoodMetalness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Wood\\EmptyMetalMap.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textWoodMetalness.c_str(), nullptr, &shaderBallWoodMaterial.MetalnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Peeling Metalness Texture.");
		}
		std::wstring textWoodRoughness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Wood\\WoodFloor07_rgh.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textWoodRoughness.c_str(), nullptr, &shaderBallWoodMaterial.RoughnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Peeling Roughness Texture.");
		}
		std::wstring textWoodNormal = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Wood\\WoodFloor07_nrm.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textWoodNormal.c_str(), nullptr, &shaderBallWoodMaterial.NormalTextureShaderResourceView)))
		{
			throw GameException("Failed to load Peeling Normal Texture.");
		}
		//									 //
		//***********************************//


		//***********************************//
		// LOADING 'GOLD' TEXTURES		     //
		std::wstring textGoldAlbedo = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Gold\\GoldMetal_albedo.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textGoldAlbedo.c_str(), nullptr, &shaderBallGoldMaterial.AlbedoTextureShaderResourceView)))
		{
			throw GameException("Failed to load Gold Albedo Texture.");
		}
		std::wstring textGoldMetalness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Gold\\GoldMetal_mtl.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textGoldMetalness.c_str(), nullptr, &shaderBallGoldMaterial.MetalnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Gold Metalness Texture.");
		}
		std::wstring textGoldRoughness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\TestTexture.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textGoldRoughness.c_str(), nullptr, &shaderBallGoldMaterial.RoughnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Gold Roughness Texture.");
		}
		std::wstring textGoldNormal = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Gold\\GoldMetal_nrm.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textGoldNormal.c_str(), nullptr, &shaderBallGoldMaterial.NormalTextureShaderResourceView)))
		{
			throw GameException("Failed to load Gold Normal Texture.");
		}
		//									 //
		//***********************************//


		//***********************************//
		// LOADING 'SILVER' TEXTURES		 //
		std::wstring textSilverAlbedo = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Silver\\SilverMetal_col.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textSilverAlbedo.c_str(), nullptr, &shaderBallSilverMaterial.AlbedoTextureShaderResourceView)))
		{
			throw GameException("Failed to load Silver Albedo Texture.");
		}
		std::wstring textSilverMetalness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\TestTexture.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textSilverMetalness.c_str(), nullptr, &shaderBallSilverMaterial.MetalnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Silver Metalness Texture.");
		}
		std::wstring textSilverRoughness = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Silver\\SilverMetal_rgh.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textSilverRoughness.c_str(), nullptr, &shaderBallSilverMaterial.RoughnessTextureShaderResourceView)))
		{
			throw GameException("Failed to load Silver Roughness Texture.");
		}
		std::wstring textSilverNormal = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Silver\\SilverMetal_nrm.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textSilverNormal.c_str(), nullptr, &shaderBallSilverMaterial.NormalTextureShaderResourceView)))
		{
			throw GameException("Failed to load Silver Normal Texture.");
		}
		//									 //
		//***********************************//


		//***********************************//
		// Initializing ShaderBall meshes
		{
			/// mesh 1 - outer part
			Mesh* mesh = model->Meshes().at(0);

			mPBRShaderBallOuterMaterial = new PBRMaterial();
			mPBRShaderBallOuterMaterial->Initialize(mEffectPBR);

			mPBRShaderBallOuterMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh, &mVertexBufferShaderBallOuterPart);
			mesh->CreateIndexBuffer(&mIndexBufferShaderBallOuterPart);
			mIndexCountShaderBallOuterPart = mesh->Indices().size();

			/// mesh 2 - inner part
			Mesh* mesh2 = model->Meshes().at(1);
			mPBRShaderBallInnerMaterial = new PBRMaterial();
			mPBRShaderBallInnerMaterial->Initialize(mEffectPBR);

			mPBRShaderBallInnerMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh2, &mVertexBufferShaderBallInnerPart);
			mesh2->CreateIndexBuffer(&mIndexBufferShaderBallInnerPart);
			mIndexCountShaderBallInnerPart = mesh2->Indices().size(); 
		
		}
		//***********************************//


		// translation first row (roughness) shader balls
		for (int i = 0; i < NUM_SPHERES; i++)
		{
			XMMATRIX* translation = &XMLoadFloat4x4(&mWorldMatricesShaderBallFirstRow[i]);
			XMFLOAT3 newPOS = {15.0f*(i-2),0, -30.0f};
			MatrixHelper::SetTranslation(*translation, newPOS);
			XMStoreFloat4x4(&mWorldMatricesShaderBallFirstRow[i], *translation);
		}

		// translation second row (metalness) shader balls
		for (int i = 0; i < NUM_SPHERES; i++)
		{
			XMMATRIX* translation = &XMLoadFloat4x4(&mWorldMatricesShaderBallSecondRow[i]);
			XMFLOAT3 newPOS = { 15.0f*(i - 2),0, -15.0f };
			MatrixHelper::SetTranslation(*translation, newPOS);
			XMStoreFloat4x4(&mWorldMatricesShaderBallSecondRow[i], *translation);
		}

		// translation brick shader ball
		{
			XMMATRIX* translation = &XMLoadFloat4x4(&mWorldMatricesShaderBallFirstRow[0]);
			XMFLOAT3 newPOS = { -15.0f, 0.0f, 0 };
			MatrixHelper::SetTranslation(*translation, newPOS);
			XMStoreFloat4x4(&shaderBallBrickMaterial.WorldMatrix, *translation);
		}

		// translation concrete shader ball
		{
			XMMATRIX* translation = &XMLoadFloat4x4(&mWorldMatricesShaderBallFirstRow[0]);
			XMFLOAT3 newPOS = { 0.0f, 0.0f, 0 };
			MatrixHelper::SetTranslation(*translation, newPOS);
			XMStoreFloat4x4(&shaderBallConcreteMaterial.WorldMatrix, *translation);
		}

		// translation bathroom shader ball
		{
			XMMATRIX* translation = &XMLoadFloat4x4(&mWorldMatricesShaderBallFirstRow[0]);
			XMFLOAT3 newPOS = { 15.0f, 0.0f, 0 };
			MatrixHelper::SetTranslation(*translation, newPOS);
			XMStoreFloat4x4(&shaderBallBathroomMaterial.WorldMatrix, *translation);
		}

		// translation floor shader ball
		{
			XMMATRIX* translation = &XMLoadFloat4x4(&mWorldMatricesShaderBallFirstRow[0]);
			XMFLOAT3 newPOS = { -7.5f, 0.0f, 15.0f };
			MatrixHelper::SetTranslation(*translation, newPOS);
			XMStoreFloat4x4(&shaderBallFloorMaterial.WorldMatrix, *translation);
		}

		// translation marble shader ball
		{
			XMMATRIX* translation = &XMLoadFloat4x4(&mWorldMatricesShaderBallFirstRow[0]);
			XMFLOAT3 newPOS = { 7.5f, 0.0f, 15.0f };
			MatrixHelper::SetTranslation(*translation, newPOS);
			XMStoreFloat4x4(&shaderBallMarbleMaterial.WorldMatrix, *translation);
		}

		// translation wood shader ball
		{
			XMMATRIX* translation = &XMLoadFloat4x4(&mWorldMatricesShaderBallFirstRow[0]);
			XMFLOAT3 newPOS = { 0.0f, 0.0f, 30.0f };
			MatrixHelper::SetTranslation(*translation, newPOS);
			XMStoreFloat4x4(&shaderBallWoodMaterial.WorldMatrix, *translation);
		}



		lightData.PointLight = new PointLight(*mGame);
		lightData.PointLight->SetRadius(120.0f);
		lightData.PointLight->SetPosition(82.5f, 0.0f, 100.0f);

		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);

		mProxyModel = new ProxyModel(*mGame, *mCamera, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\PointLightProxy.obj", 0.5f);
		mProxyModel->Initialize();

		mSkybox = new Skybox(*mGame, *mCamera, L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\PBR\\Skyboxes\\milkmill_cube_map.dds", 100.0f);
		mSkybox->Initialize();
		mGame->components.push_back(mSkybox);// push_back(mSkybox);


		mRenderStateHelper = new RenderStateHelper(*mGame);
	}

	void PhysicallyBasedRenderingDemo::Update(const GameTime& gameTime)
	{
		UpdatePointLight(gameTime);

		mProxyModel->Update(gameTime);

		UpdateImGui();
	}

	void PhysicallyBasedRenderingDemo::Draw(const GameTime& gameTime)
	{
		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//mSkybox->Draw(gameTime);


		//**************************************//
		//**************************************//
		//     'GOLD MATERIAL' SHADER BALL 	    //
		//										//
		// Pass 1 - Outer Material				//
		for (int i = 0; i < NUM_SPHERES; i++)
		{
			Pass* pass = mPBRShaderBallOuterMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallOuterMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallOuterMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallOuterPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallOuterPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&mWorldMatricesShaderBallFirstRow[i]);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
			

			mPBRShaderBallOuterMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallOuterMaterial->World() << worldMatrix;
			mPBRShaderBallOuterMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallOuterMaterial->LightRadius() <<   lightData.PointLight->Radius();
			mPBRShaderBallOuterMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallOuterMaterial->Roughness() << (float)i/(float)(2*NUM_SPHERES);
			mPBRShaderBallOuterMaterial->Metalness() << -1.0f;

			mPBRShaderBallOuterMaterial->albedoTexture()    << shaderBallGoldMaterial.AlbedoTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->metallicTexture()  << shaderBallGoldMaterial.MetalnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->roughnessTexture() << shaderBallGoldMaterial.RoughnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->normalTexture()	<< shaderBallGoldMaterial.NormalTextureShaderResourceView;

			mPBRShaderBallOuterMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallOuterPart, 0, 0);
		}
		// Pass 2 - Inner Material				//
		for (int i = 0; i < NUM_SPHERES; i++)
		{
			Pass* pass = mPBRShaderBallInnerMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallInnerMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallInnerMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallInnerPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallInnerPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&mWorldMatricesShaderBallFirstRow[i]);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallInnerMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallInnerMaterial->World() << worldMatrix;
			mPBRShaderBallInnerMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallInnerMaterial->LightRadius() <<	 lightData.PointLight->Radius();
			mPBRShaderBallInnerMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallInnerMaterial->Roughness() << i / 10.0f;
			mPBRShaderBallInnerMaterial->Metalness() << 1.0f;

			mPBRShaderBallInnerMaterial->albedoTexture()	 << mShaderBallInnerAlbedoTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->metallicTexture()  << mShaderBallInnerMetalnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->roughnessTexture() << mShaderBallInnerRoughnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->normalTexture()	 << mShaderBallInnerNormalTextureShaderResourceView;

			mPBRShaderBallInnerMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;
			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallInnerPart, 0, 0);
		}
		//										//
		//**************************************//
		//**************************************//


		//**************************************//
		//**************************************//
		//    'SILVER MATERIAL' SHADER BALL 	//
		//										//
		// Pass 1 - Outer Material				//
		for (int i = 0; i < NUM_SPHERES; i++)
		{
			Pass* pass = mPBRShaderBallOuterMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallOuterMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallOuterMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallOuterPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallOuterPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&mWorldMatricesShaderBallSecondRow[i]);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallOuterMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallOuterMaterial->World() << worldMatrix;
			mPBRShaderBallOuterMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallOuterMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallOuterMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallOuterMaterial->Roughness() << -1.0f;
			mPBRShaderBallOuterMaterial->Metalness() << (float)i / (float)(NUM_SPHERES);

			mPBRShaderBallOuterMaterial->albedoTexture()	<< shaderBallSilverMaterial.AlbedoTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->metallicTexture()  << shaderBallSilverMaterial.MetalnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->roughnessTexture() << shaderBallSilverMaterial.RoughnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->normalTexture()	<< shaderBallSilverMaterial.NormalTextureShaderResourceView;

			mPBRShaderBallOuterMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallOuterPart, 0, 0);
		}
		// Pass 2 - Inner Material				//
		for (int i = 0; i < NUM_SPHERES; i++)
		{
			Pass* pass = mPBRShaderBallInnerMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallInnerMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallInnerMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallInnerPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallInnerPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&mWorldMatricesShaderBallSecondRow[i]);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallInnerMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallInnerMaterial->World() << worldMatrix;
			mPBRShaderBallInnerMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallInnerMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallInnerMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallInnerMaterial->Roughness() << i / 10.0f;
			mPBRShaderBallInnerMaterial->Metalness() << 1.0f;

			mPBRShaderBallInnerMaterial->albedoTexture() << mShaderBallInnerAlbedoTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->metallicTexture() << mShaderBallInnerMetalnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->roughnessTexture() << mShaderBallInnerRoughnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->normalTexture() << mShaderBallInnerNormalTextureShaderResourceView;

			mPBRShaderBallInnerMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;
			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallInnerPart, 0, 0);
		}
		//										//
		//**************************************//
		//**************************************//


		//**************************************//
		//**************************************//
		//     'BRICK MATERIAL' SHADER BALL     //
		//										//
		// Pass 1 - Outer Material				//
		{
			Pass* pass = mPBRShaderBallOuterMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallOuterMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallOuterMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallOuterPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallOuterPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallBrickMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallOuterMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallOuterMaterial->World() << worldMatrix;
			mPBRShaderBallOuterMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallOuterMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallOuterMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallOuterMaterial->Roughness() << 0.0f;
			mPBRShaderBallOuterMaterial->Metalness() << 1.0f;

			mPBRShaderBallOuterMaterial->albedoTexture()	<< shaderBallBrickMaterial.AlbedoTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->metallicTexture()  << shaderBallBrickMaterial.MetalnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->roughnessTexture() << shaderBallBrickMaterial.RoughnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->normalTexture()	<< shaderBallBrickMaterial.NormalTextureShaderResourceView;

			mPBRShaderBallOuterMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallOuterPart, 0, 0);
		}								//								//
		// Pass 2 - Inner Material				//
		{
			Pass* pass = mPBRShaderBallInnerMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallInnerMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallInnerMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallInnerPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallInnerPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallBrickMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallInnerMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallInnerMaterial->World() << worldMatrix;
			mPBRShaderBallInnerMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallInnerMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallInnerMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallInnerMaterial->Roughness() << 0.0f;
			mPBRShaderBallInnerMaterial->Metalness() << 1.0f;

			mPBRShaderBallInnerMaterial->albedoTexture() << mShaderBallInnerAlbedoTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->metallicTexture() << mShaderBallInnerMetalnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->roughnessTexture() << mShaderBallInnerRoughnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->normalTexture() << mShaderBallInnerNormalTextureShaderResourceView;

			mPBRShaderBallInnerMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;
			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallInnerPart, 0, 0);
		}								//
		//										//
		//**************************************//
		//**************************************//


		//**************************************//
		//**************************************//
		//   'CONCRETE MATERIAL' SHADER BALL 	//
		//										//
		// Pass 1 - Outer Material				//
		{
			Pass* pass = mPBRShaderBallOuterMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallOuterMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallOuterMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallOuterPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallOuterPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallConcreteMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallOuterMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallOuterMaterial->World() << worldMatrix;
			mPBRShaderBallOuterMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallOuterMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallOuterMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallOuterMaterial->Roughness() << 0.0f;
			mPBRShaderBallOuterMaterial->Metalness() << 1.0f;

			mPBRShaderBallOuterMaterial->albedoTexture()	<< shaderBallConcreteMaterial.AlbedoTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->metallicTexture()  << shaderBallConcreteMaterial.MetalnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->roughnessTexture() << shaderBallConcreteMaterial.RoughnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->normalTexture()	<< shaderBallConcreteMaterial.NormalTextureShaderResourceView;

			mPBRShaderBallOuterMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallOuterPart, 0, 0);
		}								//
		// Pass 2 - Inner Material				//
		{
			Pass* pass = mPBRShaderBallInnerMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallInnerMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallInnerMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallInnerPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallInnerPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallConcreteMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallInnerMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallInnerMaterial->World() << worldMatrix;
			mPBRShaderBallInnerMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallInnerMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallInnerMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallInnerMaterial->Roughness() << 0.0f;
			mPBRShaderBallInnerMaterial->Metalness() << 1.0f;

			mPBRShaderBallInnerMaterial->albedoTexture()	 << mShaderBallInnerAlbedoTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->metallicTexture()	 << mShaderBallInnerMetalnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->roughnessTexture()	 << mShaderBallInnerRoughnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->normalTexture()	 << mShaderBallInnerNormalTextureShaderResourceView;

			mPBRShaderBallInnerMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;
			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallInnerPart, 0, 0);
		}								//
		//										//
		//**************************************//
		//**************************************//


		//**************************************//
		//**************************************//
		//   'BATHROOM MATERIAL' SHADER BALL 	//
		//										//
		// Pass 1 - Outer Material				//
		{
			Pass* pass = mPBRShaderBallOuterMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallOuterMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallOuterMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallOuterPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallOuterPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallBathroomMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallOuterMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallOuterMaterial->World() << worldMatrix;
			mPBRShaderBallOuterMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallOuterMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallOuterMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallOuterMaterial->Roughness() << 0.0f;
			mPBRShaderBallOuterMaterial->Metalness() << 1.0f;

			mPBRShaderBallOuterMaterial->albedoTexture()	<< shaderBallBathroomMaterial.AlbedoTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->metallicTexture()  << shaderBallBathroomMaterial.MetalnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->roughnessTexture() << shaderBallBathroomMaterial.RoughnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->normalTexture()	<< shaderBallBathroomMaterial.NormalTextureShaderResourceView;

			mPBRShaderBallOuterMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallOuterPart, 0, 0);
		}								//
		// Pass 2 - Inner Material				//
		{
			Pass* pass = mPBRShaderBallInnerMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallInnerMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallInnerMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallInnerPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallInnerPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallBathroomMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallInnerMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallInnerMaterial->World() << worldMatrix;
			mPBRShaderBallInnerMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallInnerMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallInnerMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallInnerMaterial->Roughness() << 0.0f;
			mPBRShaderBallInnerMaterial->Metalness() << 1.0f;

			mPBRShaderBallInnerMaterial->albedoTexture()	<< mShaderBallInnerAlbedoTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->metallicTexture()  << mShaderBallInnerMetalnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->roughnessTexture() << mShaderBallInnerRoughnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->normalTexture()	<< mShaderBallInnerNormalTextureShaderResourceView;

			mPBRShaderBallInnerMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;
			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallInnerPart, 0, 0);
		}								//
		//										//
		//**************************************//
		//**************************************//


		//**************************************//
		//**************************************//
		//    'FLOOR MATERIAL' SHADER BALL 	    //
		//										//
		// Pass 1 - Outer Material				//
		{
			Pass* pass = mPBRShaderBallOuterMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallOuterMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallOuterMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallOuterPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallOuterPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallFloorMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallOuterMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallOuterMaterial->World() << worldMatrix;
			mPBRShaderBallOuterMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallOuterMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallOuterMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallOuterMaterial->Roughness() << 0.0f;
			mPBRShaderBallOuterMaterial->Metalness() << 1.0f;

			mPBRShaderBallOuterMaterial->albedoTexture()	<< shaderBallFloorMaterial.AlbedoTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->metallicTexture()  << shaderBallFloorMaterial.MetalnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->roughnessTexture() << shaderBallFloorMaterial.RoughnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->normalTexture()	<< shaderBallFloorMaterial.NormalTextureShaderResourceView;

			mPBRShaderBallOuterMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallOuterPart, 0, 0);
		}								//
		// Pass 2 - Inner Material				//
		{
			Pass* pass = mPBRShaderBallInnerMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallInnerMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallInnerMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallInnerPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallInnerPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallFloorMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallInnerMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallInnerMaterial->World() << worldMatrix;
			mPBRShaderBallInnerMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallInnerMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallInnerMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallInnerMaterial->Roughness() << 0.0f;
			mPBRShaderBallInnerMaterial->Metalness() << 1.0f;

			mPBRShaderBallInnerMaterial->albedoTexture() << mShaderBallInnerAlbedoTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->metallicTexture() << mShaderBallInnerMetalnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->roughnessTexture() << mShaderBallInnerRoughnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->normalTexture() << mShaderBallInnerNormalTextureShaderResourceView;

			mPBRShaderBallInnerMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;
			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallInnerPart, 0, 0);
		}								//
	    //										//
	    //**************************************//
	    //**************************************//


		//**************************************//
		//**************************************//
		//    'MARBLE MATERIAL' SHADER BALL 	//
		//										//
		// Pass 1 - Outer Material				//
		{
			Pass* pass = mPBRShaderBallOuterMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallOuterMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallOuterMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallOuterPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallOuterPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallMarbleMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallOuterMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallOuterMaterial->World() << worldMatrix;
			mPBRShaderBallOuterMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallOuterMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallOuterMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallOuterMaterial->Roughness() << 0.0f;
			mPBRShaderBallOuterMaterial->Metalness() << 1.0f;

			mPBRShaderBallOuterMaterial->albedoTexture()	<< shaderBallMarbleMaterial.AlbedoTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->metallicTexture()  << shaderBallMarbleMaterial.MetalnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->roughnessTexture() << shaderBallMarbleMaterial.RoughnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->normalTexture()	<< shaderBallMarbleMaterial.NormalTextureShaderResourceView;

			mPBRShaderBallOuterMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallOuterPart, 0, 0);
		}								//
		// Pass 2 - Inner Material				//
		{
			Pass* pass = mPBRShaderBallInnerMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallInnerMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallInnerMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallInnerPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallInnerPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallMarbleMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallInnerMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallInnerMaterial->World() << worldMatrix;
			mPBRShaderBallInnerMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallInnerMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallInnerMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallInnerMaterial->Roughness() << 0.0f;
			mPBRShaderBallInnerMaterial->Metalness() << 1.0f;

			mPBRShaderBallInnerMaterial->albedoTexture() << mShaderBallInnerAlbedoTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->metallicTexture() << mShaderBallInnerMetalnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->roughnessTexture() << mShaderBallInnerRoughnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->normalTexture() << mShaderBallInnerNormalTextureShaderResourceView;

			mPBRShaderBallInnerMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;
			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallInnerPart, 0, 0);
		}								//
		//										//
		//**************************************//
		//**************************************//


		//**************************************//
		//**************************************//
		//     'WOOD MATERIAL' SHADER BALL      //
		//										//
		// Pass 1 - Outer Material				//
		{
			Pass* pass = mPBRShaderBallOuterMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallOuterMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallOuterMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallOuterPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallOuterPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallWoodMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallOuterMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallOuterMaterial->World() << worldMatrix;
			mPBRShaderBallOuterMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallOuterMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallOuterMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallOuterMaterial->Roughness() << 0.0f;
			mPBRShaderBallOuterMaterial->Metalness() << 1.0f;

			mPBRShaderBallOuterMaterial->albedoTexture()	<< shaderBallWoodMaterial.AlbedoTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->metallicTexture()  << shaderBallWoodMaterial.MetalnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->roughnessTexture() << shaderBallWoodMaterial.RoughnessTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->normalTexture()	<< shaderBallWoodMaterial.NormalTextureShaderResourceView;

			mPBRShaderBallOuterMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallOuterMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallOuterPart, 0, 0);
		}								//
		// Pass 2 - Inner Material				//
		{
			Pass* pass = mPBRShaderBallInnerMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mPBRShaderBallInnerMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mPBRShaderBallInnerMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBufferShaderBallInnerPart, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mIndexBufferShaderBallInnerPart, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&shaderBallWoodMaterial.WorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();


			mPBRShaderBallInnerMaterial->WorldViewProjection() << wvp;
			mPBRShaderBallInnerMaterial->World() << worldMatrix;
			mPBRShaderBallInnerMaterial->LightPosition() << lightData.PointLight->PositionVector();
			mPBRShaderBallInnerMaterial->LightRadius() << lightData.PointLight->Radius();
			mPBRShaderBallInnerMaterial->CameraPosition() << mCamera->PositionVector();
			mPBRShaderBallInnerMaterial->Roughness() << 0.0f;
			mPBRShaderBallInnerMaterial->Metalness() << 1.0f;

			mPBRShaderBallInnerMaterial->albedoTexture() << mShaderBallInnerAlbedoTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->metallicTexture() << mShaderBallInnerMetalnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->roughnessTexture() << mShaderBallInnerRoughnessTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->normalTexture() << mShaderBallInnerNormalTextureShaderResourceView;

			mPBRShaderBallInnerMaterial->irradianceTexture() << mIrradianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->radianceTexture() << mRadianceTextureShaderResourceView;
			mPBRShaderBallInnerMaterial->integrationTexture() << mIntegrationTextureShaderResourceView;
			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mIndexCountShaderBallInnerPart, 0, 0);
		}								//
		//										//
		//**************************************//
		//**************************************//


		mProxyModel->Draw(gameTime);

		mRenderStateHelper->SaveAll();
		mRenderStateHelper->RestoreAll();

		#pragma region DRAW_IMGUI
				ImGui::Render();
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		#pragma endregion
	}
	void PhysicallyBasedRenderingDemo::UpdateImGui()
	{
		#pragma region LEVEL_SPECIFIC_IMGUI
		ImGui::Begin("Physically Based Rendering - Demo");

		ImGui::Checkbox("Show skybox", &mShowSkybox);
		mSkybox->SetVisible(mShowSkybox);

		if (ImGui::CollapsingHeader("Light Properties"))
		{
			static float radius = lightData.PointLight->Radius();
			ImGui::SliderFloat("Radius", &radius, 0, 1000);

			lightData.PointLight->SetRadius(radius);

		}

		ImGui::Separator();
		ImGui::Text("PBR textures are taken from:");
		ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1), "www.freepbr.com");
		ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1), "www.cgbookcase.com");
		ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1), "www.cc0textures.com");
		



		ImGui::End();
#pragma endregion
	}
	void PhysicallyBasedRenderingDemo::UpdatePointLight(const GameTime& gameTime)
	{
		static float pointLightIntensity = lightData.PointLight->Color().a;
		float elapsedTime = (float)gameTime.ElapsedGameTime();

		// Update point light intensity		
		if (mKeyboard->IsKeyDown(DIK_HOME) && pointLightIntensity < UCHAR_MAX)
		{
			pointLightIntensity += LightModulationRate * elapsedTime;

			XMCOLOR pointLightLightColor = lightData.PointLight->Color();
			pointLightLightColor.a = (UCHAR)XMMin<float>(pointLightIntensity, UCHAR_MAX);
			lightData.PointLight->SetColor(pointLightLightColor);
		}
		if (mKeyboard->IsKeyDown(DIK_END) && pointLightIntensity > 0)
		{
			pointLightIntensity -= LightModulationRate * elapsedTime;

			XMCOLOR pointLightLightColor = lightData.PointLight->Color();
			pointLightLightColor.a = (UCHAR)XMMax<float>(pointLightIntensity, 0.0f);
			lightData.PointLight->SetColor(pointLightLightColor);
		}

		// Move point light
		XMFLOAT3 movementAmount = Vector3Helper::Zero;
		if (mKeyboard != nullptr)
		{
			if (mKeyboard->IsKeyDown(DIK_NUMPAD4))
			{
				movementAmount.x = -1.0f;
			}

			if (mKeyboard->IsKeyDown(DIK_NUMPAD6))
			{
				movementAmount.x = 1.0f;
			}

			if (mKeyboard->IsKeyDown(DIK_NUMPAD9))
			{
				movementAmount.y = 1.0f;
			}

			if (mKeyboard->IsKeyDown(DIK_NUMPAD3))
			{
				movementAmount.y = -1.0f;
			}

			if (mKeyboard->IsKeyDown(DIK_NUMPAD8))
			{
				movementAmount.z = -1.0f;
			}

			if (mKeyboard->IsKeyDown(DIK_NUMPAD2))
			{
				movementAmount.z = 1.0f;
			}
		}

		XMVECTOR movement = XMLoadFloat3(&movementAmount) * LightMovementRate * elapsedTime;
		lightData.PointLight->SetPosition(lightData.PointLight->PositionVector() + movement);
		mProxyModel->SetPosition(lightData.PointLight->Position());
	}

}