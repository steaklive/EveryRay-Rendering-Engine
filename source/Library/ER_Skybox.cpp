#include "stdafx.h"

#include "ER_Skybox.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_CoreTime.h"
#include "ER_Camera.h"
#include "ER_MatrixHelper.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "ER_Utility.h"
#include "ER_QuadRenderer.h"
#include "ER_VertexDeclarations.h"

namespace Library
{
	ER_Skybox::ER_Skybox(ER_Core& game, ER_Camera& camera, float scale)
		: ER_CoreComponent(game), mCore(game), mCamera(camera),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mIndexCount(0),
		mWorldMatrix(ER_MatrixHelper::Identity), mScaleMatrix(ER_MatrixHelper::Identity), mScale(scale)
	{
		XMStoreFloat4x4(&mScaleMatrix, XMMatrixScaling(scale, scale, scale));
	}

	ER_Skybox::~ER_Skybox()
	{
		DeleteObject(mVertexBuffer);
		DeleteObject(mIndexBuffer);
		DeleteObject(mInputLayout);
		DeleteObject(mSkyboxVS);
		DeleteObject(mSkyboxPS);
		DeleteObject(mSunPS);
		DeleteObject(mSunOcclusionPS);
		mSunConstantBuffer.Release();
		mSkyboxConstantBuffer.Release();
	}

	void ER_Skybox::Initialize()
	{
		auto rhi = mCore.GetRHI();

		std::unique_ptr<ER_Model> model(new ER_Model(mCore, ER_Utility::GetFilePath("content\\models\\sphere_lowpoly.obj"), true));

		auto& meshes = model->Meshes();
		mVertexBuffer = rhi->CreateGPUBuffer();
		meshes[0].CreateVertexBuffer_Position(mVertexBuffer);
		mIndexBuffer = rhi->CreateGPUBuffer();
		meshes[0].CreateIndexBuffer(mIndexBuffer);
		mIndexCount = meshes[0].Indices().size();

		//skybox
		{
			ER_RHI_INPUT_ELEMENT_DESC inputElementDescriptions[] =
			{
				{ "POSITION", 0, ER_FORMAT_R32G32B32A32_FLOAT, 0, 0, true, 0 }
			};
			mInputLayout = rhi->CreateInputLayout(inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
			
			mSkyboxVS = rhi->CreateGPUShader();
			mSkyboxVS->CompileShader(rhi, "content\\shaders\\Skybox.hlsl", "VSMain", ER_VERTEX, mInputLayout);

			mSkyboxPS = rhi->CreateGPUShader();
			mSkyboxPS->CompileShader(rhi, "content\\shaders\\Skybox.hlsl", "PSMain", ER_PIXEL);		

			mSkyboxConstantBuffer.Initialize(rhi);
		}

		//sun
		{
			mSunPS = rhi->CreateGPUShader();
			mSunPS->CompileShader(rhi, "content\\shaders\\Sun.hlsl", "main", ER_PIXEL);

			mSunOcclusionPS = rhi->CreateGPUShader();
			mSunOcclusionPS->CompileShader(rhi, "content\\shaders\\Sun.hlsl", "occlusion", ER_PIXEL);

			mSunConstantBuffer.Initialize(rhi);
		}
	}

	void ER_Skybox::Update(ER_Camera* aCustomCamera)
	{
		XMFLOAT3 position;
		if (aCustomCamera)
			position = aCustomCamera->Position();
		else
			position = mCamera.Position();

		XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * XMMatrixTranslation(position.x, position.y, position.z));
	}

