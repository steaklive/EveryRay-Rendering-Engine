#pragma once

#include "Common.h"
#include "SpatialElement.h"
#include "SpatialCell.h"
#include "RenderableAABB.h"
#include <chrono>

namespace Library
{
	class OctreeNode;

	


	class OctreeStructure 
	{
	public:
		OctreeStructure(const XMFLOAT3& center, float pHalfWidth);
		~OctreeStructure();

		void AddObjects(const std::vector<SpatialElement*>& pObjects);
		void Update();
		float GetCollisionUpdateTime() { return mElapsedTimeCollisionUpdate.count(); }
		float GetRebuildUpdateTime() { return mElapsedRebuildUpdate.count(); }

		OctreeNode* GetRootNode() { return mRootNode; };

	private:
		const int mMaxDepth = 4;
		const float mMaxObjectNodeRatio = 4.0;
		float mHalfWidth;

		OctreeNode* mRootNode;
		std::vector<SpatialElement*> mObjects;

		std::chrono::duration<double> mElapsedTimeCollisionUpdate;
		std::chrono::duration<double> mElapsedRebuildUpdate;
	};

	class OctreeNode : public SpatialCell {
		friend class OctreeStructure;

	public:
		OctreeNode(OctreeNode* pParentNode, const XMFLOAT3& center, float pHalfWidth, int pDepth);
		~OctreeNode();

		void Rebuild();

		void Preallocate(int pDepth);

		//bool IsObjectInside();
		bool IsLeafNode();

		void AddObjects(SpatialElement* pObjects, int pObjCount);
		void AddObject(SpatialElement* pObject);

		void CheckCollisions(std::vector<OctreeNode*>& pAncestors);
		void CheckMutualCollisions(SpatialElement * pObject);
		void Free();
		void InitializeCellAABBs(Game& game, Camera& camera);
		void DrawCellAABB(const GameTime& gameTime);
		void DrawCells(const GameTime& gameTime);
		void UpdateCellsAABBs(const GameTime& gameTime);


	private:

		const int mMinSplitCount = 32;
		OctreeNode* mParentNode;
		OctreeNode* mChildNodes[8];
		SpatialElement* mObjects;

		int mObjectCount;
		XMFLOAT3 mCenter;
		float mHalfWidth;
		int mDepth;


		RenderableAABB* mCellAABB;



	};

}