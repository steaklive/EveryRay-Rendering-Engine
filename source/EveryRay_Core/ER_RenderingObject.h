#pragma once

#include "Common.h"
#include "ER_GenericEvent.h"
#include "ER_ModelMaterial.h"

#include "RHI\ER_RHI.h"

const UINT MAX_INSTANCE_COUNT = 20000;

namespace EveryRay_Core
{
	class ER_Core;
	class ER_CoreTime;
	class ER_Material;
	class ER_RenderableAABB;
	class ER_Camera;
	class ER_Model;

	enum RenderingObjectTextureQuality
	{
		OBJECT_TEXTURE_LOW = 0,
		OBJECT_TEXTURE_MEDIUM,
		OBJECT_TEXTURE_HIGH,

		OBJECT_TEXTURE_COUNT,
	};

	struct RenderBufferData
	{
		ER_RHI_GPUBuffer*		VertexBuffer;
		ER_RHI_GPUBuffer*		IndexBuffer;
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

		RenderBufferData(ER_RHI_GPUBuffer* vertexBuffer, ER_RHI_GPUBuffer* indexBuffer, UINT stride, UINT offset, UINT indicesCount)
			:
			VertexBuffer(vertexBuffer),
			IndexBuffer(indexBuffer),
			Stride(stride),
			Offset(offset),
			IndicesCount(indicesCount)
		{ }

		~RenderBufferData()
		{
			DeleteObject(VertexBuffer);
			DeleteObject(IndexBuffer);
		}
	};

	struct ER_ALIGN_GPU_BUFFER ObjectCB
	{
		XMMATRIX World;
		float UseGlobalProbe;
		float SkipIndirectProbeLighting;
	};

	struct TextureData
	{
		ER_RHI_GPUTexture* AlbedoMap			= nullptr;
		ER_RHI_GPUTexture* NormalMap			= nullptr;
		ER_RHI_GPUTexture* SpecularMap			= nullptr;
		ER_RHI_GPUTexture* MetallicMap			= nullptr;
		ER_RHI_GPUTexture* RoughnessMap			= nullptr;
		ER_RHI_GPUTexture* HeightMap			= nullptr;
		ER_RHI_GPUTexture* ReflectionMaskMap	= nullptr;
		ER_RHI_GPUTexture* ExtraMap2			= nullptr;  // can be used for extra textures (AO, opacity, displacement)
		ER_RHI_GPUTexture* ExtraMap3			= nullptr;  // can be used for extra textures (AO, opacity, displacement)

		TextureData(){}

		TextureData(ER_RHI_GPUTexture* albedo, ER_RHI_GPUTexture* normal, ER_RHI_GPUTexture* specular, ER_RHI_GPUTexture* roughness, ER_RHI_GPUTexture* metallic, ER_RHI_GPUTexture* height,
			ER_RHI_GPUTexture* extra1, ER_RHI_GPUTexture* extra2, ER_RHI_GPUTexture* extra3)
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
			DeleteObject(AlbedoMap);
			DeleteObject(NormalMap);
			DeleteObject(SpecularMap);
			DeleteObject(RoughnessMap);
			DeleteObject(MetallicMap);
			DeleteObject(HeightMap);
			DeleteObject(ReflectionMaskMap);
			DeleteObject(ExtraMap2);
			DeleteObject(ExtraMap3);
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
		UINT				Stride = 0; 
		UINT				Offset = 0;
		ER_RHI_GPUBuffer*	InstanceBuffer = nullptr;

		InstanceBufferData() : 
			Stride(0),
			Offset(0),
			InstanceBuffer(nullptr)
		{
		}

		~InstanceBufferData()
		{
			DeleteObject(InstanceBuffer);
		}
		
	};

	class ER_RenderingObject
	{
		using Delegate_MeshMaterialVariablesUpdate = std::function<void(int)>; // mesh index for input

	public:
		ER_RenderingObject(const std::string& pName, int index, ER_Core& pCore, ER_Camera& pCamera, std::unique_ptr<ER_Model> pModel, bool availableInEditor = false, bool isInstanced = false);
		~ER_RenderingObject();

