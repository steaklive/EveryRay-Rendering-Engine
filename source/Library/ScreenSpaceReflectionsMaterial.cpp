#include "stdafx.h"
#include "ScreenSpaceReflectionsMaterial.h"
#include "GameException.h"
#include "Mesh.h"
namespace Rendering
{
	RTTI_DEFINITIONS(ScreenSpaceReflectionsMaterial)

	ScreenSpaceReflectionsMaterial::ScreenSpaceReflectionsMaterial()
		: Material("ssr"),
		MATERIAL_VARIABLE_INITIALIZATION(ColorTexture),
		MATERIAL_VARIABLE_INITIALIZATION(GBufferDepth),
		MATERIAL_VARIABLE_INITIALIZATION(GBufferNormals),
		MATERIAL_VARIABLE_INITIALIZATION(GBufferExtra),
		MATERIAL_VARIABLE_INITIALIZATION(ViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(InvProjMatrix),
		MATERIAL_VARIABLE_INITIALIZATION(InvViewMatrix),
		MATERIAL_VARIABLE_INITIALIZATION(ViewMatrix),
		MATERIAL_VARIABLE_INITIALIZATION(ProjMatrix),
		MATERIAL_VARIABLE_INITIALIZATION(CameraPosition),
		MATERIAL_VARIABLE_INITIALIZATION(StepSize),
		MATERIAL_VARIABLE_INITIALIZATION(MaxThickness),
		MATERIAL_VARIABLE_INITIALIZATION(Time),
		MATERIAL_VARIABLE_INITIALIZATION(MaxRayCount)

	{
	}
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, ColorTexture)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, GBufferDepth)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, GBufferNormals)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, GBufferExtra)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, ViewProjection)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, InvProjMatrix)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, InvViewMatrix)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, ViewMatrix)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, ProjMatrix)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, CameraPosition)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, StepSize)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, MaxThickness)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, Time)
	MATERIAL_VARIABLE_DEFINITION(ScreenSpaceReflectionsMaterial, MaxRayCount)

	void ScreenSpaceReflectionsMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);
		MATERIAL_VARIABLE_RETRIEVE(ColorTexture)
		MATERIAL_VARIABLE_RETRIEVE(GBufferDepth)
		MATERIAL_VARIABLE_RETRIEVE(GBufferNormals)
		MATERIAL_VARIABLE_RETRIEVE(GBufferExtra)
		MATERIAL_VARIABLE_RETRIEVE(ViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(InvProjMatrix)
		MATERIAL_VARIABLE_RETRIEVE(InvViewMatrix)
		MATERIAL_VARIABLE_RETRIEVE(ViewMatrix)
		MATERIAL_VARIABLE_RETRIEVE(ProjMatrix)
		MATERIAL_VARIABLE_RETRIEVE(CameraPosition)
		MATERIAL_VARIABLE_RETRIEVE(StepSize)
		MATERIAL_VARIABLE_RETRIEVE(MaxThickness)
		MATERIAL_VARIABLE_RETRIEVE(Time)
		MATERIAL_VARIABLE_RETRIEVE(MaxRayCount)

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		CreateInputLayout("ssr", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		CreateInputLayout("no_ssr", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));

	}

	void ScreenSpaceReflectionsMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = mesh.Vertices();
		std::vector<XMFLOAT3>* textureCoordinates = mesh.TextureCoordinates().at(0);
		assert(textureCoordinates->size() == sourceVertices.size());

		std::vector<VertexPositionTexture> vertices;
		vertices.reserve(sourceVertices.size());

		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			XMFLOAT3 uv = textureCoordinates->at(i);

			vertices.push_back(VertexPositionTexture(XMFLOAT4(position.x, position.y, position.z, 1.0f), XMFLOAT2(uv.x, uv.y)));
		}

		CreateVertexBuffer(device, &vertices[0], vertices.size(), vertexBuffer);
	}

	void ScreenSpaceReflectionsMaterial::CreateVertexBuffer(ID3D11Device* device, VertexPositionTexture* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT ScreenSpaceReflectionsMaterial::VertexSize() const
	{
		return sizeof(VertexPositionTexture);
	}
}