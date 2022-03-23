#include "ER_MaterialsCallbacks.h"
#include "Materials.inl"
#include "MaterialHelper.h"
#include "RenderingObject.h"
#include "ShadowMapper.h"
#include "DirectionalLight.h"
#include "Camera.h"

namespace Library
{
	void ER_MaterialsCallbacks::UpdateDeferredPrepassMaterialVariables(ER_MaterialSystems neededSystems, Rendering::RenderingObject* obj, int meshIndex)
	{
		assert(neededSystems.mCamera);

		if (!obj)
			return;

		XMMATRIX worldMatrix = XMLoadFloat4x4(&(obj->GetTransformationMatrix4X4()));
		XMMATRIX vp = neededSystems.mCamera->ViewMatrix() * neededSystems.mCamera->ProjectionMatrix();

		auto material = static_cast<DeferredMaterial*>(obj->GetMaterials()[MaterialHelper::deferredPrepassMaterialName]);
		if (material)
		{
			material->ViewProjection() << vp;
			material->World() << worldMatrix;
			material->AlbedoMap() << obj->GetTextureData(meshIndex).AlbedoMap;
			material->NormalMap() << obj->GetTextureData(meshIndex).NormalMap;
			material->RoughnessMap() << obj->GetTextureData(meshIndex).RoughnessMap;
			material->MetallicMap() << obj->GetTextureData(meshIndex).MetallicMap;
			material->HeightMap() << obj->GetTextureData(meshIndex).HeightMap;
			material->ReflectionMaskFactor() << obj->GetMeshReflectionFactor(meshIndex);
			material->FoliageMaskFactor() << obj->GetFoliageMask();
			material->UseGlobalDiffuseProbeMaskFactor() << obj->GetUseGlobalLightProbeMask();
			material->UsePOM() << obj->IsParallaxOcclusionMapping();
			material->SkipDeferredLighting() << obj->IsForwardShading();
		}
	}

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
