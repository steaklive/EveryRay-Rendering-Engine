#include "stdafx.h"

#include "VoxelizationGIMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Rendering
{
	RTTI_DEFINITIONS(VoxelizationGIMaterial)

	VoxelizationGIMaterial::VoxelizationGIMaterial()
		: Material("voxelizationGI"),
		MATERIAL_VARIABLE_INITIALIZATION(ViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(LightViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(WorldVoxelScale),
		MATERIAL_VARIABLE_INITIALIZATION(MeshWorld),
		MATERIAL_VARIABLE_INITIALIZATION(MeshAlbedo),
		MATERIAL_VARIABLE_INITIALIZATION(ShadowMap)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(VoxelizationGIMaterial, ViewProjection)
	MATERIAL_VARIABLE_DEFINITION(VoxelizationGIMaterial, LightViewProjection)
	MATERIAL_VARIABLE_DEFINITION(VoxelizationGIMaterial, WorldVoxelScale)
	MATERIAL_VARIABLE_DEFINITION(VoxelizationGIMaterial, MeshWorld)
	MATERIAL_VARIABLE_DEFINITION(VoxelizationGIMaterial, MeshAlbedo)
	MATERIAL_VARIABLE_DEFINITION(VoxelizationGIMaterial, ShadowMap)

	void VoxelizationGIMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(ViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(LightViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(WorldVoxelScale)
		MATERIAL_VARIABLE_RETRIEVE(MeshWorld)
		MATERIAL_VARIABLE_RETRIEVE(MeshAlbedo)
		MATERIAL_VARIABLE_RETRIEVE(ShadowMap)

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		CreateInputLayout("voxelizationGI", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
	}

	void VoxelizationGIMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
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

	void VoxelizationGIMaterial::CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormalTangent* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT VoxelizationGIMaterial::VertexSize() const
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}
}