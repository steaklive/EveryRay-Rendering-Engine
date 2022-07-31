#include "ER_FoliageManager.h"
#include "ER_CoreException.h"
#include "ER_Core.h"
#include "ER_MatrixHelper.h"
#include "ER_Utility.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "ER_VertexDeclarations.h"
#include "ER_Scene.h"
#include "DirectionalLight.h"
#include "ER_ShadowMapper.h"
#include "ER_PostProcessingStack.h"
#include "ER_Illumination.h"
#include "ER_Camera.h"
#include "ER_RenderableAABB.h"
#include "ER_Terrain.h"

namespace Library
{
	static int currentSplatChannnel = (int)TerrainSplatChannels::NONE;

	ER_FoliageManager::ER_FoliageManager(ER_Core& pCore, ER_Scene* aScene, DirectionalLight& light) 
		: ER_CoreComponent(pCore), mScene(aScene)
	{
		assert(aScene);
		if (aScene->HasFoliage())
			aScene->LoadFoliageZones(mFoliageCollection, light);
	}

	ER_FoliageManager::~ER_FoliageManager()
	{
		DeletePointerCollection(mFoliageCollection);
		DeleteObject(FoliageSystemInitializedEvent);
	}

	void ER_FoliageManager::Initialize()
	{
		int zoneIndex = 0;
		std::string name;
		for (auto& foliage : mFoliageCollection) {
			name = "Foliage zone #" + std::to_string(zoneIndex);
			foliage->SetName(name);
			zoneIndex++;
		}

		for (auto listener : FoliageSystemInitializedEvent->GetListeners())
			listener();
	}

	void ER_FoliageManager::Update(const ER_CoreTime& gameTime, float gustDistance, float strength, float frequency)
	{
		ER_Camera* camera = (ER_Camera*)(mCore->Services().GetService(ER_Camera::TypeIdClass()));

		if (mEnabled)
		{
			for (auto& foliage : mFoliageCollection)
			{
				foliage->SetDynamicDeltaDistanceToCamera(mDeltaDistanceToCamera);
				foliage->SetDynamicLODMaxDistance(mMaxDistanceToCamera);

				foliage->SetWindParams(gustDistance, strength, frequency);
				foliage->Update(gameTime);
				foliage->PerformCPUFrustumCulling((ER_Utility::IsMainCameraCPUFrustumCulling && mEnableCulling) ? camera : nullptr);
			}
		}
		UpdateImGui();
	}

	void ER_FoliageManager::Draw(const ER_CoreTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass)
	{
		if (!mEnabled)
			return;

		for (auto& object : mFoliageCollection)
			object->Draw(gameTime, worldShadowMapper, renderPass);
	}

	void ER_FoliageManager::DrawDebugGizmos()
	{
		if (ER_Utility::IsEditorMode && ER_Utility::IsFoliageEditor)
			for (auto& object : mFoliageCollection)
				object->DrawDebugGizmos();
	}

	void ER_FoliageManager::AddFoliage(ER_Foliage* foliage)
	{
		assert(foliage);
		if (foliage)
			mFoliageCollection.emplace_back(foliage);
	}

	void ER_FoliageManager::SetVoxelizationParams(float* scale, const float* dimensions, XMFLOAT4* voxelCamera)
	{
		for (auto& object : mFoliageCollection)
			object->SetVoxelizationParams(scale, dimensions, voxelCamera);
	}
	
	const std::string showNoteInEditorText = "Note: enable full editor mode for this to work.";

