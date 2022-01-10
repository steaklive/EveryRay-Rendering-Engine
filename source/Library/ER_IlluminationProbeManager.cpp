#include "ER_IlluminationProbeManager.h"
#include "Game.h"
#include "GameTime.h"
#include "ShadowMapper.h"
#include "DirectionalLight.h"
#include "MatrixHelper.h"
#include "MaterialHelper.h"
#include "VectorHelper.h"
#include "StandardLightingMaterial.h"
#include "Skybox.h"

#include "DirectXTex.h"

namespace Library
{
	ER_IlluminationProbeManager::ER_IlluminationProbeManager(Game& game, DirectionalLight& light, ShadowMapper& shadowMapper)
	{
		//TODO temp
		mLightProbes.push_back(new ER_LightProbe(game, light, shadowMapper, XMFLOAT3(0.0f, 10.0f, 35.0f), 256));
	}

	ER_IlluminationProbeManager::~ER_IlluminationProbeManager()
	{
		DeletePointerCollection(mLightProbes);
	}

	void ER_IlluminationProbeManager::ComputeOrLoadProbes(Game& game, const GameTime& gameTime, ProbesRenderingObjectsInfo& aObjects, Skybox* skybox)
	{
		for (auto& lightProbe : mLightProbes)
			lightProbe->Compute(game, gameTime, aObjects, skybox, mLevelPath);
	}

