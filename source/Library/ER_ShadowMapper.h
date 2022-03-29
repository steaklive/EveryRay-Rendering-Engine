#pragma once
#include "Common.h"
#include "GameComponent.h"

namespace Library
{
	class DepthMap;
	class Frustum;
	class Projector;
	class Camera;
	class DirectionalLight;
	class Scene;

	class ER_ShadowMapper : public GameComponent 
	{

	public:
		ER_ShadowMapper(Game& pGame, Camera& camera, DirectionalLight& dirLight, UINT pWidth, UINT pHeight, bool isCascaded = true);
		~ER_ShadowMapper();

		void Draw(const Scene* scene);
		void Update(const GameTime& gameTime);
		void BeginRenderingToShadowMap(int cascadeIndex = 0);
		void StopRenderingToShadowMap(int cascadeIndex = 0);
		XMMATRIX GetViewMatrix(int cascadeIndex = 0) const;
		XMMATRIX GetProjectionMatrix(int cascadeIndex = 0) const;
		ID3D11ShaderResourceView* GetShadowTexture(int cascadeIndex = 0) const;
		UINT GetResolution() const { return mResolution; }
		void ApplyTransform();
		//void ApplyRotation();

	private:
		XMMATRIX GetLightProjectionMatrixInFrustum(int index, Frustum& cameraFrustum, DirectionalLight& light);
		XMMATRIX GetProjectionBoundingSphere(int index);

		Camera& mCamera;
		DirectionalLight& mDirectionalLight;

		std::vector<DepthMap*> mShadowMaps;
		std::vector<Projector*> mLightProjectors;
		std::vector<Frustum> mCameraCascadesFrustums;
		std::vector<XMFLOAT3> mLightProjectorCenteredPositions;

		XMMATRIX mShadowMapViewMatrix;
		XMMATRIX mShadowMapProjectionMatrix;
		ID3D11RasterizerState* mShadowRasterizerState;
		ID3D11RasterizerState* mOriginalRasterizerState;
		ID3D11DepthStencilState* mDepthStencilState;
		UINT mResolution = 0;
		bool mIsCascaded = true;
	};
}