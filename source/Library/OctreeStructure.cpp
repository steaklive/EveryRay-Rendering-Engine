#include "OctreeStructure.h"
#include "RenderableAABB.h"
#include "MatrixHelper.h"

namespace Library 
{
	// Octree
	OctreeStructure::OctreeStructure(const XMFLOAT3& center, float pHalfWidth):
		mHalfWidth(pHalfWidth)
	{
		mRootNode = new OctreeNode(nullptr, center, pHalfWidth, 0);
	}

	OctreeStructure:: ~OctreeStructure()
	{
		if (mRootNode)
		{
			delete mRootNode;
			mRootNode = nullptr;
		}
	}

	void OctreeStructure::AddObjects(const std::vector<SpatialElement*>& pObjects)
	{
		float minDiameter = 1.0e38f;
		float curCellSize = 2.0f * mHalfWidth;
		float curDiameter;
		int curDepth = 0;
		int curDivisions = 1;

		std::vector<SpatialElement*>::const_iterator iter_object;
		for (iter_object = pObjects.begin(); iter_object != pObjects.end(); iter_object++)
		{
			curDiameter = 2*(*iter_object)->GetRadius();

			if (curDiameter < minDiameter)
			{
				// Found a smaller diametr
				minDiameter = curDiameter;
			}
		}

		minDiameter *= mMaxObjectNodeRatio;


		// Calculate depth
		curCellSize = 2.0f * mHalfWidth;

		while (minDiameter <= curCellSize)
		{
			if (mMaxDepth == curDepth)
			{
				break;
			}

			curDivisions *= 2;
			curCellSize = 2.0f * mHalfWidth / static_cast<float>(curDivisions);

			curDepth++;
		}
 
		mRootNode->Free();

		// Pre-allocate the tree
		mRootNode->Preallocate(curDepth);


		// add elements into the tree
		for (iter_object = pObjects.begin(); iter_object != pObjects.end(); iter_object++)
		{
			mRootNode->AddObject((*iter_object));
			mObjects.push_back((*iter_object));
		}

	}

	void OctreeStructure::Update()
	{

		auto startTimerRebuildUpdate = std::chrono::high_resolution_clock::now();
		// rebuilding octree every frame
		{
			
			mRootNode->Rebuild();

			std::vector<SpatialElement*>::iterator iter_object;
			for (iter_object = mObjects.begin(); iter_object != mObjects.end(); iter_object++)
			{
				mRootNode->AddObject((*iter_object));
			}
		}
		auto endTimerRebuildUpdate = std::chrono::high_resolution_clock::now();
		mElapsedRebuildUpdate = endTimerRebuildUpdate - startTimerRebuildUpdate;


		std::vector<OctreeNode*> ancestors;


		auto startTimerCollisionUpdate = std::chrono::high_resolution_clock::now();

		mRootNode->CheckCollisions(ancestors);

		auto endTimerCollisionUpdate = std::chrono::high_resolution_clock::now();
		mElapsedTimeCollisionUpdate = endTimerCollisionUpdate - startTimerCollisionUpdate;

	}
	
	/////////////////////////////////////////////////////////////////////////////////////////////
	
	// Octree Node
	OctreeNode::OctreeNode(OctreeNode* pParent, const XMFLOAT3& center, float pHalfWidth, int pDepth) :
		mCenter(center),
		mHalfWidth(pHalfWidth),
		mDepth(pDepth),
		mParentNode(pParent),
		mObjectCount(0)
	{
		for (int i = 0; i < 8; i++)
		{
			mChildNodes[i] = nullptr;
		}
	}

	OctreeNode::~OctreeNode()
	{
		for (int i = 0; i < 8; i++)
		{
			if (mChildNodes[i])
			{
				delete(mChildNodes[i]);
				mChildNodes[i] = nullptr;
			}
		}
	}

