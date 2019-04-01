////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Based on NVIDIA's GameWorks - Volumetric Lighting
//	https://developer.nvidia.com/sites/default/files/akamai/gameworks/downloads/papers/NVVL/Fast_Flexible_Physically-Based_Volumetric_Light_Scattering.pdf
//	
//	P.S. Unfortunately, I can not share my VolumetricLightingDemo.cpp nor the shaders.
//  You can check out NVIDIA's GitHub for their source code.
//	My implementation is for educational purposes only.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



#pragma once
#include "..\Library\DrawableGameComponent.h"
#include "..\Library\Frustum.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\ConstantBuffer.h"
#include "..\Library\CustomRenderTarget.h"
#include "..\Library\DepthTarget.h"

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
	class NormalMappingMaterial;
	class ShadowMappingMaterial;
	class ShadowMappingDirectionalMaterial;
	class DepthMapMaterial;
	class DepthMap;
	class Skybox;
	class FullScreenRenderTarget;
}

namespace VolumetricLightingDemoInfo
{
	//shadow
	enum ShadowMappingTechnique
	{
		ShadowMappingTechniqueSimplePCF = 0,
		CascadedShadowMappingTechniquePCF,
		ShadowMappingTechniqueEnd
	};

	const std::string ShadowMappingTechniqueNames[] = { "shadow_mapping_pcf", "csm_shadow_mapping_pcf" };
	const std::string ShadowMappingDisplayNames[] = { "Shadow Mapping + PCF", "Cascaded Shadow Mapping + PCF" };
	const std::string DepthMappingTechniqueNames[] = { "create_depthmap_w_bias", "create_depthmap_w_bias" };

	
	struct ContextConstantBuffer
	{
		XMFLOAT2 outputSize;
		XMFLOAT2 outputSizeInv;
		XMFLOAT2 bufferSize;
		XMFLOAT2 bufferSizeInv;

		float resMultiplier;
		int sampleCount;
		float pad1[2];

	};

	struct FrameConstantBuffer
	{
		XMMATRIX	projection;
		XMMATRIX	viewProj;
		XMMATRIX	viewProjInv;
		XMFLOAT2	outputViewportSize;
		XMFLOAT2	outputViewportSizeInv;
		XMFLOAT2	viewportSize;
		XMFLOAT2	viewportSizeInv;
		XMFLOAT3	eyePos;
		float pad1[1];
		XMFLOAT2	jitterOffset;
		float		zNear;
		float		zFar;
		XMFLOAT3	scatterPower;
		int			numPhaseTerms;
		XMFLOAT3	sigmaExtinction;
		float pad2[1];
		int			phaseFunc[4][4];
		XMFLOAT4	phaseParams[4];
	};

	struct VolumeConstantBuffer
	{
		XMMATRIX lightToWorldMatrix;

		float lightFalloffAngle;
		float lightFalloffPower;
		float gridSectionSize;
		float lightToEyeDepth;
		float lightZNear;
		float lightZFar;
		float pad[2];

		XMFLOAT4 attenuationFactors;

		XMMATRIX lightProjMatrices[4];
		XMMATRIX lightProjInvMatrices[4];

		XMFLOAT3 lightDir;
		float depthBias;
		XMFLOAT3 lightPos;
		uint32_t meshResolution;
		XMFLOAT3 lightIntensity;

		float targetRaySize;

		XMFLOAT4 elementOffsetAndScale[4];

		XMFLOAT4 shadowMapDim;

		// Only first index of each "row" is used.
		// (Need to do this because HLSL can only stride arrays by full offset)
		uint32_t elementIndex[4][4];
	};

	struct PostLightConstantBuffer
	{
		XMMATRIX historyXform;

		float filterThreshold;
		float historyFactor;
		float pad1[2];

		XMFLOAT3 fogLight;
		float multiScattering;
	};



	struct LightInfo
	{
		XMMATRIX lightViewProj;
		XMFLOAT3 lightDir;
	};


}

