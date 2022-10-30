#pragma once
#include "Common.h"
#include "RHI/ER_RHI.h"
namespace EveryRay_Core
{
	class ER_Core;
	class ER_BasicColorMaterial;

	class ER_RenderableAABB
	{
	public:
		ER_RenderableAABB(ER_Core& game, const XMFLOAT4& color);
		~ER_RenderableAABB();

		void InitializeGeometry(const std::vector<XMFLOAT3>& aabb);
		void Update(ER_AABB& aabb);
		void Draw(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs);
		void SetColor(const XMFLOAT4& color);
		void SetAABB(const std::vector<XMFLOAT3>& aabb);

	private:
		void UpdateVertices();

		ER_Core& mCore;

		ER_RHI_GPUBuffer* mVertexBuffer = nullptr;
		ER_RHI_GPUBuffer* mIndexBuffer = nullptr;
		ER_BasicColorMaterial* mMaterial = nullptr;
		
		XMFLOAT4 mColor;

		XMFLOAT4 mVertices[8];
		ER_AABB mAABB;
	};
}