	void ER_FoliageManager::UpdateImGui()
	{
		if (!mShowDebug || mFoliageCollection.size() == 0)
			return;

		ImGui::Begin("Foliage System");
		ImGui::Checkbox("Enabled", &mEnabled);
		ImGui::Checkbox("CPU frustum cull", &mEnableCulling);
		ImGui::SliderFloat("Max LOD distance", &mMaxDistanceToCamera, 150.0f, 1500.0f);
		ImGui::SliderFloat("Delta LOD distance", &mDeltaDistanceToCamera, 15.0f, 150.0f);
		ImGui::Checkbox("Enable foliage editor", &ER_Utility::IsFoliageEditor);
		if (ImGui::Button("Save foliage changes"))
			mScene->SaveFoliageZonesTransforms(mFoliageCollection);

		ImGui::Text(showNoteInEditorText.c_str());

		for (int i = 0; i < mFoliageCollection.size(); i++)
			mFoliageZonesNamesUI[i] = mFoliageCollection[i]->GetName().c_str();

		ImGui::PushItemWidth(-1);
		ImGui::ListBox("##empty", &mEditorSelectedFoliageZoneIndex, mFoliageZonesNamesUI, mFoliageCollection.size(), 15);
		ImGui::End();

		for (int i = 0; i < mFoliageCollection.size(); i++)
			mFoliageCollection[i]->SetSelected(i == mEditorSelectedFoliageZoneIndex);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	ER_Foliage::ER_Foliage(ER_Core& pCore, ER_Camera& pCamera, DirectionalLight& pLight, int pPatchesCount, const std::string& textureName, float scale, float distributionRadius,
		const XMFLOAT3& distributionCenter, FoliageBillboardType bType, bool isPlacedOnTerrain, int placeChannel)
		:
		mCore(pCore),
		mCamera(pCamera),
		mDirectionalLight(pLight),
		mPatchesCount(pPatchesCount),
		mPatchesCountToRender(pPatchesCount),
		mScale(scale),
		mDistributionRadius(distributionRadius - 0.1f),
		mDistributionCenter(distributionCenter),
		mType(bType),
		mTextureName(textureName),
		mIsPlacedOnTerrain(isPlacedOnTerrain),
		mTerrainSplatChannel(placeChannel)
	{
		auto rhi = mCore.GetRHI();

		//shaders
		{
			ER_RHI_INPUT_ELEMENT_DESC inputElementDescriptions[] =
			{
				{ "POSITION", 0, ER_FORMAT_R32G32B32A32_FLOAT, 0, 0, true, 0 },
				{ "TEXCOORD", 0, ER_FORMAT_R32G32_FLOAT, 0, 0xffffffff, true, 0 },
				{ "NORMAL", 0, ER_FORMAT_R32G32B32_FLOAT, 0, 0xffffffff, true, 0 },
				{ "WORLD", 0, ER_FORMAT_R32G32B32A32_FLOAT, 1, 0,  false, 1 },
				{ "WORLD", 1, ER_FORMAT_R32G32B32A32_FLOAT, 1, 16, false, 1 },
				{ "WORLD", 2, ER_FORMAT_R32G32B32A32_FLOAT, 1, 32, false, 1 },
				{ "WORLD", 3, ER_FORMAT_R32G32B32A32_FLOAT, 1, 48, false, 1 }
			};
			mInputLayout = rhi->CreateInputLayout(inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));

			mVS = rhi->CreateGPUShader();
			mVS->CompileShader(rhi, "content\\shaders\\Foliage.hlsl", "VSMain", ER_VERTEX, mInputLayout);

			mGS = rhi->CreateGPUShader();
			mGS->CompileShader(rhi, "content\\shaders\\Foliage.hlsl", "GSMain", ER_GEOMETRY);

			mPS = rhi->CreateGPUShader();
			mPS->CompileShader(rhi, "content\\shaders\\Foliage.hlsl", "PSMain", ER_PIXEL);

			mPS_GBuffer = rhi->CreateGPUShader();
			mPS_GBuffer->CompileShader(rhi, "content\\shaders\\Foliage.hlsl", "PSMain_gbuffer", ER_PIXEL);

			mPS_Voxelization = rhi->CreateGPUShader();
			mPS_Voxelization->CompileShader(rhi, "content\\shaders\\Foliage.hlsl", "PSMain_voxelization", ER_PIXEL);
		}

		LoadBillboardModel(mType);

		mAlbedoTexture = rhi->CreateGPUTexture();
		mAlbedoTexture->CreateGPUTextureResource(rhi, textureName, true);

		Initialize();
	}

	ER_Foliage::~ER_Foliage()
	{
		DeleteObject(mVertexBuffer);
		DeleteObject(mInstanceBuffer);
		DeleteObject(mIndexBuffer);
		DeleteObject(mAlbedoTexture);
		DeleteObject(mPatchesBufferCPU);
		DeleteObject(mCurrentPositions);
		DeleteObject(mPatchesBufferGPU);
		DeleteObject(mDebugGizmoAABB);
		DeleteObject(mInputLayout);
		DeleteObject(mVS);
		DeleteObject(mGS);
		DeleteObject(mPS);
		DeleteObject(mPS_GBuffer);
		DeleteObject(mPS_Voxelization);
		mFoliageConstantBuffer.Release();
	}

