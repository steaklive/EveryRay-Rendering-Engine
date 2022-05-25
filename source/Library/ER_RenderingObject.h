#pragma once

#include "Common.h"
#include "GeneralEvent.h"
#include "ModelMaterial.h"

const UINT MAX_INSTANCE_COUNT = 20000;

namespace Library
{
	class Game;
	class GameTime;
	class ER_Material;
	class ER_RenderableAABB;
	class Camera;
	class Model;

	struct RenderBufferData
	{
		ID3D11Buffer*			VertexBuffer;
		ID3D11Buffer*			IndexBuffer;
		UINT					Stride;
		UINT					Offset;
		UINT					IndicesCount;

		RenderBufferData()
			:
			VertexBuffer(nullptr),
			IndexBuffer(nullptr),
			Stride(0),
			Offset(0),
			IndicesCount(0)
		{}

		RenderBufferData(ID3D11Buffer* vertexBuffer, ID3D11Buffer* indexBuffer, UINT stride, UINT offset, UINT indicesCount)
			:
			VertexBuffer(vertexBuffer),
			IndexBuffer(indexBuffer),
			Stride(stride),
			Offset(offset),
			IndicesCount(indicesCount)
		{ }

		~RenderBufferData()
		{
			ReleaseObject(VertexBuffer);
			ReleaseObject(IndexBuffer);
		}
	};

	struct TextureData
	{
		ID3D11ShaderResourceView* AlbedoMap			= nullptr;
		ID3D11ShaderResourceView* NormalMap			= nullptr;
		ID3D11ShaderResourceView* SpecularMap		= nullptr;
		ID3D11ShaderResourceView* MetallicMap		= nullptr;
		ID3D11ShaderResourceView* RoughnessMap		= nullptr;
		ID3D11ShaderResourceView* HeightMap			= nullptr;
		ID3D11ShaderResourceView* ReflectionMaskMap	= nullptr;
		ID3D11ShaderResourceView* ExtraMap2			= nullptr;  // can be used for extra textures (AO, opacity, displacement)
		ID3D11ShaderResourceView* ExtraMap3			= nullptr;  // can be used for extra textures (AO, opacity, displacement)

		TextureData(){}

		TextureData(ID3D11ShaderResourceView* albedo, ID3D11ShaderResourceView* normal, ID3D11ShaderResourceView* specular, ID3D11ShaderResourceView* roughness, ID3D11ShaderResourceView* metallic, ID3D11ShaderResourceView* height,
			ID3D11ShaderResourceView* extra1, ID3D11ShaderResourceView* extra2, ID3D11ShaderResourceView* extra3)
			:
			AlbedoMap(albedo),
			NormalMap(normal),
			SpecularMap(specular),
			RoughnessMap(roughness),
			MetallicMap(metallic),
			HeightMap(height),
			ReflectionMaskMap(extra1),
			ExtraMap2(extra2),
			ExtraMap3(extra3)
		{}

		~TextureData()
		{
			ReleaseObject(AlbedoMap);
			ReleaseObject(NormalMap);
			ReleaseObject(SpecularMap);
			ReleaseObject(RoughnessMap);
			ReleaseObject(MetallicMap);
			ReleaseObject(HeightMap);
			ReleaseObject(ReflectionMaskMap);
			ReleaseObject(ExtraMap2);
			ReleaseObject(ExtraMap3);
		}
	};

	struct InstancedData
	{
		XMFLOAT4X4 World;

		InstancedData() {}
		InstancedData(const XMFLOAT4X4& world) : World(world) {}
		InstancedData(CXMMATRIX world) : World() { XMStoreFloat4x4(&World, world); }
	};

	struct InstanceBufferData
	{
		UINT								Stride = 0; 
		UINT								Offset = 0;
		ID3D11Buffer*						InstanceBuffer = nullptr;

		InstanceBufferData() : 
			Stride(0),
			Offset(0),
			InstanceBuffer(nullptr)
		{
		}

		~InstanceBufferData()
		{
			ReleaseObject(InstanceBuffer);
		}
		
	};

	class ER_RenderingObject
	{
		using Delegate_MeshMaterialVariablesUpdate = std::function<void(int)>; // mesh index for input

	public:
		ER_RenderingObject(const std::string& pName, int index, Game& pGame, Camera& pCamera, std::unique_ptr<Model> pModel, bool availableInEditor = false, bool isInstanced = false);
		~ER_RenderingObject();

		void LoadCustomMeshTextures(int meshIndex);
		void LoadMaterial(ER_Material* pMaterial, const std::string& materialName);
		void LoadRenderBuffers(int lod = 0);
		void Draw(const std::string& materialName, bool toDepth = false, int meshIndex = -1);
		void DrawLOD(const std::string& materialName, bool toDepth, int meshIndex, int lod);
		void DrawAABB();
		void UpdateGizmos();
		void Update(const GameTime& time);

		std::map<std::string, ER_Material*>& GetMaterials() { return mMaterials; }
		
