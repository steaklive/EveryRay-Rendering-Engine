#include "SpatialElement.h"
#include "MatrixHelper.h"

namespace Library
{
	SpatialElement::SpatialElement(int id): mId(id)
	{
	}

	SpatialElement::~SpatialElement()
	{
		DeleteObject(OBB);
		DeleteObject(AABB);
		//DeleteObject(mCell);
		//DeleteObject(mNextSpatialElement);
	}
	
	bool SpatialElement::CheckCollision(SpatialElement* pObj)
	{

		if (mDoSphereCheckFirst && !CheckCollisionSphere(pObj)) return false;

		switch (CollisionType)
		{
		case CollisionDetectionTypes::AABBvsAABB:
			return CheckCollisionAABB(pObj);
			break;
		case CollisionDetectionTypes::OBBvsOBB:
			return CheckCollisionOBB(pObj);
			break;


		default:
			return CheckCollisionAABB(pObj);
		}
	}

	bool SpatialElement::CheckCollisionAABB(SpatialElement* pObj)
	{

		XMFLOAT3 minBounds2 = pObj->GetBoundsAABB().first;
		XMFLOAT3 maxBounds2 = pObj->GetBoundsAABB().second;

		return
			(mMinBounds.x <= maxBounds2.x && mMaxBounds.x >= minBounds2.x) &&
			(mMinBounds.y <= maxBounds2.y && mMaxBounds.y >= minBounds2.y) &&
			(mMinBounds.z <= maxBounds2.z && mMaxBounds.z >= minBounds2.z);
	}

