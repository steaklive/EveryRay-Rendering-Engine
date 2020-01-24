#pragma once
#include "Common.h"
#include "RenderTarget.h"

namespace Library
{
	class Game;

	class DepthMap : public RenderTarget
	{
		RTTI_DECLARATIONS(DepthMap, RenderTarget)

	public:
		DepthMap(Game& game, UINT width, UINT height);
		~DepthMap();

		ID3D11ShaderResourceView* OutputTexture() const;
		ID3D11DepthStencilView* DepthStencilView() const;

		virtual void Begin() override;
		virtual void End() override;

	private:
		DepthMap();
		DepthMap(const DepthMap& rhs);
		DepthMap& operator=(const DepthMap& rhs);

		Game* mGame;
		ID3D11DepthStencilView* mDepthStencilView;
		ID3D11ShaderResourceView* mOutputTexture;
		D3D11_VIEWPORT mViewport;
	};
}