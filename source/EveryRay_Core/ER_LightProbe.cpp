#include <iostream>
#include <fstream>

#include "ER_LightProbe.h"
#include "ER_LightProbesManager.h"
#include "ER_Skybox.h"
#include "ER_Camera.h"
#include "ER_Core.h"
#include "ER_ShadowMapper.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_CoreException.h"
#include "ER_Utility.h"
#include "ER_MatrixHelper.h"
#include "ER_MaterialHelper.h"
#include "ER_VectorHelper.h"
#include "ER_DirectionalLight.h"
#include "ER_QuadRenderer.h"
#include "ER_RenderToLightProbeMaterial.h"
#include "ER_MaterialsCallbacks.h"

#define DIFFUSE_PROBE 0
#define SPECULAR_PROBE 1

namespace EveryRay_Core
{
	//X+, X-, Y+, Y-, Z+, Z-
	static const XMFLOAT3 cubemapFacesDirections[CUBEMAP_FACES_COUNT] = {
		ER_Vector3Helper::Right,
		ER_Vector3Helper::Left,
		ER_Vector3Helper::Up,
		ER_Vector3Helper::Down,
		ER_Vector3Helper::Backward,
		ER_Vector3Helper::Forward
	};
	static const XMFLOAT3 cubemapUpDirections[CUBEMAP_FACES_COUNT] = {
		ER_Vector3Helper::Up,
		ER_Vector3Helper::Up,
		ER_Vector3Helper::Forward,
		ER_Vector3Helper::Backward,
		ER_Vector3Helper::Up,
		ER_Vector3Helper::Up
	};

	ER_LightProbe::ER_LightProbe(ER_Core& game, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper, int size, int aProbeType, int index)
		: 
		mSize(size),
		mProbeType(aProbeType),
		mIndex(index),
		mDirectionalLight(light),
		mShadowMapper(shadowMapper)
	{
		Initialize(game, light, shadowMapper, size, aProbeType, index);
	}

