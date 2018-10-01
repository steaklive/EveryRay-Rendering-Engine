#pragma once
#include "..\Library\DrawableGameComponent.h"
#include "..\Library\Frustum.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\DemoLevel.h"

using namespace Library;

namespace Library
{
	class Effect;
	class PointLight;
	class DirectionalLight;
	class Keyboard;
	class ProxyModel;
	class Projector;
	class RenderableFrustum;
	class ShadowMappingMaterial;
	class ShadowMappingDirectionalMaterial;
	class DepthMapMaterial;
	class DepthMap;
}

namespace Rendering
{
	class EnvironmentMappingMaterial;


	enum ShadowMappingTechnique
	{
		ShadowMappingTechniqueSimplePCF = 0,
		CascadedShadowMappingTechniquePCF,
		ShadowMappingTechniqueEnd
	};


	const std::string ShadowMappingTechniqueNames[] = { "shadow_mapping_pcf", "csm_shadow_mapping_pcf" };
	const std::string ShadowMappingDisplayNames[] = { "Shadow Mapping + PCF", "Cascaded Shadow Mapping + PCF" };
	const std::string DepthMappingTechniqueNames[] = {"create_depthmap_w_bias", "create_depthmap_w_bias" };

	class ShadowMappingDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(ShadowMappingDemo, DrawableGameComponent)

	public:
		ShadowMappingDemo(Game& game, Camera& camera);
		~ShadowMappingDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;


	private:
		ShadowMappingDemo();
		ShadowMappingDemo(const ShadowMappingDemo& rhs);
		ShadowMappingDemo& operator=(const ShadowMappingDemo& rhs);

		//methods
		void UpdateImGui();
		void UpdateDepthBias(const GameTime& gameTime);
		void UpdateDepthBiasState();
		void UpdateDirectionalLightAndProjector(const GameTime& gameTime);
		void InitializeProjectedTextureScalingMatrix();
		XMMATRIX GetProjectionAABB(int index, Frustum& cameraFrustum, DirectionalLight& light, ProxyModel& positionObject);
		XMMATRIX SetMatrixForCustomFrustum(Game& game, Camera& camera, int number, XMFLOAT3 pos, XMVECTOR dir);

		//statics
		static const float LightModulationRate;
		static const float LightMovementRate;
		static const XMFLOAT2 LightRotationRate;

		static const UINT DepthMapWidth;
		static const UINT DepthMapHeight;
		static const RECT DepthMapDestinationRectangle0;
		static const RECT DepthMapDestinationRectangle1;
		static const RECT DepthMapDestinationRectangle2;
		static const float DepthBiasModulationRate;


		//Light data
		XMCOLOR mAmbientColor;
		PointLight* mPointLight;
		DirectionalLight* mDirectionalLight;
		XMCOLOR mSpecularColor;
		float mSpecularPower;

		//Frustums and projectors data
		std::vector<Projector*> mLightProjectors;
		std::vector<Frustum> mLightProjectorFrustums;
		std::vector<XMFLOAT3> mLightProjectorCenteredPositions;
		std::vector<RenderableFrustum*> mLightProjectorFrustumsDebug;

		std::vector<Frustum> mCameraViewFrustums;
		std::vector<RenderableFrustum*> mCameraFrustumsDebug;
		bool mIsDrawFrustum;
		bool mIsVisualizeCascades;


		//Effects and materials data
		Effect* mShadowMappingEffect;
		ShadowMappingDirectionalMaterial* mShadowMappingDirectionalMaterial;
		
		Effect* mDepthMapEffect;
		DepthMapMaterial* mDepthMapMaterial;

		Effect* mEnvironmentMappingEffect;
		EnvironmentMappingMaterial* mEnvironmentMappingMaterial;
		ID3D11ShaderResourceView* mTextureShaderResourceView;
		ID3D11ShaderResourceView* mCubeMapShaderResourceView;

		//Plane model data
		ID3D11Buffer* mPlanePositionVertexBuffer;
		ID3D11Buffer* mPlanePositionUVNormalVertexBuffer;
		ID3D11Buffer* mPlaneIndexBuffer;
		UINT mPlaneVertexCount;
		XMFLOAT4X4 mPlaneWorldMatrix;
		ID3D11ShaderResourceView* mPlaneTexture;

		//Shadow receiver model data
		ID3D11Buffer* mModelPositionVertexBuffer;
		ID3D11Buffer* mModelPositionUVNormalVertexBuffer;
		ID3D11Buffer* mModelIndexBuffer;
		UINT mModelIndexCount;
		XMFLOAT4X4 mModelWorldMatrix;
		std::vector<XMFLOAT4X4> mModelWorldMatrices;

		//Depth data
		DepthMap* mDepthMap0;
		DepthMap* mDepthMap1;
		DepthMap* mDepthMap2;

		bool mIsDrawDepthMap;
		ID3D11RasterizerState* mDepthBiasState;
		float mDepthBias;
		float mSlopeScaledDepthBias;

		//Additional data
		Keyboard* mKeyboard;
		ProxyModel* mProxyModel;
		RenderStateHelper mRenderStateHelper;

		ShadowMappingTechnique mActiveTechnique;
		XMFLOAT4X4 mProjectedTextureScalingMatrix;

	};
}