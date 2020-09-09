#include "stdafx.h"

#include "ShadowMapper.h"
#include "Frustum.h"
#include "Projector.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "GameTime.h"
#include "Game.h"
#include "DepthMap.h"
#include "GameException.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <limits>

using namespace std;

namespace Library
{
	ShadowMapper::ShadowMapper(Game& pGame, Camera& camera, DirectionalLight& dirLight,  UINT pWidth, UINT pHeight, bool isCascaded)
		: GameComponent(pGame),
		mShadowMaps(0, nullptr), 
		mShadowRasterizerState(nullptr),
		mGame(pGame),
		mDirectionalLight(dirLight),
		mCamera(camera),
		mResolution(pWidth),
		mIsCascaded(isCascaded)
	{
		for (int i = 0; i < MAX_NUM_OF_CASCADES; i++)
		{
			mLightProjectorCenteredPositions.push_back(XMFLOAT3(0, 0, 0));
			mShadowMaps.push_back(new DepthMap(pGame, pWidth, pHeight));

			mCameraCascadesFrustums.push_back(XMMatrixIdentity());
			(isCascaded) ? mCameraCascadesFrustums[i].SetMatrix(mCamera.GetCustomViewProjectionMatrixForCascade(i)) : mCameraCascadesFrustums[i].SetMatrix(mCamera.ProjectionMatrix());

			mLightProjectors.push_back(new Projector(mGame));
			mLightProjectors[i]->Initialize();
			mLightProjectors[i]->SetProjectionMatrix(GetLightProjectionMatrixInFrustum(i, mCameraCascadesFrustums[i], mDirectionalLight));
			mLightProjectors[i]->ApplyRotation(mDirectionalLight.GetTransform());

		}

		D3D11_RASTERIZER_DESC rasterizerStateDesc;
		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.CullMode = D3D11_CULL_BACK;
		rasterizerStateDesc.DepthClipEnable = true;
		rasterizerStateDesc.DepthBias = 0.05f;
		rasterizerStateDesc.SlopeScaledDepthBias = 3.0f;
		rasterizerStateDesc.FrontCounterClockwise = false;
		HRESULT hr = mGame.Direct3DDevice()->CreateRasterizerState(&rasterizerStateDesc, &mShadowRasterizerState);
		if (FAILED(hr))
			throw GameException("ID3D11Device::CreateRasterizerState() failed while generating a shadow map.", hr);
	}

	ShadowMapper::~ShadowMapper()
	{
		DeletePointerCollection(mShadowMaps);
		DeletePointerCollection(mLightProjectors);
		ReleaseObject(mShadowRasterizerState);
	}

	void ShadowMapper::Update(const GameTime& gameTime)
	{
		for (size_t i = 0; i < MAX_NUM_OF_CASCADES; i++)
		{
			(mIsCascaded) ? mCameraCascadesFrustums[i].SetMatrix(mCamera.GetCustomViewProjectionMatrixForCascade(i)) : mCameraCascadesFrustums[i].SetMatrix(mCamera.ProjectionMatrix());

			mLightProjectors[i]->SetPosition(mLightProjectorCenteredPositions[i].x, mLightProjectorCenteredPositions[i].y, mLightProjectorCenteredPositions[i].z);
			mLightProjectors[i]->SetProjectionMatrix(GetLightProjectionMatrixInFrustum(i, mCameraCascadesFrustums[i], mDirectionalLight));
			mLightProjectors[i]->Update(gameTime);
		}
	}

