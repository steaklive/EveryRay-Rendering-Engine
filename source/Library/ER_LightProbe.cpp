#include "ER_LightProbe.h"
#include "Skybox.h"
#include "Game.h"
#include "ShadowMapper.h"
#include "Game.h"
#include "GameTime.h"
#include "GameException.h"
#include "Utility.h"
#include "MatrixHelper.h"
#include "MaterialHelper.h"
#include "VectorHelper.h"
#include "DirectionalLight.h"
#include "QuadRenderer.h"
#include "Material.h"
#include "StandardLightingMaterial.h"

namespace Library
{
	//X+, X-, Y+, Y-, Z+, Z-
	static const XMFLOAT3 cubemapFacesDirections[CUBEMAP_FACES_COUNT] = {
		Vector3Helper::Right,
		Vector3Helper::Left,
		Vector3Helper::Up,
		Vector3Helper::Down,
		Vector3Helper::Backward,
		Vector3Helper::Forward
	};
	static const XMFLOAT3 cubemapUpDirections[CUBEMAP_FACES_COUNT] = {
		Vector3Helper::Up,
		Vector3Helper::Up,
		Vector3Helper::Forward,
		Vector3Helper::Backward,
		Vector3Helper::Up,
		Vector3Helper::Up
	};

	ER_LightProbe::ER_LightProbe(Game& game, DirectionalLight& light, ShadowMapper& shadowMapper, int size, ER_ProbeType aType)
		: mSize(size)
		, mDirectionalLight(light)
		, mShadowMapper(shadowMapper)
		, mProbeType(aType)
	{
		mCubemapTexture = new ER_GPUTexture(game.Direct3DDevice(), size, size, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0,
			(aType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);

		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mCubemapCameras[i] = new Camera(game, XM_PIDIV2, 1.0f, 0.1f, 100000.0f);
			mCubemapCameras[i]->Initialize();
			mCubemapCameras[i]->SetDirection(cubemapFacesDirections[i]);
			mCubemapCameras[i]->SetUp(cubemapUpDirections[i]);
			mCubemapCameras[i]->UpdateViewMatrix(true); //fix for mirrored result due to right-hand coordinate system
			mCubemapCameras[i]->UpdateProjectionMatrix(true); //fix for mirrored result due to right-hand coordinate system
		}

		mConvolutionCB.Initialize(game.Direct3DDevice());
	}

