#include "stdafx.h"

#include "SkinShadingColorMaterial.h"
#include "GameException.h"
#include "Mesh.h"

namespace Rendering
{
	RTTI_DEFINITIONS(SkinShadingColorMaterial)

		SkinShadingColorMaterial::SkinShadingColorMaterial()
		: Material("Render"),
		MATERIAL_VARIABLE_INITIALIZATION(cameraPosition)		   ,
		MATERIAL_VARIABLE_INITIALIZATION(lightposition)			   ,
		MATERIAL_VARIABLE_INITIALIZATION(lightdirection)		   ,
		MATERIAL_VARIABLE_INITIALIZATION(lightfalloffStart)		   ,
		MATERIAL_VARIABLE_INITIALIZATION(lightfalloffWidth)		   ,
		MATERIAL_VARIABLE_INITIALIZATION(lightcolor)			   ,
		MATERIAL_VARIABLE_INITIALIZATION(lightattenuation)		   ,
		MATERIAL_VARIABLE_INITIALIZATION(farPlane)				   ,
		MATERIAL_VARIABLE_INITIALIZATION(bias)					   ,
		MATERIAL_VARIABLE_INITIALIZATION(lightviewProjection)	   ,
		MATERIAL_VARIABLE_INITIALIZATION(currWorldViewProj)		   ,
		MATERIAL_VARIABLE_INITIALIZATION(world)					   ,
		MATERIAL_VARIABLE_INITIALIZATION(id)					   ,
		MATERIAL_VARIABLE_INITIALIZATION(bumpiness)				   ,
		MATERIAL_VARIABLE_INITIALIZATION(specularIntensity)		   ,
		MATERIAL_VARIABLE_INITIALIZATION(specularRoughness)		   ,
		MATERIAL_VARIABLE_INITIALIZATION(specularFresnel)		   ,
		MATERIAL_VARIABLE_INITIALIZATION(translucency)			   ,
		MATERIAL_VARIABLE_INITIALIZATION(sssEnabled)			   ,
		MATERIAL_VARIABLE_INITIALIZATION(sssWidth)				   ,
		MATERIAL_VARIABLE_INITIALIZATION(ambient)				   ,
		MATERIAL_VARIABLE_INITIALIZATION(shadowMap)				   ,
		MATERIAL_VARIABLE_INITIALIZATION(diffuseTex)			   ,
		MATERIAL_VARIABLE_INITIALIZATION(normalTex)				   ,
		MATERIAL_VARIABLE_INITIALIZATION(specularAOTex)			   ,
		MATERIAL_VARIABLE_INITIALIZATION(beckmannTex)			   ,
		MATERIAL_VARIABLE_INITIALIZATION(irradianceTex)			   ,
		MATERIAL_VARIABLE_INITIALIZATION(ProjectiveTextureMatrix)



	{
	}

	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, cameraPosition)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, lightposition)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, lightdirection)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, lightfalloffStart)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, lightfalloffWidth)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, lightcolor)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, lightattenuation)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, farPlane)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, bias)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, lightviewProjection)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, currWorldViewProj)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, world)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, id)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, bumpiness)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, specularIntensity)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, specularRoughness)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, specularFresnel)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, translucency)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, sssEnabled)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, sssWidth)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, ambient)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, shadowMap)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, diffuseTex)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, normalTex)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, specularAOTex)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, beckmannTex)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, irradianceTex)
	MATERIAL_VARIABLE_DEFINITION(SkinShadingColorMaterial, ProjectiveTextureMatrix)



	void SkinShadingColorMaterial::Initialize(Effect* effect)
	{
		Material::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(cameraPosition)
		MATERIAL_VARIABLE_RETRIEVE(lightposition)
		MATERIAL_VARIABLE_RETRIEVE(lightdirection)
		MATERIAL_VARIABLE_RETRIEVE(lightfalloffStart)
		MATERIAL_VARIABLE_RETRIEVE(lightfalloffWidth)
		MATERIAL_VARIABLE_RETRIEVE(lightcolor)
		MATERIAL_VARIABLE_RETRIEVE(lightattenuation)
		MATERIAL_VARIABLE_RETRIEVE(farPlane)
		MATERIAL_VARIABLE_RETRIEVE(bias)
		MATERIAL_VARIABLE_RETRIEVE(lightviewProjection)
		MATERIAL_VARIABLE_RETRIEVE(currWorldViewProj)
		MATERIAL_VARIABLE_RETRIEVE(world)
		MATERIAL_VARIABLE_RETRIEVE(id)
		MATERIAL_VARIABLE_RETRIEVE(bumpiness)
		MATERIAL_VARIABLE_RETRIEVE(specularIntensity)
		MATERIAL_VARIABLE_RETRIEVE(specularRoughness)
		MATERIAL_VARIABLE_RETRIEVE(specularFresnel)
		MATERIAL_VARIABLE_RETRIEVE(translucency)
		MATERIAL_VARIABLE_RETRIEVE(sssEnabled)
		MATERIAL_VARIABLE_RETRIEVE(sssWidth)
		MATERIAL_VARIABLE_RETRIEVE(ambient)
		MATERIAL_VARIABLE_RETRIEVE(shadowMap)
		MATERIAL_VARIABLE_RETRIEVE(diffuseTex)
		MATERIAL_VARIABLE_RETRIEVE(normalTex)
		MATERIAL_VARIABLE_RETRIEVE(specularAOTex)
		MATERIAL_VARIABLE_RETRIEVE(beckmannTex)
		MATERIAL_VARIABLE_RETRIEVE(irradianceTex)
		MATERIAL_VARIABLE_RETRIEVE(ProjectiveTextureMatrix)



		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }

		};

		CreateInputLayout("Render", "Render", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		//CreateInputLayout("csm_shadow_mapping_pcf", "p0", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
	}

	void SkinShadingColorMaterial::CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const
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

	void SkinShadingColorMaterial::CreateVertexBuffer(ID3D11Device* device, NormalMappingMaterialVertex* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const
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

	UINT SkinShadingColorMaterial::VertexSize() const
	{
		return sizeof(NormalMappingMaterialVertex);
	}
}