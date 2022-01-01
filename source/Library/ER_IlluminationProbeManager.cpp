#include "ER_IlluminationProbeManager.h"
#include "Game.h"
#include "ShadowMapper.h"
#include "DirectionalLight.h"
#include "MatrixHelper.h"
#include "MaterialHelper.h"
#include "VectorHelper.h"
#include "StandardLightingMaterial.h"

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

	void ER_IlluminationProbeManager::ComputeProbes(Game& game, ProbesRenderingObjectsInfo& aObjects)
	{
		for (auto& lightProbe : mLightProbes)
		{
			lightProbe->DrawProbe(game, aObjects);
		}
	}

	ER_LightProbe::ER_LightProbe(Game& game, DirectionalLight& light, ShadowMapper& shadowMapper, const XMFLOAT3& position, int size)
		: mPosition(position)
		, mSize(size)
		, mDirectionalLight(light)
		, mShadowMapper(shadowMapper)
	{

		const XMFLOAT3 facesDirections[CUBEMAP_FACES_COUNT] = {
			Vector3Helper::Left,
			Vector3Helper::Forward,
			Vector3Helper::Right,
			Vector3Helper::Backward,
			Vector3Helper::Down,
			Vector3Helper::Up
		};

		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mCubemapFacesRTs[i] = new CustomRenderTarget(game.Direct3DDevice(), size, size, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
			mCubemapCameras[i] = new Camera(game, 90.0f, 1.0f, 0.01f, 600.0f); //copied from FPS camera for now
			mCubemapCameras[i]->Initialize();
			mCubemapCameras[i]->SetPosition(mPosition);
			mCubemapCameras[i]->SetDirection(facesDirections[i]);
			mCubemapCameras[i]->UpdateViewMatrix();
			mCubemapCameras[i]->UpdateProjectionMatrix();

		}
	}

	ER_LightProbe::~ER_LightProbe()
	{
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			DeleteObject(mCubemapFacesRTs[i]);
			DeleteObject(mCubemapCameras[i]);
		}

	}

	void ER_LightProbe::DrawProbe(Game& game, const LightProbeRenderingObjectsInfo& objectsToRender)
	{
		auto context = game.Direct3DDeviceContext();
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		UINT viewportsCount = 1;

		CD3D11_VIEWPORT oldViewPort;
		context->RSGetViewports(&viewportsCount, &oldViewPort);
		CD3D11_VIEWPORT newViewPort(0.0f, 0.0f, static_cast<float>(mSize), static_cast<float>(mSize));
		context->RSSetViewports(viewportsCount, &newViewPort);

		for (int cubeMapFace = 0; cubeMapFace < 6; cubeMapFace++)
		{
			// Set the render target and clear it.
			context->OMSetRenderTargets(1, mCubemapFacesRTs[cubeMapFace]->getRTVs(), NULL);
			context->ClearRenderTargetView(mCubemapFacesRTs[cubeMapFace]->getRTV(), clearColor);

			//TODO change to culled objects per face
			for (auto& object : objectsToRender)
			{
				if (object.second->IsInLightProbe())
				{
					UpdateStandardLightingPBRMaterialVariables(object.second, cubeMapFace);
					object.second->DrawLOD(MaterialHelper::forwardLightingForProbesMaterialName, false, -1, object.second->GetLODCount() - 1);
				}
			}
		}

		context->RSSetViewports(1, &oldViewPort);
	}

	void ER_LightProbe::UpdateStandardLightingPBRMaterialVariables(Rendering::RenderingObject* obj, int cubeFaceIndex)
	{
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

		for (int meshIndex = 0; meshIndex < obj->GetMeshCount(); meshIndex++)
		{
			auto material = static_cast<Rendering::StandardLightingMaterial*>(obj->GetMaterials()[MaterialHelper::forwardLightingForProbesMaterialName]);
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
				material->ShadowCascadeDistances() << XMVECTOR{ mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(0), mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(1), mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(2), 1.0f };
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
}