namespace Rendering
{
	class NormalMappingMaterial;
	class PostProcessingStack;

	namespace VolumetricDemoObjects
	{
		struct StatueObject
		{

			StatueObject()
				:
				SimpleMaterial(nullptr),
				DepthMaterial(nullptr),
				VertexBuffer(nullptr),
				VertexBufferForDepth(nullptr),
				IndexBuffer(nullptr),
				DiffuseTexture(nullptr),
				NormalTexture(nullptr)
			{}

			~StatueObject()
			{

				DeleteObject(SimpleMaterial);
				DeleteObject(DepthMaterial);
				ReleaseObject(VertexBuffer);
				ReleaseObject(VertexBufferForDepth);
				ReleaseObject(IndexBuffer);
				ReleaseObject(DiffuseTexture);
				ReleaseObject(NormalTexture);


			}

			NormalMappingMaterial* SimpleMaterial;
			DepthMapMaterial* DepthMaterial;

			ID3D11Buffer* VertexBuffer;
			ID3D11Buffer* VertexBufferForDepth;
			ID3D11Buffer* IndexBuffer;

			ID3D11ShaderResourceView* DiffuseTexture;
			ID3D11ShaderResourceView* NormalTexture;

			UINT IndexCount = 0;
		};

		struct PlaneObject
		{

			PlaneObject()
				:
				ShadowMaterial(nullptr),
				DepthMaterial(nullptr),
				VertexBuffer(nullptr),
				VertexBufferForDepth(nullptr),
				IndexBuffer(nullptr),
				DiffuseTexture(nullptr)
			{}

			~PlaneObject()
			{

				DeleteObject(ShadowMaterial);
				DeleteObject(DepthMaterial);
				ReleaseObject(VertexBuffer);
				ReleaseObject(VertexBufferForDepth);
				ReleaseObject(IndexBuffer);
				ReleaseObject(DiffuseTexture);

			}

			ShadowMappingDirectionalMaterial* ShadowMaterial;
			DepthMapMaterial* DepthMaterial;

			ID3D11Buffer* VertexBuffer;
			ID3D11Buffer* VertexBufferForDepth;
			ID3D11Buffer* IndexBuffer;

			ID3D11ShaderResourceView* DiffuseTexture;

			UINT IndexCount = 0;
		};
	}

	
	class VolumetricLightingDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(VolumetricLightingDemo, DrawableGameComponent)

	public:
		VolumetricLightingDemo(Game& game, Camera& camera);
		~VolumetricLightingDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;


	private:
		VolumetricLightingDemo();
		VolumetricLightingDemo(const VolumetricLightingDemo& rhs);
		VolumetricLightingDemo& operator=(const VolumetricLightingDemo& rhs);

		void UpdateImGui();
		void UpdateDepthBias(const GameTime& gameTime);
		void UpdateDepthBiasState();
		void UpdateDirectionalLightAndProjector(const GameTime& gameTime);
		void InitializeProjectedTextureScalingMatrix();
		void AccumulationStart				(ID3D11DeviceContext* pContext);
		void AccumulationUpdateMedium		(ID3D11DeviceContext* pContext);
		void AccumulationCopyDepth			(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pShadowMap);
		void AccumulationEnd				(ID3D11DeviceContext* pContext);
		void RenderVolumeStart				(ID3D11DeviceContext* pContext, VolumetricLightingDemoInfo::LightInfo* pLightInfo);
		void RenderVolumeCreateVolume		(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pShadowMap, VolumetricLightingDemoInfo::LightInfo* pLightInfo );
		void RenderVolumeEnd				(ID3D11DeviceContext* pContext);
		void ApplyLightingStart				(ID3D11DeviceContext* pContext);
		void ApplyLightingResolve			(ID3D11DeviceContext* pContext);
		void ApplyLightingComposite			(ID3D11DeviceContext* pContext, FullScreenRenderTarget* sceneTarget, ID3D11ShaderResourceView* pShadowMap);		
		void CreateStates					(ID3D11Device* device);
		void DrawFullscreen					(ID3D11DeviceContext* context);
		void DrawFrustumBase				(ID3D11DeviceContext* context, int resolution);
		void DrawFrustumGrid				(ID3D11DeviceContext* context, int resolution);
		
