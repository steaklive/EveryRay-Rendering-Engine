#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"
#include "RHI/ER_RHI.h"

namespace EveryRay_Core
{
	class ER_Frustum;
	class ER_Projector;
	class ER_Camera;
	class ER_DirectionalLight;
	class ER_Scene;
	class ER_Terrain;

	class ER_ShadowMapper : public ER_CoreComponent 
	{
	public:
		ER_ShadowMapper(ER_Core& pCore, ER_Camera& camera, ER_DirectionalLight& dirLight, UINT pWidth, UINT pHeight, bool isCascaded = true);
		~ER_ShadowMapper();

		void Draw(const ER_Scene* scene, ER_Terrain* terrain = nullptr);
		void Update(const ER_CoreTime& gameTime);
		void BeginRenderingToShadowMap(int cascadeIndex = 0);
		void StopRenderingToShadowMap(int cascadeIndex = 0);
		XMMATRIX GetViewMatrix(int cascadeIndex = 0) const;
		XMMATRIX GetProjectionMatrix(int cascadeIndex = 0) const;
		ER_RHI_GPUTexture* GetShadowTexture(int cascadeIndex = 0) const;
		UINT GetResolution() const { return mResolution; }
		void ApplyTransform();
		//void ApplyRotation();

	private:
		XMMATRIX GetLightProjectionMatrixInFrustum(int index, ER_Frustum& cameraFrustum, ER_DirectionalLight& light);
		XMMATRIX GetProjectionBoundingSphere(int index);

		ER_Camera& mCamera;
		ER_DirectionalLight& mDirectionalLight;

		ER_RHI_GPURootSignature* mRootSignature = nullptr;

		std::vector<ER_RHI_GPUTexture*> mShadowMaps;
		std::vector<ER_Projector*> mLightProjectors;
		std::vector<ER_Frustum> mCameraCascadesFrustums;
		std::vector<XMFLOAT3> mLightProjectorCenteredPositions;

		ER_RHI_RASTERIZER_STATE mOriginalRS;
		ER_RHI_Viewport mOriginalViewport;
		ER_RHI_Rect mOriginalRect;
		XMMATRIX mShadowMapViewMatrix;
		XMMATRIX mShadowMapProjectionMatrix;
		UINT mResolution = 0;
		bool mIsCascaded = true;
	};
}