	void OctreeNode::CheckCollisions(std::vector<OctreeNode*>& refAncestors)
	{
		// Push this node
		refAncestors.push_back(this);

		// Check collisions between all objects
		std::vector<OctreeNode*>::iterator iter_node;
		for (iter_node = refAncestors.begin(); iter_node != refAncestors.end(); iter_node++)
		{
			(*iter_node)->CheckMutualCollisions(mObjects);
		}

		// Recursively visit children
		for (int i = 0; i < 8; i++)
		{
			if (mChildNodes[i])
			{
				mChildNodes[i]->CheckCollisions(refAncestors);
			}
		}

		// Pop this node
		refAncestors.pop_back();
	}

	void OctreeNode::CheckMutualCollisions(SpatialElement* pObject)
	{
		SpatialElement* pIter1 = pObject;
		SpatialElement* pIter2;

		while (pIter1)
		{
			pIter2 = mObjects;

			while (pIter2)
			{

				if (pIter1 == pIter2)
				{
					break;
				}


				if (pIter1->CheckCollision(pIter2))
				{
					
					pIter1->IsColliding(true);
					pIter2->IsColliding(true);
				}


				

				pIter2 = pIter2->GetNextSpatialElement();
			}

			pIter1 = pIter1->GetNextSpatialElement();
		}
	}

	inline bool	OctreeNode::IsLeafNode()
	{
		return(
			!mChildNodes[0] && !mChildNodes[1] && !mChildNodes[2] &&
			!mChildNodes[3] && !mChildNodes[4] && !mChildNodes[5] &&
			!mChildNodes[6] && !mChildNodes[7]
			);
	}
	
	void OctreeNode::Free()
	{
		for (int i = 0; i < 8; i++)
		{
			if (mChildNodes[i])
			{
				delete(mChildNodes[i]);
				mChildNodes[i] = nullptr;
			}
		}
	}
	
	void OctreeNode::Rebuild()
	{
		for (int i = 0; i < 8; i++)
		{
			if (mChildNodes[i])
			{
				mChildNodes[i]->Rebuild();
			}

			mObjects = nullptr;
		}
	}

	void OctreeNode::Preallocate(int pDepth)
	{
		// Check conditions
		if (pDepth && IsLeafNode())
		{
			XMFLOAT3 vec3NewCenter;

			float step = mHalfWidth * 0.5f;

			for (int i = 0; i < 8; i++)
			{
				vec3NewCenter = mCenter;

				if (i & 1)
				{
					vec3NewCenter.x += step;
				}
				else
				{
					vec3NewCenter.x -= step;
				}


				if (i & 2)
				{
					vec3NewCenter.y += step;
				}
				else
				{
					vec3NewCenter.y -= step;
				}


				if (i & 4)
				{
					vec3NewCenter.z += step;
				}
				else
				{
					vec3NewCenter.z -= step;
				}


				// Allocate node
				mChildNodes[i] = new OctreeNode(this, vec3NewCenter, step, mDepth + 1);

				// Build node recursively
				mChildNodes[i]->Preallocate(pDepth - 1);
			}
		}
	}

	void OctreeNode::AddObject(SpatialElement* pObject)
	{
		int curIndex = 0;
		int curPosition = 0;
		bool isStraddle = false;

		float curRadius = pObject->GetRadius();
		XMFLOAT3 vec3Center = pObject->GetPosition();

		float centerNode[3] = {mCenter.x, mCenter.y, mCenter.z};
		float centerObj[3] = { vec3Center.x, vec3Center.y, vec3Center.z };


		for (int index = 0; index < 3; index++)
		{
			if (centerNode[index] < centerObj[index])
			{
				if (centerNode[index] > centerObj[index] - curRadius)
				{
					isStraddle = true;
					break;
				}
				else
				{
					curPosition |= (1 << index);
				}
			}
			else
			{
				if (centerNode[index] < centerObj[index] + curRadius)
				{
					isStraddle = 1;
					break;
				}
			}

		}


		if (!isStraddle && mChildNodes[curPosition])
		{
			// Contained in existing child node
			mChildNodes[curPosition]->AddObject(pObject);
		}
		else
		{

			// Straddling or no child node available
			pObject->SetNextSpatialElement(mObjects);
			mObjects = pObject;

			mObjectCount++;
		}

	}

