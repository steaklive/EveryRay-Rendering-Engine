#include "stdafx.h"

#include "SkyboxMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Library
{
	RTTI_DEFINITIONS(SkyboxMaterial)

	SkyboxMaterial::SkyboxMaterial()
		: Material("main11"),
	
		MATERIAL_VARIABLE_INITIALIZATION(WorldViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(SkyboxTexture),
		MATERIAL_VARIABLE_INITIALIZATION(UseCustomColor),
		MATERIAL_VARIABLE_INITIALIZATION(BottomColor),
		MATERIAL_VARIABLE_INITIALIZATION(TopColor)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(SkyboxMaterial, WorldViewProjection)
	MATERIAL_VARIABLE_DEFINITION(SkyboxMaterial, SkyboxTexture)
	MATERIAL_VARIABLE_DEFINITION(SkyboxMaterial, UseCustomColor)
	MATERIAL_VARIABLE_DEFINITION(SkyboxMaterial, BottomColor)
	MATERIAL_VARIABLE_DEFINITION(SkyboxMaterial, TopColor)

	void SkyboxMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(WorldViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(SkyboxTexture)
		MATERIAL_VARIABLE_RETRIEVE(UseCustomColor)
		MATERIAL_VARIABLE_RETRIEVE(BottomColor)
		MATERIAL_VARIABLE_RETRIEVE(TopColor)

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		CreateInputLayout("main11", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
	}

	void SkyboxMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = mesh.Vertices();

		std::vector<XMFLOAT4> vertices;
		vertices.reserve(sourceVertices.size());
		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			vertices.push_back(XMFLOAT4(position.x, position.y, position.z, 1.0f));
		}

		CreateVertexBuffer(device, &vertices[0], vertices.size(), vertexBuffer);
	}

	void SkyboxMaterial::CreateVertexBuffer(ID3D11Device* device, XMFLOAT4* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT SkyboxMaterial::VertexSize() const
	{
		return sizeof(XMFLOAT4);
	}
}