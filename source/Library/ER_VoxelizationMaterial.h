#pragma once
#include "ER_Material.h"

namespace Rendering
{
	class RenderingObject;
}
namespace Library
{
	class Mesh;
	class Camera;

	namespace VoxelizationMaterial_CBufferData {
		struct VoxelizationCB
		{
			XMMATRIX World;
			XMMATRIX ViewProjection;
			XMMATRIX ShadowMatrix;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 VoxelCameraPos;
			float VoxelTextureDimension;
			float WorldVoxelScale;
		};
	}
	class ER_VoxelizationMaterial : public ER_Material
	{
	public:
		ER_VoxelizationMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_VoxelizationMaterial();

		void PrepareForRendering(ER_MaterialSystems neededSystems, Rendering::RenderingObject* aObj, int meshIndex, float voxelScale, float voxelTexSize, const XMFLOAT4& voxelCameraPos);
		virtual void CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer) override;
		virtual int VertexSize() override;

		ConstantBuffer<VoxelizationMaterial_CBufferData::VoxelizationCB> mConstantBuffer;
	};
}