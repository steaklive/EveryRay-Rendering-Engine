#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\InstancingMaterial.h"
#include "..\Library\AmbientLightingMaterial.h"

#include "..\Library\DemoLevel.h"
#include "..\Library\Frustum.h"

#include "..\Library\DirectionalLight.h"
#include "..\Library\RenderStateHelper.h"



using namespace Library;

const int NUM_INSTANCES = 100;


namespace Library
{
	class Effect;
	class Keyboard;
	class DirectionalLight;
	class ProxyModel;
	class RenderStateHelper;
	class RenderableFrustum;
	class RenderableAABB;
	class Skybox;
}

namespace DirectX
{
	class SpriteBatch;
	class SpriteFont;
}

namespace FrustumCullingDemoLightInfo
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

	struct DefaultPlaneObject
	{

		DefaultPlaneObject()
			:
			Material(nullptr),
			VertexBuffer(nullptr),
			IndexBuffer(nullptr),
			DiffuseTexture(nullptr)

		{}


		~DefaultPlaneObject()
		{

			DeleteObject(Material);
			ReleaseObject(VertexBuffer);
			ReleaseObject(IndexBuffer);
			ReleaseObject(DiffuseTexture);
			
		}

		AmbientLightingMaterial* Material		 ;
		ID3D11Buffer* VertexBuffer				 ;
		ID3D11Buffer* IndexBuffer				 ;
		ID3D11ShaderResourceView* DiffuseTexture ;

		UINT IndexCount = 0;
	};

	struct InstancedObject
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

		InstancedObject()
			: 
			Materials(0, nullptr),
			InstanceData(0),
			MeshesVertexBuffers(),
			MeshesIndexBuffers(0, nullptr),
			InstanceBuffer(nullptr),
			MeshesIndicesCounts(0, 0),
			InstanceCount(0),
			AlbedoMap(nullptr),
			NormalMap(nullptr),
			ModelAABB(0),
			InstancesPositions(0),
			DebugInstancesAABBs(NUM_INSTANCES, nullptr)

		
		{}

		std::vector<InstancingMaterial*> Materials;
		std::vector<InstancingMaterial::InstancedData> InstanceData;

		std::vector<VertexBufferData>	 MeshesVertexBuffers ;
		std::vector<ID3D11Buffer*>		 MeshesIndexBuffers  ;
		std::vector<UINT>				 MeshesIndicesCounts ;

		ID3D11Buffer*		 InstanceBuffer;
		UINT InstanceCount = 0;

		ID3D11ShaderResourceView* AlbedoMap;
		ID3D11ShaderResourceView* NormalMap;

		std::vector<XMFLOAT3>		 ModelAABB; 								
		std::vector<XMFLOAT3>		 InstancesPositions; 						
		std::vector<RenderableAABB*> DebugInstancesAABBs; 

		float RandomDistributionRadius = 100.0f;
		

		~InstancedObject()
		{
			DeletePointerCollection(Materials);
			DeletePointerCollection(DebugInstancesAABBs);

			ReleasePointerCollection(MeshesIndexBuffers);

			for (VertexBufferData& bufferData : MeshesVertexBuffers)
			{
				ReleaseObject(bufferData.VertexBuffer);
			}

			ReleaseObject(AlbedoMap);
			ReleaseObject(NormalMap);
		}
	};

	class FrustumCullingDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(FrustumCullingDemo, DrawableGameComponent)

	public:
		FrustumCullingDemo(Game& game, Camera& camera);
		~FrustumCullingDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;

	private:

		FrustumCullingDemo();
		FrustumCullingDemo(const FrustumCullingDemo& rhs);
		FrustumCullingDemo& operator=(const FrustumCullingDemo& rhs);

		void UpdateDirectionalLight(const GameTime& gameTime);
		void RotateCustomFrustum(const GameTime& gameTime);
		void UpdateImGui();

		void DiskRandomDistribution();
		void CullAABB(bool isEnabled, const Frustum& frustum, const std::vector<XMFLOAT3>& aabb, const std::vector<XMFLOAT3>& positions);
		
		XMMATRIX SetMatrixForCustomFrustum(Game& game, Camera& camera);


		Keyboard* mKeyboard;
		XMFLOAT4X4 mWorldMatrix;

		Frustum mFrustum;
		RenderableFrustum* mDebugFrustum;

		DefaultPlaneObject* mDefaultPlaneObject;
		InstancedObject*	mInstancedObject;


		Skybox* mSkybox;

		RenderStateHelper mRenderStateHelper;

		bool mIsCustomFrustumActive = false;
		bool mIsCustomFrustumRotating = false;
		bool mIsCullingEnabled = false;
		bool mIsAABBRendered = false;
		bool mIsCustomFrustumInitialized = false;


	};
}
