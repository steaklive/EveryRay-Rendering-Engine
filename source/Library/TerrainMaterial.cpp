#include "stdafx.h"

#include "TerrainMaterial.h"
#include "GameException.h"
#include "Mesh.h"
#include "ColorHelper.h"

namespace Library
{
	RTTI_DEFINITIONS(TerrainMaterial)

	TerrainMaterial::TerrainMaterial() : Material("main"),
		MATERIAL_VARIABLE_INITIALIZATION(World),
		MATERIAL_VARIABLE_INITIALIZATION(View),
		MATERIAL_VARIABLE_INITIALIZATION(Projection),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowMatrices),
		MATERIAL_VARIABLE_INITIALIZATION(heightTexture),
		MATERIAL_VARIABLE_INITIALIZATION(groundTexture),
		MATERIAL_VARIABLE_INITIALIZATION(grassTexture),
		MATERIAL_VARIABLE_INITIALIZATION(rockTexture),
		MATERIAL_VARIABLE_INITIALIZATION(mudTexture),
		MATERIAL_VARIABLE_INITIALIZATION(splatTexture),
		MATERIAL_VARIABLE_INITIALIZATION(cascadedShadowTextures),
		//MATERIAL_VARIABLE_INITIALIZATION(normalTexture),
		MATERIAL_VARIABLE_INITIALIZATION(SunDirection),
		MATERIAL_VARIABLE_INITIALIZATION(SunColor),
		MATERIAL_VARIABLE_INITIALIZATION(AmbientColor),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowCascadeDistances),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowTexelSize),
		MATERIAL_VARIABLE_INITIALIZATION(CameraPosition),
		MATERIAL_VARIABLE_INITIALIZATION(TessellationFactor),
		MATERIAL_VARIABLE_INITIALIZATION(TerrainHeightScale),
		MATERIAL_VARIABLE_INITIALIZATION(UseDynamicTessellation),
		MATERIAL_VARIABLE_INITIALIZATION(TessellationFactorDynamic),
		MATERIAL_VARIABLE_INITIALIZATION(DistanceFactor)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, World)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, View)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, Projection)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, ShadowMatrices)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, heightTexture)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, groundTexture)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, grassTexture)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, rockTexture)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, mudTexture)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, splatTexture)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, cascadedShadowTextures)
	//MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, normalTexture)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, SunDirection)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, SunColor)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, AmbientColor)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, ShadowCascadeDistances)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, ShadowTexelSize)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, CameraPosition)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, TessellationFactor)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, TerrainHeightScale)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, UseDynamicTessellation)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, TessellationFactorDynamic)
	MATERIAL_VARIABLE_DEFINITION(TerrainMaterial, DistanceFactor)

	void TerrainMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(World)
		MATERIAL_VARIABLE_RETRIEVE(View)
		MATERIAL_VARIABLE_RETRIEVE(Projection)
		MATERIAL_VARIABLE_RETRIEVE(ShadowMatrices)
		MATERIAL_VARIABLE_RETRIEVE(heightTexture)
		MATERIAL_VARIABLE_RETRIEVE(groundTexture)
		MATERIAL_VARIABLE_RETRIEVE(grassTexture)
		MATERIAL_VARIABLE_RETRIEVE(rockTexture)
		MATERIAL_VARIABLE_RETRIEVE(mudTexture)
		MATERIAL_VARIABLE_RETRIEVE(splatTexture)
		MATERIAL_VARIABLE_RETRIEVE(cascadedShadowTextures)
		//MATERIAL_VARIABLE_RETRIEVE(normalTexture)
			MATERIAL_VARIABLE_RETRIEVE(SunDirection)
			MATERIAL_VARIABLE_RETRIEVE(SunColor)
			MATERIAL_VARIABLE_RETRIEVE(AmbientColor)
			MATERIAL_VARIABLE_RETRIEVE(ShadowCascadeDistances)
			MATERIAL_VARIABLE_RETRIEVE(ShadowTexelSize)
			MATERIAL_VARIABLE_RETRIEVE(CameraPosition)
			MATERIAL_VARIABLE_RETRIEVE(TessellationFactor)
			MATERIAL_VARIABLE_RETRIEVE(TerrainHeightScale)
			MATERIAL_VARIABLE_RETRIEVE(UseDynamicTessellation)
			MATERIAL_VARIABLE_RETRIEVE(TessellationFactorDynamic)
			MATERIAL_VARIABLE_RETRIEVE(DistanceFactor)

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			//{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptionsTS[] =
		{ 
			{ "PATCH_INFO", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		
		CreateInputLayout("main", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		CreateInputLayout("main", "tessellation", inputElementDescriptionsTS, ARRAYSIZE(inputElementDescriptionsTS));
	}

	void TerrainMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
	{
		//const std::vector<XMFLOAT3>& sourceVertices = mesh.Vertices();
		//std::vector<XMFLOAT3>* textureCoordinates = mesh.TextureCoordinates().at(0);
		//const std::vector<XMFLOAT3>& normals = mesh.Normals();
		//
		//std::vector<TerrainVertexInput> vertices;
		//vertices.reserve(sourceVertices.size());
		//for (UINT i = 0; i < sourceVertices.size(); i++)
		//{
		//	XMFLOAT3 position = sourceVertices.at(i);
		//	XMFLOAT3 uv = textureCoordinates->at(i);
		//	XMFLOAT3 normal = normals.at(i);
		//	vertices.push_back(TerrainVertexInput(XMFLOAT4(position.x, position.y, position.z, 1.0f), XMFLOAT2(uv.x, uv.y), normal));
		//}
		//
		//CreateVertexBuffer(device, &vertices[0], vertices.size(), vertexBuffer);
	}

	void TerrainMaterial::CreateVertexBuffer(ID3D11Device* device, TerrainVertexInput* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
	{
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.ByteWidth = VertexSize() * vertexCount;
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexSubResourceData;
		ZeroMemory(&vertexSubResourceData, sizeof(vertexSubResourceData));
		vertexSubResourceData.pSysMem = vertices;
		if (FAILED(device->CreateBuffer(&vertexBufferDesc, &vertexSubResourceData, vertexBuffer)))
		{
			throw GameException("ID3D11Device::CreateBuffer() failed.");
		}
	}

	UINT TerrainMaterial::VertexSize() const
	{
		return sizeof(TerrainVertexInput);
	}
}