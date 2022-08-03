#include "stdafx.h"

#include "ER_ShadowMapper.h"
#include "ER_Frustum.h"
#include "ER_Projector.h"
#include "ER_Camera.h"
#include "ER_DirectionalLight.h"
#include "ER_CoreTime.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_Scene.h"
#include "ER_MaterialHelper.h"
#include "ER_RenderingObject.h"
#include "ER_ShadowMapMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_Terrain.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <limits>

namespace Library
{
	ER_ShadowMapper::ER_ShadowMapper(ER_Core& pCore, ER_Camera& camera, ER_DirectionalLight& dirLight,  UINT pWidth, UINT pHeight, bool isCascaded)
		: ER_CoreComponent(pCore),
		mShadowMaps(0, nullptr), 
		mDirectionalLight(dirLight),
		mCamera(camera),
		mResolution(pWidth),
		mIsCascaded(isCascaded)
	{
		auto rhi = GetCore()->GetRHI();

		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
		{
			mLightProjectorCenteredPositions.push_back(XMFLOAT3(0, 0, 0));
			
			mShadowMaps.push_back(rhi->CreateGPUTexture());
			mShadowMaps[i]->CreateGPUTextureResource(rhi, pWidth, pHeight, 1u, ER_FORMAT_D24_UNORM_S8_UINT, ER_BIND_DEPTH_STENCIL | ER_BIND_SHADER_RESOURCE);

			mCameraCascadesFrustums.push_back(XMMatrixIdentity());
			(isCascaded) ? mCameraCascadesFrustums[i].SetMatrix(mCamera.GetCustomViewProjectionMatrixForCascade(i)) : mCameraCascadesFrustums[i].SetMatrix(mCamera.ProjectionMatrix());

			mLightProjectors.push_back(new ER_Projector(pCore));
			mLightProjectors[i]->Initialize();
			mLightProjectors[i]->SetProjectionMatrix(GetProjectionBoundingSphere(i));
			//mLightProjectors[i]->ApplyRotation(mDirectionalLight.GetTransform());
		}
	}

	ER_ShadowMapper::~ER_ShadowMapper()
	{
		DeletePointerCollection(mShadowMaps);
		DeletePointerCollection(mLightProjectors);
	}

	void ER_ShadowMapper::Update(const ER_CoreTime& gameTime)
	{
		for (size_t i = 0; i < NUM_SHADOW_CASCADES; i++)
		{
			(mIsCascaded) ? mCameraCascadesFrustums[i].SetMatrix(mCamera.GetCustomViewProjectionMatrixForCascade(i)) : mCameraCascadesFrustums[i].SetMatrix(mCamera.ProjectionMatrix());

			mLightProjectors[i]->SetPosition(mLightProjectorCenteredPositions[i].x, mLightProjectorCenteredPositions[i].y, mLightProjectorCenteredPositions[i].z);
			mLightProjectors[i]->SetProjectionMatrix(GetProjectionBoundingSphere(i));
			mLightProjectors[i]->SetViewMatrix(mLightProjectorCenteredPositions[i], mDirectionalLight.Direction(), mDirectionalLight.Up());
			mLightProjectors[i]->Update();
		}
	}

	void ER_ShadowMapper::BeginRenderingToShadowMap(int cascadeIndex)
	{
		assert(cascadeIndex < NUM_SHADOW_CASCADES);

		auto rhi = GetCore()->GetRHI();

		mOriginalRS = rhi->GetCurrentRasterizerState();
		mOriginalViewport = rhi->GetCurrentViewport();

		ER_RHI_Viewport newViewport;
		newViewport.TopLeftX = 0.0f;
		newViewport.TopLeftY = 0.0f;
		newViewport.Width = static_cast<float>(mShadowMaps[cascadeIndex]->GetWidth());
		newViewport.Height = static_cast<float>(mShadowMaps[cascadeIndex]->GetHeight());
		newViewport.MinDepth = 0.0f;
		newViewport.MaxDepth = 1.0f;

		rhi->SetDepthTarget(mShadowMaps[cascadeIndex]);
		rhi->SetViewport(newViewport);
		rhi->ClearDepthStencilTarget(mShadowMaps[cascadeIndex], 1.0f, 0);
		rhi->SetRasterizerState(ER_SHADOW_RS);
	}

