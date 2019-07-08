#pragma once

#include "Common.h"
#include "SpatialCell.h"
#include "RenderableOBB.h"
#include "RenderableAABB.h"
#include "CollisionDetectionTypes.h"


namespace Library
{
	class SpatialElement 
	{
	public :
		SpatialElement(int id);
		~SpatialElement();


		std::pair<XMFLOAT3&, XMFLOAT3&> GetBoundsAABB() { return std::make_pair(std::ref(mMinBounds), std::ref(mMaxBounds)); };
		void SetBoundsAABB(XMFLOAT3& min, XMFLOAT3& max) { mMinBounds = min; mMaxBounds = max; };
		
		int GetID() { return mId; };


		void SetPosition(XMFLOAT3& pos) { mPosition = pos; };
		XMFLOAT3& GetPosition() { return mPosition; };

		float GetRadius() { return mRadius; };
		void SetRadius(float val) { mRadius = val; };

		void SetNextSpatialElement(SpatialElement* next) { mNextSpatialElement = next; };
		SpatialElement* GetNextSpatialElement() { return mNextSpatialElement; };
		bool CheckCollision(SpatialElement* pObj);

		void IsColliding(bool val) { mIsColliding = val; };
		bool IsColliding() { return mIsColliding; }

		void DoSphereCheckFirst(bool val) { mDoSphereCheckFirst = val; };

		RenderableOBB* OBB;
		RenderableAABB* AABB;
		CollisionDetectionTypes CollisionType = CollisionDetectionTypes::AABBvsAABB;

	private:


		bool CheckCollisionAABB(SpatialElement* obj);
		bool CheckCollisionOBB(SpatialElement* pObj);
		bool CheckCollisionSphere(SpatialElement * pObj);

		int mId;
		SpatialElement* mNextSpatialElement;

		XMFLOAT3 mMinBounds;
		XMFLOAT3 mMaxBounds;
		XMFLOAT3 mPosition;
		float mRadius;
		bool mIsColliding = false;
		bool mDoSphereCheckFirst = false;


	};
}