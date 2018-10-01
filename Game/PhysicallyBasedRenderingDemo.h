#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\DemoLevel.h"

#include "..\Library\PointLight.h"

using namespace Library;

namespace Library
{
	class Effect;
	class PointLight;
	class Keyboard;
	class ProxyModel;
	class RenderStateHelper;
	class PBRMaterial;
	class Skybox;
	class IBLRadianceMap;
}

namespace Rendering
{
	struct LightData
	{
		PointLight* PointLight = nullptr;

		void Destroy()
		{
			delete PointLight;
		}
	};

	struct ShaderBallWoodMaterial
	{
		ID3D11ShaderResourceView* AlbedoTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* MetalnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* RoughnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* NormalTextureShaderResourceView = nullptr;

		XMFLOAT4X4 WorldMatrix = MatrixHelper::Identity;

		void Destroy()
		{
			ReleaseObject(AlbedoTextureShaderResourceView);
			ReleaseObject(MetalnessTextureShaderResourceView);
			ReleaseObject(RoughnessTextureShaderResourceView);
			ReleaseObject(NormalTextureShaderResourceView);
		}
	};
	struct ShaderBallFloorMaterial
	{
		ID3D11ShaderResourceView* AlbedoTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* MetalnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* RoughnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* NormalTextureShaderResourceView = nullptr;

		XMFLOAT4X4 WorldMatrix = MatrixHelper::Identity;

		void Destroy()
		{
			ReleaseObject(AlbedoTextureShaderResourceView);
			ReleaseObject(MetalnessTextureShaderResourceView);
			ReleaseObject(RoughnessTextureShaderResourceView);
			ReleaseObject(NormalTextureShaderResourceView);
		}
	};
	struct ShaderBallMarbleMaterial
	{
		ID3D11ShaderResourceView* AlbedoTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* MetalnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* RoughnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* NormalTextureShaderResourceView = nullptr;

		XMFLOAT4X4 WorldMatrix = MatrixHelper::Identity;

		void Destroy()
		{
			ReleaseObject(AlbedoTextureShaderResourceView);
			ReleaseObject(MetalnessTextureShaderResourceView);
			ReleaseObject(RoughnessTextureShaderResourceView);
			ReleaseObject(NormalTextureShaderResourceView);
		}
	};
	struct ShaderBallBathroomMaterial
	{
		ID3D11ShaderResourceView* AlbedoTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* MetalnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* RoughnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* NormalTextureShaderResourceView = nullptr;

		XMFLOAT4X4 WorldMatrix = MatrixHelper::Identity;

		void Destroy()
		{
			ReleaseObject(AlbedoTextureShaderResourceView);
			ReleaseObject(MetalnessTextureShaderResourceView);
			ReleaseObject(RoughnessTextureShaderResourceView);
			ReleaseObject(NormalTextureShaderResourceView);
		}
	};
	struct ShaderBallBrickMaterial
	{
		ID3D11ShaderResourceView* AlbedoTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* MetalnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* RoughnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* NormalTextureShaderResourceView = nullptr;

		XMFLOAT4X4 WorldMatrix = MatrixHelper::Identity;

		void Destroy()
		{
			ReleaseObject(AlbedoTextureShaderResourceView);
			ReleaseObject(MetalnessTextureShaderResourceView);
			ReleaseObject(RoughnessTextureShaderResourceView);
			ReleaseObject(NormalTextureShaderResourceView);
		}
	};
	struct ShaderBallConcreteMaterial
	{
		ID3D11ShaderResourceView* AlbedoTextureShaderResourceView		= nullptr;
		ID3D11ShaderResourceView* MetalnessTextureShaderResourceView	= nullptr;
		ID3D11ShaderResourceView* RoughnessTextureShaderResourceView	= nullptr;
		ID3D11ShaderResourceView* NormalTextureShaderResourceView		= nullptr;

		XMFLOAT4X4 WorldMatrix = MatrixHelper::Identity;

		void Destroy()
		{
			ReleaseObject(AlbedoTextureShaderResourceView);
			ReleaseObject(MetalnessTextureShaderResourceView);
			ReleaseObject(RoughnessTextureShaderResourceView);
			ReleaseObject(NormalTextureShaderResourceView);
		}
	};
	struct ShaderBallGoldMaterial
	{
		ID3D11ShaderResourceView* AlbedoTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* MetalnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* RoughnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* NormalTextureShaderResourceView = nullptr;