	ER_LightProbe::ER_LightProbe(Game& game, DirectionalLight& light, ShadowMapper& shadowMapper, const XMFLOAT3& position, int size)
		: mPosition(position)
		, mSize(size)
		, mDirectionalLight(light)
		, mShadowMapper(shadowMapper)
	{

		//X+, X-, Y+, Y-, Z+, Z-
		const XMFLOAT3 facesDirections[CUBEMAP_FACES_COUNT] = {
			Vector3Helper::Right,
			Vector3Helper::Left,
			Vector3Helper::Up,
			Vector3Helper::Down,
			Vector3Helper::Backward,
			Vector3Helper::Forward
		};
		const XMFLOAT3 upDirections[CUBEMAP_FACES_COUNT] = {
			Vector3Helper::Up,
			Vector3Helper::Up,
			Vector3Helper::Forward,
			Vector3Helper::Backward,
			Vector3Helper::Up,
			Vector3Helper::Up
		};

		mCubemapFacesRT = new CustomRenderTarget(game.Direct3DDevice(), size, size, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1, -1, 6, true);
		
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mCubemapCameras[i] = new Camera(game, XM_PIDIV2, 1.0f, 0.1f, 100000.0f);
			mCubemapCameras[i]->Initialize();
			mCubemapCameras[i]->SetPosition(mPosition);
			mCubemapCameras[i]->SetDirection(facesDirections[i]);
			mCubemapCameras[i]->SetUp(upDirections[i]);
			mCubemapCameras[i]->UpdateViewMatrix(true); //fix for mirrored result due to right-hand coordinate system
			mCubemapCameras[i]->UpdateProjectionMatrix(true); //fix for mirrored result due to right-hand coordinate system

			mDepthBuffers[i] = DepthTarget::Create(game.Direct3DDevice(), size, size, 1u, DXGI_FORMAT_D24_UNORM_S8_UINT);
		}
	}

	ER_LightProbe::~ER_LightProbe()
	{
		DeleteObject(mCubemapFacesRT);
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			DeleteObject(mCubemapCameras[i]);
			DeleteObject(mDepthBuffers[i]);
		}

	}

	void ER_LightProbe::Compute(Game& game, const GameTime& gameTime, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox, const std::wstring& levelPath, bool forceRecompute)
	{
		if (mIsComputed && !forceRecompute)
			return;

		bool alreadyExistsInFile = false;
		std::wstring probeName = GetConstructedProbeName(levelPath, LIGHT_PROBE);

		if (!forceRecompute)
		{
			ID3D11ShaderResourceView* srv = mCubemapFacesRT->getSRV();
			if (FAILED(DirectX::CreateDDSTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(), probeName.c_str(), nullptr, &srv)))
			{
				//std::wstring status = L"Failed to load DDS probe texture: " + probeName;
				//output to LOG (not exception)
			}
			else
			{
				alreadyExistsInFile = true;
				mIsComputed = true;
			}
		}

		if (!alreadyExistsInFile || forceRecompute)
			DrawProbe(game, gameTime, levelPath, objectsToRender, skybox);
	}

	void ER_LightProbe::DrawProbe(Game& game, const GameTime& gameTime, const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox)
	{
		if (mIsComputed)
			return;

		for (int cubemapFaceIndex = 0; cubemapFaceIndex < CUBEMAP_FACES_COUNT; cubemapFaceIndex++)
			for (auto& object : objectsToRender)
				object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::forwardLightingForProbesMaterialName + "_" + std::to_string(cubemapFaceIndex),
					[&, cubemapFaceIndex](int meshIndex) { UpdateStandardLightingPBRMaterialVariables(object.second, meshIndex, cubemapFaceIndex); });

		auto context = game.Direct3DDeviceContext();
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		UINT viewportsCount = 1;

		CD3D11_VIEWPORT oldViewPort;
		context->RSGetViewports(&viewportsCount, &oldViewPort);
		CD3D11_VIEWPORT newViewPort(0.0f, 0.0f, static_cast<float>(mSize), static_cast<float>(mSize));
		context->RSSetViewports(1, &newViewPort);

		for (int cubeMapFace = 0; cubeMapFace < CUBEMAP_FACES_COUNT; cubeMapFace++)
		{
			// Set the render target and clear it.
			context->OMSetRenderTargets(1, &mCubemapFacesRT->getRTVs()[cubeMapFace], mDepthBuffers[cubeMapFace]->getDSV());
			context->ClearRenderTargetView(mCubemapFacesRT->getRTVs()[cubeMapFace], clearColor);
			context->ClearDepthStencilView(mDepthBuffers[cubeMapFace]->getDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			if (skybox)
			{
				skybox->Update(gameTime, mCubemapCameras[cubeMapFace]);
				skybox->Draw(mCubemapCameras[cubeMapFace]);
				//TODO draw sun
				//...
				//skybox->UpdateSun(gameTime, mCubemapCameras[cubeMapFace]);
			}

			// We dont do lodding because it is bound to main camera... We force pick lod 0.
			// This is incorrect and might cause issues like: 
			// Probe P is next to object A, but object A is far from main camera => A does not have lod 0, probe P can not render A.
			const int lod = 0;
			
			//TODO change to culled objects per face (not a priority since we compute probes once)
			for (auto& object : objectsToRender)
			{
				if (object.second->IsInLightProbe())
					object.second->DrawLOD(MaterialHelper::forwardLightingForProbesMaterialName + "_" + std::to_string(cubeMapFace), false, -1, lod);
			}
		}

		context->RSSetViewports(1, &oldViewPort);

		for (int cubeMapFaceIndex = 0; cubeMapFaceIndex < CUBEMAP_FACES_COUNT; cubeMapFaceIndex++)
			for (auto& object : objectsToRender)
				object.second->MeshMaterialVariablesUpdateEvent->RemoveListener(MaterialHelper::forwardLightingForProbesMaterialName + "_" + std::to_string(cubeMapFaceIndex));

		SaveProbeOnDisk(game, levelPath, LIGHT_PROBE);
		
		//for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		//{
		//	DeleteObject(mCubemapCameras[i]);
		//	DeleteObject(mDepthBuffers[i]);
		//}

		mIsComputed = true;
	}

	void ER_LightProbe::SaveProbeOnDisk(Game& game, const std::wstring& levelPath, ER_ProbeType aType)
	{
		DirectX::ScratchImage tempImage;
		HRESULT res = DirectX::CaptureTexture(game.Direct3DDevice(), game.Direct3DDeviceContext(), mCubemapFacesRT->getTexture2D(), tempImage);
		if (FAILED(res))
			throw GameException("Failed to capture a probe texture when saving!", res);

		std::wstring fileName = GetConstructedProbeName(levelPath, aType);

		res = DirectX::SaveToDDSFile(tempImage.GetImages(), tempImage.GetImageCount(), tempImage.GetMetadata(), DDS_FLAGS_NONE, fileName.c_str());
		if (FAILED(res))
		{
			std::string str(fileName.begin(), fileName.end());
			std::string msg = "Failed to save a probe texture: " + str;
			throw GameException(msg.c_str());
		}
	}

	std::wstring ER_LightProbe::GetConstructedProbeName(const std::wstring& levelPath, ER_ProbeType aType)
	{
		std::wstring fileName = levelPath;
		if (aType == LIGHT_PROBE)
			fileName += L"light_probe";
		else if (aType == REFLECTION_PROBE)
			fileName += L"reflection_probe";

		fileName += L"_"
			+ std::to_wstring(static_cast<int>(mCubemapCameras[0]->Position().x)) + L"_"
			+ std::to_wstring(static_cast<int>(mCubemapCameras[0]->Position().y)) + L"_"
			+ std::to_wstring(static_cast<int>(mCubemapCameras[0]->Position().z)) + L".dds";

		return fileName;
	}

	void ER_LightProbe::UpdateStandardLightingPBRMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, int cubeFaceIndex)
	{
		assert(mCubemapCameras);

		XMMATRIX worldMatrix = XMLoadFloat4x4(&(obj->GetTransformationMatrix4X4()));
		XMMATRIX vp = mCubemapCameras[cubeFaceIndex]->ViewMatrix() * mCubemapCameras[cubeFaceIndex]->ProjectionMatrix();

		XMMATRIX shadowMatrices[3] =
		{
			mShadowMapper.GetViewMatrix(0) * mShadowMapper.GetProjectionMatrix(0) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper.GetViewMatrix(1) * mShadowMapper.GetProjectionMatrix(1) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper.GetViewMatrix(2) * mShadowMapper.GetProjectionMatrix(2) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix())
		};

		ID3D11ShaderResourceView* shadowMaps[3] =
		{
			mShadowMapper.GetShadowTexture(0),
			mShadowMapper.GetShadowTexture(1),
			mShadowMapper.GetShadowTexture(2)
		};

		std::string materialName = MaterialHelper::forwardLightingForProbesMaterialName + "_" + std::to_string(cubeFaceIndex);
		auto material = static_cast<Rendering::StandardLightingMaterial*>(obj->GetMaterials()[materialName]);
		if (material)
		{
			material->ViewProjection() << vp;
			material->World() << worldMatrix;
			material->ShadowMatrices().SetMatrixArray(shadowMatrices, 0, NUM_SHADOW_CASCADES);
			material->CameraPosition() << mCubemapCameras[cubeFaceIndex]->PositionVector();
			material->SunDirection() << XMVectorNegate(mDirectionalLight.DirectionVector());
			material->SunColor() << XMVECTOR{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, 5.0f };
			material->AmbientColor() << XMVECTOR{ mDirectionalLight.GetAmbientLightColor().x,  mDirectionalLight.GetAmbientLightColor().y, mDirectionalLight.GetAmbientLightColor().z , 1.0f };
			material->ShadowTexelSize() << XMVECTOR{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
			material->ShadowCascadeDistances() << XMVECTOR{ 
				mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(0),
				mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(1),
				mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(2), 1.0f };
			material->AlbedoTexture() << obj->GetTextureData(meshIndex).AlbedoMap;
			material->NormalTexture() << obj->GetTextureData(meshIndex).NormalMap;
			material->SpecularTexture() << obj->GetTextureData(meshIndex).SpecularMap;
			material->RoughnessTexture() << obj->GetTextureData(meshIndex).RoughnessMap;
			material->MetallicTexture() << obj->GetTextureData(meshIndex).MetallicMap;
			material->CascadedShadowTextures().SetResourceArray(shadowMaps, 0, NUM_SHADOW_CASCADES);
			//material->IrradianceDiffuseTexture() << mIllumination->GetIBLIrradianceDiffuseSRV();
			//material->IrradianceSpecularTexture() << mIllumination->GetIBLIrradianceSpecularSRV();
			//material->IntegrationTexture() << mIllumination->GetIBLIntegrationSRV();
		}
	}
}

