#pragma once

#include "Common.h"

namespace Library
{
	class Game;
	class Technique;

	class Pass
	{
	public:
		Pass(Game& game, Technique& technique, ID3DX11EffectPass* pass);

		Technique& GetTechnique();
		ID3DX11EffectPass* GetPass() const;
		const D3DX11_PASS_DESC& PassDesc() const;
		const std::string& Name() const;

		void CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* inputElementDesc, UINT numElements, ID3D11InputLayout **inputLayout);
		void Apply(UINT flags, ID3D11DeviceContext* context);

	private:
		Pass(const Pass& rhs);
		Pass& operator=(const Pass& rhs);

		Game& mGame;
		Technique& mTechnique;
		ID3DX11EffectPass* mPass;
		D3DX11_PASS_DESC mPassDesc;
		std::string mName;
	};
}