#pragma once

#include "DrawableGameComponent.h"
#include "MatrixHelper.h"
namespace Library
{
	class ER_BasicColorMaterial;
	class Pass;

	class RenderableOBB : public DrawableGameComponent
	{
		RTTI_DECLARATIONS(RenderableOBB, DrawableGameComponent)

	public:
		RenderableOBB(Game& game, Camera& camera);
		RenderableOBB(Game& game, Camera& camera, const XMFLOAT4& color);
		~RenderableOBB();


		void InitializeGeometry(const std::vector<XMFLOAT3>& aabb, XMMATRIX matrix);
		void SetPosition(const XMFLOAT3& position);
		void SetColor(const XMFLOAT4& color);
		void UpdateColor(const XMFLOAT4& color);
		void SetRotationMatrix(const XMMATRIX& rotationMat);
		void SetOBB(const std::vector<XMFLOAT3>& aabb);

		XMFLOAT3& GetExtents() { return mExtents; };
		XMMATRIX& GetTransformationMatrix() { return mTranformationMatrix; };
		std::vector<XMFLOAT3>& GetRotationRowMatrix() { return MatrixHelper::GetRows(mTranformationMatrix); };
		XMFLOAT3 GetCenter() { return mCenter; };


		const std::vector<XMFLOAT3>& GetOBB() { return mModifiedOBB; };

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		bool isColliding = false;

	private:
		RenderableOBB();
		RenderableOBB(const RenderableOBB& rhs);
		RenderableOBB& operator=(const RenderableOBB& rhs);

		void InitializeVertexBuffer(const std::vector<XMFLOAT3>& aabb);
		void InitializeIndexBuffer();

		void TransformOBB(const GameTime& gameTime);

		static const XMVECTORF32 DefaultColor;
		static const UINT OBBVertexCount;
		static const UINT OBBPrimitiveCount;
		static const UINT OBBIndicesPerPrimitive;
		static const UINT OBBIndexCount;
		static const USHORT OBBIndices[];

		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mIndexBuffer;
		ER_BasicColorMaterial* mMaterial;

		XMFLOAT4 mColor;
		XMFLOAT3 mPosition;
		XMFLOAT3 mDirection;
		XMFLOAT3 mUp;
		XMFLOAT3 mRight;

		XMFLOAT4X4 mWorldMatrix;

		XMMATRIX mTransformMatrix;

		XMFLOAT3 mExtents;
		XMMATRIX mTranformationMatrix;
		XMFLOAT3 mCenter;
		XMFLOAT3 mCenterConstant;

		std::vector<XMFLOAT3> mVertices;
		std::vector<XMFLOAT3> mCornersAABB;

		const std::vector<XMFLOAT3>* mOBB;

		std::vector<XMFLOAT3> mModifiedOBB;
		std::vector<XMFLOAT3> mModifiedOBB2;
	};
}