	void ER_Foliage::LoadBillboardModel(FoliageBillboardType bType)
	{
		auto rhi = mCore.GetRHI();

		mVertexBuffer = rhi->CreateGPUBuffer();
		mIndexBuffer = rhi->CreateGPUBuffer();
		if (bType == FoliageBillboardType::SINGLE) {
			mIsRotating = true;
			std::unique_ptr<ER_Model> quadSingleModel(new ER_Model(mCore, ER_Utility::GetFilePath("content\\models\\vegetation\\foliage_quad_single.obj"), true));
			quadSingleModel->GetMesh(0).CreateVertexBuffer_PositionUvNormal(mVertexBuffer);
			quadSingleModel->GetMesh(0).CreateIndexBuffer(mIndexBuffer);
			mVerticesCount = quadSingleModel->GetMesh(0).Indices().size();
		}
		else if (bType == FoliageBillboardType::TWO_QUADS_CROSSING) {
			mIsRotating = false;
			std::unique_ptr<ER_Model> quadDoubleModel(new ER_Model(mCore, ER_Utility::GetFilePath("content\\models\\vegetation\\foliage_quad_double.obj"), true));
			quadDoubleModel->GetMesh(0).CreateVertexBuffer_PositionUvNormal(mVertexBuffer);
			quadDoubleModel->GetMesh(0).CreateIndexBuffer(mIndexBuffer);
			mVerticesCount = quadDoubleModel->GetMesh(0).Indices().size();
		}
		else if (bType == FoliageBillboardType::THREE_QUADS_CROSSING) {
			mIsRotating = false;
			std::unique_ptr<ER_Model> quadTripleModel(new ER_Model(mCore, ER_Utility::GetFilePath("content\\models\\vegetation\\foliage_quad_triple.obj"), true));
			quadTripleModel->GetMesh(0).CreateVertexBuffer_PositionUvNormal(mVertexBuffer);
			quadTripleModel->GetMesh(0).CreateIndexBuffer(mIndexBuffer);
			mVerticesCount = quadTripleModel->GetMesh(0).Indices().size();
		}
		else if (bType == FoliageBillboardType::MULTIPLE_QUADS_CROSSING) {
			mIsRotating = false;
			std::unique_ptr<ER_Model> quadMultipleModel(new ER_Model(mCore, ER_Utility::GetFilePath("content\\models\\vegetation\\foliage_quad_multiple.obj"), true));
			quadMultipleModel->GetMesh(0).CreateVertexBuffer_PositionUvNormal(mVertexBuffer);
			quadMultipleModel->GetMesh(0).CreateIndexBuffer(mIndexBuffer);
			mVerticesCount = quadMultipleModel->GetMesh(0).Indices().size();
		}
	}
	void ER_Foliage::Initialize()
	{
		mFoliageConstantBuffer.Initialize(mCore.GetRHI());
		InitializeBuffersCPU();
		InitializeBuffersGPU(mPatchesCount);

		float radius = mDistributionRadius * 0.5f + mAABBExtentXZ;
		XMFLOAT3 minP = XMFLOAT3(mDistributionCenter.x - radius, mDistributionCenter.y - mAABBExtentY, mDistributionCenter.z - radius);
		XMFLOAT3 maxP = XMFLOAT3(mDistributionCenter.x + radius, mDistributionCenter.y + mAABBExtentY, mDistributionCenter.z + radius);
		mAABB = ER_AABB(minP, maxP);

		mDebugGizmoAABB = new ER_RenderableAABB(mCore, XMFLOAT4(0.0, 0.0, 1.0, 1.0));
		mDebugGizmoAABB->InitializeGeometry({ mAABB.first, mAABB.second });

		if (mIsPlacedOnTerrain)
		{
			ER_Terrain* terrain = mCore.GetLevel()->mTerrain;
			if (terrain && terrain->IsLoaded())
			{
				terrain->PlaceOnTerrain(mCurrentPositions, mPatchesCount, (TerrainSplatChannels)mTerrainSplatChannel);
				for (int i = 0; i < mPatchesCount; i++)
				{
					mPatchesBufferCPU[i].xPos = mCurrentPositions[i].x;
					mPatchesBufferCPU[i].yPos = mCurrentPositions[i].y;
					mPatchesBufferCPU[i].zPos = mCurrentPositions[i].z;
				}
				UpdateBuffersGPU();

				//update AABB based on the first patch Y position (not accurate, but fast)
				float radius = mDistributionRadius * 0.5f + mAABBExtentXZ;
				XMFLOAT3 minP = XMFLOAT3(mDistributionCenter.x - radius, mPatchesBufferCPU[0].yPos - mAABBExtentY, mDistributionCenter.z - radius);
				XMFLOAT3 maxP = XMFLOAT3(mDistributionCenter.x + radius, mPatchesBufferCPU[0].yPos + mAABBExtentY, mDistributionCenter.z + radius);
				mAABB = ER_AABB(minP, maxP);
			}
		}

		mTransformationMatrix = XMMatrixTranslation(mDistributionCenter.x, mDistributionCenter.y, mDistributionCenter.z);
		ER_MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
	}

