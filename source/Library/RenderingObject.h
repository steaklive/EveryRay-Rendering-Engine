#pragma once

#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"
#include "Material.h"
#include "VertexDeclarations.h"
#include "InstancingMaterial.h"
#include "ModelMaterial.h"
#include "Effect.h"
#include "GeneralEvent.h"
#include "RenderableAABB.h"
#include "MatrixHelper.h"

const UINT MAX_INSTANCE_COUNT = 10000;

namespace Rendering
{
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
		ID3D11ShaderResourceView* AlbedoMap		= nullptr;
		ID3D11ShaderResourceView* NormalMap		= nullptr;
		ID3D11ShaderResourceView* SpecularMap	= nullptr;
		ID3D11ShaderResourceView* MetallicMap	= nullptr;
		ID3D11ShaderResourceView* RoughnessMap	= nullptr;
		ID3D11ShaderResourceView* ExtraMap1		= nullptr;  // can be used for extra textures (AO, opacity, displacement)
		ID3D11ShaderResourceView* ExtraMap2		= nullptr;  // can be used for extra textures (AO, opacity, displacement)
		ID3D11ShaderResourceView* ExtraMap3		= nullptr;  // can be used for extra textures (AO, opacity, displacement)

		TextureData(){}

		TextureData(ID3D11ShaderResourceView* albedo, ID3D11ShaderResourceView* normal, ID3D11ShaderResourceView* specular, ID3D11ShaderResourceView* roughness, ID3D11ShaderResourceView* metallic, ID3D11ShaderResourceView* extra1, ID3D11ShaderResourceView* extra2, ID3D11ShaderResourceView* extra3)
			:
			AlbedoMap(albedo),
			NormalMap(normal),
			SpecularMap(specular),
			RoughnessMap(roughness),
			MetallicMap(metallic),
			ExtraMap1(extra1),
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
			ReleaseObject(ExtraMap1);
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

	class RenderingObject : public GameComponent
	{
		using Delegate_MeshMaterialVariablesUpdate = std::function<void(int)>; // mesh index for input

	public:
		RenderingObject(std::string pName, Game& pGame, Camera& pCamera, std::unique_ptr<Model> pModel, bool availableInEditor = false, bool isInstanced = false);
		~RenderingObject();

		void LoadCustomMeshTextures(int meshIndex, std::wstring albedoPath, std::wstring normalPath, std::wstring specularPath, std::wstring roughnessPath, std::wstring metallicPath, std::wstring extra1Path, std::wstring extra2Path, std::wstring extra3Path);
		void LoadCustomMeshTextures(int meshIndex);
		void LoadMaterial(Material* pMaterial, Effect* pEffect, std::string materialName);
		void LoadRenderBuffers(int lod = 0);
		void LoadInstanceBuffers(std::vector<InstancingMaterial::InstancedData>& pInstanceData, std::string materialName, int lod = 0);
		void Draw(std::string materialName,bool toDepth = false, int meshIndex = -1);
		void DrawLOD(std::string materialName, bool toDepth, int meshIndex, int lod);
		void DrawAABB();
		void UpdateInstanceData(std::vector<InstancingMaterial::InstancedData> pInstanceData, std::string materialName, int lod = 0);
		void UpdateGizmos();
		void Update(const GameTime& time);
		void Selected(bool val) { mIsSelected = val; }
		void SetVisible(bool val) { mIsRendered = val; }
		void SetCulled(bool val) { mIsCulled = val; }

		void SetMeshReflectionFactor(int meshIndex, float factor) { mMeshesReflectionFactors[meshIndex] = factor; }
		float GetMeshReflectionFactor(int meshIndex) { return mMeshesReflectionFactors[meshIndex]; }

		std::map<std::string, Material*>& GetMaterials() { return mMaterials; }
		TextureData& GetTextureData(int meshIndex) { return mMeshesTextureBuffers[meshIndex]; }
		
		const int GetMeshCount(int lod = 0) const { return mMeshesCount[lod]; }
		const std::vector<XMFLOAT3>& GetVertices(int lod = 0) { return mMeshAllVertices[lod]; }
		const UINT GetInstanceCount(int lod = 0) { return (mIsInstanced ? mInstanceData[lod].size() : 0); }
		std::vector<InstancedData>& GetInstancesData(int lod = 0) { return mInstanceData[lod]; }
		
		XMFLOAT4X4 GetTransformationMatrix4X4() const { return XMFLOAT4X4(mCurrentObjectTransformMatrix); }
		XMMATRIX GetTransformationMatrix() const { return mTransformationMatrix; }

		const ER_AABB& GetLocalAABB() { return mLocalAABB; }

		bool IsAvailableInEditor() { return mAvailableInEditorMode; }
		bool IsSelected() { return mIsSelected; }
		bool IsInstanced() { return mIsInstanced; }
		bool IsVisible() { return mIsRendered; }

		void SetTransformationMatrix(XMMATRIX mat)
		{
			mTransformationMatrix = mat;
			MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
		}
		void SetTranslation(float x, float y, float z)
		{
			mTransformationMatrix *= XMMatrixTranslation(x, y, z);
			MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
		}
		void SetScale(float x, float y, float z)
		{ 
			mTransformationMatrix *= XMMatrixScaling(x, y, z);
			MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
		}
		void SetRotation(float x, float y, float z)
		{
			mTransformationMatrix *= XMMatrixRotationRollPitchYaw(x, y, z);
			MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
		}

