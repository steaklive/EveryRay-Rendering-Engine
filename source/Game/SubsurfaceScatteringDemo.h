#ifndef SUBSURFACE_SCATTERING_DEMO
#define SUBSURFACE_SCATTERING_DEMO

#include "..\Library\DrawableGameComponent.h"

#include "..\Library\InstancingMaterial.h"
#include "..\Library\AmbientLightingMaterial.h"
#include "..\Library\SkinShadingColorMaterial.h"
#include "..\Library\SkinShadingScatterMaterial.h"

#include "..\Library\ER_Sandbox.h"
#include "..\Library\Frustum.h"

#include "..\Library\DirectionalLight.h"


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
	class Skybox;
	class DepthMap;
	class DepthMapMaterial;
	class FullScreenQuad;
	class FullScreenRenderTarget;

}

namespace DirectX
{
	class SpriteBatch;
	class SpriteFont;
}

namespace SubsurfaceScatteringDemoLightInfo
{
	struct LightData
	{

		DirectionalLight* DirectionalLight = nullptr;
		ProxyModel* ProxyModel = nullptr;

		float FarPlane = 0;
		float NearPlane = 0;
		float SpotLightAngle = 0;

		void Destroy()
		{
			DirectionalLight = nullptr;
			ProxyModel = nullptr;

			DeleteObject(DirectionalLight);
			DeleteObject(ProxyModel);
		}

	};

}

namespace Rendering
{

	class AmbientLightingMaterial;
	class InstancingMaterial;
	class SkinShadingColorMaterial;
	class SkinShadingScatterMaterial;

	namespace SubsurfaceScatteringDemoStructs
	{
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

			AmbientLightingMaterial* Material;
			ID3D11Buffer* VertexBuffer;
			ID3D11Buffer* IndexBuffer;
			ID3D11ShaderResourceView* DiffuseTexture;


			UINT IndexCount = 0;
		};
		struct HeadObject
		{

			HeadObject()
				:
				SkinColorMaterial(nullptr),
				SkinScatterMaterial(nullptr),
				VertexBuffer(nullptr),
				IndexBuffer(nullptr),
				DiffuseTexture(nullptr),
				NormalTexture(nullptr),
				SpecularAOTexture(nullptr),
				BeckmannTexture(nullptr),
				IrradianceTexture(nullptr)

			{}


			~HeadObject()
			{

				DeleteObject(SkinColorMaterial);
				DeleteObject(SkinScatterMaterial);
				ReleaseObject(VertexBuffer);
				ReleaseObject(IndexBuffer);
				ReleaseObject(DiffuseTexture);
				ReleaseObject(NormalTexture);
				ReleaseObject(SpecularAOTexture);
				ReleaseObject(BeckmannTexture);
				ReleaseObject(IrradianceTexture);

			}

			SkinShadingColorMaterial* SkinColorMaterial;
			SkinShadingScatterMaterial* SkinScatterMaterial;

			ID3D11Buffer* VertexBuffer;
			ID3D11Buffer* IndexBuffer;
			ID3D11ShaderResourceView* DiffuseTexture;
			ID3D11ShaderResourceView* NormalTexture;
			ID3D11ShaderResourceView* SpecularAOTexture;
			ID3D11ShaderResourceView* BeckmannTexture;
			ID3D11ShaderResourceView* IrradianceTexture;

			UINT IndexCount = 0;
		};
	}


	class SubsurfaceScatteringDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(SubsurfaceScatteringDemo, DrawableGameComponent)

	public:
		SubsurfaceScatteringDemo(Game& game, Camera& camera);
		~SubsurfaceScatteringDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;

	private:

		SubsurfaceScatteringDemo();
		SubsurfaceScatteringDemo(const SubsurfaceScatteringDemo& rhs);
		SubsurfaceScatteringDemo& operator=(const SubsurfaceScatteringDemo& rhs);

		void UpdateLight();
		void UpdateDepthBiasState();
		void UpdateImGui();
		void UpdateScatterMaterial(int side);


		Keyboard* mKeyboard;
		XMFLOAT4X4 mWorldMatrix;

		Frustum mFrustum;
		RenderableFrustum* mDebugFrustum;

		SubsurfaceScatteringDemoStructs::DefaultPlaneObject* mDefaultPlaneObject;
		SubsurfaceScatteringDemoStructs::HeadObject* mHeadObject;

		DepthMap* mDepthMap;
		float mDepthBias;
		float mSlopeScaledDepthBias;
		ID3D11RasterizerState* mDepthBiasState;
		DepthMapMaterial* mDepthMapMaterial;
		ID3D11Buffer* mShadowMapVertexBuffer;

		FullScreenRenderTarget* mRenderTarget;
		FullScreenQuad* mQuad;

		Skybox* mSkybox;
		RenderStateHelper* mRenderStateHelper;


		float mBumpiness = 0.8f;
		float mSpecularIntensity = 1.5f;
		float mSpecularRoughness = 0.3f;
		float mSpecularFresnel = 0.8f;
		float mSSSWidth = 0.012f;
		float mSSSStrength = 0.4f;
		float mAmbientFactor = 0.6f;
		float mAttenuation = 128.0f;
		float mShadowBias = -0.001f;
		float mLightFarPlane = 50.0f;
		float mLightNearPlane = 0.5f;
		float mLightAngle = 45.0f;
		bool mIsSSSEnabled = true;
		bool mIsSSSBlurEnabled = false;
		bool mIsSSSFollowSurface = false;

	};
}

#endif
