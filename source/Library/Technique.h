#pragma once

#include "Common.h"
#include "Pass.h"

namespace Library
{
	class Game;
	class Effect;

	class Technique
	{
	public:
		Technique(Game& game, Effect& effect, ID3DX11EffectTechnique* technique);
		~Technique();

		Effect& GetEffect();
		ID3DX11EffectTechnique* GetTechnique() const;
		const D3DX11_TECHNIQUE_DESC& TechniqueDesc() const;
		const std::string& Name() const;
		const std::vector<Pass*>& Passes() const;
		const std::map<std::string, Pass*>& PassesByName() const;

	private:
		Technique(const Technique& rhs);
		Technique& operator=(const Technique& rhs);

		Effect& mEffect;
		ID3DX11EffectTechnique* mTechnique;
		D3DX11_TECHNIQUE_DESC mTechniqueDesc;
		std::string mName;
		std::vector<Pass*> mPasses;
		std::map<std::string, Pass*> mPassesByName;
	};
}