		void LoadInstanceBuffers(int lod = 0);
		void UpdateInstanceBuffer(std::vector<InstancedData>& instanceData, int lod = 0);
		UINT InstanceSize() const;

		void ResetInstanceData(int count, bool clear = false, int lod = 0);
		void AddInstanceData(XMMATRIX worldMatrix, int lod = -1);

		void SetPlacedOnTerrain(bool flag) { mPlacedOnTerrain = flag; }
		bool IsPlacedOnTerrain() { return mPlacedOnTerrain; }

		bool GetIsSavedOnTerrain() { return mSavedOnTerrain; }
		void SetSavedOnTerrain(bool flag) { mSavedOnTerrain = flag; }
		void SetNumInstancesPerVegetationZone(int count) { mNumInstancesPerVegetationZone = count; }
		int GetNumInstancesPerVegetationZone() { return mNumInstancesPerVegetationZone; }

		void Rename(std::string name) { mName = name; }
		std::string GetName() { return mName; }

		void UpdateLODs();
		void LoadLOD(std::unique_ptr<Model> pModel);
		int GetLODCount() {
			return 1 + mModelLODs.size();
		}
		void LoadPostCullingInstanceData(std::vector<InstancedData>& instanceData) {
			mPostCullingInstanceDataPerLOD.clear();
			for (int i = 0; i < GetLODCount(); i++)
				mPostCullingInstanceDataPerLOD.push_back(instanceData);
		}
		
		void SetMinScale(float v) { mMinScale = v; }
		void SetMaxScale(float v) { mMaxScale = v; }
		float GetMinScale() { return mMinScale; }
		float GetMaxScale() { return mMaxScale; }

		GeneralEvent<Delegate_MeshMaterialVariablesUpdate>* MeshMaterialVariablesUpdateEvent = new GeneralEvent<Delegate_MeshMaterialVariablesUpdate>();
	
		std::vector<std::string>								mCustomAlbedoTextures;
		std::vector<std::string>								mCustomNormalTextures;
		std::vector<std::string>								mCustomRoughnessTextures;
		std::vector<std::string>								mCustomMetalnessTextures;
	private:
		RenderingObject();
		RenderingObject(const RenderingObject& rhs);
		RenderingObject& operator=(const RenderingObject& rhs);

		void LoadAssignedMeshTextures();
		void LoadTexture(TextureType type, std::wstring path, int meshIndex);
		void CreateInstanceBuffer(ID3D11Device* device, InstancedData* instanceData, UINT instanceCount, ID3D11Buffer** instanceBuffer);
		
		void ShowInstancesListUI();
		void UpdateGizmoTransform(const float *cameraView, float *cameraProjection, float* matrix);
		
		Camera& mCamera;
		std::vector<TextureData>								mMeshesTextureBuffers;
		std::vector<std::vector<InstanceBufferData*>>			mMeshesInstanceBuffers;
		std::vector<std::vector<std::vector<XMFLOAT3>>>			mMeshVertices; 
		std::vector<std::vector<XMFLOAT3>>						mMeshAllVertices; 
		std::vector<std::map<std::string, std::vector<RenderBufferData*>>>	mMeshesRenderBuffers;
		std::vector<float>										mMeshesReflectionFactors; 
		std::map<std::string, Material*>						mMaterials;
		ER_AABB													mLocalAABB;
		std::unique_ptr<Model>									mModel;
		std::vector<std::unique_ptr<Model>>						mModelLODs;
		std::vector<int>										mMeshesCount;
		std::vector<UINT>										mInstanceCount;
		std::vector<UINT>										mInstanceCountToRender;
		std::vector<std::vector<InstancedData>>					mInstanceData;
		std::vector<std::vector<std::string>>					mInstancesNames;
		std::vector<std::vector<InstancedData>>					mTempInstanceDataPerLOD;
		std::vector<std::vector<InstancedData>>					mPostCullingInstanceDataPerLOD;
	
		std::string												mName;
		RenderableAABB*											mDebugAABB;
		bool													mEnableAABBDebug = true;
		bool													mWireframeMode = false;
		bool													mAvailableInEditorMode = false;
		bool													mIsSelected = false;
		bool													mIsRendered = true;
		bool													mIsInstanced = false;
		int														mSelectedInstancedObjectIndex = 0;
		bool													mPlacedOnTerrain = false;
		bool													mSavedOnTerrain = false;
		int														mNumInstancesPerVegetationZone = 0;
		bool													mIsCulled = false;
		float													mMinScale = 1.0f;
		float													mMaxScale = 1.0f;

		float													mCameraViewMatrix[16];
		float													mCameraProjectionMatrix[16];
		float													mCurrentObjectTransformMatrix[16] = 
		{   
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f 
		};
		float													mMatrixTranslation[3], mMatrixRotation[3], mMatrixScale[3];
		XMMATRIX												mTransformationMatrix;
		const char* listbox_items[MAX_INSTANCE_COUNT];
	};
}