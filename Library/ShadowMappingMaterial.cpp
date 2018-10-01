#include "stdafx.h"

#include "ShadowMappingMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Library
{
	RTTI_DEFINITIONS(ShadowMappingMaterial)

		ShadowMappingMaterial::ShadowMappingMaterial()
		: Material("shadow_mapping"),
		MATERIAL_VARIABLE_INITIALIZATION(WorldViewProjection), MATERIAL_VARIABLE_INITIALIZATION(World),
		MATERIAL_VARIABLE_INITIALIZATION(SpecularColor), MATERIAL_VARIABLE_INITIALIZATION(SpecularPower),
		MATERIAL_VARIABLE_INITIALIZATION(AmbientColor), MATERIAL_VARIABLE_INITIALIZATION(LightColor),
		MATERIAL_VARIABLE_INITIALIZATION(LightPosition), MATERIAL_VARIABLE_INITIALIZATION(LightRadius),
		MATERIAL_VARIABLE_INITIALIZATION(CameraPosition), MATERIAL_VARIABLE_INITIALIZATION(ColorTexture),
		MATERIAL_VARIABLE_INITIALIZATION(ProjectiveTextureMatrix), MATERIAL_VARIABLE_INITIALIZATION(ShadowMap),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowMapSize)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, WorldViewProjection)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, World)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, SpecularColor)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, SpecularPower)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, AmbientColor)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, LightColor)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, LightPosition)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, LightRadius)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, CameraPosition)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, ColorTexture)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, ProjectiveTextureMatrix)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, ShadowMap)
		MATERIAL_VARIABLE_DEFINITION(ShadowMappingMaterial, ShadowMapSize)

	void ShadowMappingMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(WorldViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(World)
		MATERIAL_VARIABLE_RETRIEVE(SpecularColor)
		MATERIAL_VARIABLE_RETRIEVE(SpecularPower)
		MATERIAL_VARIABLE_RETRIEVE(AmbientColor)
		MATERIAL_VARIABLE_RETRIEVE(LightColor)
		MATERIAL_VARIABLE_RETRIEVE(LightPosition)
		MATERIAL_VARIABLE_RETRIEVE(LightRadius)
		MATERIAL_VARIABLE_RETRIEVE(CameraPosition)
		MATERIAL_VARIABLE_RETRIEVE(ColorTexture)
		MATERIAL_VARIABLE_RETRIEVE(ProjectiveTextureMatrix)
		MATERIAL_VARIABLE_RETRIEVE(ShadowMap)
		MATERIAL_VARIABLE_RETRIEVE(ShadowMapSize)

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		CreateInputLayout("shadow_mapping", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		CreateInputLayout("shadow_mapping_manual_pcf", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		CreateInputLayout("shadow_mapping_pcf", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
	}

	void ShadowMappingMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
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

	void ShadowMappingMaterial::CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormal* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT ShadowMappingMaterial::VertexSize() const
	{
		return sizeof(VertexPositionTextureNormal);
	}
}