#ifndef INSTANCING_DEMO
#define INSTANCING_DEMO

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\InstancingMaterial.h"
#include "..\Library\DemoLevel.h"

#include "..\Library\PointLight.h"

using namespace Library;

namespace Library
{
	class Effect;
	class Keyboard;
	class PointLight;
	class ProxyModel;
	class RenderStateHelper;
}

namespace DirectX
{
	class SpriteBatch;
	class SpriteFont;
}

namespace InstancingDemoLightInfo
{
	struct LightData
	{
		PointLight* pointLight = nullptr;
		float lightColor[3] = {0.9, 0.9, 0.9};
		float lightRadius = 25.0f;
		float orbitRadius = 0.0f;
		float height = 0.0f;
		float movementSpeed = 1.0f;

		void Destroy()
		{
			pointLight = nullptr;
			delete pointLight;
		}

	};

}

namespace Rendering
{

	class InstancingMaterial;

	class InstancingDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(InstancingDemo, DrawableGameComponent)

	public:
		InstancingDemo(Game& game, Camera& camera);
		~InstancingDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;

	private:

		struct VertexBufferData
		{
			ID3D11Buffer* VertexBuffer;
			UINT Stride;
			UINT Offset;

			VertexBufferData(ID3D11Buffer* vertexBuffer, UINT stride, UINT offset)
				: VertexBuffer(vertexBuffer), Stride(stride), Offset(offset) { }
		};

		InstancingDemo();
		InstancingDemo(const InstancingDemo& rhs);
		InstancingDemo& operator=(const InstancingDemo& rhs);

		void UpdatePointLight(const GameTime& gameTime);
		void UpdateImGui();

		Effect* mEffect;
		std::vector<InstancingMaterial*> mModelMaterials;

		ID3D11ShaderResourceView* mAlbedoMap;
		ID3D11ShaderResourceView* mNormalMap;

		std::vector<VertexBufferData> mVertexBuffers;
		std::vector<ID3D11Buffer*> mModelIndexBuffers;
		std::vector<UINT> mModelMeshIndexCounts;

		UINT mInstanceCount;

		Keyboard* mKeyboard;
		XMFLOAT4X4 mWorldMatrix;

		ProxyModel* mProxyModel0;
		ProxyModel* mProxyModel1;
		ProxyModel* mProxyModel2;
		ProxyModel* mProxyModel3;


		RenderStateHelper* mRenderStateHelper;
	};
}

#endif