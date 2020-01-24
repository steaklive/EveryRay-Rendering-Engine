#include "stdafx.h"

#include "ShadowMappingDirectionalMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Library
{
	RTTI_DEFINITIONS(ShadowMappingDirectionalMaterial)

		ShadowMappingDirectionalMaterial::ShadowMappingDirectionalMaterial()
		: Material("shadow_mapping_pcf"),
		MATERIAL_VARIABLE_INITIALIZATION(WorldViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(World),
		MATERIAL_VARIABLE_INITIALIZATION(AmbientColor), 
		MATERIAL_VARIABLE_INITIALIZATION(LightColor),
		MATERIAL_VARIABLE_INITIALIZATION(LightDirection),
		MATERIAL_VARIABLE_INITIALIZATION(CameraPosition),

		MATERIAL_VARIABLE_INITIALIZATION(ColorTexture),
		MATERIAL_VARIABLE_INITIALIZATION(Distances),
		MATERIAL_VARIABLE_INITIALIZATION(ProjectiveTextureMatrix0),
		MATERIAL_VARIABLE_INITIALIZATION(ProjectiveTextureMatrix1),
		MATERIAL_VARIABLE_INITIALIZATION(ProjectiveTextureMatrix2),

		MATERIAL_VARIABLE_INITIALIZATION(ShadowMap0),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowMap1),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowMap2),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowMapSize),
		MATERIAL_VARIABLE_INITIALIZATION(visualizeCascades)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, WorldViewProjection)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, World)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, AmbientColor)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, LightColor)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, LightDirection)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, CameraPosition)

	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, ColorTexture)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, Distances)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, ProjectiveTextureMatrix0)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, ProjectiveTextureMatrix1)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, ProjectiveTextureMatrix2)

	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, ShadowMap0)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, ShadowMap1)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, ShadowMap2)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, ShadowMapSize)
	MATERIAL_VARIABLE_DEFINITION(ShadowMappingDirectionalMaterial, visualizeCascades)

	void ShadowMappingDirectionalMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(WorldViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(World)
		MATERIAL_VARIABLE_RETRIEVE(AmbientColor)
		MATERIAL_VARIABLE_RETRIEVE(LightColor)
		MATERIAL_VARIABLE_RETRIEVE(LightDirection)
		MATERIAL_VARIABLE_RETRIEVE(CameraPosition)
		MATERIAL_VARIABLE_RETRIEVE(ColorTexture)
		MATERIAL_VARIABLE_RETRIEVE(Distances)
		MATERIAL_VARIABLE_RETRIEVE(ProjectiveTextureMatrix0)
		MATERIAL_VARIABLE_RETRIEVE(ProjectiveTextureMatrix1)
		MATERIAL_VARIABLE_RETRIEVE(ProjectiveTextureMatrix2)
		MATERIAL_VARIABLE_RETRIEVE(ShadowMap0)
		MATERIAL_VARIABLE_RETRIEVE(ShadowMap1)
		MATERIAL_VARIABLE_RETRIEVE(ShadowMap2)
		MATERIAL_VARIABLE_RETRIEVE(ShadowMapSize)
		MATERIAL_VARIABLE_RETRIEVE(visualizeCascades)

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		CreateInputLayout("shadow_mapping_pcf", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		CreateInputLayout("csm_shadow_mapping_pcf", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
	}

	void ShadowMappingDirectionalMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = mesh.Vertices();
		std::vector<XMFLOAT3>* textureCoordinates = mesh.TextureCoordinates().at(0);
		assert(textureCoordinates->size() == sourceVertices.size());
		const std::vector<XMFLOAT3>& normals = mesh.Normals();
		assert(textureCoordinates->size() == sourceVertices.size());

		std::vector<VertexPositionTextureNormal> vertices;
		vertices.reserve(sourceVertices.size());
		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			XMFLOAT3 uv = textureCoordinates->at(i);
			XMFLOAT3 normal = normals.at(i);
			vertices.push_back(VertexPositionTextureNormal(XMFLOAT4(position.x, position.y, position.z, 1.0f), XMFLOAT2(uv.x, uv.y), normal));
		}

		CreateVertexBuffer(device, &vertices[0], vertices.size(), vertexBuffer);
	}

	void ShadowMappingDirectionalMaterial::CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormal* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT ShadowMappingDirectionalMaterial::VertexSize() const
	{
		return sizeof(VertexPositionTextureNormal);
	}
}