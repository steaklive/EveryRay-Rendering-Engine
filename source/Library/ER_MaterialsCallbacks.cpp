#include "ER_MaterialsCallbacks.h"
#include "Materials.inl"
#include "MaterialHelper.h"
#include "RenderingObject.h"
#include "ER_ShadowMapper.h"
#include "DirectionalLight.h"
#include "Camera.h"

namespace Library
{
	void ER_MaterialsCallbacks::UpdateVoxelizationGIMaterialVariables(ER_MaterialSystems neededSystems, Rendering::RenderingObject* obj, int meshIndex, int voxelCascadeIndex)
	{
		assert(neededSystems.mShadowMapper && neededSystems.mCamera);

		if (!obj)
			return;

		XMMATRIX vp = neededSystems.mCamera->ViewMatrix() * neededSystems.mCamera->ProjectionMatrix();

		const int shadowCascade = 1;
		std::string materialName = MaterialHelper::voxelizationGIMaterialName + "_" + std::to_string(voxelCascadeIndex);
		auto material = static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[materialName]);
		if (material)
		{
			material->ViewProjection() << vp;
			material->ShadowMatrix() << neededSystems.mShadowMapper->GetViewMatrix(shadowCascade) * neededSystems.mShadowMapper->GetProjectionMatrix(shadowCascade) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix());
			material->ShadowTexelSize() << XMVECTOR{ 1.0f / neededSystems.mShadowMapper->GetResolution(), 1.0f, 1.0f , 1.0f };
			material->ShadowCascadeDistances() << XMVECTOR{ neededSystems.mCamera->GetCameraFarCascadeDistance(0), neededSystems.mCamera->GetCameraFarCascadeDistance(1), neededSystems.mCamera->GetCameraFarCascadeDistance(2), 1.0f };
			material->MeshWorld() << obj->GetTransformationMatrix();
			material->MeshAlbedo() << obj->GetTextureData(meshIndex).AlbedoMap;
			material->ShadowTexture() << neededSystems.mShadowMapper->GetShadowTexture(shadowCascade);
		}
	}

	void ER_MaterialsCallbacks::UpdateDebugLightProbeMaterialVariables(ER_MaterialSystems neededSystems, Rendering::RenderingObject* obj, int meshIndex, ER_ProbeType aType, int volumeIndex)
	{
		assert(neededSystems.mProbesManager && neededSystems.mCamera);

		if (!obj)
			return;

		XMMATRIX vp = neededSystems.mCamera->ViewMatrix() * neededSystems.mCamera->ProjectionMatrix();

		auto material = static_cast<DebugLightProbeMaterial*>(obj->GetMaterials()[MaterialHelper::debugLightProbeMaterialName]);
		if (material)
		{
			material->ViewProjection() << vp;
			material->World() << XMMatrixIdentity();
			material->CameraPosition() << neededSystems.mCamera->PositionVector();
			material->DiscardCulledProbe() << neededSystems.mProbesManager->mDebugDiscardCulledProbes;
			if (aType == DIFFUSE_PROBE)
				material->CubemapTexture() << neededSystems.mProbesManager->GetCulledDiffuseProbesTextureArray(volumeIndex)->GetSRV();
			else
				material->CubemapTexture() << neededSystems.mProbesManager->GetCulledSpecularProbesTextureArray(volumeIndex)->GetSRV();
		}
	}
}