	void OctreeNode::AddObjects(SpatialElement* pObjectList, int iObjectCount)
	{
		SpatialElement* pIter;
		SpatialElement* pObject;

		// if we can stop splitting
		if (iObjectCount < mMinSplitCount)
		{
			mObjects = pObjectList;

			pIter = mObjects;
			while (pIter)
			{
				pIter = pIter->GetNextSpatialElement();
			}


			mObjectCount = iObjectCount;

			return;
		}



		int index = 0;
		SpatialElement* apChildObjects[8];
		int aiChildCounts[8];

		for (index = 0; index < 8; index++)
		{
			apChildObjects[index] = nullptr;
			aiChildCounts[index] = 0;
		}


		pIter = pObjectList;
		while (pIter)
		{
			pObject = pIter->GetNextSpatialElement();

			int curPos = 0;
			bool isStraddle = false;

			float f32Radius = pIter->GetRadius();
			XMFLOAT3 vec3Center = pIter->GetPosition();

			float centerNode[3] = { mCenter.x, mCenter.y, mCenter.z };
			float centerObj[3] = { vec3Center.x, vec3Center.y, vec3Center.z };

			for (index = 0; index < 3; index++)
			{
				if (centerNode[index] < centerObj[index])
				{
					if (centerNode[index] > centerObj[index] - f32Radius)
					{
						isStraddle = false;
						break;
					}
					else
					{
						curPos |= (1 << index);
					}
				}
				else
				{
					if (centerNode[index] < centerObj[index] + f32Radius)
					{
						
						isStraddle = true;
						break;
					}
				}
			}


			if (!isStraddle && mChildNodes[curPos])
			{
				// Contained in existing child node
				pIter->SetNextSpatialElement(apChildObjects[curPos]);
				apChildObjects[curPos] = pIter;

				aiChildCounts[curPos]++;

			}
			else
			{


				// Straddle or no child node available
				pIter->SetNextSpatialElement(mObjects);
				mObjects = pIter;

				mObjectCount++;
			}


			pIter = pObject;
		}


		for (index = 0; index < 8; index++)
		{
			// send to children
			if (mChildNodes[index] && aiChildCounts[index])
			{
				mChildNodes[index]->AddObjects(apChildObjects[index], aiChildCounts[index]);
			}
		}
	}


	// Cells visualization
	void OctreeNode::InitializeCellAABBs(Game& game, Camera& camera)
	{
		for (size_t i = 0; i < 8; i++)
		{
			if (IsLeafNode()) break;
			mChildNodes[i]->InitializeCellAABBs(game, camera);
		}
		mCellAABB = new RenderableAABB(game, XMFLOAT4{ 0.0, 0.0, 0.0, 1.0 });

		std::vector<XMFLOAT3> volumeAABB;
		volumeAABB.push_back(XMFLOAT3(mHalfWidth + mCenter.x, mHalfWidth + mCenter.y, mHalfWidth + mCenter.z));
		volumeAABB.push_back(XMFLOAT3(-mHalfWidth + mCenter.x, -mHalfWidth + mCenter.y, -mHalfWidth + mCenter.z));
		mCellAABB->InitializeGeometry(volumeAABB);
	}
	void OctreeNode::UpdateCellsAABBs(const GameTime& gameTime)
	{
		for (size_t i = 0; i < 8; i++)
		{
			if (IsLeafNode()) break;
			mChildNodes[i]->UpdateCellsAABBs(gameTime);
		}
		//mCellAABB->Update(); TODO
	}
	void OctreeNode::DrawCells(const GameTime& gameTime)
	{
		for (size_t i = 0; i < 8; i++)
		{
			if (IsLeafNode()) break;
			mChildNodes[i]->DrawCells(gameTime);
		}
		DrawCellAABB(gameTime);
		
	}
	void OctreeNode::DrawCellAABB(const GameTime& gameTime)
	{
		mCellAABB->Draw();
	}
}
