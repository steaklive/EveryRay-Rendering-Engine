#pragma once
#include "ER_Material.h"

namespace Library
{
	class ER_RenderingObject;
	class Mesh;
	class Camera;

	namespace RenderToLightProbeMaterial_CBufferData {
		struct RenderToLightProbeCB
		{
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX ViewProjection;
			XMMATRIX World;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
			float UseGlobalDiffuseProbe;
		};
	}
	class ER_RenderToLightProbeMaterial : public ER_Material
	{
	public:
		ER_RenderToLightProbeMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_RenderToLightProbeMaterial();

		void PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, Camera* cubemapCamera);
		virtual void CreateVertexBuffer(const Mesh& mesh, ID3D11Buffer** vertexBuffer) override;
		virtual int VertexSize() override;

		ConstantBuffer<RenderToLightProbeMaterial_CBufferData::RenderToLightProbeCB> mConstantBuffer;
	};
}