	bool SpatialElement::CheckCollisionOBB(SpatialElement* pObj)
	{

		//http://www.3dkingdoms.com/weekly/bbox.cpp
		XMFLOAT3& SizeA = this->OBB->GetExtents();
		XMFLOAT3& SizeB = pObj->OBB->GetExtents();

		
		XMFLOAT4X4 R;       // Rotation from B to A
		XMFLOAT4X4 AR;      // absolute values of R matrix, to use with box extents

		float ExtentA, ExtentB, Separation;
		int i, k;
		XMFLOAT3 tempDot3;


		//// Calculate B to A rotation matrix

		// 0 0
		//XMStoreFloat3(&tempDot3, XMVector3Dot(temp, temp));
		R.m[0][0] = XMVector3Dot(this->OBB->GetTransformationMatrix().r[0], pObj->OBB->GetTransformationMatrix().r[0]).m128_f32[0];
		AR.m[0][0] = fabs(R.m[0][0]) + 0.0001f;

		// 0 1
		//XMStoreFloat3(&tempDot3, XMVector3Dot(temp, temp));
		R.m[0][1] = XMVector3Dot(this->OBB->GetTransformationMatrix().r[0], pObj->OBB->GetTransformationMatrix().r[1]).m128_f32[0];
		AR.m[0][1] = fabs(R.m[0][1]) + 0.0001f;

		// 0 2
		//XMStoreFloat3(&tempDot3, XMVector3Dot(temp, temp));
		R.m[0][2] = XMVector3Dot(this->OBB->GetTransformationMatrix().r[0], pObj->OBB->GetTransformationMatrix().r[2]).m128_f32[0];
		AR.m[0][2] = fabs(R.m[0][2]) + 0.0001f;

		// 1 0
		//XMStoreFloat3(&tempDot3, XMVector3Dot(temp, temp));
		R.m[1][0] = XMVector3Dot(this->OBB->GetTransformationMatrix().r[1], pObj->OBB->GetTransformationMatrix().r[0]).m128_f32[0];
		AR.m[1][0] = fabs(R.m[1][0]) + 0.0001f;

		// 1 1
		//XMStoreFloat3(&tempDot3, XMVector3Dot(temp, temp));
		R.m[1][1] = XMVector3Dot(this->OBB->GetTransformationMatrix().r[1], pObj->OBB->GetTransformationMatrix().r[1]).m128_f32[0];
		AR.m[1][1] = fabs(R.m[1][1]) + 0.0001f;

		// 1 2
		//XMStoreFloat3(&tempDot3, XMVector3Dot(temp, temp));
		R.m[1][2] = XMVector3Dot(this->OBB->GetTransformationMatrix().r[1], pObj->OBB->GetTransformationMatrix().r[2]).m128_f32[0];
		AR.m[1][2] = fabs(R.m[1][2]) + 0.0001f;


		// 2 0
		//XMStoreFloat3(&tempDot3, XMVector3Dot(temp, temp));
		R.m[2][0] = XMVector3Dot(this->OBB->GetTransformationMatrix().r[2], pObj->OBB->GetTransformationMatrix().r[0]).m128_f32[0];
		AR.m[2][0] = fabs(R.m[2][0]) + 0.0001f;

		// 2 1
		//XMStoreFloat3(&tempDot3, XMVector3Dot(temp, temp));
		R.m[2][1] = XMVector3Dot(this->OBB->GetTransformationMatrix().r[2], pObj->OBB->GetTransformationMatrix().r[1]).m128_f32[0];
		AR.m[2][1] = fabs(R.m[2][1]) + 0.0001f;

		// 2 2
		//XMStoreFloat3(&tempDot3, XMVector3Dot(temp, temp));
		R.m[2][2] = XMVector3Dot(this->OBB->GetTransformationMatrix().r[2], pObj->OBB->GetTransformationMatrix().r[2]).m128_f32[0];
		AR.m[2][2] = fabs(R.m[2][2]) + 0.0001f;

		// Vector separating the centers of Box B and of Box A	
		XMFLOAT3 vSepWS = XMFLOAT3
		(
			pObj->OBB->GetCenter().x - this->OBB->GetCenter().x,
			pObj->OBB->GetCenter().y - this->OBB->GetCenter().y,
			pObj->OBB->GetCenter().z - this->OBB->GetCenter().z
		);


		// Rotated into Box A's coordinates
		XMFLOAT3 vSepA = XMFLOAT3
		(
			XMVector3Dot(XMLoadFloat3(&vSepWS), this->OBB->GetTransformationMatrix().r[0]).m128_f32[0],
			XMVector3Dot(XMLoadFloat3(&vSepWS), this->OBB->GetTransformationMatrix().r[1]).m128_f32[0],
			XMVector3Dot(XMLoadFloat3(&vSepWS), this->OBB->GetTransformationMatrix().r[2]).m128_f32[0]

		);

		/////////////////

		// A's x axis
		ExtentA = SizeA.x;

		//XMStoreFloat3(&tempDot3, XMVector3Dot(XMLoadFloat3(&SizeB), XMVECTOR{ AR.m[0][0], AR.m[0][1], AR.m[0][2] }));
		ExtentB = XMVector3Dot(XMLoadFloat3(&SizeB), XMVECTOR{ AR.m[0][0], AR.m[0][1], AR.m[0][2] }).m128_f32[0];

		Separation = fabs(vSepA.x);

		if (Separation > ExtentA + ExtentB)
			return false;

		// A's y axis
		ExtentA = SizeA.y;

		//XMStoreFloat3(&tempDot3, XMVector3Dot(XMLoadFloat3(&SizeB), XMVECTOR{ AR.m[1][0], AR.m[1][1], AR.m[1][2] }));
		ExtentB = XMVector3Dot(XMLoadFloat3(&SizeB), XMVECTOR{ AR.m[1][0], AR.m[1][1], AR.m[1][2] }).m128_f32[0];

		Separation = fabs(vSepA.y);

		if (Separation > ExtentA + ExtentB)
			return false;

		// A's z axis
		ExtentA = SizeA.z;

		//XMStoreFloat3(&tempDot3, XMVector3Dot(XMLoadFloat3(&SizeB), XMVECTOR{ AR.m[2][0], AR.m[2][1], AR.m[2][2] }));
		ExtentB = XMVector3Dot(XMLoadFloat3(&SizeB), XMVECTOR{ AR.m[2][0], AR.m[2][1], AR.m[2][2] }).m128_f32[0];

		Separation = fabs(vSepA.z);

		if (Separation > ExtentA + ExtentB)
			return false;

		/////////////////

		// B's x axis
		//XMStoreFloat3(&tempDot3, XMVector3Dot(XMLoadFloat3(&SizeA), XMVECTOR{ AR.m[0][0], AR.m[1][0], AR.m[2][0] }));

		ExtentA = XMVector3Dot(XMLoadFloat3(&SizeA), XMVECTOR{ AR.m[0][0], AR.m[1][0], AR.m[2][0] }).m128_f32[0];
		ExtentB = SizeB.x;

		//XMStoreFloat3(&tempDot3, XMVector3Dot(XMLoadFloat3(&vSepA), XMVECTOR{ R.m[0][0], R.m[1][0], R.m[2][0] }));
		Separation = fabs(XMVector3Dot(XMLoadFloat3(&vSepA), XMVECTOR{ R.m[0][0], R.m[1][0], R.m[2][0] }).m128_f32[0]);

		if (Separation > ExtentA + ExtentB)
			return false;

		// B's y axis
		//XMStoreFloat3(&tempDot3, XMVector3Dot(XMLoadFloat3(&SizeA), XMVECTOR{ AR.m[0][1], AR.m[1][1], AR.m[2][1] }));

		ExtentA = XMVector3Dot(XMLoadFloat3(&SizeA), XMVECTOR{ AR.m[0][1], AR.m[1][1], AR.m[2][1] }).m128_f32[0];
		ExtentB = SizeB.y;

		//XMStoreFloat3(&tempDot3, XMVector3Dot(XMLoadFloat3(&vSepA), XMVECTOR{ R.m[0][1], R.m[1][1], R.m[2][1] }));
		Separation = fabs(XMVector3Dot(XMLoadFloat3(&vSepA), XMVECTOR{ R.m[0][1], R.m[1][1], R.m[2][1] }).m128_f32[0]);

		if (Separation > ExtentA + ExtentB)
			return false;

		// B's z axis
		//XMStoreFloat3(&tempDot3, XMVector3Dot(XMLoadFloat3(&SizeA), XMVECTOR{ AR.m[0][2], AR.m[1][2], AR.m[2][2] }));

		ExtentA = XMVector3Dot(XMLoadFloat3(&SizeA), XMVECTOR{ AR.m[0][2], AR.m[1][2], AR.m[2][2] }).m128_f32[0];
		ExtentB = SizeB.z;

		//XMStoreFloat3(&tempDot3, XMVector3Dot(XMLoadFloat3(&vSepA), XMVECTOR{ R.m[0][2], R.m[1][2], R.m[2][2] }));
		Separation = fabs(XMVector3Dot(XMLoadFloat3(&vSepA), XMVECTOR{ R.m[0][2], R.m[1][2], R.m[2][2] }).m128_f32[0]);

		if (Separation > ExtentA + ExtentB)
			return false;

		

		// a[0] b[0] cross
		ExtentA = SizeA.y * AR.m[2][0] + SizeA.z * AR.m[1][0];
		ExtentB = SizeB.y * AR.m[0][2] + SizeB.z * AR.m[0][1];
		Separation = fabs(vSepA.z * R.m[1][0] - vSepA.y * R.m[2][0]);

		if (Separation > ExtentA + ExtentB)
			return false;

		// a[0] b[1] cross
		ExtentA = SizeA.y * AR.m[2][1] + SizeA.z * AR.m[1][1];
		ExtentB = SizeB.z * AR.m[0][0] + SizeB.x * AR.m[0][2];
		Separation = fabs(vSepA.z * R.m[1][1] - vSepA.y * R.m[2][1]);

		if (Separation > ExtentA + ExtentB)
			return false;


		// a[0] b[2] cross
		ExtentA = SizeA.y * AR.m[2][2] + SizeA.z * AR.m[1][2];
		ExtentB = SizeB.x * AR.m[0][1] + SizeB.y * AR.m[0][0];
		Separation = fabs(vSepA.z * R.m[1][2] - vSepA.y * R.m[2][2]);

		if (Separation > ExtentA + ExtentB)
			return false;

		// a[1] b[0] cross
		ExtentA = SizeA.z * AR.m[0][0] + SizeA.x * AR.m[2][0];
		ExtentB = SizeB.y * AR.m[1][2] + SizeB.z * AR.m[1][1];
		Separation = fabs(vSepA.x * R.m[2][0] - vSepA.z * R.m[0][0]);

		if (Separation > ExtentA + ExtentB)
			return false;

		// a[1] b[1] cross
		ExtentA = SizeA.z * AR.m[0][1] + SizeA.x * AR.m[2][1];
		ExtentB = SizeB.z * AR.m[1][0] + SizeB.x * AR.m[1][2];
		Separation = fabs(vSepA.x * R.m[2][1] - vSepA.z * R.m[0][1]);

		if (Separation > ExtentA + ExtentB)
			return false;

		// a[1] b[2] cross
		ExtentA = SizeA.z * AR.m[0][2] + SizeA.x * AR.m[2][2];
		ExtentB = SizeB.x * AR.m[1][1] + SizeB.y * AR.m[1][0];
		Separation = fabs(vSepA.x * R.m[2][2] - vSepA.z * R.m[0][2]);

		if (Separation > ExtentA + ExtentB)
			return false;


		// a[2] b[0] cross
		ExtentA = SizeA.x * AR.m[1][0] + SizeA.y * AR.m[0][0];
		ExtentB = SizeB.y * AR.m[2][2] + SizeB.z * AR.m[2][1];
		Separation = fabs(vSepA.y * R.m[0][0] - vSepA.x * R.m[1][0]);

		if (Separation > ExtentA + ExtentB)
			return false;


		// a[2] b[1] cross
		ExtentA = SizeA.x * AR.m[1][1] + SizeA.y * AR.m[0][1];
		ExtentB = SizeB.z * AR.m[2][0] + SizeB.x * AR.m[2][2];
		Separation = fabs(vSepA.y * R.m[0][1] - vSepA.x * R.m[1][1]);

		if (Separation > ExtentA + ExtentB)
			return false;


		// a[2] b[2] cross
		ExtentA = SizeA.x * AR.m[1][2] + SizeA.y * AR.m[0][2];
		ExtentB = SizeB.x * AR.m[2][1] + SizeB.y * AR.m[2][0];
		Separation = fabs(vSepA.y * R.m[0][2] - vSepA.x * R.m[1][2]);

		if (Separation > ExtentA + ExtentB)
			return false;


		// No separating axis found, the boxes overlap	
		return true;
	}
	
	bool SpatialElement::CheckCollisionSphere(SpatialElement* pObj)
	{
		float distance = sqrt(
			(mPosition.x - pObj->GetPosition().x) * (mPosition.x - pObj->GetPosition().x) +
			(mPosition.y - pObj->GetPosition().y) * (mPosition.y - pObj->GetPosition().y) +
			(mPosition.z - pObj->GetPosition().z) * (mPosition.z - pObj->GetPosition().z) 		
		);

		return distance < (mRadius + pObj->GetRadius());
	}
}