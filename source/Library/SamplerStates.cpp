#include "stdafx.h"

#include "SamplerStates.h"
#include "GameException.h"

namespace Library
{
	ID3D11SamplerState* SamplerStates::TrilinearWrap = nullptr;
	ID3D11SamplerState* SamplerStates::TrilinearMirror = nullptr;
	ID3D11SamplerState* SamplerStates::TrilinearClamp = nullptr;
	ID3D11SamplerState* SamplerStates::TrilinerBorder = nullptr;
	ID3D11SamplerState* SamplerStates::ShadowSamplerState = nullptr;

	XMVECTORF32 SamplerStates::BorderColor = { 0.0f, 0.0f, 0.0f, 1.0f };

	void SamplerStates::Initialize(ID3D11Device* direct3DDevice)
	{
		assert(direct3DDevice != nullptr);

		D3D11_SAMPLER_DESC samplerStateDesc;
		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.MipLODBias = 0;
		samplerStateDesc.MaxAnisotropy = 1;
		samplerStateDesc.MinLOD = -1000.0f;
		samplerStateDesc.MaxLOD = 1000.0f;
		HRESULT hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &TrilinearWrap);
		if (FAILED(hr))
		{
			throw GameException("ID3D11Device::CreateSamplerState() failed.", hr);
		}

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;

		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &TrilinearMirror);
		if (FAILED(hr))
		{
			throw GameException("ID3D11Device::CreateSamplerState() failed.", hr);
		}

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &TrilinearClamp);
		if (FAILED(hr))
		{
			throw GameException("ID3D11Device::CreateSamplerState() failed.", hr);
		}

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		memcpy(samplerStateDesc.BorderColor, reinterpret_cast<FLOAT*>(&BorderColor), sizeof(FLOAT) * 4);

		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &TrilinerBorder);
		if (FAILED(hr))
		{
			throw GameException("ID3D11Device::CreateSamplerState() failed.", hr);
		}

		samplerStateDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.MipLODBias = 0;
		samplerStateDesc.MaxAnisotropy = 1;
		samplerStateDesc.MinLOD = -1000.0f;
		samplerStateDesc.MaxLOD = 1000.0f;
		float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		memcpy(samplerStateDesc.BorderColor, reinterpret_cast<FLOAT*>(&white), sizeof(FLOAT) * 4);
		samplerStateDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(direct3DDevice->CreateSamplerState(&samplerStateDesc, &ShadowSamplerState)))
			throw GameException("Failed to create sampler ShadowSamplerState!");
	}

	void SamplerStates::Release()
	{
		ReleaseObject(TrilinearWrap);
		ReleaseObject(TrilinearMirror);
		ReleaseObject(TrilinearClamp);
		ReleaseObject(TrilinerBorder);
		ReleaseObject(ShadowSamplerState);
	}
}