	void ER_Foliage::InitializeBuffersGPU(int count)
	{
		auto rhi = mCore.GetRHI();

		assert(count > 0);

		// instance buffer
		int instanceCount = count;
		mPatchesBufferGPU = new GPUFoliageInstanceData[instanceCount];

		for (int i = 0; i < instanceCount; i++)
		{
			float randomScale = ER_Utility::RandomFloat(mScale - 1.0f, mScale + 1.0f);
			mPatchesBufferCPU[i].scale = randomScale;
			mPatchesBufferGPU[i].worldMatrix = XMMatrixScaling(randomScale, randomScale, randomScale) * XMMatrixTranslation(mPatchesBufferCPU[i].xPos, mPatchesBufferCPU[i].yPos, mPatchesBufferCPU[i].zPos);
			//mPatchesBufferGPU[i].color = XMFLOAT3(mPatchesBufferCPU[i].r, mPatchesBufferCPU[i].g, mPatchesBufferCPU[i].b);
			mCurrentPositions[i] = XMFLOAT4(mPatchesBufferCPU[i].xPos, mPatchesBufferCPU[i].yPos, mPatchesBufferCPU[i].zPos, 1.0f);
		}

		mInstanceBuffer = rhi->CreateGPUBuffer();
		mInstanceBuffer->CreateGPUBufferResource(mCore.GetRHI(), mPatchesBufferGPU, instanceCount, sizeof(GPUFoliageInstanceData), true, ER_BIND_VERTEX_BUFFER);
	}

	void ER_Foliage::InitializeBuffersCPU()
	{
		// randomly generate positions and color
		mPatchesBufferCPU = new CPUFoliageData[mPatchesCount];
		mCurrentPositions = new XMFLOAT4[mPatchesCount];

		for (int i = 0; i < mPatchesCount; i++)
		{
			mPatchesBufferCPU[i].xPos = mDistributionCenter.x + ((float)rand() / (float)(RAND_MAX)) * mDistributionRadius - mDistributionRadius /2;
			mPatchesBufferCPU[i].yPos = mDistributionCenter.y;
			mPatchesBufferCPU[i].zPos = mDistributionCenter.z + ((float)rand() / (float)(RAND_MAX)) * mDistributionRadius - mDistributionRadius /2;
			mCurrentPositions[i] = XMFLOAT4(mPatchesBufferCPU[i].xPos, mPatchesBufferCPU[i].yPos, mPatchesBufferCPU[i].zPos, 1.0f);

			mPatchesBufferCPU[i].r = ((float)rand() / (float)(RAND_MAX)) * 1.0f + 1.0f;
			mPatchesBufferCPU[i].g = ((float)rand() / (float)(RAND_MAX)) * 1.0f + 0.5f;
			mPatchesBufferCPU[i].b = 0.0f;
		}
	}

	void ER_Foliage::Draw(const ER_CoreTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass)
	{
		if(renderPass == TO_VOXELIZATION)
			assert(worldShadowMapper);

		auto rhi = mCore.GetRHI();

		if (mPatchesCountToRender == 0 || mIsCulled)
			return;

		float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		rhi->SetBlendState(ER_ALPHA_TO_COVERAGE, blendFactor, 0xffffffff);

		rhi->SetVertexBuffers({mVertexBuffer, mInstanceBuffer});
		rhi->SetIndexBuffer(mIndexBuffer);
		rhi->SetTopologyType(ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		if (worldShadowMapper)
		{
			for (int cascade = 0; cascade < NUM_SHADOW_CASCADES; cascade++)
				mFoliageConstantBuffer.Data.ShadowMatrices[cascade] = XMMatrixTranspose(worldShadowMapper->GetViewMatrix(cascade) * worldShadowMapper->GetProjectionMatrix(cascade) * XMLoadFloat4x4(&ER_MatrixHelper::GetProjectionShadowMatrix()));
			mFoliageConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / worldShadowMapper->GetResolution(), 1.0f, 1.0f , 1.0f };
			mFoliageConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ mCamera.GetCameraFarShadowCascadeDistance(0), mCamera.GetCameraFarShadowCascadeDistance(1), mCamera.GetCameraFarShadowCascadeDistance(2), 1.0f };
		}
		