		TextureData& GetTextureData(int meshIndex) { return mMeshesTextureBuffers[meshIndex]; }
		
		const int GetMeshCount(int lod = 0) const { return mMeshesCount[lod]; }
		const std::vector<XMFLOAT3>& GetVertices(int lod = 0) { return mMeshAllVertices[lod]; }
		const UINT GetInstanceCount(int lod = 0) { return (mIsInstanced ? mInstanceData[lod].size() : 0); }
		std::vector<InstancedData>& GetInstancesData(int lod = 0) { return mInstanceData[lod]; }
		
		XMFLOAT4X4 GetTransformationMatrix4X4() const { return XMFLOAT4X4(mCurrentObjectTransformMatrix); }
		XMMATRIX GetTransformationMatrix() const { return mTransformationMatrix; }

		ER_AABB& GetLocalAABB() { return mLocalAABB; } //local space (no transforms)
		ER_AABB& GetGlobalAABB() { return mGlobalAABB; } //world space (with transforms)
		ER_AABB& GetInstanceAABB(int index) { return mInstanceAABBs[index]; } //world space (with transforms)

		void SetTransformationMatrix(const XMMATRIX& mat);
		void SetTranslation(float x, float y, float z);
		void SetScale(float x, float y, float z);
		void SetRotation(float x, float y, float z);

		void LoadInstanceBuffers(int lod = 0);
		void UpdateInstanceBuffer(std::vector<InstancedData>& instanceData, int lod = 0);
		void ResetInstanceData(int count, bool clear = false, int lod = 0);
		void AddInstanceData(const XMMATRIX& worldMatrix, int lod = -1);
		UINT InstanceSize() const;
		
		void PerformCPUFrustumCull(Camera* camera);
		
		void Rename(const std::string& name) { mName = name; }
		const std::string& GetName() { return mName; }

		int GetLODCount() {
			return 1 + mModelLODs.size();
		}
		void UpdateLODs();
		void LoadLOD(std::unique_ptr<Model> pModel);
		
		float GetMinScale() { return mMinScale; }
		void SetMinScale(float v) { mMinScale = v; }
		
		float GetMaxScale() { return mMaxScale; }
		void SetMaxScale(float v) { mMaxScale = v; }

		bool IsSelected() { return mIsSelected; }
		void SetSelected(bool val) { mIsSelected = val; }

		bool IsInstanced() { return mIsInstanced; }
		bool IsAvailableInEditor() { return mAvailableInEditorMode; }

		bool IsVisible() { return mIsRendered; }
		void SetVisible(bool val) { mIsRendered = val; }

		//TODO remove that (culling can be different in different views)
		bool IsCulled() { return mIsCulled; }
		void SetCulled(bool val) { mIsCulled = val; }

		void SetMeshReflectionFactor(int meshIndex, float factor) { mMeshesReflectionFactors[meshIndex] = factor; }
		float GetMeshReflectionFactor(int meshIndex) { return mMeshesReflectionFactors[meshIndex]; }

		void SetPlacedOnTerrain(bool flag) { mPlacedOnTerrain = flag; }
		bool IsPlacedOnTerrain() { return mPlacedOnTerrain; }

		bool GetIsSavedOnTerrain() { return mSavedOnTerrain; }
		void SetSavedOnTerrain(bool flag) { mSavedOnTerrain = flag; }
		
		void SetNumInstancesPerVegetationZone(int count) { mNumInstancesPerVegetationZone = count; }
		int GetNumInstancesPerVegetationZone() { return mNumInstancesPerVegetationZone; }

		bool GetFoliageMask() { return mFoliageMask; }
		void SetFoliageMask(bool value) { mFoliageMask = value; }

		bool IsForwardShading() { return mIsForwardShading; }
		void SetForwardShading(bool value) { mIsForwardShading = value; }

		bool IsInGBuffer() { return mIsInGbuffer; }
		void SetInGBuffer(bool value) { mIsInGbuffer = value; }

		bool IsInVoxelization() { return mIsInVoxelization; }
		void SetInVoxelization(bool value) { mIsInVoxelization = value; }

		bool IsParallaxOcclusionMapping() { return mIsPOM; }
		void SetParallaxOcclusionMapping(bool value) { mIsPOM = value; }

		bool IsInLightProbe() { return mIsInLightProbe; }
		void SetInLightProbe(bool value) { mIsInLightProbe = value; }
		
		void SetUseIndirectGlobalLightProbe(bool value) { mUseIndirectGlobalLightProbe = value; }
		bool GetUseIndirectGlobalLightProbe() { return mUseIndirectGlobalLightProbe; }
		float GetUseIndirectGlobalLightProbeMask() { return mUseIndirectGlobalLightProbe ? 1.0f : 0.0f; }

		bool IsUsedForGlobalLightProbeRendering() { return mIsUsedForGlobalLightProbeRendering; }
		void SetIsUsedForGlobalLightProbeRendering(bool value) { mIsUsedForGlobalLightProbeRendering = value; }

