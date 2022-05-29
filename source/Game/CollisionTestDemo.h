#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\InstancingMaterial.h"
#include "..\Library\AmbientLightingMaterial.h"

#include "..\Library\ER_Sandbox.h"
#include "..\Library\Frustum.h"

#include "..\Library\DirectionalLight.h"
#include "..\Library\RenderStateHelper.h"

#include <chrono>
#include <thread>
#include <mutex>

using namespace Library;

namespace Library
{
	class Effect;
	class Keyboard;
	class DirectionalLight;
	class ProxyModel;
	class RenderStateHelper;
	class RenderableFrustum;
	class RenderableAABB;
	class OctreeStructure;
	class BruteforceStructure;
	class SpatialElement;
	class Skybox;
}

namespace DirectX
{
	class SpriteBatch;
	class SpriteFont;
}

namespace CollisionTestDemoLightInfo
{
	struct LightData
	{

		DirectionalLight* DirectionalLight = nullptr;
		ProxyModel* ProxyModel = nullptr;

		void Destroy()
		{
			DirectionalLight = nullptr;
			delete DirectionalLight;
		}

	};

}

namespace Rendering
{
	class RenderingObject;
	class AmbientLightingMaterial;
	class InstancingMaterial;

	class CollisionTestDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(CollisionTestDemo, DrawableGameComponent)

	public:
		CollisionTestDemo(Game& game, Camera& camera);
		~CollisionTestDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		virtual void Create() override;
		virtual void Destroy() override;

	private:

		CollisionTestDemo();
		CollisionTestDemo(const CollisionTestDemo& rhs);
		CollisionTestDemo& operator=(const CollisionTestDemo& rhs);

		void UpdateInstancingMaterialVariables(RenderingObject& object, int meshIndex);

		void UpdateDirectionalLight(const GameTime& gameTime);
		void UpdateObjectsTransforms_UnoptimizedDataAccess(const GameTime & gameTime);
		void UpdateObjectsTransforms_OptimizedDataAccess(const GameTime & gameTime);
		void UpdateSpatialElement(std::vector<SpatialElement*>& elements, int startIndex, int endIndex, const GameTime& gameTime);
		void UpdateImGui();
		
		void CalculateDynamicObjectsRandomDistribution();
		void CalculateStaticObjectsRandomDistribution();
		float CalculateObjectRadius(std::vector<XMFLOAT3> vertices);
		void RecalculateAABB(int index, RenderingObject& object, XMMATRIX & mat);
		void VisualizeCollisionDetection();

		std::vector<SpatialElement*> mSpatialElements;
		
		RenderingObject*								mDynamicConvexHullObject;

		RenderingObject*								mDynamicInstancedObject;
		std::vector<XMFLOAT3>							mDynamicObjectInstancesPositions;
		std::vector<XMFLOAT3>							mDynamicObjectInstancesDirections;
		std::vector<std::vector<XMFLOAT3>>				mDynamicMeshVertices;
		std::vector<std::vector<XMFLOAT3>>				mDynamicMeshAABBs;
		std::vector<std::vector<XMFLOAT3>>				mDynamicMeshOBBs;
		std::vector<InstancingMaterial::InstancedData>  mDynamicObjectInstanceData;

		RenderingObject*								mStaticInstancedObject;
		std::vector<XMFLOAT3>							mStaticObjectInstancesPositions;
		std::vector<std::vector<XMFLOAT3>>				mStaticMeshVertices;
		std::vector<std::vector<XMFLOAT3>>				mStaticMeshAABBs;
		std::vector<std::vector<XMFLOAT3>>				mStaticMeshOBBs;
		std::vector<InstancingMaterial::InstancedData>  mStaticObjectInstanceData;
		
		//std::vector<std::thread> mElementUpdateThreads;

		XMFLOAT4X4 mWorldMatrix;
		Skybox* mSkybox;
		Keyboard* mKeyboard;
		RenderableAABB* mVolumeDebugAABB;
		RenderStateHelper mRenderStateHelper;

		OctreeStructure* mOctreeStructure;
		BruteforceStructure* mBruteforceStructure;

		bool mIsCustomFrustumActive = false;
		bool mIsCustomFrustumRotating = false;
		bool mIsCullingEnabled = false;
		bool mIsAABBRendered = false;
		bool mIsOctreeRendered = false;
		bool mIsOBBRendered = false;
		bool mIsDynamic = false;
		bool mIsTransformOptimized = false;
		bool mIsCollisionPrechecked = false;
		bool mRecalculateAABBs = false;
		bool mIsAABBRecalculatedFromConvexHull = false;

		float mSpeed = 0.001f;
		int mNumberOfCollisions = 0;
		int mSpatialHierarchyNumber = 0;
		int mCollisionMethodNumber = 0;

		std::chrono::duration<double> mElapsedTimeStructureUpdate;
		std::chrono::duration<double> mElapsedTimeTransformUpdate;
		std::chrono::duration<double> mElapsedTimeElementUpdate;
		std::chrono::duration<double> mElapsedTimeRecalculateAABBUpdate;


	};
}
