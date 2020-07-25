#include "stdafx.h"

#include "DepthMapMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Library
{
	RTTI_DEFINITIONS(DepthMapMaterial)

	DepthMapMaterial::DepthMapMaterial()
		: Material("create_depthmap_w_render_target"),
		MATERIAL_VARIABLE_INITIALIZATION(WorldLightViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(LightViewProjection)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(DepthMapMaterial, WorldLightViewProjection)
	MATERIAL_VARIABLE_DEFINITION(DepthMapMaterial, LightViewProjection)

	void DepthMapMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(WorldLightViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(LightViewProjection)

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptionsInstanced[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
		};

		CreateInputLayout("create_depthmap", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		CreateInputLayout("create_depthmap_w_bias", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		CreateInputLayout("create_depthmap_w_render_target", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));

		CreateInputLayout("create_depthmap_instanced", "p0", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
		CreateInputLayout("create_depthmap_w_bias_instanced", "p0", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
		CreateInputLayout("create_depthmap_w_render_target_instanced", "p0", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
	}

	void DepthMapMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = mesh.Vertices();

		std::vector<VertexPosition> vertices;
		vertices.reserve(sourceVertices.size());
		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			vertices.push_back(VertexPosition(XMFLOAT4(position.x, position.y, position.z, 1.0f)));
		}

		CreateVertexBuffer(device, &vertices[0], vertices.size(), vertexBuffer);
	}

	void DepthMapMaterial::CreateVertexBuffer(ID3D11Device* device, VertexPosition* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT DepthMapMaterial::VertexSize() const
	{
		return sizeof(VertexPosition);
	}
}