	ER_LightProbe::~ER_LightProbe()
	{
		ReleaseObject(mLinearSamplerState);
		DeleteObject(mCubemapTexture);
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			DeleteObject(mCubemapCameras[i]);
		}
		mConvolutionCB.Release();
	}

	void ER_LightProbe::SetPosition(const XMFLOAT3& pos)
	{
		mPosition = pos;
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mCubemapCameras[i]->SetPosition(pos);
			mCubemapCameras[i]->UpdateViewMatrix(true); //fix for mirrored result due to right-hand coordinate system
			mCubemapCameras[i]->UpdateProjectionMatrix(true); //fix for mirrored result due to right-hand coordinate system
		}
	}

	void ER_LightProbe::CPUCullAgainstProbeBoundingVolume(int volumeIndex, const XMFLOAT3& aMin, const XMFLOAT3& aMax)
	{
		mIsCulled[volumeIndex] =
			(mPosition.x > aMax.x || mPosition.x < aMin.x) ||
			(mPosition.y > aMax.y || mPosition.y < aMin.y) ||
			(mPosition.z > aMax.z || mPosition.z < aMin.z);
	}

	void ER_LightProbe::Compute(Game& game, const GameTime& gameTime, ER_GPUTexture* aTextureNonConvoluted, ER_GPUTexture* aTextureConvoluted,
		DepthTarget** aDepthBuffers, const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, QuadRenderer* quadRenderer, Skybox* skybox)
	{
		if (mIsProbeLoadedFromDisk)
			return;

		bool isGlobal = (mIndex == -1);
		std::string materialListenerName = ((mProbeType == DIFFUSE_PROBE) ? "diffuse_" : "specular_") + MaterialHelper::forwardLightingForProbesMaterialName;

		if (!isGlobal)
		{
			for (int cubemapFaceIndex = 0; cubemapFaceIndex < CUBEMAP_FACES_COUNT; cubemapFaceIndex++)
				for (auto& object : objectsToRender)
					object.second->MeshMaterialVariablesUpdateEvent->AddListener(materialListenerName + "_" + std::to_string(cubemapFaceIndex),
						[&, cubemapFaceIndex](int meshIndex) { UpdateStandardLightingPBRProbeMaterialVariables(object.second, meshIndex, cubemapFaceIndex); });
		}

		auto context = game.Direct3DDeviceContext();
		UINT viewportsCount = 1;
		CD3D11_VIEWPORT oldViewPort;
		context->RSGetViewports(&viewportsCount, &oldViewPort);
		CD3D11_VIEWPORT newViewPort(0.0f, 0.0f, static_cast<float>(mSize), static_cast<float>(mSize));
		context->RSSetViewports(1, &newViewPort);

		//sadly, we can't combine or multi-thread these two functions, because of the artifacts on edges of the convoluted faces of the cubemap...
		DrawGeometryToProbe(game, gameTime, aTextureNonConvoluted, aDepthBuffers, objectsToRender, skybox);
		ConvoluteProbe(game, quadRenderer, aTextureNonConvoluted, aTextureConvoluted);

		context->RSSetViewports(1, &oldViewPort);

		if (!isGlobal)
		{
			for (int cubeMapFaceIndex = 0; cubeMapFaceIndex < CUBEMAP_FACES_COUNT; cubeMapFaceIndex++)
				for (auto& object : objectsToRender)
					object.second->MeshMaterialVariablesUpdateEvent->RemoveListener(materialListenerName + "_" + std::to_string(cubeMapFaceIndex));
		}
		SaveProbeOnDisk(game, levelPath, aTextureConvoluted);
		mIsProbeLoadedFromDisk = true;
	}

	void ER_LightProbe::DrawGeometryToProbe(Game& game, const GameTime& gameTime, ER_GPUTexture* aTextureNonConvoluted, DepthTarget** aDepthBuffers,
		const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox)
	{
		auto context = game.Direct3DDeviceContext();
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		bool isGlobal = (mIndex == -1);

		std::string materialListenerName = ((mProbeType == DIFFUSE_PROBE) ? "diffuse_" : "specular_") + MaterialHelper::forwardLightingForProbesMaterialName;

		//draw world to probe
		for (int cubeMapFace = 0; cubeMapFace < CUBEMAP_FACES_COUNT; cubeMapFace++)
		{
			// Set the render target and clear it.
			int rtvShift = (mProbeType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT;
			context->OMSetRenderTargets(1, &aTextureNonConvoluted->GetRTVs()[cubeMapFace * rtvShift], aDepthBuffers[cubeMapFace]->getDSV());
			context->ClearRenderTargetView(aTextureNonConvoluted->GetRTVs()[cubeMapFace], clearColor);
			context->ClearDepthStencilView(aDepthBuffers[cubeMapFace]->getDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			//rendering objects and sky
			{
				if (skybox)
				{
					skybox->Update(gameTime, mCubemapCameras[cubeMapFace]);
					skybox->Draw(mCubemapCameras[cubeMapFace]);
					//TODO draw sun
					//...
					//skybox->UpdateSun(gameTime, mCubemapCameras[cubeMapFace]);
				}

				if (isGlobal)
					continue;

				// We don't do lodding because it is bound to main camera... We force pick lod 0.
				// This is incorrect and might cause issues like: 
				// Probe P is next to object A, but object A is far from main camera => A does not have lod 0, probe P can not render A.
				const int lod = 0;

				//TODO change to culled objects per face (not a priority since we compute probes once)
				for (auto& object : objectsToRender)
				{
					if (object.second->IsInLightProbe())
						object.second->DrawLOD(materialListenerName + "_" + std::to_string(cubeMapFace), false, -1, lod);
				}
			}
		}

		if (mProbeType == SPECULAR_PROBE)
			context->GenerateMips(aTextureNonConvoluted->GetSRV());
	}

	void ER_LightProbe::ConvoluteProbe(Game& game, QuadRenderer* quadRenderer, ER_GPUTexture* aTextureNonConvoluted, ER_GPUTexture* aTextureConvoluted)
	{
		int mipCount = -1;
		if (mProbeType == SPECULAR_PROBE)
			mipCount = SPECULAR_PROBE_MIP_COUNT;

		auto context = game.Direct3DDeviceContext();
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		int totalMips = (mipCount == -1) ? 1 : mipCount;
		int rtvShift = (mProbeType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT;

		ID3D11ShaderResourceView* SRVs[1] = { aTextureNonConvoluted->GetSRV() };
		ID3D11SamplerState* SSs[1] = { mLinearSamplerState };

		for (int cubeMapFace = 0; cubeMapFace < CUBEMAP_FACES_COUNT; cubeMapFace++)
		{
			int currentSize = mSize;

			for (int mip = 0; mip < totalMips; mip++)
			{
				mConvolutionCB.Data.FaceIndex = cubeMapFace;
				mConvolutionCB.Data.MipIndex = (mProbeType == DIFFUSE_PROBE) ? -1 : mip;
				mConvolutionCB.ApplyChanges(context);

				CD3D11_VIEWPORT newViewPort(0.0f, 0.0f, static_cast<float>(currentSize), static_cast<float>(currentSize));
				context->RSSetViewports(1, &newViewPort);

				context->OMSetRenderTargets(1, &aTextureConvoluted->GetRTVs()[cubeMapFace * rtvShift + mip], NULL);
				context->ClearRenderTargetView(aTextureConvoluted->GetRTVs()[cubeMapFace * rtvShift + mip], clearColor);

				ID3D11Buffer* CBs[1] = { mConvolutionCB.Buffer() };

				context->IASetInputLayout(mInputLayout);
				context->VSSetShader(mConvolutionVS, NULL, NULL);
				context->VSSetConstantBuffers(0, 1, CBs);

				context->PSSetShader(mConvolutionPS, NULL, NULL);
				context->PSSetSamplers(0, 1, SSs);
				context->PSSetShaderResources(0, 1, SRVs);
				context->PSSetConstantBuffers(0, 1, CBs);

				if (quadRenderer)
					quadRenderer->Draw(context);

				currentSize >>= 1;
			}
		}
	}

	bool ER_LightProbe::LoadProbeFromDisk(Game& game, const std::wstring& levelPath)
	{
		std::wstring probeName = GetConstructedProbeName(levelPath);

		ID3D11Texture2D* tex2d = NULL;
		ID3D11ShaderResourceView* srv = NULL;
		ID3D11Resource* resourceTex = NULL;

		if (FAILED(DirectX::CreateDDSTextureFromFile(game.Direct3DDevice(), probeName.c_str(), &resourceTex, &srv)))
		{
			//std::wstring status = L"Failed to load DDS probe texture: " + probeName;
			//TODO output to LOG (not exception)
			mIsProbeLoadedFromDisk = false;
		}
		else
		{

			if (FAILED(resourceTex->QueryInterface(IID_ID3D11Texture2D, (void**)&tex2d))) {
				mIsProbeLoadedFromDisk = false;
				return false;
			}

			// Copying loaded resource into the convoluted resource of the probe
			//int mips = (mProbeType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT;
			//const int srcDiffuseAutogeneratedMipCount = 6;
			//for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
			//{
			//	for (int mip = 0; mip < mips; mip++)
			//	{
			//		game.Direct3DDeviceContext()->CopySubresourceRegion(mCubemapTexture->getTexture2D(),
			//			D3D11CalcSubresource(mip, i, mips), 0, 0, 0, resourceTex,
			//			D3D11CalcSubresource(mip, i, (mProbeType == DIFFUSE_PROBE) ? srcDiffuseAutogeneratedMipCount : mips), NULL);
			//	}
			//}

			mCubemapTexture->SetSRV(srv);
			mCubemapTexture->SetTexture2D(tex2d);

			resourceTex->Release();

			mIsProbeLoadedFromDisk = true;
		}
		return mIsProbeLoadedFromDisk;
	}

	void ER_LightProbe::SaveProbeOnDisk(Game& game, const std::wstring& levelPath, ER_GPUTexture* aTextureConvoluted)
	{
		DirectX::ScratchImage tempImage;
		HRESULT res = DirectX::CaptureTexture(game.Direct3DDevice(), game.Direct3DDeviceContext(), aTextureConvoluted->GetTexture2D(), tempImage);
		if (FAILED(res))
			throw GameException("Failed to capture a probe texture when saving!", res);

		std::wstring fileName = GetConstructedProbeName(levelPath);

		res = DirectX::SaveToDDSFile(tempImage.GetImages(), tempImage.GetImageCount(), tempImage.GetMetadata(), DDS_FLAGS_NONE, fileName.c_str());
		if (FAILED(res))
		{
			std::string str(fileName.begin(), fileName.end());
			std::string msg = "Failed to save a probe texture: " + str;
			throw GameException(msg.c_str());
		}

		//loading the same probe from disk, since aTextureConvoluted is a temp texture and otherwise we need a GPU resource copy to mCubemapTexture (better than this, but I am just too lazy...)
		if (!LoadProbeFromDisk(game, levelPath))
			throw GameException("Could not load probe that was already generated :(");
	}

	std::wstring ER_LightProbe::GetConstructedProbeName(const std::wstring& levelPath)
	{
		std::wstring fileName = levelPath;
		if (mProbeType == DIFFUSE_PROBE)
			fileName += L"diffuse_probe";
		else if (mProbeType == SPECULAR_PROBE)
			fileName += L"specular_probe";

		if (mIndex != -1)
			fileName += L"_"
			+ std::to_wstring(static_cast<int>(mPosition.x)) + L"_"
			+ std::to_wstring(static_cast<int>(mPosition.y)) + L"_"
			+ std::to_wstring(static_cast<int>(mPosition.z)) + L".dds";
		else
			fileName += L"_global.dds";

		return fileName;
	}

	void ER_LightProbe::UpdateStandardLightingPBRProbeMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, int cubeFaceIndex)
	{
		assert(mCubemapCameras);

		XMMATRIX worldMatrix = XMLoadFloat4x4(&(obj->GetTransformationMatrix4X4()));
		XMMATRIX vp = mCubemapCameras[cubeFaceIndex]->ViewMatrix() * mCubemapCameras[cubeFaceIndex]->ProjectionMatrix();

		XMMATRIX shadowMatrices[3] =
		{
			mShadowMapper.GetViewMatrix(0) * mShadowMapper.GetProjectionMatrix(0) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper.GetViewMatrix(1) * mShadowMapper.GetProjectionMatrix(1) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper.GetViewMatrix(2) * mShadowMapper.GetProjectionMatrix(2) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix())
		};

		ID3D11ShaderResourceView* shadowMaps[3] =
		{
			mShadowMapper.GetShadowTexture(0),
			mShadowMapper.GetShadowTexture(1),
			mShadowMapper.GetShadowTexture(2)
		};

		std::string materialName = ((mProbeType == DIFFUSE_PROBE) ? "diffuse_" : "specular_") + MaterialHelper::forwardLightingForProbesMaterialName + "_" + std::to_string(cubeFaceIndex);
		auto material = static_cast<Rendering::StandardLightingMaterial*>(obj->GetMaterials()[materialName]);

		if (material)
		{
			if (mProbeType == DIFFUSE_PROBE)
			{
				if (obj->IsInstanced())
					material->SetCurrentTechnique(material->GetEffect()->TechniquesByName().at("standard_lighting_pbr_diffuse_probes_instancing"));
				else
					material->SetCurrentTechnique(material->GetEffect()->TechniquesByName().at("standard_lighting_pbr_diffuse_probes"));
			}
			else
			{
				if (obj->IsInstanced())
					material->SetCurrentTechnique(material->GetEffect()->TechniquesByName().at("standard_lighting_pbr_specular_probes_instancing"));
				else
					material->SetCurrentTechnique(material->GetEffect()->TechniquesByName().at("standard_lighting_pbr_specular_probes"));
			}

			material->ViewProjection() << vp;
			material->World() << worldMatrix;
			material->ShadowMatrices().SetMatrixArray(shadowMatrices, 0, NUM_SHADOW_CASCADES);
			material->CameraPosition() << mCubemapCameras[cubeFaceIndex]->PositionVector();
			material->SunDirection() << XMVectorNegate(mDirectionalLight.DirectionVector());
			material->SunColor() << XMVECTOR{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, 5.0f };
			material->AmbientColor() << XMVECTOR{ mDirectionalLight.GetAmbientLightColor().x,  mDirectionalLight.GetAmbientLightColor().y, mDirectionalLight.GetAmbientLightColor().z , 1.0f };
			material->ShadowTexelSize() << XMVECTOR{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
			material->ShadowCascadeDistances() << XMVECTOR{
				mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(0),
				mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(1),
				mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(2), 1.0f };
			material->AlbedoTexture() << obj->GetTextureData(meshIndex).AlbedoMap;
			material->NormalTexture() << obj->GetTextureData(meshIndex).NormalMap;
			material->SpecularTexture() << obj->GetTextureData(meshIndex).SpecularMap;
			material->RoughnessTexture() << obj->GetTextureData(meshIndex).RoughnessMap;
			material->MetallicTexture() << obj->GetTextureData(meshIndex).MetallicMap;
			material->CascadedShadowTextures().SetResourceArray(shadowMaps, 0, NUM_SHADOW_CASCADES);
		}
	}
}