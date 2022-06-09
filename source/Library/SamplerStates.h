#pragma once

#include "Common.h"

namespace Library
{
	class SamplerStates
	{
	public:
		static ID3D11SamplerState* TrilinearWrap;
		static ID3D11SamplerState* TrilinearMirror;
		static ID3D11SamplerState* TrilinearClamp;
		static ID3D11SamplerState* TrilinearPointClamp;
		static ID3D11SamplerState* TrilinerBorder;
		static ID3D11SamplerState* ShadowSamplerState;

		static XMVECTORF32 BorderColor;

		static void Initialize(ID3D11Device* direct3DDevice);
		static void Release();

	private:
		SamplerStates();
		SamplerStates(const SamplerStates& rhs);
		SamplerStates& operator=(const SamplerStates& rhs);
	};
}