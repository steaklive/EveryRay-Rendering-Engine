#pragma once
#include "ER_Material.h"

#define FUR_SHELL_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define FUR_SHELL_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

namespace EveryRay_Core
{
	class ER_RenderingObject;
	class ER_Mesh;
	class ER_Camera;

	namespace FurShellMaterial_CBufferData {
		struct ER_ALIGN_GPU_BUFFER FurShellCB //must match with FurShell.hlsl
		{
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX ViewProjection;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
			XMFLOAT4 FurColor; // xyz - color, w - interpolation with albedo texture
			XMFLOAT4 FurGravityStrength; // xyz - direction, w - strength
			XMFLOAT4 FurLengthCutoffCutoffEndFade; // x - length, y - cutoff ("thickness"), z - cutoff end ("thickness at the ends"), w - edge fade 
			XMFLOAT4 FurMultiplierUVScale; // x - multiplier, y - uv scale
		};
	}
	class ER_FurShellMaterial : public ER_Material
	{
	public:
		ER_FurShellMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false, int currentIndex = 0);
		~ER_FurShellMaterial();

		virtual void PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs) override;
		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) override;
		virtual int VertexSize() override;

		ER_RHI_GPUConstantBuffer<FurShellMaterial_CBufferData::FurShellCB> mConstantBuffer;

		int mCurrentIndex = -1;
	};
}
