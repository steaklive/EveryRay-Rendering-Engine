#pragma once
#include "Common.h"
#include "ConstantBuffer.h"
#include "CustomRenderTarget.h"

#define VCT_SCENE_VOLUME_SIZE 256
#define VCT_MIPS 6

namespace Library
{
	class FullScreenRenderTarget;
	class DirectionalLight;
	class GameComponent;
	class GameTime;
	class Camera;

	namespace IlluminationCBufferData {
		struct FrameCB
		{
			XMMATRIX	InvProj;
			XMMATRIX	InvView;
			XMVECTOR	LightDir;
			XMVECTOR	LightCol;
			XMVECTOR	CameraPos;
			XMFLOAT2	Resolution;
		};

		struct CloudsCB
		{
			XMVECTOR AmbientColor;
			XMVECTOR WindDir;
			float WindSpeed;
			float Time;
			float Crispiness;
			float Curliness;
			float Coverage;
			float Absorption;
			float CloudsBottomHeight;
			float CloudsTopHeight;
			float DensityFactor;
		};
	}

	class Illumination : public GameComponent
	{
	public:
		Illumination(Game& game, Camera& camera, DirectionalLight& light);
		~Illumination();

		void Initialize();

		void Draw(const GameTime& gameTime);
		void Update(const GameTime& gameTime);

		void SetShadowMapSRV(ID3D11ShaderResourceView* srv) { mShadowMapSRV = srv; }
	private:
		void UpdateImGui();

		Camera& mCamera;
		DirectionalLight& mDirectionalLight;

		//ConstantBuffer<VolumetricCloudsCBufferData::FrameCB> mFrameConstantBuffer;
		//ConstantBuffer<VolumetricCloudsCBufferData::CloudsCB> mCloudsConstantBuffer;

		CustomRenderTarget* mVCTVoxelization3DRT = nullptr;
		CustomRenderTarget* mVCTVoxelizationDebugRT = nullptr;
		CustomRenderTarget* mVCTMainRT = nullptr;
		CustomRenderTarget* mVCTMainUpsampleAndBlurRT = nullptr;

		ID3D11VertexShader* mVCTVoxelizationVS = nullptr;
		ID3D11GeometryShader* mVCTVoxelizationGS = nullptr;
		ID3D11PixelShader* mVCTVoxelizationPS = nullptr;
		ID3D11ComputeShader* mVCTMainCS = nullptr;

		ID3D11ShaderResourceView* mShadowMapSRV = nullptr;

		ID3D11SamplerState* mCloudSS = nullptr;
		ID3D11SamplerState* mWeatherSS = nullptr;

		ID3D11InputLayout* mVCTVoxelizationInputLayout = nullptr;
	};
}