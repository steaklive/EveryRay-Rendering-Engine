#pragma once

#include "Common.h"
#include "GameComponent.h"
#include "Material.h"
#include "VertexDeclarations.h"
#include "InstancingMaterial.h"
#include "ModelMaterial.h"
#include "Effect.h"
#include "GeneralEvent.h"

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
		ID3D11ShaderResourceView* RoughnessMap	= nullptr;
		ID3D11ShaderResourceView* MetallicMap	= nullptr;
		ID3D11ShaderResourceView* ExtraMap1		= nullptr;  // can be used for extra textures (AO, opacity, displacement)
		ID3D11ShaderResourceView* ExtraMap2		= nullptr;  // can be used for extra textures (AO, opacity, displacement)
		ID3D11ShaderResourceView* ExtraMap3		= nullptr;  // can be used for extra textures (AO, opacity, displacement)

		TextureData(){}

		TextureData(ID3D11ShaderResourceView* albedo, ID3D11ShaderResourceView* normal, ID3D11ShaderResourceView* roughness, ID3D11ShaderResourceView* metallic, ID3D11ShaderResourceView* extra1, ID3D11ShaderResourceView* extra2, ID3D11ShaderResourceView* extra3)
			:
			AlbedoMap(albedo),
			NormalMap(normal),
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
			ReleaseObject(RoughnessMap);
			ReleaseObject(MetallicMap);
			ReleaseObject(ExtraMap1);
			ReleaseObject(ExtraMap2);
			ReleaseObject(ExtraMap3);
		}
	};

	struct InstanceBufferData
	{
		InstancingMaterial::InstancedData	InstanceData;
		UINT								InstanceCount = 0;
		XMFLOAT3							InstancesPositions;
		ID3D11Buffer*						InstanceBuffer = nullptr;

		InstanceBufferData()
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
		RenderingObject(std::string pName, Game& pGame, std::unique_ptr<Model> pModel);
		~RenderingObject();

		void LoadCustomMeshTextures(int meshIndex, std::wstring albedoPath, std::wstring normalPath, std::wstring roughnessPath, std::wstring metallicPath, std::wstring extra1Path, std::wstring extra2Path, std::wstring extra3Path);

		void LoadMaterial(Material* pMaterial, Effect* pEffect);
		void LoadAssignedMeshTextures();
		void LoadRenderBuffers();
		void LoadInstanceBuffers();
		void Draw(/*Camera* camera*/);

		Material* GetMeshMaterial(int meshIndex) { return mMeshesMaterials[meshIndex]; }
		TextureData& GetTextureData(int meshIndex) { return mMeshesTextures[meshIndex]; }
		int GetMeshCount() { return mMeshesCount; }

		GeneralEvent<Delegate_MeshMaterialVariablesUpdate>* MeshMaterialVariablesUpdateEvent = new GeneralEvent<Delegate_MeshMaterialVariablesUpdate>();

	private:
		void LoadTexture(TextureType type, std::wstring path, int meshIndex);
		std::vector<RenderBufferData>						mMeshesRenderBuffers;
		std::vector<TextureData>							mMeshesTextures;
		std::vector<InstanceBufferData>						mMeshesInstanceBuffers;
		std::vector<Material*>								mMeshesMaterials;
		std::vector<std::vector<XMFLOAT3>>					mMeshVertices;

		std::vector<XMFLOAT3>								mAABB;
		std::unique_ptr<Model>								mModel;
		ID3D11Buffer*			VertexBuffer;
		ID3D11Buffer*			IndexBuffer;
		ID3D11Buffer*			VertexBuffer2;
		ID3D11Buffer*			IndexBuffer2;
		int													mMeshesCount;
		std::string											mName;

	};
}