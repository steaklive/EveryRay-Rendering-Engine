#pragma once
#include "ER_Material.h"

namespace Library
{
	class ER_RenderingObject;
	class Mesh;

	namespace DebugLightProbeMaterial_CBufferData {
		struct DebugLightProbeCB
		{
			XMMATRIX ViewProjection;
			XMMATRIX World;
			XMFLOAT4 CameraPosition;
			XMFLOAT2 DiscardCulled_IsDiffuse;
		};
	}
	class ER_DebugLightProbeMaterial : public ER_Material
	{
	public:
		ER_DebugLightProbeMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_DebugLightProbeMaterial();

		void PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, int aProbeType);
		virtual void CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer) override;
		virtual int VertexSize() override;

		ConstantBuffer<DebugLightProbeMaterial_CBufferData::DebugLightProbeCB> mConstantBuffer;
	};
}