#pragma once
#include "Common.h"

namespace Library
{
	class Game;
	class ER_BasicColorMaterial;
	class ER_GPUBuffer;

	class RenderableAABB
	{
	public:
		RenderableAABB(Game& game, const XMFLOAT4& color);
		~RenderableAABB();

		void InitializeGeometry(const std::vector<XMFLOAT3>& aabb);
		void Update(ER_AABB& aabb);
		void Draw();
		void SetColor(const XMFLOAT4& color);
		void SetAABB(const std::vector<XMFLOAT3>& aabb);

	private:
		void UpdateVertices();

		Game& mGame;

		//static const XMVECTORF32 DefaultColor;
		//static const UINT AABBVertexCount;
		//static const UINT AABBPrimitiveCount;
		//static const UINT AABBIndicesPerPrimitive;
		//static const UINT AABBIndexCount;
		//static const USHORT AABBIndices[];

		ER_GPUBuffer* mVertexBuffer;
		ER_GPUBuffer* mIndexBuffer;
		ER_BasicColorMaterial* mMaterial;
		
		XMFLOAT4 mColor;

		XMFLOAT4 mVertices[8];
		ER_AABB mAABB;
	};
}