		void LoadCustomMeshTextures(int meshIndex);
		void LoadMaterial(ER_Material* pMaterial, const std::string& materialName);
		void LoadRenderBuffers(int lod = 0);
		void Draw(const std::string& materialName, bool toDepth = false, int meshIndex = -1);
		void DrawLOD(const std::string& materialName, bool toDepth, int meshIndex, int lod, bool skipCulling = false);
		void DrawAABB(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs);
		void Update(const ER_CoreTime& time);

		std::map<std::string, ER_Material*>& GetMaterials() { return mMaterials; }
		
		TextureData& GetTextureData(int meshIndex) { return mMeshesTextureBuffers[meshIndex]; }
		
		const int GetMeshCount(int lod = 0) const { return mMeshesCount[lod]; }
		const std::vector<XMFLOAT3>& GetVertices(int lod = 0) { return mMeshAllVertices[lod]; }
		const UINT GetInstanceCount(int lod = 0) { return (mIsInstanced ? static_cast<UINT>(mInstanceData[lod].size()) : 0); }
		std::vector<InstancedData>& GetInstancesData(int lod = 0) { return mInstanceData[lod]; }
		
		const XMFLOAT4X4& GetTransformationMatrix4X4() const { return XMFLOAT4X4(mCurrentObjectTransformMatrix); }
		const XMMATRIX& GetTransformationMatrix() const { return mTransformationMatrix; }

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
		
		void PerformCPUFrustumCull(ER_Camera* camera);

		void Rename(const std::string& name) { mName = name; }
		const std::string& GetName() { return mName; }

		int GetLODCount() {
			return 1 + static_cast<int>(mModelLODs.size());
		}
		void UpdateLODs();
		void LoadLOD(std::unique_ptr<ER_Model> pModel);
		
		float GetMinScale() { return mMinScale; }
		void SetMinScale(float v) { mMinScale = v; }
		
		float GetMaxScale() { return mMaxScale; }
		void SetMaxScale(float v) { mMaxScale = v; }

		bool IsSelected() { return mIsSelected; }
		void SetSelected(bool val) { mIsSelected = val; }

		bool IsInstanced() { return mIsInstanced; }
		bool IsAvailableInEditor() { return mAvailableInEditorMode; }

		bool IsRendered() { return mIsRendered; }
		void SetRendered(bool val) { mIsRendered = val; }

		// main camera view flag
		bool IsCulled() { return mIsCulled; }
		void SetCulled(bool val) { mIsCulled = val; }

		float GetCustomAlphaDiscard() { return mCustomAlphaDiscard; }
		void SetCustomAlphaDiscard(float val) { mCustomAlphaDiscard = val; }

		void PlaceProcedurallyOnTerrain(bool isOnInit);
		void StoreInstanceDataAfterTerrainPlacement();
		void SetTerrainPlacement(bool flag) { mIsTerrainPlacement = flag; }
		bool GetTerrainPlacement() { return mIsTerrainPlacement; }
		void SetTerrainProceduralPlacementSplatChannel(int channel) { mTerrainProceduralPlacementSplatChannel = channel; }
		int GetTerrainProceduralPlacementSplatChannel() { return mTerrainProceduralPlacementSplatChannel; }
		void SetTerrainProceduralInstanceCount(int count) { mTerrainProceduralInstanceCount = count; }
		int GetTerrainProceduralInstanceCount() { return mTerrainProceduralInstanceCount; }
		void SetTerrainProceduralZoneCenterPos(const XMFLOAT3& pos) { mTerrainProceduralZoneCenterPos = pos; }
		const XMFLOAT3& GetTerrainProceduralZoneCenterPos(const XMFLOAT3& pos) { return mTerrainProceduralZoneCenterPos; }
		void SetTerrainProceduralZoneRadius(float radius) { mTerrainProceduralZoneRadius = radius; }
		float GetTerrainProceduralZoneRadius() { return mTerrainProceduralZoneRadius; }
		void SetTerrainProceduralObjectsMinMaxScale(float minScale, float maxScale) { mTerrainProceduralObjectMinScale = minScale; mTerrainProceduralObjectMaxScale = maxScale; }
		void SetTerrainProceduralObjectsMinMaxYaw(float minYaw, float maxYaw) { mTerrainProceduralObjectMinYaw = XMConvertToRadians(minYaw); mTerrainProceduralObjectMaxYaw = XMConvertToRadians(maxYaw); }
		void SetTerrainProceduralObjectsMinMaxPitch(float minPitch, float maxPitch) { mTerrainProceduralObjectMinPitch = XMConvertToRadians(minPitch); mTerrainProceduralObjectMaxPitch = XMConvertToRadians(maxPitch); }
		void SetTerrainProceduralObjectsMinMaxRoll(float minRoll, float maxRoll) { mTerrainProceduralObjectMinRoll = XMConvertToRadians(minRoll); mTerrainProceduralObjectMaxRoll = XMConvertToRadians(maxRoll); }

