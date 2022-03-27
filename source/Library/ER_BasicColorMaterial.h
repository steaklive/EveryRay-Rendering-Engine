#pragma once
#include "ER_Material.h"

namespace Library
{
	class RenderingObject;
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
		ER_BasicColorMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_BasicColorMaterial();

		virtual void PrepareForRendering(ER_MaterialSystems neededSystems, RenderingObject* aObj, int meshIndex) override;
		void PrepareForRendering(const XMMATRIX& worldTransform, const XMFLOAT4& color);

		virtual void CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer) override;
		virtual int VertexSize() override;

		ConstantBuffer<BasicMaterial_CBufferData::BasicMaterialCB> mConstantBuffer;
	};
}