#pragma once
#include "ER_Material.h"

namespace Rendering
{
	class RenderingObject;
}
namespace Library
{
	class Mesh;

	namespace BasicMaterial_CBufferData {
		struct BasicMaterialCB
		{
			XMMATRIX World;
			XMMATRIX ViewProjection;
			XMFLOAT4 Color;
		};
	}
	class ER_BasicColorMaterial : public ER_Material
	{
	public:
		ER_BasicColorMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags);
		~ER_BasicColorMaterial();

		virtual void PrepareForRendering(ER_MaterialSystems neededSystems, Rendering::RenderingObject* aObj, int meshIndex) override;
		virtual void CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer) override;
		virtual int VertexSize() override;

		ConstantBuffer<BasicMaterial_CBufferData::BasicMaterialCB> mConstantBuffer;
	};
}