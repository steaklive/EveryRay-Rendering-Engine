#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\InstancingMaterial.h"
#include "..\Library\AmbientLightingMaterial.h"

#include "..\Library\DemoLevel.h"
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

	class AmbientLightingMaterial;
	class InstancingMaterial;

	namespace CollisionTestObjects
	{
		const int NUM_DYNAMIC_INSTANCES = 300;
		const int NUM_STATIC_INSTANCES = 25;
		
		struct StaticInstancedObject
		{
			struct VertexBufferData
			{
				ID3D11Buffer* VertexBuffer;
				UINT Stride;
				UINT Offset;

				VertexBufferData(ID3D11Buffer* vertexBuffer, UINT stride, UINT offset)
					:
					VertexBuffer(vertexBuffer),
					Stride(stride),
					Offset(offset)
				{ }
			};

			StaticInstancedObject()
				:
				Materials(0, nullptr),
				InstanceData(0),
				MeshesVertexBuffers(),
				MeshesIndexBuffers(0, nullptr),
				MeshesIndicesCounts(0, 0),
				InstanceCount(0),
				AlbedoMap(nullptr),
				NormalMap(nullptr),
				ModelAABB(0),
				InstancesPositions(0),
				InstanceBuffer(nullptr)


			{}

			std::vector<InstancingMaterial*> Materials;
			std::vector<InstancingMaterial::InstancedData> InstanceData;

			ID3D11Buffer* InstanceBuffer;
			std::vector<VertexBufferData>	 MeshesVertexBuffers;
			std::vector<ID3D11Buffer*>		 MeshesIndexBuffers;
			std::vector<UINT>				 MeshesIndicesCounts;

			std::vector<XMFLOAT3> Vertices;

			UINT InstanceCount = 0;

			ID3D11ShaderResourceView* AlbedoMap;
			ID3D11ShaderResourceView* NormalMap;

			std::vector<XMFLOAT3>		 ModelAABB;
			std::vector<XMFLOAT3>		 InstancesPositions;


			~StaticInstancedObject()
			{
				DeletePointerCollection(Materials);

				ReleasePointerCollection(MeshesIndexBuffers);

				for (VertexBufferData& bufferData : MeshesVertexBuffers)
				{
					ReleaseObject(bufferData.VertexBuffer);
				}

				ReleaseObject(AlbedoMap);
				ReleaseObject(NormalMap);
				//ReleaseObject(InstanceBuffer);

			}
		};

		struct DynamicInstancedObject
		{
			struct VertexBufferData
			{
				ID3D11Buffer* VertexBuffer;
				UINT Stride;
				UINT Offset;

				VertexBufferData(ID3D11Buffer* vertexBuffer, UINT stride, UINT offset)
					:
					VertexBuffer(vertexBuffer),
					Stride(stride),
					Offset(offset)
				{ }
			};

			DynamicInstancedObject()
				:
				Materials(0, nullptr),
				InstanceData(0),
				MeshesVertexBuffers(),
				MeshesIndexBuffers(0, nullptr),
				MeshesIndicesCounts(0, 0),
				InstanceCount(0),
				AlbedoMap(nullptr),
				NormalMap(nullptr),
				ModelAABB(0),
				InstancesPositions(0),
				InstancesDirections(0),
				InstanceBuffer(nullptr)


			{}

			std::vector<InstancingMaterial*> Materials;
			std::vector<InstancingMaterial::InstancedData> InstanceData;

			ID3D11Buffer* InstanceBuffer;
			std::vector<VertexBufferData>	 MeshesVertexBuffers;
			std::vector<ID3D11Buffer*>		 MeshesIndexBuffers;
			std::vector<UINT>				 MeshesIndicesCounts;

			std::vector<XMFLOAT3> Vertices;
			std::vector<XMFLOAT3> VerticesConvexHull;

			UINT InstanceCount = 0;

			ID3D11ShaderResourceView* AlbedoMap;
			ID3D11ShaderResourceView* NormalMap;

			std::vector<XMFLOAT3>		 ModelAABB;
			std::vector<XMFLOAT3>		 InstancesPositions;
			std::vector<XMFLOAT3>		 InstancesDirections;


			~DynamicInstancedObject()
			{
				DeletePointerCollection(Materials);

				ReleasePointerCollection(MeshesIndexBuffers);

				for (VertexBufferData& bufferData : MeshesVertexBuffers)
				{
					ReleaseObject(bufferData.VertexBuffer);
				}

				ReleaseObject(AlbedoMap);
				ReleaseObject(NormalMap);
				//ReleaseObject(InstanceBuffer);

			}
		};
	}

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
		virtual bool IsComponent() override;

	private:

		CollisionTestDemo();
		CollisionTestDemo(const CollisionTestDemo& rhs);
		CollisionTestDemo& operator=(const CollisionTestDemo& rhs);

		void UpdateDirectionalLight(const GameTime& gameTime);
		void UpdateObjectsTransforms_UnoptimizedDataAccess(CollisionTestObjects::DynamicInstancedObject & object, const GameTime & gameTime);
		void UpdateObjectsTransforms_OptimizedDataAccess(CollisionTestObjects::DynamicInstancedObject & object, const GameTime & gameTime);
		void UpdateSpatialElement(std::vector<SpatialElement*>& elements, int startIndex, int endIndex, const GameTime& gameTime);
		void UpdateImGui();
		
		void CalculateDynamicObjectsRandomDistribution();
		void CalculateStaticObjectsRandomDistribution();
		float CalculateObjectRadius(std::vector<XMFLOAT3>& vertices);
		void RecalculateAABB(int index, CollisionTestObjects::DynamicInstancedObject & object, XMMATRIX & mat);
		void VisualizeCollisionDetection();




		std::vector<SpatialElement*> mSpatialElements;
		CollisionTestObjects::DynamicInstancedObject*	mDynamicInstancedObject;
		CollisionTestObjects::StaticInstancedObject*	mStaticInstancedObject;
		
		std::vector<std::vector<XMFLOAT3>> mDynamicMeshVertices;
		std::vector<std::vector<XMFLOAT3>> mDynamicMeshAABBs;
		std::vector<std::vector<XMFLOAT3>> mDynamicMeshOBBs;

		std::vector<std::vector<XMFLOAT3>> mStaticMeshVertices;
		std::vector<std::vector<XMFLOAT3>> mStaticMeshAABBs;
		std::vector<std::vector<XMFLOAT3>> mStaticMeshOBBs;
		
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