	void ER_ShadowMapper::StopRenderingToShadowMap(int cascadeIndex)
	{
		assert(cascadeIndex < NUM_SHADOW_CASCADES);

		auto rhi = GetCore()->GetRHI();

		rhi->UnbindRenderTargets();
		rhi->SetViewport(mOriginalViewport);
		rhi->SetRasterizerState(mOriginalRS);
	}

	XMMATRIX ER_ShadowMapper::GetViewMatrix(int cascadeIndex /*= 0*/) const
	{
		assert(cascadeIndex < NUM_SHADOW_CASCADES);
		return mLightProjectors.at(cascadeIndex)->ViewMatrix();
	}
	XMMATRIX ER_ShadowMapper::GetProjectionMatrix(int cascadeIndex /*= 0*/) const
	{
		assert(cascadeIndex < NUM_SHADOW_CASCADES);
		return mLightProjectors.at(cascadeIndex)->ProjectionMatrix();
	}

	ER_RHI_GPUTexture* ER_ShadowMapper::GetShadowTexture(int cascadeIndex) const
	{
		assert(cascadeIndex < NUM_SHADOW_CASCADES);
		return mShadowMaps.at(cascadeIndex);
	}

	//void ShadowMapper::ApplyRotation()
	//{
	//	for (int i = 0; i < MAX_NUM_OF_CASCADES; i++)
	//		mLightProjectors[i]->ApplyRotation(mDirectionalLight.GetTransform());
	//}	
	void ER_ShadowMapper::ApplyTransform()
	{
		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
			mLightProjectors[i]->ApplyTransform(mDirectionalLight.GetTransform());
	}

