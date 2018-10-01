#include "stdafx.h"

#include "PBRMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Library
{
	RTTI_DEFINITIONS(PBRMaterial)

		PBRMaterial::PBRMaterial()
		: Material("pbr_material"),
		MATERIAL_VARIABLE_INITIALIZATION(WorldViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(World),
		MATERIAL_VARIABLE_INITIALIZATION(LightPosition),
		MATERIAL_VARIABLE_INITIALIZATION(LightRadius),
		MATERIAL_VARIABLE_INITIALIZATION(CameraPosition),
		MATERIAL_VARIABLE_INITIALIZATION(Roughness),
		MATERIAL_VARIABLE_INITIALIZATION(irradianceTexture),
		MATERIAL_VARIABLE_INITIALIZATION(radianceTexture),
		MATERIAL_VARIABLE_INITIALIZATION(integrationTexture),

		MATERIAL_VARIABLE_INITIALIZATION(albedoTexture),
		MATERIAL_VARIABLE_INITIALIZATION(metallicTexture),
		MATERIAL_VARIABLE_INITIALIZATION(roughnessTexture),
		MATERIAL_VARIABLE_INITIALIZATION(normalTexture)

	{
	}

	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, WorldViewProjection)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, World)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, LightPosition)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, LightRadius)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, CameraPosition)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, Roughness)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, Metalness)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, irradianceTexture)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, radianceTexture)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, integrationTexture)

	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, albedoTexture)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, metallicTexture)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, roughnessTexture)
	MATERIAL_VARIABLE_DEFINITION(PBRMaterial, normalTexture)


	void PBRMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(WorldViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(World)
		MATERIAL_VARIABLE_RETRIEVE(LightPosition)
		MATERIAL_VARIABLE_RETRIEVE(LightRadius)
		MATERIAL_VARIABLE_RETRIEVE(CameraPosition)
		MATERIAL_VARIABLE_RETRIEVE(Roughness)
		MATERIAL_VARIABLE_RETRIEVE(Metalness)
		MATERIAL_VARIABLE_RETRIEVE(irradianceTexture)
		MATERIAL_VARIABLE_RETRIEVE(radianceTexture)
		MATERIAL_VARIABLE_RETRIEVE(integrationTexture)
		MATERIAL_VARIABLE_RETRIEVE(albedoTexture)
		MATERIAL_VARIABLE_RETRIEVE(metallicTexture)
		MATERIAL_VARIABLE_RETRIEVE(roughnessTexture)
		MATERIAL_VARIABLE_RETRIEVE(normalTexture)



		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }

		};

		CreateInputLayout("pbr_material", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		//CreateInputLayout("csm_shadow_mapping_pcf", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
	}

	void PBRMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = mesh.Vertices();
		std::vector<XMFLOAT3>* textureCoordinates = mesh.TextureCoordinates().at(0);
		assert(textureCoordinates->size() == sourceVertices.size());
		
		const std::vector<XMFLOAT3>& normals = mesh.Normals();
		assert(textureCoordinates->size() == sourceVertices.size());

		const std::vector<XMFLOAT3>& tangents = mesh.Tangents();
		assert(textureCoordinates->size() == sourceVertices.size());

		std::vector<NormalMappingMaterialVertex> vertices;
		vertices.reserve(sourceVertices.size());

		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			XMFLOAT3 uv = textureCoordinates->at(i);
			XMFLOAT3 normal = normals.at(i);
			XMFLOAT3 tangent = tangents.at(i);

			vertices.push_back(NormalMappingMaterialVertex(XMFLOAT4(position.x, position.y, position.z, 1.0f), XMFLOAT2(uv.x, uv.y), normal, tangent));
		}

		CreateVertexBuffer(device, &vertices[0], vertices.size(), vertexBuffer);
	}

	void PBRMaterial::CreateVertexBuffer(ID3D11Device* device, NormalMappingMaterialVertex* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT PBRMaterial::VertexSize() const
	{
		return sizeof(NormalMappingMaterialVertex);
	}
}