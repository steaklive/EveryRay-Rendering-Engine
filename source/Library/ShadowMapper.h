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

	class ShadowMapper : public GameComponent 
	{

	public:
		ShadowMapper(Game& pGame, Camera& camera, DirectionalLight& dirLight, UINT pWidth, UINT pHeight, bool isCascaded = true);
		~ShadowMapper();

		void Update(const GameTime& gameTime);
		void BeginRenderingToShadowMap(int cascadeIndex = 0);
		void StopRenderingToShadowMap(int cascadeIndex = 0);
		XMMATRIX GetViewMatrix(int cascadeIndex = 0);
		XMMATRIX GetProjectionMatrix(int cascadeIndex = 0);
		ID3D11ShaderResourceView* GetShadowTexture(int cascadeIndex = 0);
		void ApplyTransform();
		UINT GetResolution() { return mResolution; }
		//void ApplyRotation();

	private:
		XMMATRIX GetLightProjectionMatrixInFrustum(int index, Frustum& cameraFrustum, DirectionalLight& light);

		Game& mGame;
		Camera& mCamera;
		DirectionalLight& mDirectionalLight;

		std::vector<DepthMap*> mShadowMaps;
		std::vector<Projector*> mLightProjectors;
		std::vector<Frustum> mCameraCascadesFrustums;
		std::vector<XMFLOAT3> mLightProjectorCenteredPositions;

		XMMATRIX mShadowMapViewMatrix;
		XMMATRIX mShadowMapProjectionMatrix;
		ID3D11RasterizerState* mShadowRasterizerState;
		UINT mResolution = 0;
		bool mIsCascaded = true;
	};
}