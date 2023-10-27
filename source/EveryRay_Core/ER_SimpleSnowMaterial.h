#pragma once
#include "ER_Material.h"

#define SNOW_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define SNOW_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

namespace EveryRay_Core
{
	class ER_RenderingObject;
	class ER_Mesh;
	class ER_Camera;

	namespace SimpleSnowMaterial_CBufferData {
		struct ER_ALIGN_GPU_BUFFER SnowCB //must match with SimpleSnow.hlsl
		{
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX ViewProjection;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
			XMFLOAT4 SnowDepthLevel;
		};
	}
	class ER_SimpleSnowMaterial : public ER_Material
	{
	public:
		ER_SimpleSnowMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_SimpleSnowMaterial();

		virtual void PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs) override;
		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) override;
		virtual int VertexSize() override;

		ER_RHI_GPUConstantBuffer<SimpleSnowMaterial_CBufferData::SnowCB> mConstantBuffer;

		float mSnowLevel = 0.65f;
		float mSnowDepth = 0.15f;
		float mSnowUVScale = 0.2f;
	};
}
