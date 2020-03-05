#include "stdafx.h"

#pragma comment(lib, "D3DCompiler.lib")

#include <WinSDKVer.h>
#define _WIN32_WINNT 0x0600
#include <SDKDDKVer.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Common.h"

#include "Game.h"
#include "GameException.h"
#include "IBLRadianceMap.h"
#include "IBLCubemap.h"
#include "QuadRenderer.h"
#include "Utility.h"
#include "ShaderCompiler.h"
#include <DDSTextureLoader.h>

namespace Library
{

	IBLRadianceMap::IBLRadianceMap(Game& game, const std::wstring& cubemapFilename) :
		mCubeMapFileName(cubemapFilename),
		mEnvMapShaderResourceView(nullptr), 
		mQuadRenderer(nullptr), mIBLCubemap(nullptr), myGame(nullptr)
	{
		myGame = &game;
	}
	
	IBLRadianceMap::~IBLRadianceMap()
	{
		ReleaseObject(mEnvMapShaderResourceView);
		DeleteObject(myGame);
		mQuadRenderer.reset(nullptr);
		mIBLCubemap.reset(nullptr);
	}

	void IBLRadianceMap::Initialize()
	{
		assert(myGame != nullptr);

		ID3D11Device1* device = myGame->Direct3DDevice();
		ID3D11DeviceContext1* deviceContext = myGame->Direct3DDeviceContext();

		HRESULT hr = DirectX::CreateDDSTextureFromFile(myGame->Direct3DDevice(), mCubeMapFileName.c_str(), nullptr, &mEnvMapShaderResourceView);
		if (FAILED(hr))
		{
			throw GameException("Failed to load an Environment Map!", hr);
		}

		mQuadRenderer.reset(new QuadRenderer(device));
		mIBLCubemap.reset(new IBLCubemap(device, 7, 256));

		mConstantBuffer.Initialize(device);
	}

	void IBLRadianceMap::Create(Game& game)
	{
		ID3D11Device* device = game.Direct3DDevice();
		ID3D11DeviceContext* deviceContext = game.Direct3DDeviceContext();


		// vertex shader
		ID3DBlob* vertexShaderBlob = nullptr;
		HRESULT hrloadVS = ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\PBR\\RadianceMapVS.hlsl").c_str(), "main", "vs_5_0", &vertexShaderBlob);
		if (FAILED(hrloadVS)) throw GameException("Failed to load a shader: RadianceMapVS.hlsl!", hrloadVS);

		ID3D11VertexShader* vertexShader = NULL;
		HRESULT hrcreateVS = device->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), NULL, &vertexShader);
		if (FAILED(hrcreateVS)) throw GameException("Failed to create vertex shader from RadianceMapVS.hlsl!", hrcreateVS);

		//pixel shader
		ID3DBlob* pixelShaderBlob = nullptr;
		HRESULT hrloadPS = ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\PBR\\RadianceMapPS.hlsl").c_str(), "main", "ps_5_0", &pixelShaderBlob);
		if (FAILED(hrloadPS)) throw GameException("Failed to load a shader: RadianceMapPS.hlsl!", hrloadPS);

		ID3D11PixelShader* pixelShader = NULL;
		HRESULT hrcreatePS = device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), NULL, &pixelShader);
		if (FAILED(hrcreatePS)) throw GameException("Failed to create pixel shader from RadianceMapPS.hlsl!", hrcreatePS);

		
		D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[2];
		inputLayoutDesc[0].SemanticName = "POSITION";
		inputLayoutDesc[0].SemanticIndex = 0;
		inputLayoutDesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		inputLayoutDesc[0].InputSlot = 0;
		inputLayoutDesc[0].AlignedByteOffset = 0;
		inputLayoutDesc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		inputLayoutDesc[0].InstanceDataStepRate = 0;

		inputLayoutDesc[1].SemanticName = "TEXCOORD";
		inputLayoutDesc[1].SemanticIndex = 0;
		inputLayoutDesc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
		inputLayoutDesc[1].InputSlot = 0;
		inputLayoutDesc[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		inputLayoutDesc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		inputLayoutDesc[1].InstanceDataStepRate = 0;

		int numElements = sizeof(inputLayoutDesc) / sizeof(inputLayoutDesc[0]);

		ID3D11InputLayout* layout;

		device->CreateInputLayout(inputLayoutDesc, numElements, vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &layout);

		assert(layout != nullptr);

		deviceContext->PSSetShaderResources(0, 1, &mEnvMapShaderResourceView);
		deviceContext->IASetInputLayout(layout);


		// Set sampler state.
		ID3D11SamplerState* TrilinearWrap;

		D3D11_SAMPLER_DESC samplerStateDesc;
		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

		HRESULT hr = device->CreateSamplerState(&samplerStateDesc, &TrilinearWrap);
		if (FAILED(hr))
		{
			throw GameException("ID3D11Device::CreateSamplerState() failed.", hr);
		}
		deviceContext->PSSetSamplers(0, 1, &TrilinearWrap);

		// Set shaders
		deviceContext->VSSetShader(vertexShader, NULL, 0);
		deviceContext->PSSetShader(pixelShader, NULL, 0);

		// Draw faces
		for (int faceIndex = 0; faceIndex < 6; faceIndex++)
		{
			int size = 256;

			for (int mipIndex = 0; mipIndex < 7; mipIndex++)
			{
				CD3D11_VIEWPORT viewPort(0.0f, 0.0f, static_cast<float>(size), static_cast<float>(size));
				deviceContext->RSSetViewports(1, &viewPort);

				DrawFace(device, deviceContext, faceIndex, mipIndex, mIBLCubemap->mSurfaces[faceIndex]->mRenderTargets[mipIndex]);

				size /= 2;
			}
		}

		// reset buffer
		game.SetBackBuffer();
	}

	ID3D11ShaderResourceView** IBLRadianceMap:: GetShaderResourceViewAddress()
	{
		return mIBLCubemap->GetShaderResourceViewAddress();
	}

	void IBLRadianceMap::DrawFace(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
		int faceIndex, int mipIndex, ID3D11RenderTargetView* renderTarget)
	{
		mConstantBuffer.Data.Face = faceIndex;
		mConstantBuffer.Data.MipIndex = mipIndex;

		mConstantBuffer.ApplyChanges(deviceContext);
		mConstantBuffer.SetVSConstantBuffers(deviceContext);
		mConstantBuffer.SetPSConstantBuffers(deviceContext);

		float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

		// Set the render target and clear it.
		deviceContext->OMSetRenderTargets(1, &renderTarget, NULL);
		deviceContext->ClearRenderTargetView(renderTarget, clearColor);

		// Render the cubemap face.
		mQuadRenderer->Draw(deviceContext);
	}

}