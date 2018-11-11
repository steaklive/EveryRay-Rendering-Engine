#include "InstancingMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Rendering
{
	RTTI_DEFINITIONS(InstancingMaterial)

	InstancingMaterial::InstancingMaterial()
		: Material("main11"),
		MATERIAL_VARIABLE_INITIALIZATION(ViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(LightColor0),
		MATERIAL_VARIABLE_INITIALIZATION(LightPosition0),
		MATERIAL_VARIABLE_INITIALIZATION(LightColor1),
		MATERIAL_VARIABLE_INITIALIZATION(LightPosition1),
		MATERIAL_VARIABLE_INITIALIZATION(LightColor2),
		MATERIAL_VARIABLE_INITIALIZATION(LightPosition2),
		MATERIAL_VARIABLE_INITIALIZATION(LightColor3),
		MATERIAL_VARIABLE_INITIALIZATION(LightPosition3),
		MATERIAL_VARIABLE_INITIALIZATION(LightRadius0),
		MATERIAL_VARIABLE_INITIALIZATION(LightRadius1),
		MATERIAL_VARIABLE_INITIALIZATION(LightRadius2),
		MATERIAL_VARIABLE_INITIALIZATION(LightRadius3),

		MATERIAL_VARIABLE_INITIALIZATION(DirectionalLightColor),
		MATERIAL_VARIABLE_INITIALIZATION(LightDirection),


		MATERIAL_VARIABLE_INITIALIZATION(CameraPosition),
		MATERIAL_VARIABLE_INITIALIZATION(ColorTexture),
		MATERIAL_VARIABLE_INITIALIZATION(NormalTexture)

	{
	}

	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, ViewProjection)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightColor0)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightPosition0)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightColor1)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightPosition1)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightColor2)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightPosition2)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightColor3)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightPosition3)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightRadius0)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightRadius1)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightRadius2)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightRadius3)

	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, DirectionalLightColor)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, LightDirection)

	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, CameraPosition)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, ColorTexture)
	MATERIAL_VARIABLE_DEFINITION(InstancingMaterial, NormalTexture)


	void InstancingMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(ViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(LightColor0)
		MATERIAL_VARIABLE_RETRIEVE(LightPosition0)
		MATERIAL_VARIABLE_RETRIEVE(LightColor1)
		MATERIAL_VARIABLE_RETRIEVE(LightPosition1)
		MATERIAL_VARIABLE_RETRIEVE(LightColor2)
		MATERIAL_VARIABLE_RETRIEVE(LightPosition2)
		MATERIAL_VARIABLE_RETRIEVE(LightColor3)
		MATERIAL_VARIABLE_RETRIEVE(LightPosition3)
		MATERIAL_VARIABLE_RETRIEVE(LightRadius0)
		MATERIAL_VARIABLE_RETRIEVE(LightRadius1)
		MATERIAL_VARIABLE_RETRIEVE(LightRadius2)
		MATERIAL_VARIABLE_RETRIEVE(LightRadius3)

		MATERIAL_VARIABLE_RETRIEVE(DirectionalLightColor)
		MATERIAL_VARIABLE_RETRIEVE(LightDirection)

		MATERIAL_VARIABLE_RETRIEVE(CameraPosition)
		MATERIAL_VARIABLE_RETRIEVE(ColorTexture)
		MATERIAL_VARIABLE_RETRIEVE(NormalTexture)


		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		};

		CreateInputLayout("main11", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
	}

	void InstancingMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = mesh.Vertices();
		std::vector<XMFLOAT3>* textureCoordinates = mesh.TextureCoordinates().at(0);
		
		assert(textureCoordinates->size() == sourceVertices.size());
		
		const std::vector<XMFLOAT3>& normals = mesh.Normals();
		assert(normals.size() == sourceVertices.size());

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

	void InstancingMaterial::CreateVertexBuffer(ID3D11Device* device, NormalMappingMaterialVertex* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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
			throw GameException("ID3D11Device::CreateBuffer() while creating VertexBuffer failed.");
		}
	}

	UINT InstancingMaterial::VertexSize() const
	{
		return sizeof(NormalMappingMaterialVertex);
	}

	void InstancingMaterial::CreateInstanceBuffer(ID3D11Device* device, std::vector<InstancedData>& instanceData, ID3D11Buffer** instanceBuffer) const
	{
		CreateInstanceBuffer(device, &instanceData[0], instanceData.size(), instanceBuffer);
	}

	void InstancingMaterial::CreateInstanceBuffer(ID3D11Device* device, InstancedData* instanceData, UINT instanceCount, ID3D11Buffer** instanceBuffer) const
	{
		D3D11_BUFFER_DESC instanceBufferDesc;
		ZeroMemory(&instanceBufferDesc, sizeof(instanceBufferDesc));
		instanceBufferDesc.ByteWidth = InstanceSize() * instanceCount;
		instanceBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA instanceSubResourceData;
		ZeroMemory(&instanceSubResourceData, sizeof(instanceSubResourceData));
		instanceSubResourceData.pSysMem = instanceData;
		if (FAILED(device->CreateBuffer(&instanceBufferDesc, &instanceSubResourceData, instanceBuffer)))
		{
			throw GameException("ID3D11Device::CreateBuffer() failed while creating InstanceBuffer.");
		}
	}

	UINT InstancingMaterial::InstanceSize() const
	{
		return sizeof(InstancedData);
	}
}