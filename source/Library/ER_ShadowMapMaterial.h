#pragma once
#include "ER_Material.h"

namespace Rendering
{
	class RenderingObject;
}
namespace Library
{
	class Mesh;

	namespace ShadowMapMaterial_CBufferData {
		struct ShadowMapCB
		{
			XMMATRIX WorldLightViewProjection;
			XMMATRIX LightViewProjection;
		};
	}
	class ER_ShadowMapMaterial : public ER_Material
	{
	public:
		ER_ShadowMapMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_ShadowMapMaterial();

		void PrepareForRendering(ER_MaterialSystems neededSystems, Rendering::RenderingObject* aObj, int meshIndex, int cascadeIndex);
		virtual void CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer) override;
		virtual int VertexSize() override;

		ConstantBuffer<ShadowMapMaterial_CBufferData::ShadowMapCB> mConstantBuffer;
	};
}