	void ShadowMapper::BeginRenderingToShadowMap(int cascadeIndex)
	{
		assert(cascadeIndex < MAX_NUM_OF_CASCADES);

		mShadowMaps[cascadeIndex]->Begin();
		mGame.Direct3DDeviceContext()->ClearDepthStencilView(mShadowMaps[cascadeIndex]->DepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		mGame.Direct3DDeviceContext()->RSSetState(mShadowRasterizerState);
	}

	void ShadowMapper::StopRenderingToShadowMap(int cascadeIndex)
	{
		assert(cascadeIndex < MAX_NUM_OF_CASCADES);

		mShadowMaps[cascadeIndex]->End();
	}

	XMMATRIX ShadowMapper::GetViewMatrix(int cascadeIndex /*= 0*/)
	{
		assert(cascadeIndex < MAX_NUM_OF_CASCADES);
		return mLightProjectors.at(cascadeIndex)->ViewMatrix();
	}
	XMMATRIX ShadowMapper::GetProjectionMatrix(int cascadeIndex /*= 0*/)
	{
		assert(cascadeIndex < MAX_NUM_OF_CASCADES);
		return mLightProjectors.at(cascadeIndex)->ProjectionMatrix();
	}

	ID3D11ShaderResourceView* ShadowMapper::GetShadowTexture(int cascadeIndex)
	{
		assert(cascadeIndex < MAX_NUM_OF_CASCADES);
		return mShadowMaps.at(cascadeIndex)->OutputTexture();
	}

	//void ShadowMapper::ApplyRotation()
	//{
	//	for (int i = 0; i < MAX_NUM_OF_CASCADES; i++)
	//		mLightProjectors[i]->ApplyRotation(mDirectionalLight.GetTransform());
	//}	
	void ShadowMapper::ApplyTransform()
	{
		for (int i = 0; i < MAX_NUM_OF_CASCADES; i++)
			mLightProjectors[i]->ApplyTransform(mDirectionalLight.GetTransform());
	}

	XMMATRIX ShadowMapper::GetLightProjectionMatrixInFrustum(int index, Frustum& cameraFrustum, DirectionalLight& light)
	{
		assert(index < MAX_NUM_OF_CASCADES);

		//create corners
		XMFLOAT3 frustumCorners[9] = {};

		frustumCorners[0] = (cameraFrustum.Corners()[0]);
		frustumCorners[1] = (cameraFrustum.Corners()[1]);
		frustumCorners[2] = (cameraFrustum.Corners()[2]);
		frustumCorners[3] = (cameraFrustum.Corners()[3]);
		frustumCorners[4] = (cameraFrustum.Corners()[4]);
		frustumCorners[5] = (cameraFrustum.Corners()[5]);
		frustumCorners[6] = (cameraFrustum.Corners()[6]);
		frustumCorners[7] = (cameraFrustum.Corners()[7]);
		frustumCorners[8] = (cameraFrustum.Corners()[8]);

		XMFLOAT3 frustumCenter = { 0, 0, 0 };

		for (size_t i = 0; i < 8; i++)
		{
			frustumCenter = XMFLOAT3(frustumCenter.x + frustumCorners[i].x,
				frustumCenter.y + frustumCorners[i].y,
				frustumCenter.z + frustumCorners[i].z);
		}

		//calculate frustum's center position
		frustumCenter = XMFLOAT3(frustumCenter.x * (1.0f / 8.0f),
			frustumCenter.y * (1.0f / 8.0f),
			frustumCenter.z * (1.0f / 8.0f));

		mLightProjectorCenteredPositions[index] = frustumCenter;

		float minX = (std::numeric_limits<float>::max)();
		float maxX = (std::numeric_limits<float>::min)();
		float minY = (std::numeric_limits<float>::max)();
		float maxY = (std::numeric_limits<float>::min)();
		float minZ = (std::numeric_limits<float>::max)();
		float maxZ = (std::numeric_limits<float>::min)();

		for (int j = 0; j < 8; j++) {

			// Transform the frustum coordinate from world to light space
			XMVECTOR frustumCornerVector = XMLoadFloat3(&frustumCorners[j]);
			frustumCornerVector = XMVector3Transform(frustumCornerVector, (light.LightMatrix(frustumCenter)));

			XMStoreFloat3(&frustumCorners[j], frustumCornerVector);

			minX = min(minX, frustumCorners[j].x);
			maxX = max(maxX, frustumCorners[j].x);
			minY = min(minY, frustumCorners[j].y);
			maxY = max(maxY, frustumCorners[j].y);
			minZ = min(minZ, frustumCorners[j].z);
			maxZ = max(maxZ, frustumCorners[j].z);
		}

		//set orthographic proj with proper boundaries
		float delta = 5.0f;
		XMMATRIX projectionMatrix = XMMatrixOrthographicLH(maxX - minX + delta, maxY - minY + delta, maxZ, minZ);
		return projectionMatrix;
	}

}