	XMMATRIX ER_ShadowMapper::GetLightProjectionMatrixInFrustum(int index, ER_Frustum& cameraFrustum, ER_DirectionalLight& light)
	{
		assert(index < NUM_SHADOW_CASCADES);

		//create corners
		XMFLOAT3 frustumCorners[8] = {};

		frustumCorners[0] = (cameraFrustum.Corners()[0]);
		frustumCorners[1] = (cameraFrustum.Corners()[1]);
		frustumCorners[2] = (cameraFrustum.Corners()[2]);
		frustumCorners[3] = (cameraFrustum.Corners()[3]);
		frustumCorners[4] = (cameraFrustum.Corners()[4]);
		frustumCorners[5] = (cameraFrustum.Corners()[5]);
		frustumCorners[6] = (cameraFrustum.Corners()[6]);
		frustumCorners[7] = (cameraFrustum.Corners()[7]);

		XMFLOAT3 frustumCenter = { 0, 0, 0 };

		for (size_t i = 0; i < 8; i++)
		{
			frustumCenter = XMFLOAT3(frustumCenter.x + frustumCorners[i].x,
				frustumCenter.y + frustumCorners[i].y,
				frustumCenter.z + frustumCorners[i].z);
		}

		//calculate frustum's center position
		frustumCenter = XMFLOAT3(
			frustumCenter.x * (1.0f / 8.0f),
			frustumCenter.y * (1.0f / 8.0f),
			frustumCenter.z * (1.0f / 8.0f));

		//mLightProjectorCenteredPositions[index] = frustumCenter;

		float minX = (std::numeric_limits<float>::max)();
		float maxX = (std::numeric_limits<float>::min)();
		float minY = (std::numeric_limits<float>::max)();
		float maxY = (std::numeric_limits<float>::min)();
		float minZ = (std::numeric_limits<float>::max)();
		float maxZ = (std::numeric_limits<float>::min)();

		for (int j = 0; j < 8; j++) {

			// Transform the frustum coordinate from world to light space
			XMVECTOR frustumCornerVector = XMLoadFloat3(&frustumCorners[j]);
			frustumCornerVector = XMVector3Transform(frustumCornerVector, (light.LightMatrix(frustumCenter)));

			XMStoreFloat3(&frustumCorners[j], frustumCornerVector);

			minX = std::min(minX, frustumCorners[j].x);
			maxX = std::max(maxX, frustumCorners[j].x);
			minY = std::min(minY, frustumCorners[j].y);
			maxY = std::max(maxY, frustumCorners[j].y);
			minZ = std::min(minZ, frustumCorners[j].z);
			maxZ = std::max(maxZ, frustumCorners[j].z);
		}

		mLightProjectorCenteredPositions[index] =
			XMFLOAT3(
				frustumCenter.x + light.Direction().x * -maxZ,
				frustumCenter.y + light.Direction().y * -maxZ,
				frustumCenter.z + light.Direction().z * -maxZ
			);

		//set orthographic proj with proper boundaries
		float delta = 10.0f;
		XMMATRIX projectionMatrix = XMMatrixOrthographicRH(maxX - minX, maxY - minY, -delta, maxZ - minZ);
		return projectionMatrix;
	}
	XMMATRIX ER_ShadowMapper::GetProjectionBoundingSphere(int index)
	{
		// Create a bounding sphere around the camera frustum for 360 rotation
		float nearV = mCamera.GetCameraNearShadowCascadeDistance(index);
		float farV = mCamera.GetCameraFarShadowCascadeDistance(index);
		float endV = nearV + farV;
		XMFLOAT3 sphereCenter = XMFLOAT3(
			mCamera.Position().x + mCamera.Direction().x * (nearV + 0.5f * endV),
			mCamera.Position().y + mCamera.Direction().y * (nearV + 0.5f * endV),
			mCamera.Position().z + mCamera.Direction().z * (nearV + 0.5f * endV)
		);
		// Create a vector to the frustum far corner
		float tanFovY = tanf(mCamera.FieldOfView());
		float tanFovX = mCamera.AspectRatio() * tanFovY;

		XMFLOAT3 farCorner = XMFLOAT3(
			mCamera.Direction().x + mCamera.Right().x * tanFovX + mCamera.Up().x * tanFovY,
			mCamera.Direction().y + mCamera.Right().y * tanFovX + mCamera.Up().y * tanFovY,
			mCamera.Direction().z + mCamera.Right().z * tanFovX + mCamera.Up().z * tanFovY);
		// Compute the frustumBoundingSphere radius
		XMFLOAT3 boundVec = XMFLOAT3(
			mCamera.Position().x + farCorner.x  * farV - sphereCenter.x,
			mCamera.Position().y + farCorner.y  * farV - sphereCenter.y,
			mCamera.Position().z + farCorner.z  * farV - sphereCenter.z);
		float sphereRadius = sqrt(boundVec.x * boundVec.x + boundVec.y * boundVec.y + boundVec.z * boundVec.z);

		mLightProjectorCenteredPositions[index] =
			XMFLOAT3(
				mCamera.Position().x + mCamera.Direction().x * 0.5f * mCamera.GetCameraFarShadowCascadeDistance(index),
				mCamera.Position().y + mCamera.Direction().y * 0.5f * mCamera.GetCameraFarShadowCascadeDistance(index),
				mCamera.Position().z + mCamera.Direction().z * 0.5f * mCamera.GetCameraFarShadowCascadeDistance(index)
			);

		XMMATRIX projectionMatrix = XMMatrixOrthographicRH(sphereRadius, sphereRadius, -sphereRadius, sphereRadius);
		return projectionMatrix;
	}

	void ER_ShadowMapper::Draw(const ER_Scene* scene, ER_Terrain* terrain)
	{
		auto rhi = GetCore()->GetRHI();

		ER_MaterialSystems materialSystems;
		materialSystems.mShadowMapper = this;

		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
		{
			BeginRenderingToShadowMap(i);

			if (terrain)
				terrain->Draw(this, nullptr, i);

			const std::string name = ER_MaterialHelper::shadowMapMaterialName + " " + std::to_string(i);

			int objectIndex = 0;
			for (auto it = scene->objects.begin(); it != scene->objects.end(); it++, objectIndex++)
			{
				auto materialInfo = it->second->GetMaterials().find(name);
				if (materialInfo != it->second->GetMaterials().end())
				{
					for (int meshIndex = 0; meshIndex < it->second->GetMeshCount(); meshIndex++)
					{
						static_cast<ER_ShadowMapMaterial*>(materialInfo->second)->PrepareForRendering(materialSystems, it->second, meshIndex, i);
						if (!it->second->IsInstanced())
							it->second->DrawLOD(name, true, meshIndex, it->second->GetLODCount() - 1); //drawing highest LOD
						else
							it->second->Draw(name, true, meshIndex);

					}
				}
			}

			StopRenderingToShadowMap(i);
		}
	}
}