		mFoliageConstantBuffer.Data.World = XMMatrixIdentity();
		mFoliageConstantBuffer.Data.View = XMMatrixTranspose(mCamera.ViewMatrix());
		mFoliageConstantBuffer.Data.Projection = XMMatrixTranspose(mCamera.ProjectionMatrix());
		mFoliageConstantBuffer.Data.SunDirection = XMFLOAT4(-mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f);
		mFoliageConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z , 1.0f };
		mFoliageConstantBuffer.Data.AmbientColor = XMFLOAT4{ mDirectionalLight.GetAmbientLightColor().x, mDirectionalLight.GetAmbientLightColor().y, mDirectionalLight.GetAmbientLightColor().z , 1.0f };
		mFoliageConstantBuffer.Data.CameraDirection = XMFLOAT4(mCamera.Direction().x, mCamera.Direction().y, mCamera.Direction().z, 1.0f);
		mFoliageConstantBuffer.Data.CameraPos = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);
		mFoliageConstantBuffer.Data.WindDirection = XMFLOAT4{ 0.0f, 0.0f, 1.0f , 1.0f };
		mFoliageConstantBuffer.Data.VoxelCameraPos = XMFLOAT4{ mVoxelCameraPos->x, mVoxelCameraPos->y, mVoxelCameraPos->z, 1.0 };
		mFoliageConstantBuffer.Data.RotateToCamera = (mIsRotating) ? 1.0f : 0.0f;;
		mFoliageConstantBuffer.Data.Time = static_cast<float>(gameTime.TotalGameTime());
		mFoliageConstantBuffer.Data.WindStrength = mWindStrength;
		mFoliageConstantBuffer.Data.WindFrequency = mWindFrequency;
		mFoliageConstantBuffer.Data.WindGustDistance = mWindGustDistance;
		mFoliageConstantBuffer.Data.WorldVoxelScale = *mWorldVoxelScale;
		mFoliageConstantBuffer.Data.VoxelTextureDimension = *mVoxelTextureDimension;
		mFoliageConstantBuffer.ApplyChanges(rhi);

		rhi->SetInputLayout(mInputLayout);
		rhi->SetShader(mVS);
		rhi->SetConstantBuffers(ER_VERTEX, { mFoliageConstantBuffer.Buffer() });

		if (renderPass == TO_VOXELIZATION)
		{
			rhi->SetShader(mGS);
			rhi->SetConstantBuffers(ER_GEOMETRY, { mFoliageConstantBuffer.Buffer() });
			rhi->SetShader(mPS_Voxelization);
		}
		else if (renderPass == TO_GBUFFER)
			rhi->SetShader(mPS_GBuffer);

		rhi->SetConstantBuffers(ER_PIXEL, { mFoliageConstantBuffer.Buffer() });
		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS });
		
		std::vector<ER_RHI_GPUResource*> resources(1 + NUM_SHADOW_CASCADES);
		resources[0] = mAlbedoTexture;
		if (worldShadowMapper)
		{
			for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
				resources[1 + i] = worldShadowMapper->GetShadowTexture(i);
			rhi->SetShaderResources(ER_PIXEL, resources);
		}
		else
			rhi->SetShaderResources(ER_PIXEL, { mAlbedoTexture });

		rhi->DrawIndexedInstanced(mVerticesCount, mPatchesCountToRender, 0, 0, 0);
		rhi->SetBlendState(ER_NO_BLEND);

		rhi->UnbindResourcesFromShader(ER_VERTEX);
		rhi->UnbindResourcesFromShader(ER_GEOMETRY);
		rhi->UnbindResourcesFromShader(ER_PIXEL);
	}

	void ER_Foliage::DrawDebugGizmos()
	{
		if (mDebugGizmoAABB && mIsSelectedInEditor)
			mDebugGizmoAABB->Draw();
	}

	void ER_Foliage::Update(const ER_CoreTime& gameTime)
	{
		bool editable = mIsSelectedInEditor && ER_Utility::IsEditorMode && ER_Utility::IsFoliageEditor;

		if (editable)
		{
			mDistributionCenter = XMFLOAT3(mMatrixTranslation[0], mMatrixTranslation[1], mMatrixTranslation[2]);
			for (int i = 0; i < mPatchesCount; i++)
			{
				mPatchesBufferCPU[i].xPos = mDistributionCenter.x + ((float)rand() / (float)(RAND_MAX)) * mDistributionRadius - mDistributionRadius / 2;
				mPatchesBufferCPU[i].yPos = mDistributionCenter.y;
				mPatchesBufferCPU[i].zPos = mDistributionCenter.z + ((float)rand() / (float)(RAND_MAX)) * mDistributionRadius - mDistributionRadius / 2;
				mCurrentPositions[i] = XMFLOAT4(mPatchesBufferCPU[i].xPos, mPatchesBufferCPU[i].yPos, mPatchesBufferCPU[i].zPos, 1.0f);
			}
			UpdateBuffersGPU();

			float radius = mDistributionRadius * 0.5f + mAABBExtentXZ;
			XMFLOAT3 minP = XMFLOAT3(mDistributionCenter.x - radius, mDistributionCenter.y - mAABBExtentY, mDistributionCenter.z - radius);
			XMFLOAT3 maxP = XMFLOAT3(mDistributionCenter.x + radius, mDistributionCenter.y + mAABBExtentY, mDistributionCenter.z + radius);
			mAABB = ER_AABB(minP, maxP);
		}

		XMFLOAT3 toCam = { mDistributionCenter.x - mCamera.Position().x, mDistributionCenter.y - mCamera.Position().y, mDistributionCenter.z - mCamera.Position().z };
		float distanceToCam = sqrt(toCam.x * toCam.x + toCam.y * toCam.y + toCam.z * toCam.z);

		//UpdateBufferGPU();
		CalculateDynamicLOD(distanceToCam);

		if (mDebugGizmoAABB)
			mDebugGizmoAABB->Update(mAABB);

		//imgui
		if (editable)
		{
			ER_MatrixHelper::GetFloatArray(mCamera.ViewMatrix4X4(), mCameraViewMatrix);
			ER_MatrixHelper::GetFloatArray(mCamera.ProjectionMatrix4X4(), mCameraProjectionMatrix);

			static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
			static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
			static bool useSnap = false;
			static float snap[3] = { 1.f, 1.f, 1.f };
			static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
			static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
			static bool boundSizing = false;
			static bool boundSizingSnap = false;
			std::string fullName = mName;
			if (mIsCulled)
				fullName += " (Culled)";

			const char* name = fullName.c_str();

			ImGui::Begin("Foliage Editor");
			ImGui::TextColored(ImVec4(0.0f, 0.9f, 0.1f, 1), name);
			ImGui::Separator();

			std::string patchCountText = "* Patch count: " + std::to_string(mPatchesCount);
			ImGui::Text(patchCountText.c_str());

			std::string patchRenderedCountText = "* Patch count rendered: " + std::to_string(mPatchesCountToRender);
			ImGui::Text(patchRenderedCountText.c_str());

			std::string textureText = "* Texture: " + mTextureName;
			ImGui::Text(textureText.c_str());
			
			if (!mIsPlacedOnTerrain)
			{
				//terrain
				{
					ImGui::Combo("Terrain splat channel", &currentSplatChannnel, DisplayedSplatChannnelNames, 5);
					TerrainSplatChannels currentChannel = (TerrainSplatChannels)currentSplatChannnel;

					ER_Terrain* terrain = mCore.GetLevel()->mTerrain;
					if (ImGui::Button("Place patch on terrain") && terrain && terrain->IsLoaded())
					{
						terrain->PlaceOnTerrain(mCurrentPositions, mPatchesCount, currentChannel);
						for (int i = 0; i < mPatchesCount; i++)
						{
							mPatchesBufferCPU[i].xPos = mCurrentPositions[i].x;
							mPatchesBufferCPU[i].yPos = mCurrentPositions[i].y;
							mPatchesBufferCPU[i].zPos = mCurrentPositions[i].z;
						}
						UpdateBuffersGPU();
						ER_Utility::IsFoliageEditor = false;

						//update AABB based on the first patch Y position (not accurate, but fast)
						float radius = mDistributionRadius * 0.5f + mAABBExtentXZ;
						XMFLOAT3 minP = XMFLOAT3(mDistributionCenter.x - radius, mPatchesBufferCPU[0].yPos - mAABBExtentY, mDistributionCenter.z - radius);
						XMFLOAT3 maxP = XMFLOAT3(mDistributionCenter.x + radius, mPatchesBufferCPU[0].yPos + mAABBExtentY, mDistributionCenter.z + radius);
						mAABB = ER_AABB(minP, maxP);
					}
				}

				if (ImGui::IsKeyPressed(84))
					mCurrentGizmoOperation = ImGuizmo::TRANSLATE;

				ImGuizmo::DecomposeMatrixToComponents(mCurrentObjectTransformMatrix, mMatrixTranslation, mMatrixRotation, mMatrixScale);
				ImGui::InputFloat3("Tr", mMatrixTranslation, 3);
				ImGuizmo::RecomposeMatrixFromComponents(mMatrixTranslation, mMatrixRotation, mMatrixScale, mCurrentObjectTransformMatrix);
				ImGui::End();

				ImGuiIO& io = ImGui::GetIO();
				ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
				ImGuizmo::Manipulate(mCameraViewMatrix, mCameraProjectionMatrix, mCurrentGizmoOperation, mCurrentGizmoMode, mCurrentObjectTransformMatrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);

				XMFLOAT4X4 mat(mCurrentObjectTransformMatrix);
				mTransformationMatrix = XMLoadFloat4x4(&mat);
			}
		}
	}

	// updating world matrices of visible patches
	void ER_Foliage::UpdateBuffersGPU() 
	{
		ER_RHI* rhi = mCore.GetRHI();

		double angle;
		float rotation, windRotation;
		XMMATRIX translationMatrix;

		for (int i = 0; i < mPatchesCount; i++)
		{
			translationMatrix = XMMatrixTranslation(mPatchesBufferCPU[i].xPos, mPatchesBufferCPU[i].yPos, mPatchesBufferCPU[i].zPos);
			mPatchesBufferGPU[i].worldMatrix = XMMatrixScaling(mPatchesBufferCPU[i].scale, mPatchesBufferCPU[i].scale, mPatchesBufferCPU[i].scale) * translationMatrix;
		}

		rhi->UpdateBuffer(mInstanceBuffer, (void*)mPatchesBufferGPU, (sizeof(GPUFoliageInstanceData) * mPatchesCount));
	}

	bool ER_Foliage::PerformCPUFrustumCulling(ER_Camera* camera)
	{
		if (!camera)
		{
			mIsCulled = false;
			return mIsCulled;
		}

		auto frustum = camera->GetFrustum();
		bool culled = false;
		// start a loop through all frustum planes
		for (int planeID = 0; planeID < 6; ++planeID)
		{
			XMVECTOR planeNormal = XMVectorSet(frustum.Planes()[planeID].x, frustum.Planes()[planeID].y, frustum.Planes()[planeID].z, 0.0f);
			float planeConstant = frustum.Planes()[planeID].w;

			XMFLOAT3 axisVert;

			// x-axis
			if (frustum.Planes()[planeID].x > 0.0f)
				axisVert.x = mAABB.first.x;
			else
				axisVert.x = mAABB.second.x;

			// y-axis
			if (frustum.Planes()[planeID].y > 0.0f)
				axisVert.y = mAABB.first.y;
			else
				axisVert.y = mAABB.second.y;

			// z-axis
			if (frustum.Planes()[planeID].z > 0.0f)
				axisVert.z = mAABB.first.z;
			else
				axisVert.z = mAABB.second.z;


			if (XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&axisVert))) + planeConstant > 0.0f)
			{
				culled = true;
				// Skip remaining planes to check and move on 
				break;
			}
		}
		mIsCulled = culled;
		return culled;
	}

	void ER_Foliage::CalculateDynamicLOD(float distanceToCam)
	{
		float factor = (distanceToCam - mDeltaDistanceToCamera) / mMaxDistanceToCamera;

		if (factor > 1.0f)
			factor = 1.0f;
		else if (factor < 0.0f)
			factor = 0.0f;

		mPatchesCountToRender = mPatchesCount * (1.0f - factor);
	}

}