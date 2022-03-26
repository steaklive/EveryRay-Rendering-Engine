#pragma once
#include "ER_Material.h"

namespace Rendering
{
	class RenderingObject;
}
namespace Library
{
	class Mesh;

	namespace DebugLightProbeMaterial_CBufferData {
		struct DebugLightProbeCB
		{
			XMMATRIX ViewProjection;
			XMMATRIX World;
			XMFLOAT4 CameraPosition;
			bool DiscardCulledProbe;
		};
	}
	class ER_DebugLightProbeMaterial : public ER_Material
	{
	public:
		ER_DebugLightProbeMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_DebugLightProbeMaterial();

		void PrepareForRendering(ER_MaterialSystems neededSystems, Rendering::RenderingObject* aObj, int meshIndex, int aProbeType, int volumeIndex);
		virtual void CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer) override;
		virtual int VertexSize() override;

		ConstantBuffer<DebugLightProbeMaterial_CBufferData::DebugLightProbeCB> mConstantBuffer;
	};
}