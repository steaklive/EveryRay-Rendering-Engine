#pragma once

#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"

using namespace Library;

namespace Rendering
{
	class InstancingMaterial : public Material
	{
		RTTI_DECLARATIONS(InstancingMaterial, Material)

			MATERIAL_VARIABLE_DECLARATION(ViewProjection)
			MATERIAL_VARIABLE_DECLARATION(LightColor0)
			MATERIAL_VARIABLE_DECLARATION(LightPosition0)
			MATERIAL_VARIABLE_DECLARATION(LightColor1)
			MATERIAL_VARIABLE_DECLARATION(LightPosition1)
			MATERIAL_VARIABLE_DECLARATION(LightColor2)
			MATERIAL_VARIABLE_DECLARATION(LightPosition2)
			MATERIAL_VARIABLE_DECLARATION(LightColor3)
			MATERIAL_VARIABLE_DECLARATION(LightPosition3)

			MATERIAL_VARIABLE_DECLARATION(LightRadius0)
			MATERIAL_VARIABLE_DECLARATION(LightRadius1)
			MATERIAL_VARIABLE_DECLARATION(LightRadius2)
			MATERIAL_VARIABLE_DECLARATION(LightRadius3)

			MATERIAL_VARIABLE_DECLARATION(DirectionalLightColor)
			MATERIAL_VARIABLE_DECLARATION(LightDirection)

			MATERIAL_VARIABLE_DECLARATION(CameraPosition)
			MATERIAL_VARIABLE_DECLARATION(ColorTexture)
			MATERIAL_VARIABLE_DECLARATION(NormalTexture)


	public:
		InstancingMaterial();

		struct InstancedData
		{
			XMFLOAT4X4 World;

			InstancedData(){}

			InstancedData(const XMFLOAT4X4& world)
				: World(world)
			{
			}

			InstancedData(CXMMATRIX world)
				: World()
			{
				XMStoreFloat4x4(&World, world);
			}
		};

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, NormalMappingMaterialVertex* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;

		void CreateInstanceBuffer(ID3D11Device* device, std::vector<InstancedData>& instancedData, ID3D11Buffer** instanceBuffer) const;
		void UpdateInstanceBuffer(ID3D11DeviceContext * context, std::vector<InstancedData> instanceData, UINT instanceCount, ID3D11Buffer * instanceBuffer);
		void CreateInstanceBuffer(ID3D11Device* device, InstancedData* instanceData, UINT instanceCount, ID3D11Buffer** instanceBuffer) const;
		UINT InstanceSize() const;

	};

}