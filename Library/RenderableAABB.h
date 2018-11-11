#pragma once

#include "DrawableGameComponent.h"

namespace Library
{
	class BasicMaterial;
	class Pass;

	class RenderableAABB : public DrawableGameComponent
	{
		RTTI_DECLARATIONS(RenderableAABB, DrawableGameComponent)

	public:
		RenderableAABB(Game& game, Camera& camera);
		RenderableAABB(Game& game, Camera& camera, const XMFLOAT4& color);
		~RenderableAABB();


		void InitializeGeometry(const std::vector<XMFLOAT3>& aabb, XMMATRIX matrix);
		void SetPosition(const XMFLOAT3& position);
		void SetColor(XMFLOAT4 color);

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

	private:
		RenderableAABB();
		RenderableAABB(const RenderableAABB& rhs);
		RenderableAABB& operator=(const RenderableAABB& rhs);

		void InitializeVertexBuffer(const std::vector<XMFLOAT3>& aabb);
		void InitializeIndexBuffer();



		static const XMVECTORF32 DefaultColor;
		static const UINT AABBVertexCount;
		static const UINT AABBPrimitiveCount;
		static const UINT AABBIndicesPerPrimitive;
		static const UINT AABBIndexCount;
		static const USHORT AABBIndices[];

		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mIndexBuffer;
		BasicMaterial* mMaterial;
		Pass* mPass;
		ID3D11InputLayout* mInputLayout;

		XMFLOAT4 mColor;
		XMFLOAT3 mPosition;
		XMFLOAT3 mDirection;
		XMFLOAT3 mUp;
		XMFLOAT3 mRight;

		XMFLOAT4X4 mWorldMatrix;
	};
}