		int GetIndexInScene() { return mIndexInScene; }
		void SetIndexInScene(int index) { mIndexInScene = index; }

		GeneralEvent<Delegate_MeshMaterialVariablesUpdate>* MeshMaterialVariablesUpdateEvent = new GeneralEvent<Delegate_MeshMaterialVariablesUpdate>();
	
		std::vector<std::string>								mCustomAlbedoTextures;
		std::vector<std::string>								mCustomNormalTextures;
		std::vector<std::string>								mCustomRoughnessTextures;
		std::vector<std::string>								mCustomMetalnessTextures;
		std::vector<std::string>								mCustomHeightTextures;
		std::vector<std::string>								mCustomReflectionMaskTextures;
	private:
		void UpdateAABB(ER_AABB& aabb, const XMMATRIX& transformMatrix);
		void LoadAssignedMeshTextures();
		void LoadTexture(TextureType type, const std::wstring& path, int meshIndex);
		void CreateInstanceBuffer(ID3D11Device* device, InstancedData* instanceData, UINT instanceCount, ID3D11Buffer** instanceBuffer);
		
		void ShowInstancesListWindow();
		void ShowObjectsEditorWindow(const float *cameraView, float *cameraProjection, float* matrix);
		
		Game* mGame = nullptr;
		Camera& mCamera;

		std::map<std::string, ER_Material*>						mMaterials;

		///****************************************************************************************************************************
		// *** mesh/model data (buffers, textures, etc.) ***
		std::vector<std::map<std::string, std::vector<RenderBufferData*>>>	mMeshesRenderBuffers;
		std::vector<TextureData>								mMeshesTextureBuffers;
		std::vector<std::vector<InstanceBufferData*>>			mMeshesInstanceBuffers;
		std::vector<std::vector<std::vector<XMFLOAT3>>>			mMeshVertices; // vertices per mesh, per LOD group
		std::vector<std::vector<XMFLOAT3>>						mMeshAllVertices; // vertices of all meshes combined, per LOD group
		std::vector<float>										mMeshesReflectionFactors; 
		std::vector<int>										mMeshesCount;
		std::unique_ptr<Model>									mModel;
		std::vector<std::unique_ptr<Model>>						mModelLODs;
		// 
		///****************************************************************************************************************************

		///****************************************************************************************************************************
		// *** instancing data (counters, transforms etc.) ***
		UINT													mInstanceCount;
		std::vector<std::string>								mInstancesNames; // collection of names of instances (mName + index)
		std::vector<ER_AABB>									mInstanceAABBs; // collection of AABBs for every instance (shared for LODs)
		std::vector<bool>										mInstanceCullingFlags; // collection of culling flags for every instance (vector is lame here btw...)
		std::vector<InstancedData>								mTempPostCullingInstanceData; // temp instance data after CPU culling
		std::vector<std::vector<InstancedData>>					mTempPostLoddingInstanceData; // temp instance data after lodding (per LOD group)
		std::vector<UINT>										mInstanceCountToRender; //instance render count  (per LOD group)
		std::vector<std::vector<InstancedData>>					mInstanceData; //original instance data  (per LOD group)
		// 
		///****************************************************************************************************************************

		ER_AABB													mLocalAABB; //mesh space AABB
		ER_AABB													mGlobalAABB; //world space AABB
		XMFLOAT3												mCurrentGlobalAABBVertices[8];
		ER_RenderableAABB*										mDebugGizmoAABB;
	
		std::string												mName;
		const char*												mInstancedNamesUI[MAX_INSTANCE_COUNT];
		int														mIndexInScene = -1;
		int														mCurrentLODIndex = 0; //only used for non-instanced object
		int														mEditorSelectedInstancedObjectIndex = 0;
		int														mNumInstancesPerVegetationZone = 0;
		bool													mEnableAABBDebug = true;
		bool													mWireframeMode = false;
		bool													mAvailableInEditorMode = false;
		bool													mIsSelected = false;
		bool													mIsRendered = true;
		bool													mIsInstanced = false;
		bool													mIsForwardShading = false;
		bool													mIsPOM = false;
		bool													mPlacedOnTerrain = false;
		bool													mSavedOnTerrain = false;
		bool													mIsCulled = false; //only for non-instanced objects
		bool													mFoliageMask = false;
		bool													mIsInLightProbe = false;
		bool													mIsInVoxelization = false;
		bool													mIsInGbuffer = false;
		bool													mUseIndirectGlobalLightProbe = false;
		bool													mIsUsedForGlobalLightProbeRendering = false;
		float													mMinScale = 1.0f;
		float													mMaxScale = 1.0f;
		float													mCameraViewMatrix[16];
		float													mCameraProjectionMatrix[16];
		XMMATRIX												mTransformationMatrix;
		float													mMatrixTranslation[3], mMatrixRotation[3], mMatrixScale[3];
		float													mCurrentObjectTransformMatrix[16] = 
		{   
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f 
		};

	};
}