		XMFLOAT4X4 WorldMatrix = MatrixHelper::Identity;

		void Destroy()
		{
			ReleaseObject(AlbedoTextureShaderResourceView);
			ReleaseObject(MetalnessTextureShaderResourceView);
			ReleaseObject(RoughnessTextureShaderResourceView);
			ReleaseObject(NormalTextureShaderResourceView);
		}
	};
	struct ShaderBallSilverMaterial
	{
		ID3D11ShaderResourceView* AlbedoTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* MetalnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* RoughnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* NormalTextureShaderResourceView = nullptr;

		XMFLOAT4X4 WorldMatrix = MatrixHelper::Identity;

		void Destroy()
		{
			ReleaseObject(AlbedoTextureShaderResourceView);
			ReleaseObject(MetalnessTextureShaderResourceView);
			ReleaseObject(RoughnessTextureShaderResourceView);
			ReleaseObject(NormalTextureShaderResourceView);
		}
	};
	
	struct ShaderBallInnerMaterial
	{
		ID3D11ShaderResourceView* AlbedoTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* MetalnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* RoughnessTextureShaderResourceView = nullptr;
		ID3D11ShaderResourceView* NormalTextureShaderResourceView = nullptr;

		XMFLOAT4X4 WorldMatrix = MatrixHelper::Identity;

		void Destroy()
		{
			ReleaseObject(AlbedoTextureShaderResourceView);
			ReleaseObject(MetalnessTextureShaderResourceView);
			ReleaseObject(RoughnessTextureShaderResourceView);
			ReleaseObject(NormalTextureShaderResourceView);
		}
	};

	class PhysicallyBasedRenderingDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(PhysicallyBasedRenderingDemo, DrawableGameComponent)

	public:
		PhysicallyBasedRenderingDemo(Game& game, Camera& camera);
		~PhysicallyBasedRenderingDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;

	private:
		PhysicallyBasedRenderingDemo();
		PhysicallyBasedRenderingDemo(const PhysicallyBasedRenderingDemo& rhs);
		PhysicallyBasedRenderingDemo& operator=(const PhysicallyBasedRenderingDemo& rhs);

		void UpdateImGui();

		void UpdatePointLight(const GameTime& gameTime);

		static const float LightModulationRate;
		static const float LightMovementRate;

		Effect* mEffectPBR;

		PBRMaterial* mPBRShaderBallOuterMaterial;
		PBRMaterial* mPBRShaderBallInnerMaterial;

		ID3D11Buffer* mVertexBufferShaderBallOuterPart;
		ID3D11Buffer* mIndexBufferShaderBallOuterPart;
		UINT mIndexCountShaderBallOuterPart;

		ID3D11Buffer* mVertexBufferShaderBallInnerPart;
		ID3D11Buffer* mIndexBufferShaderBallInnerPart;
		UINT mIndexCountShaderBallInnerPart;

		ID3D11ShaderResourceView* mShaderBallInnerAlbedoTextureShaderResourceView;
		ID3D11ShaderResourceView* mShaderBallInnerMetalnessTextureShaderResourceView;
		ID3D11ShaderResourceView* mShaderBallInnerRoughnessTextureShaderResourceView;
		ID3D11ShaderResourceView* mShaderBallInnerNormalTextureShaderResourceView;

		ID3D11ShaderResourceView* mIrradianceTextureShaderResourceView;
		ID3D11ShaderResourceView* mRadianceTextureShaderResourceView;
		ID3D11ShaderResourceView* mIntegrationTextureShaderResourceView;
		std::unique_ptr<IBLRadianceMap> mRadianceMap;

		Keyboard* mKeyboard;
		std::vector<XMFLOAT4X4> mWorldMatricesShaderBallFirstRow;
		std::vector<XMFLOAT4X4> mWorldMatricesShaderBallSecondRow;

		ProxyModel* mProxyModel;
		Skybox* mSkybox;
		bool mShowSkybox = true;

		RenderStateHelper* mRenderStateHelper;
	};
}