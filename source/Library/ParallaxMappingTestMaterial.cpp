#include "stdafx.h"

#include "ParallaxMappingTestMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Rendering
{
	RTTI_DEFINITIONS(ParallaxMappingTestMaterial)

		ParallaxMappingTestMaterial::ParallaxMappingTestMaterial()
		: Material("parallax"),
		MATERIAL_VARIABLE_INITIALIZATION(ViewProjection)	  ,
		MATERIAL_VARIABLE_INITIALIZATION(World)				  ,
		MATERIAL_VARIABLE_INITIALIZATION(CameraPosition)	  ,
		MATERIAL_VARIABLE_INITIALIZATION(Albedo)			  ,
		MATERIAL_VARIABLE_INITIALIZATION(Metalness)			  ,
		MATERIAL_VARIABLE_INITIALIZATION(Roughness)			  ,
		MATERIAL_VARIABLE_INITIALIZATION(LightPosition)		  ,
		MATERIAL_VARIABLE_INITIALIZATION(LightColor)		  ,
		MATERIAL_VARIABLE_INITIALIZATION(UseAmbientOcclusion) ,
		MATERIAL_VARIABLE_INITIALIZATION(POMHeightScale)	  ,
		MATERIAL_VARIABLE_INITIALIZATION(POMSoftShadows)	  ,
		MATERIAL_VARIABLE_INITIALIZATION(DemonstrateAO)		  ,
		MATERIAL_VARIABLE_INITIALIZATION(DemonstrateNormal)	  ,
		MATERIAL_VARIABLE_INITIALIZATION(HeightMap)			  ,
		MATERIAL_VARIABLE_INITIALIZATION(EnvMap)			  
	{
	}

		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, ViewProjection)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, World)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, CameraPosition)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, Albedo)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, Metalness)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, Roughness)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, UseAmbientOcclusion)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, LightPosition)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, LightColor)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, POMHeightScale)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, POMSoftShadows)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, DemonstrateAO)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, DemonstrateNormal)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, HeightMap)
		MATERIAL_VARIABLE_DEFINITION(ParallaxMappingTestMaterial, EnvMap)

		void ParallaxMappingTestMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

			MATERIAL_VARIABLE_RETRIEVE(ViewProjection)
			MATERIAL_VARIABLE_RETRIEVE(World)
			MATERIAL_VARIABLE_RETRIEVE(CameraPosition)
			MATERIAL_VARIABLE_RETRIEVE(Albedo)
			MATERIAL_VARIABLE_RETRIEVE(Metalness)
			MATERIAL_VARIABLE_RETRIEVE(Roughness)
			MATERIAL_VARIABLE_RETRIEVE(UseAmbientOcclusion)
			MATERIAL_VARIABLE_RETRIEVE(LightPosition)
			MATERIAL_VARIABLE_RETRIEVE(LightColor)
			MATERIAL_VARIABLE_RETRIEVE(POMHeightScale)
			MATERIAL_VARIABLE_RETRIEVE(POMSoftShadows)
			MATERIAL_VARIABLE_RETRIEVE(DemonstrateAO)
			MATERIAL_VARIABLE_RETRIEVE(DemonstrateNormal)
			MATERIAL_VARIABLE_RETRIEVE(HeightMap)
			MATERIAL_VARIABLE_RETRIEVE(EnvMap)

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};


		CreateInputLayout("parallax", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
	}

	void ParallaxMappingTestMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
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

	void ParallaxMappingTestMaterial::CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormalTangent* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT ParallaxMappingTestMaterial::VertexSize() const
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}
}