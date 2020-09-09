#include "stdafx.h"

#include "StandardLightingMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Rendering
{
	RTTI_DEFINITIONS(StandardLightingMaterial)

		StandardLightingMaterial::StandardLightingMaterial()
		: Material("standard_lighting_no_pbr"),
		MATERIAL_VARIABLE_INITIALIZATION(ViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(World),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowMatrices),
		MATERIAL_VARIABLE_INITIALIZATION(CameraPosition),
		MATERIAL_VARIABLE_INITIALIZATION(SunDirection),
		MATERIAL_VARIABLE_INITIALIZATION(SunColor),
		MATERIAL_VARIABLE_INITIALIZATION(AmbientColor),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowCascadeDistances),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowTexelSize),
		MATERIAL_VARIABLE_INITIALIZATION(AlbedoTexture),
		MATERIAL_VARIABLE_INITIALIZATION(NormalTexture),
		MATERIAL_VARIABLE_INITIALIZATION(SpecularTexture),
		MATERIAL_VARIABLE_INITIALIZATION(MetallicTexture),
		MATERIAL_VARIABLE_INITIALIZATION(RoughnessTexture),
		MATERIAL_VARIABLE_INITIALIZATION(CascadedShadowTextures),
		MATERIAL_VARIABLE_INITIALIZATION(IrradianceTexture),
		MATERIAL_VARIABLE_INITIALIZATION(RadianceTexture),
		MATERIAL_VARIABLE_INITIALIZATION(IntegrationTexture)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, ViewProjection)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, World)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, ShadowMatrices)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, CameraPosition)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, SunDirection)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, SunColor)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, AmbientColor)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, ShadowCascadeDistances)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, ShadowTexelSize)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, AlbedoTexture)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, NormalTexture)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, SpecularTexture)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, MetallicTexture)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, RoughnessTexture)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, CascadedShadowTextures)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, IrradianceTexture)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, RadianceTexture)
		MATERIAL_VARIABLE_DEFINITION(StandardLightingMaterial, IntegrationTexture)


	void StandardLightingMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(ViewProjection)
			MATERIAL_VARIABLE_RETRIEVE(World)
			MATERIAL_VARIABLE_RETRIEVE(ShadowMatrices)
			MATERIAL_VARIABLE_RETRIEVE(CameraPosition)
			MATERIAL_VARIABLE_RETRIEVE(SunDirection)
			MATERIAL_VARIABLE_RETRIEVE(SunColor)
			MATERIAL_VARIABLE_RETRIEVE(AmbientColor)
			MATERIAL_VARIABLE_RETRIEVE(ShadowCascadeDistances)
			MATERIAL_VARIABLE_RETRIEVE(ShadowTexelSize)
			MATERIAL_VARIABLE_RETRIEVE(AlbedoTexture)
			MATERIAL_VARIABLE_RETRIEVE(NormalTexture)
			MATERIAL_VARIABLE_RETRIEVE(SpecularTexture)
			MATERIAL_VARIABLE_RETRIEVE(MetallicTexture)
			MATERIAL_VARIABLE_RETRIEVE(RoughnessTexture)
			MATERIAL_VARIABLE_RETRIEVE(CascadedShadowTextures)
			MATERIAL_VARIABLE_RETRIEVE(IrradianceTexture)
			MATERIAL_VARIABLE_RETRIEVE(RadianceTexture)
			MATERIAL_VARIABLE_RETRIEVE(IntegrationTexture)

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptionsInstancing[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
		};

		CreateInputLayout("standard_lighting_no_pbr", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		CreateInputLayout("standard_lighting_pbr", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));

		CreateInputLayout("standard_lighting_no_pbr_instancing", "p0", inputElementDescriptionsInstancing, ARRAYSIZE(inputElementDescriptionsInstancing));
		CreateInputLayout("standard_lighting_pbr_instancing", "p0", inputElementDescriptionsInstancing, ARRAYSIZE(inputElementDescriptionsInstancing));
	}

	void StandardLightingMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = mesh.Vertices();
		std::vector<XMFLOAT3>* textureCoordinates = mesh.TextureCoordinates().at(0);
		assert(textureCoordinates->size() == sourceVertices.size());

		const std::vector<XMFLOAT3>& normals = mesh.Normals();
		assert(textureCoordinates->size() == sourceVertices.size());

		const std::vector<XMFLOAT3>& tangents = mesh.Tangents();
		assert(textureCoordinates->size() == sourceVertices.size());

		std::vector<VertexPositionTextureNormalTangent> vertices;
		vertices.reserve(sourceVertices.size());

		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			XMFLOAT3 uv = textureCoordinates->at(i);
			XMFLOAT3 normal = normals.at(i);
			XMFLOAT3 tangent = tangents.at(i);

			vertices.push_back(VertexPositionTextureNormalTangent(XMFLOAT4(position.x, position.y, position.z, 1.0f), XMFLOAT2(uv.x, uv.y), normal, tangent));
		}

		CreateVertexBuffer(device, &vertices[0], vertices.size(), vertexBuffer);
	}

	void StandardLightingMaterial::CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormalTangent* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT StandardLightingMaterial::VertexSize() const
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}
}