		XMMATRIX GetProjectionAABB(int index, Frustum& cameraFrustum, DirectionalLight& light, ProxyModel& positionObject);
		XMMATRIX SetMatrixForCustomFrustum(Game& game, Camera& camera, int number, XMFLOAT3 pos, XMVECTOR dir);


		bool mIsContextConstantBufferInitialized = false;


		ConstantBuffer<VolumetricLightingDemoInfo::ContextConstantBuffer> mContextConstantBuffer;
		ConstantBuffer<VolumetricLightingDemoInfo::FrameConstantBuffer> mFrameConstantBuffer;
		ConstantBuffer<VolumetricLightingDemoInfo::VolumeConstantBuffer> mVolumeConstantBuffer;
		ConstantBuffer<VolumetricLightingDemoInfo::PostLightConstantBuffer> mPostLightConstantBuffer;


		DepthTarget* mDepthTarget0;
		DepthTarget* mSceneDepth;


		CustomRenderTarget* mSceneRT;

		CustomRenderTarget* mPhaseLUTRenderTarget;
		CustomRenderTarget* mAccumulationRenderTarget;
		CustomRenderTarget* mAccumulationOutputRenderTarget;
		CustomRenderTarget* mResolvedAccumulationRenderTarget;
		CustomRenderTarget* mResolvedDepthTarget0;


		std::vector<Projector*> mLightProjectors;
		std::vector<Frustum> mLightProjectorFrustums;
		std::vector<XMFLOAT3> mLightProjectorCenteredPositions;
		std::vector<RenderableFrustum*> mLightProjectorFrustumsDebug;

		std::vector<Frustum> mCameraViewFrustums;
		std::vector<RenderableFrustum*> mCameraFrustumsDebug;
		bool mIsDrawFrustum;
		bool mIsVisualizeCascades;


		VolumetricDemoObjects::StatueObject* mStatueObject;
		VolumetricDemoObjects::PlaneObject* mPlaneObject;

		
		XMFLOAT4X4 mPlaneWorldMatrix;
		XMFLOAT4X4 mModelWorldMatrix;
		std::vector<XMFLOAT4X4> mModelWorldMatrices;


		DepthMap* mDepthMapFromLight;
		DepthMap* mDepthMapFromCamera;
		DepthMap* mShadowMap0;
		DepthMap* mShadowMap1;
		DepthMap* mShadowMap2;

		bool mIsDrawDepthMap;
		ID3D11RasterizerState* mDepthBiasState;
		float mDepthBias;
		float mSlopeScaledDepthBias;

		//Additional data
		Keyboard* mKeyboard;
		ProxyModel* mProxyModel;
		PointLight* mPointLight;
		DirectionalLight* mDirectionalLight;
		Skybox* mSkybox;
		
		XMCOLOR mAmbientColor;
		XMCOLOR mSpecularColor;
		float mSpecularPower;

		PostProcessingStack* mPostProcessingStack;
		FullScreenRenderTarget* mRenderTargetPP0;
		FullScreenRenderTarget* mRenderTargetPP1;
		FullScreenRenderTarget* mRenderTargetPP2;


		RenderStateHelper mRenderStateHelper;

		VolumetricLightingDemoInfo::ShadowMappingTechnique mActiveShadowMappingTechnique;
		XMFLOAT4X4 mProjectedTextureScalingMatrix;


		float scatterPower = 0.005f;
		float sigmaExtinction[3] = { 0.0021, 0.00282, 0.00481 };
		float lightIntensity = 250;
		float lightPos[3] = { 0, 0, 0 };
		float targetRaySize = 12;



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
	};
}