	void ER_Skybox::UpdateSun(const ER_CoreTime& gameTime, ER_Camera* aCustomCamera)
	{
		auto rhi = mCore.GetRHI();

		if (aCustomCamera)
		{
			mSunConstantBuffer.Data.InvProj = XMMatrixInverse(nullptr, aCustomCamera->ProjectionMatrix());
			mSunConstantBuffer.Data.InvView = XMMatrixInverse(nullptr, aCustomCamera->ViewMatrix());
		}
		else
		{
			mSunConstantBuffer.Data.InvProj = XMMatrixInverse(nullptr, mCamera.ProjectionMatrix());
			mSunConstantBuffer.Data.InvView = XMMatrixInverse(nullptr, mCamera.ViewMatrix());
		}
		mSunConstantBuffer.Data.SunDir = mSunDir;
		mSunConstantBuffer.Data.SunColor = mSunColor;
		mSunConstantBuffer.Data.SunBrightness = mSunBrightness;
		mSunConstantBuffer.Data.SunExponent = mSunExponent;
		mSunConstantBuffer.ApplyChanges(rhi);
	}

	void ER_Skybox::Draw(ER_Camera* aCustomCamera)
	{
		auto rhi = mCore.GetRHI();

		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		rhi->SetInputLayout(mInputLayout);
		rhi->SetVertexBuffers({ mVertexBuffer });
		rhi->SetIndexBuffer(mIndexBuffer);

		XMMATRIX wvp = XMMatrixIdentity();
		if (aCustomCamera)
			wvp = XMLoadFloat4x4(&mWorldMatrix) * aCustomCamera->ViewMatrix() * aCustomCamera->ProjectionMatrix();
		else
			wvp = XMLoadFloat4x4(&mWorldMatrix) * mCamera.ViewMatrix() * mCamera.ProjectionMatrix();

		mSkyboxConstantBuffer.Data.WorldViewProjection = XMMatrixTranspose(wvp);
		mSkyboxConstantBuffer.Data.SunColor = mSunColor;
		mSkyboxConstantBuffer.Data.BottomColor = mBottomColor;
		mSkyboxConstantBuffer.Data.TopColor = mTopColor;
		mSkyboxConstantBuffer.ApplyChanges(rhi);

		rhi->SetShader(mSkyboxVS);
		rhi->SetConstantBuffers(ER_VERTEX, { mSkyboxConstantBuffer.Buffer() });

		rhi->SetShader(mSkyboxPS);
		rhi->SetConstantBuffers(ER_PIXEL, { mSkyboxConstantBuffer.Buffer() });

		rhi->DrawIndexed(mIndexCount);

		rhi->UnbindResourcesFromShader(ER_VERTEX);
		rhi->UnbindResourcesFromShader(ER_PIXEL);
	}

	void ER_Skybox::DrawSun(ER_Camera* aCustomCamera, ER_RHI_GPUTexture* aSky, ER_RHI_GPUTexture* aSceneDepth)
	{
		auto rhi = mCore.GetRHI();

		assert(aSceneDepth);
		auto quadRenderer = (ER_QuadRenderer*)mCore.GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(quadRenderer);

		if (mDrawSun) {
			rhi->SetShader(mSunPS);
			rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
			rhi->SetConstantBuffers(ER_PIXEL, { mSunConstantBuffer.Buffer() });
			rhi->SetShaderResources(ER_PIXEL, { aSky, aSceneDepth });
			quadRenderer->Draw(rhi);
			rhi->UnbindResourcesFromShader(ER_PIXEL);
		}
	}

	XMFLOAT4 ER_Skybox::CalculateSunPositionOnSkybox(XMFLOAT3 dir, ER_Camera* aCustomCamera)
	{
		float t = 0.0f;

		XMFLOAT3 sphereCenter;
		if (aCustomCamera )
			sphereCenter = aCustomCamera->Position();
		else 
			sphereCenter = mCamera.Position();

		float radius2 = mScale * mScale;

		XMFLOAT3 L = XMFLOAT3(-sphereCenter.x,-sphereCenter.y,-sphereCenter.z);
		float a = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
		float b = 2.0f * (dir.x * L.x + dir.y * L.y + dir.z * L.z);
		float c = L.x * L.x + L.y * L.y + L.z * L.z - radius2;

		float discr = b * b - 4.0 * a * c;
		t = (-b + sqrt(discr)) / 2;

		return XMFLOAT4(dir.x * t, dir.y * t, dir.z * t, 1.0f);
	}

}