		void SetMeshReflectionFactor(int meshIndex, float factor) { mMeshesReflectionFactors[meshIndex] = factor; }
		float GetMeshReflectionFactor(int meshIndex) { return mMeshesReflectionFactors[meshIndex]; }

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
		
		bool IsSeparableSubsurfaceScattering() { return mIsSeparableSubsurfaceScattering; }
		void SetSeparableSubsurfaceScattering(bool value) { mIsSeparableSubsurfaceScattering = value; }

		void SetUseIndirectGlobalLightProbe(bool value) { mUseIndirectGlobalLightProbe = value; }
		bool GetUseIndirectGlobalLightProbe() { return mUseIndirectGlobalLightProbe; }
		float GetUseIndirectGlobalLightProbeMask() { return mUseIndirectGlobalLightProbe ? 1.0f : 0.0f; }

		bool IsUsedForGlobalLightProbeRendering() { return mIsUsedForGlobalLightProbeRendering; }
		void SetIsUsedForGlobalLightProbeRendering(bool value) { mIsUsedForGlobalLightProbeRendering = value; }

		int GetIndexInScene() { return mIndexInScene; }
		void SetIndexInScene(int index) { mIndexInScene = index; }

		ER_RHI_GPUConstantBuffer<ObjectCB>& GetObjectsConstantBuffer() { return mObjectConstantBuffer; }

		ER_GenericEvent<Delegate_MeshMaterialVariablesUpdate>* MeshMaterialVariablesUpdateEvent = new ER_GenericEvent<Delegate_MeshMaterialVariablesUpdate>();
	
		std::vector<std::string> mCustomAlbedoTextures;
		std::vector<std::string> mCustomNormalTextures;
		std::vector<std::string> mCustomRoughnessTextures;
		std::vector<std::string> mCustomMetalnessTextures;
		std::vector<std::string> mCustomHeightTextures;
		std::vector<std::string> mCustomReflectionMaskTextures;
	private:
		void UpdateAABB(ER_AABB& aabb, const XMMATRIX& transformMatrix);
		void LoadAssignedMeshTextures();
		void LoadTexture(TextureType type, const std::wstring& path, int meshIndex, bool isPlaceholder = false);
		void CreateInstanceBuffer(InstancedData* instanceData, UINT instanceCount, ER_RHI_GPUBuffer* instanceBuffer);
		
		void UpdateGizmos();
		void ShowInstancesListWindow();
		void ShowObjectsEditorWindow(const float *cameraView, float *cameraProjection, float* matrix);
		
		ER_Core* mCore = nullptr;
		ER_Camera& mCamera;

		std::map<std::string, ER_Material*>						mMaterials;

		ER_RHI_GPUConstantBuffer<ObjectCB>						mObjectConstantBuffer;

