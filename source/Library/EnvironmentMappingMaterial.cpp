#include "stdafx.h"

#include "EnvironmentMappingMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Rendering
{
	RTTI_DEFINITIONS(EnvironmentMappingMaterial)

	EnvironmentMappingMaterial::EnvironmentMappingMaterial() : Material("main10"),
		MATERIAL_VARIABLE_INITIALIZATION(WorldViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(World),
		MATERIAL_VARIABLE_INITIALIZATION(ReflectionAmount),
		MATERIAL_VARIABLE_INITIALIZATION(AmbientColor),
		MATERIAL_VARIABLE_INITIALIZATION(EnvColor),
		MATERIAL_VARIABLE_INITIALIZATION(CameraPosition),
		MATERIAL_VARIABLE_INITIALIZATION(ColorTexture),
		MATERIAL_VARIABLE_INITIALIZATION(EnvironmentMap)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(EnvironmentMappingMaterial, WorldViewProjection)
	MATERIAL_VARIABLE_DEFINITION(EnvironmentMappingMaterial, World)
	MATERIAL_VARIABLE_DEFINITION(EnvironmentMappingMaterial, ReflectionAmount)
	MATERIAL_VARIABLE_DEFINITION(EnvironmentMappingMaterial, AmbientColor)
	MATERIAL_VARIABLE_DEFINITION(EnvironmentMappingMaterial, EnvColor)
	MATERIAL_VARIABLE_DEFINITION(EnvironmentMappingMaterial, CameraPosition)
	MATERIAL_VARIABLE_DEFINITION(EnvironmentMappingMaterial, ColorTexture)
	MATERIAL_VARIABLE_DEFINITION(EnvironmentMappingMaterial, EnvironmentMap)

	void EnvironmentMappingMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);
		MATERIAL_VARIABLE_RETRIEVE(WorldViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(World)
		MATERIAL_VARIABLE_RETRIEVE(ReflectionAmount)
		MATERIAL_VARIABLE_RETRIEVE(AmbientColor)
		MATERIAL_VARIABLE_RETRIEVE(EnvColor)
		MATERIAL_VARIABLE_RETRIEVE(CameraPosition)
		MATERIAL_VARIABLE_RETRIEVE(ColorTexture)
		MATERIAL_VARIABLE_RETRIEVE(EnvironmentMap)

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		CreateInputLayout("main10", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
	}
	void EnvironmentMappingMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = mesh.Vertices();
		std::vector<XMFLOAT3>* textureCoordinates = mesh.TextureCoordinates().at(0);
		assert(textureCoordinates->size() == sourceVertices.size());
		const std::vector<XMFLOAT3>& normals = mesh.Normals();
		assert(textureCoordinates->size() == sourceVertices.size());

		std::vector<EnvironmentMappingMaterialVertex> vertices;
		vertices.reserve(sourceVertices.size());
		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			XMFLOAT3 uv = textureCoordinates->at(i);
			XMFLOAT3 normal = normals.at(i);
			vertices.push_back(EnvironmentMappingMaterialVertex(XMFLOAT4(position.x, position.y, position.z, 1.0f), XMFLOAT2(uv.x, uv.y), normal));
		}

		CreateVertexBuffer(device, &vertices[0], vertices.size(), vertexBuffer);
	}

	void EnvironmentMappingMaterial::CreateVertexBuffer(ID3D11Device* device, EnvironmentMappingMaterialVertex* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT EnvironmentMappingMaterial::VertexSize() const
	{
		return sizeof(EnvironmentMappingMaterialVertex);
	}

}