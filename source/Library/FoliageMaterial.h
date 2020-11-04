 #pragma once
#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"

namespace Library
{
	class FoliageMaterial : public Material
	{
		RTTI_DECLARATIONS(FoliageMaterial, Material)

			MATERIAL_VARIABLE_DECLARATION(World)
			MATERIAL_VARIABLE_DECLARATION(View)
			MATERIAL_VARIABLE_DECLARATION(Projection)
			MATERIAL_VARIABLE_DECLARATION(albedoTexture)
			MATERIAL_VARIABLE_DECLARATION(SunDirection)
			MATERIAL_VARIABLE_DECLARATION(SunColor)
			MATERIAL_VARIABLE_DECLARATION(AmbientColor)
			MATERIAL_VARIABLE_DECLARATION(ShadowTexelSize)
			MATERIAL_VARIABLE_DECLARATION(ShadowCascadeDistances)
			MATERIAL_VARIABLE_DECLARATION(RotateToCamera)
			MATERIAL_VARIABLE_DECLARATION(Time)
			MATERIAL_VARIABLE_DECLARATION(WindFrequency)
			MATERIAL_VARIABLE_DECLARATION(WindStrength)
			MATERIAL_VARIABLE_DECLARATION(WindGustDistance)
			MATERIAL_VARIABLE_DECLARATION(WindDirection)

	public:
		FoliageMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTexture* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		void CreateIndexBuffer(Mesh& mesh, ID3D11Buffer** indexBuffer);
		virtual UINT VertexSize() const override;
	};
}