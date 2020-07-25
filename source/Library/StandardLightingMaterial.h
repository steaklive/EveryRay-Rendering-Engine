#pragma once

#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"
using namespace Library;

namespace Rendering
{
	class StandardLightingMaterial : public Material
	{
		RTTI_DECLARATIONS(StandardLightingMaterial, Material)

		MATERIAL_VARIABLE_DECLARATION(ViewProjection)
		MATERIAL_VARIABLE_DECLARATION(World)
		MATERIAL_VARIABLE_DECLARATION(ShadowMatrix)
		MATERIAL_VARIABLE_DECLARATION(CameraPosition)

		MATERIAL_VARIABLE_DECLARATION(SunDirection)
		MATERIAL_VARIABLE_DECLARATION(SunColor)
		MATERIAL_VARIABLE_DECLARATION(AmbientColor)
		MATERIAL_VARIABLE_DECLARATION(ShadowTexelSize)

		MATERIAL_VARIABLE_DECLARATION(AlbedoTexture)
		MATERIAL_VARIABLE_DECLARATION(NormalTexture)
		MATERIAL_VARIABLE_DECLARATION(SpecularTexture)
		MATERIAL_VARIABLE_DECLARATION(MetallicTexture)
		MATERIAL_VARIABLE_DECLARATION(RoughnessTexture)
		MATERIAL_VARIABLE_DECLARATION(ShadowTexture)
		MATERIAL_VARIABLE_DECLARATION(IrradianceTexture)
		MATERIAL_VARIABLE_DECLARATION(RadianceTexture)
		MATERIAL_VARIABLE_DECLARATION(IntegrationTexture)

	public:

		struct InstancedData
		{
			XMFLOAT4X4 World;

			InstancedData() {}
			InstancedData(const XMFLOAT4X4& world): World(world) {}
			InstancedData(CXMMATRIX world) : World() { XMStoreFloat4x4(&World, world); }
		};

		StandardLightingMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormalTangent* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;

		void CreateInstanceBuffer(ID3D11Device * device, std::vector<InstancedData>& instanceData, ID3D11Buffer ** instanceBuffer) const;
		void CreateInstanceBuffer(ID3D11Device* device, InstancedData* instanceData, UINT instanceCount, ID3D11Buffer** instanceBuffer) const;
		void UpdateInstanceBuffer(ID3D11DeviceContext* context, std::vector<InstancedData> instanceData, UINT instanceCount, ID3D11Buffer * instanceBuffer);
		UINT InstanceSize() const;
	};
}