		///****************************************************************************************************************************
		// *** mesh/model data (buffers, textures, etc.) ***
		std::vector<TextureData>								mMeshesTextureBuffers;
		std::vector<std::vector<std::vector<XMFLOAT3>>>			mMeshVertices; // vertices per mesh, per LOD group
		std::vector<std::vector<RenderBufferData*>>				mMeshRenderBuffers; // vertex/index buffers per mesh, per LOD group
		std::vector<std::vector<InstanceBufferData*>>			mMeshesInstanceBuffers; // instance buffers per mesh, per LOD group
		std::vector<std::vector<XMFLOAT3>>						mMeshAllVertices; // vertices of all meshes combined, per LOD group
		std::vector<float>										mMeshesReflectionFactors; 
		std::vector<int>										mMeshesCount;
		std::unique_ptr<ER_Model>								mModel;
		std::vector<std::unique_ptr<ER_Model>>					mModelLODs;
		// 
		///****************************************************************************************************************************

		///****************************************************************************************************************************
		// *** instancing data (counters, transforms etc.) ***
		UINT													mInstanceCount = 0;
		std::vector<std::string>								mInstancesNames; // collection of names of instances (mName + index)
		std::vector<ER_AABB>									mInstanceAABBs; // collection of AABBs for every instance (shared for LODs)
		std::vector<bool>										mInstanceCullingFlags; // collection of culling flags for every instance (vector is lame here btw...)
		std::vector<InstancedData>								mTempPostCullingInstanceData; // temp instance data after CPU culling
		std::vector<std::vector<InstancedData>>					mTempPostLoddingInstanceData; // temp instance data after lodding (per LOD group)
		std::vector<UINT>										mInstanceCountToRender; //instance render count  (per LOD group)
		std::vector<std::vector<InstancedData>>					mInstanceData; //original instance data  (per LOD group)
		XMFLOAT4*												mTempInstancesPositions = nullptr;
		// 
		///****************************************************************************************************************************

		///****************************************************************************************************************************
		// *** terrain placement & procedural fields ***
		ER_RHI_GPUBuffer*										mInputPositionsOnTerrainBuffer = nullptr; //input positions for on-terrain placement pass
		ER_RHI_GPUBuffer*										mOutputPositionsOnTerrainBuffer = nullptr; //output positions for on-terrain placement pass
		int														mTerrainProceduralPlacementSplatChannel = 4; //TerrainSplatChannel::NONE // on which terrain splat to place
		int														mTerrainProceduralInstanceCount = 0;
		XMFLOAT3												mTerrainProceduralZoneCenterPos; // center of procedural placement
		float													mTerrainProceduralZoneRadius = 0.0f; // radius of procedural placement
		float													mTerrainProceduralObjectMinScale = 1.0f;
		float													mTerrainProceduralObjectMaxScale = 1.0f;
		float													mTerrainProceduralObjectMinRoll = 0.0f;
		float													mTerrainProceduralObjectMaxRoll = 0.0f;
		float													mTerrainProceduralObjectMinPitch = 0.0f;
		float													mTerrainProceduralObjectMaxPitch = 0.0f;
		float													mTerrainProceduralObjectMinYaw = 0.0f;
		float													mTerrainProceduralObjectMaxYaw = 0.0f;
		bool													mIsTerrainPlacementFinished = false;
		bool													mIsTerrainPlacement = false; //possible/wanted or not
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
		bool													mEnableAABBDebug = true;
		bool													mWireframeMode = false;
		bool													mAvailableInEditorMode = false;
		bool													mIsSelected = false;
		bool													mIsRendered = true;
		bool													mIsInstanced = false;
		bool													mIsForwardShading = false;
		bool													mIsPOM = false;
		bool													mIsCulled = false; //only for non-instanced objects
		bool													mFoliageMask = false;
		bool													mIsInLightProbe = false;
		bool													mIsSeparableSubsurfaceScattering = false;
		bool													mIsInVoxelization = false;
		bool													mIsInGbuffer = false;
		bool													mUseIndirectGlobalLightProbe = false;
		bool													mIsUsedForGlobalLightProbeRendering = false;
		float													mCustomAlphaDiscard = 0.1f;
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

		RenderingObjectTextureQuality							mCurrentTextureQuality = RenderingObjectTextureQuality::OBJECT_TEXTURE_LOW;
	};
}