	ER_LightProbe::~ER_LightProbe()
	{
		DeleteObject(mCubemapTexture);
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			DeleteObject(mCubemapCameras[i]);
		}
		mConvolutionCB.Release();
	}

	void ER_LightProbe::Initialize(ER_Core& game, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper, int size, int aProbeType, int index)
	{
		ER_RHI* rhi = game.GetRHI();

		for (int i = 0; i < SPHERICAL_HARMONICS_COEF_COUNT; i++)
			mSphericalHarmonicsRGB.push_back(XMFLOAT3(0.0, 0.0, 0.0));

		std::wstring probeType = (aProbeType == DIFFUSE_PROBE) ? L"diffuse" : L"specular";
		mCubemapTexture = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Light Probe Cubemap " + probeType + L" #" + std::to_wstring(mIndex));
		mCubemapTexture->CreateGPUTextureResource(rhi, size, size, 1, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_NONE, (aProbeType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);

		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mCubemapCameras[i] = new ER_Camera(game, XM_PIDIV2, 1.0f, 0.1f, 100000.0f);
			mCubemapCameras[i]->Initialize();
			mCubemapCameras[i]->SetDirection(cubemapFacesDirections[i]);
			mCubemapCameras[i]->SetUp(cubemapUpDirections[i]);
			mCubemapCameras[i]->UpdateViewMatrix(true); //fix for mirrored result due to right-hand coordinate system
			mCubemapCameras[i]->UpdateProjectionMatrix(true); //fix for mirrored result due to right-hand coordinate system
		}

		mConvolutionCB.Initialize(rhi, "ER_RHI_GPUBuffer: Light Probe Convolution CB");
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

	void ER_LightProbe::CPUCullAgainstProbeBoundingVolume(const XMFLOAT3& aMin, const XMFLOAT3& aMax)
	{
		mIsCulled =
			(mPosition.x > aMax.x || mPosition.x < aMin.x) ||
			(mPosition.y > aMax.y || mPosition.y < aMin.y) ||
			(mPosition.z > aMax.z || mPosition.z < aMin.z);
	}

	void ER_LightProbe::StoreSphericalHarmonicsFromCubemap(ER_Core& game, ER_RHI_GPUTexture* aTextureConvoluted)
	{
		if (game.GetRHI()->GetAPI() != ER_GRAPHICS_API::DX11)
			throw ER_CoreException("Storing SH from cubemap is only available on DX11 at the moment.");

		assert(aTextureConvoluted);

		ER_RHI* rhi = game.GetRHI();

		float rgbCoefficients[3][9] = { 
			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
		};
		if (!rhi->ProjectCubemapToSH(aTextureConvoluted, SPHERICAL_HARMONICS_ORDER + 1, &rgbCoefficients[0][0], &rgbCoefficients[1][0], &rgbCoefficients[2][0]))
		{
			ER_OUTPUT_LOG(L"[ER Logger][ER_LightProbe] Could not project a cubemap to spherical harmonics. Storing empty coefficients... \n");
			for (int i = 0; i < SPHERICAL_HARMONICS_COEF_COUNT; i++)
				mSphericalHarmonicsRGB[i] = XMFLOAT3(0.0, 0.0, 0.0);
		}
		else
		{
			for (int i = 0; i < SPHERICAL_HARMONICS_COEF_COUNT; i++)
				mSphericalHarmonicsRGB[i] = XMFLOAT3(rgbCoefficients[0][i], rgbCoefficients[1][i], rgbCoefficients[2][i]);
		}
	}

	void ER_LightProbe::Compute(ER_Core& game, ER_RHI_GPUTexture* aTextureNonConvoluted, ER_RHI_GPUTexture* aTextureConvoluted,
		ER_RHI_GPUTexture** aDepthBuffers, const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, ER_QuadRenderer* quadRenderer, ER_Skybox* skybox)
	{
		if (game.GetRHI()->GetAPI() != ER_GRAPHICS_API::DX11)
			throw ER_CoreException("Computing light probes is only available on DX11 at the moment.");

		if (mIsProbeLoadedFromDisk)
			return;

		assert(quadRenderer);
		ER_RHI* rhi = game.GetRHI();

		ER_RHI_Viewport oldViewport = rhi->GetCurrentViewport();

		ER_RHI_Viewport newViewPort;
		newViewPort.TopLeftX = 0.0f;
		newViewPort.TopLeftY = 0.0f;
		newViewPort.Width = static_cast<float>(mSize);
		newViewPort.Height = static_cast<float>(mSize);
		rhi->SetViewport(newViewPort);

		//sadly, we can't combine or multi-thread these two functions, because of the artifacts on edges of the convoluted faces of the cubemap...
		DrawGeometryToProbe(game, aTextureNonConvoluted, aDepthBuffers, objectsToRender, skybox);
		ConvoluteProbe(game, quadRenderer, aTextureNonConvoluted, aTextureConvoluted);

		rhi->SetViewport(oldViewport);

		if (mProbeType == DIFFUSE_PROBE && mIndex != -1)
			StoreSphericalHarmonicsFromCubemap(game, aTextureConvoluted);

		SaveProbeOnDisk(game, levelPath, aTextureConvoluted);

		mIsProbeLoadedFromDisk = true;

		{
			std::wstring probeName = GetConstructedProbeName(levelPath, mProbeType == DIFFUSE_PROBE && mIndex != -1);
			std::wstring msg = L"[ER Logger][ER_LightProbe] Finished computing and saving the probe: " + probeName + L'\n';
			ER_OUTPUT_LOG(msg.c_str());
		}
	}

	void ER_LightProbe::DrawGeometryToProbe(ER_Core& game, ER_RHI_GPUTexture* aTextureNonConvoluted, ER_RHI_GPUTexture** aDepthBuffers,
		const LightProbeRenderingObjectsInfo& objectsToRender, ER_Skybox* skybox)
	{
		if (game.GetRHI()->GetAPI() != ER_GRAPHICS_API::DX11)
			throw ER_CoreException("Rendering to light probes is only available on DX11 at the moment.");

		ER_RHI* rhi = game.GetRHI();
		bool isGlobal = (mIndex == -1);

		std::string materialListenerName = ((mProbeType == DIFFUSE_PROBE) ? "diffuse_" : "specular_") + ER_MaterialHelper::renderToLightProbeMaterialName;
		
		ER_MaterialSystems matSystems;
		matSystems.mDirectionalLight = mDirectionalLight;
		matSystems.mShadowMapper = mShadowMapper;

		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//draw world to probe
		for (int cubeMapFaceIndex = 0; cubeMapFaceIndex < CUBEMAP_FACES_COUNT; cubeMapFaceIndex++)
		{
			// Set the render target and clear it.
			int rtvShift = (mProbeType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT;
			rhi->SetRenderTargets({ aTextureNonConvoluted }, aDepthBuffers[cubeMapFaceIndex], nullptr, cubeMapFaceIndex * rtvShift);
			rhi->ClearRenderTarget(aTextureNonConvoluted, clearColorBlack, cubeMapFaceIndex);
			rhi->ClearDepthStencilTarget(aDepthBuffers[cubeMapFaceIndex], 1.0f, 0);

			//rendering objects and sky
			{
				if (skybox)
				{
					skybox->Update(game.GetCoreTime(), mCubemapCameras[cubeMapFaceIndex]);
					skybox->Draw(aTextureNonConvoluted, mCubemapCameras[cubeMapFaceIndex], aDepthBuffers[cubeMapFaceIndex]);
					//TODO draw sun
					//...
					//skybox->UpdateSun(game.GetCoreTime(), mCubemapCameras[cubeMapFace]);
				}

				// We don't do lodding because it is bound to main camera... We force pick lod 0.
				// This is incorrect and might cause issues like: 
				// Probe P is next to object A, but object A is far from main camera => A does not have lod 0, probe P can not render A.
				const int lod = 0;

				//TODO change to culled objects per face (not a priority since we compute probes once)
				for (auto& object : objectsToRender)
				{
					if (isGlobal && !object.second->IsUsedForGlobalLightProbeRendering())
						continue;

					if (!object.second->IsInLightProbe())
						continue;
				
					auto materialInfo = object.second->GetMaterials().find(materialListenerName + "_" + std::to_string(cubeMapFaceIndex));
					if (materialInfo != object.second->GetMaterials().end())
					{
						for (int meshIndex = 0; meshIndex < object.second->GetMeshCount(); meshIndex++)
						{
							materialInfo->second->PrepareShaders();
							static_cast<ER_RenderToLightProbeMaterial*>(materialInfo->second)->PrepareForRendering(matSystems, object.second, meshIndex, mCubemapCameras[cubeMapFaceIndex], nullptr);
							object.second->DrawLOD(materialInfo->first, false, meshIndex, lod, true);
						}
					}
				}
			}
		}

		if (mProbeType == SPECULAR_PROBE)
			rhi->GenerateMips(aTextureNonConvoluted, nullptr);
	}

	void ER_LightProbe::ConvoluteProbe(ER_Core& game, ER_QuadRenderer* quadRenderer, ER_RHI_GPUTexture* aTextureNonConvoluted, ER_RHI_GPUTexture* aTextureConvoluted)
	{
		if (game.GetRHI()->GetAPI() != ER_GRAPHICS_API::DX11)
			throw ER_CoreException("Convoluting light probes is only available on DX11 at the moment.");

		int mipCount = -1;
		if (mProbeType == SPECULAR_PROBE)
			mipCount = SPECULAR_PROBE_MIP_COUNT;

		ER_RHI* rhi = game.GetRHI();

		int totalMips = (mipCount == -1) ? 1 : mipCount;
		int rtvShift = (mProbeType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT;

		for (int cubeMapFace = 0; cubeMapFace < CUBEMAP_FACES_COUNT; cubeMapFace++)
		{
			int currentSize = mSize;

			for (int mip = 0; mip < totalMips; mip++)
			{
				mConvolutionCB.Data.FaceIndex = cubeMapFace;
				mConvolutionCB.Data.MipIndex = (mProbeType == DIFFUSE_PROBE) ? -1 : mip;
				mConvolutionCB.ApplyChanges(rhi);

				ER_RHI_Viewport newViewPort;
				newViewPort.TopLeftX = 0.0f;
				newViewPort.TopLeftY = 0.0f;
				newViewPort.Width = static_cast<float>(currentSize);
				newViewPort.Height = static_cast<float>(currentSize);
				rhi->SetViewport(newViewPort);

				rhi->SetRenderTargets({ aTextureConvoluted }, nullptr, nullptr, cubeMapFace * rtvShift + mip);
				rhi->ClearRenderTarget(aTextureConvoluted, clearColorBlack, cubeMapFace * rtvShift + mip);

				rhi->SetConstantBuffers(ER_VERTEX, { mConvolutionCB.Buffer() });

				rhi->SetShader(mConvolutionPS);
				rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
				rhi->SetShaderResources(ER_PIXEL, { aTextureNonConvoluted });
				rhi->SetConstantBuffers(ER_PIXEL, { mConvolutionCB.Buffer() });

				if (quadRenderer)
				{
					quadRenderer->PrepareDraw(rhi);
					quadRenderer->Draw(rhi);
				}

				currentSize >>= 1;
			}
		}

		rhi->UnbindRenderTargets();
		rhi->UnbindResourcesFromShader(ER_VERTEX);
		rhi->UnbindResourcesFromShader(ER_PIXEL);
	}

	void ER_LightProbe::SaveProbeOnDisk(ER_Core& game, const std::wstring& levelPath, ER_RHI_GPUTexture* aTextureConvoluted)
	{
		if (game.GetRHI()->GetAPI() != ER_GRAPHICS_API::DX11)
			throw ER_CoreException("Saving light probes is only available on DX11 at the moment.");

		bool saveAsSphericalHarmonics = mProbeType == DIFFUSE_PROBE && mIndex != -1;
		std::wstring probeName = GetConstructedProbeName(levelPath, saveAsSphericalHarmonics);

		if (saveAsSphericalHarmonics)
		{
			std::wofstream shFile(probeName.c_str());
			if (shFile.is_open())
			{
				std::string tempString = "";
				shFile << L"r ";
				for (int i = 0; i < SPHERICAL_HARMONICS_COEF_COUNT; i++)
				{
					tempString = std::to_string(mSphericalHarmonicsRGB[i].x);
					if (i == SPHERICAL_HARMONICS_COEF_COUNT - 1)
						tempString += "\n";
					else
						tempString += " ";
					shFile << ER_Utility::ToWideString(tempString);
				}

				shFile << L"g ";
				for (int i = 0; i < SPHERICAL_HARMONICS_COEF_COUNT; i++)
				{
					tempString = std::to_string(mSphericalHarmonicsRGB[i].y);
					if (i == SPHERICAL_HARMONICS_COEF_COUNT - 1)
						tempString += "\n";
					else
						tempString += " ";
					shFile << ER_Utility::ToWideString(tempString);
				}

				shFile << L"b ";
				for (int i = 0; i < SPHERICAL_HARMONICS_COEF_COUNT; i++)
				{
					tempString = std::to_string(mSphericalHarmonicsRGB[i].z);
					if (i == SPHERICAL_HARMONICS_COEF_COUNT - 1)
						tempString += "\n";
					else
						tempString += " ";
					shFile << ER_Utility::ToWideString(tempString);
				}
				shFile.close();
			}
			else
			{
				std::string str(probeName.begin(), probeName.end());
				std::string msg = "Failed to create a probe SH file: " + str;
				throw ER_CoreException(msg.c_str());
			}

		}
		else
		{
			game.GetRHI()->SaveGPUTextureToFile(aTextureConvoluted, probeName);

			//loading the same probe from disk, since aTextureConvoluted is a temp texture and otherwise we need a GPU resource copy to mCubemapTexture (better than this, but I am just too lazy...)
			if (!LoadProbeFromDisk(game, levelPath))
				throw ER_CoreException("Could not load probe that was already generated :(");
		}
	}

	// Method for loading probe from disk in 2 ways: spherical harmonics coefficients and light probe cubemap texture
	bool ER_LightProbe::LoadProbeFromDisk(ER_Core& game, const std::wstring& levelPath)
	{
		ER_RHI* rhi = game.GetRHI();

		bool loadAsSphericalHarmonics = mProbeType == DIFFUSE_PROBE && mIndex != -1;
		std::wstring probeName = GetConstructedProbeName(levelPath, loadAsSphericalHarmonics);

		if (loadAsSphericalHarmonics)
		{
			FILE* shFile = _wfopen(probeName.c_str(), L"r");
			if (shFile)
			{
				float coefficients[3][SPHERICAL_HARMONICS_COEF_COUNT] =
				{
					{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
					{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
					{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }
				};
				int channel = 0;
				char line[256];
				char channelSymbol;
				while (fgets(line, sizeof(line), shFile))
				{
					if (channel == 3)
						break;

					if (line[0] == 'r' || line[0] == 'g' || line[0] == 'b')
					{
						sscanf(line, "%c %f %f %f %f %f %f %f %f %f", &channelSymbol,
							&coefficients[channel][0], &coefficients[channel][1], &coefficients[channel][2],
							&coefficients[channel][3], &coefficients[channel][4], &coefficients[channel][5],
							&coefficients[channel][6], &coefficients[channel][7], &coefficients[channel][8]
						);
					}
					else
					{
						std::wstring message = L"[ER Logger][ER_LightProbe] Corrupt probe's spherical harmonics file: " + probeName + L". Loading empty coefficients... \n";
						ER_OUTPUT_LOG(message.c_str());
						for (int i = 0; i < SPHERICAL_HARMONICS_COEF_COUNT; i++)
							mSphericalHarmonicsRGB[i] = XMFLOAT3(0, 0, 0);

						break;
					}
					channel++;
				}

				for (int i = 0; i < SPHERICAL_HARMONICS_COEF_COUNT; i++)
					mSphericalHarmonicsRGB[i] = XMFLOAT3(coefficients[0][i], coefficients[1][i], coefficients[2][i]);

				std::wstring message = L"[ER Logger][ER_LightProbe] Successfully loaded probe's spherical harmonics file: " + probeName + L"\n";
				ER_OUTPUT_LOG(message.c_str());

				mIsProbeLoadedFromDisk = true;
				fclose(shFile);
			}
			else
			{
				DWORD error = GetLastError();
				std::wstring message = L"[ER Logger][ER_LightProbe] Could not load probe's spherical harmonics file: " + probeName + L". This probe will be recomputed and saved to disk. \n";
				ER_OUTPUT_LOG(message.c_str());
				mIsProbeLoadedFromDisk = false;
			}
		}
		else
		{
			assert(mCubemapTexture);
			mCubemapTexture->CreateGPUTextureResource(rhi, probeName, true, false, true, &mIsProbeLoadedFromDisk);
			if (!mIsProbeLoadedFromDisk)
			{
				std::wstring message = L"[ER Logger][ER_LightProbe] Could not load probe's texture file: " + probeName + L". This probe will be recomputed and saved to disk. \n";
				ER_OUTPUT_LOG(message.c_str());
			}
			else
			{
				std::wstring message = L"[ER Logger][ER_LightProbe] Successfully loaded specular probe's cubemap texture: " + probeName + L"\n";
				ER_OUTPUT_LOG(message.c_str());
			}
		}
		return mIsProbeLoadedFromDisk;
	}

	std::wstring ER_LightProbe::GetConstructedProbeName(const std::wstring& levelPath, bool inSphericalHarmonics)
	{
		std::wstring fileName = levelPath;
		if (mProbeType == DIFFUSE_PROBE)
			fileName += L"diffuse_probe";
		else if (mProbeType == SPECULAR_PROBE)
			fileName += L"specular_probe";

		if (mIndex != -1)
		{
			fileName += L"_"
				+ std::to_wstring(static_cast<int>(mPosition.x)) + L"_"
				+ std::to_wstring(static_cast<int>(mPosition.y)) + L"_"
				+ std::to_wstring(static_cast<int>(mPosition.z));

			if (!inSphericalHarmonics)
				fileName += L".dds";
			else
				fileName += L"_sh.txt";
		}
		else
			fileName += L"_global.dds";

		return fileName;
	}
}