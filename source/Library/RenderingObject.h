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
		RenderingObject(std::string pName, Game& pGame, Camera& pCamera, std::unique_ptr<Model> pModel, bool availableInEditor = false);
		~RenderingObject();

		void LoadCustomMeshTextures(int meshIndex, std::wstring albedoPath, std::wstring normalPath, std::wstring specularPath, std::wstring roughnessPath, std::wstring metallicPath, std::wstring extra1Path, std::wstring extra2Path, std::wstring extra3Path);
		void LoadMaterial(Material* pMaterial, Effect* pEffect);
		void LoadRenderBuffers();
		void LoadInstanceBuffers(std::vector<InstancingMaterial::InstancedData>& pInstanceData, int materialIndex);
		void Draw(int materialIndex, bool toDepth = false);
		void DrawAABB();
		void DrawInstanced(int materialIndex);
		void UpdateInstanceData(std::vector<InstancingMaterial::InstancedData> pInstanceData, int materialIndex);
		void UpdateGizmos();
		void Update(const GameTime& time);
		void Selected(bool val) { mSelected = val; }

		Material* GetMeshMaterial() { return mMaterials[0]; }
		std::vector<Material*> GetMaterials() { return mMaterials; }
		TextureData& GetTextureData(int meshIndex) { return mMeshesTextureBuffers[meshIndex]; }
		int GetMeshCount() { return mMeshesCount; }
		std::vector<XMFLOAT3> GetAABB() { return mAABB; }
		const std::vector<XMFLOAT3>& GetVertices();
		UINT GetInstanceCount() { return mInstanceCount; }
		XMFLOAT4X4 GetTransformMatrix() { return XMFLOAT4X4(mObjectTransformMatrix); }
		bool IsAvailableInEditor() { return mAvailableInEditorMode; }
		bool IsSelected() { return mSelected; }

		void SetTranslation(float x, float y, float z) { mObjectTransformMatrix[3] = 0; mObjectTransformMatrix[7] = 0; mObjectTransformMatrix[11] = 0; }
		void SetScale(float x, float y, float z) { mObjectTransformMatrix[0] = x; mObjectTransformMatrix[5] = y; mObjectTransformMatrix[10] = z; }

		GeneralEvent<Delegate_MeshMaterialVariablesUpdate>* MeshMaterialVariablesUpdateEvent = new GeneralEvent<Delegate_MeshMaterialVariablesUpdate>();

	private:
		RenderingObject();
		RenderingObject(const RenderingObject& rhs);
		RenderingObject& operator=(const RenderingObject& rhs);

		void UpdateGizmoTransform(const float *cameraView, float *cameraProjection, float* matrix);
		void LoadAssignedMeshTextures();
		void LoadTexture(TextureType type, std::wstring path, int meshIndex);
		
		Camera& mCamera;
		std::vector<std::vector<RenderBufferData*>>			mMeshesRenderBuffers;
		std::vector<TextureData>							mMeshesTextureBuffers;
		std::vector<InstanceBufferData*>					mMeshesInstanceBuffers;
		std::vector<std::vector<XMFLOAT3>>					mMeshVertices;
		std::vector<XMFLOAT3>								mMeshAllVertices;
		std::vector<Material*>								mMaterials;
		std::vector<XMFLOAT3>								mAABB;
		std::unique_ptr<Model>								mModel;
		int													mMeshesCount;
		UINT												mInstanceCount;

		//Editor variables
		std::string											mName;
		RenderableAABB*										mDebugAABB;
		bool												mEnableAABBDebug = true;
		bool												mWireframeMode = false;
		bool												mAvailableInEditorMode = false;
		bool												mSelected = false;


		float mCameraViewMatrix[16];
		float mCameraProjectionMatrix[16];
		float mObjectTransformMatrix[16] = 
		{   
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f 
		};
		float mMatrixTranslation[3], mMatrixRotation[3], mMatrixScale[3];

	};
}