#pragma once
#include "Common.h"

namespace Library
{
	class Game;

	class FullScreenRenderTarget
	{
	public:
		FullScreenRenderTarget(Game& game);
		~FullScreenRenderTarget();

		ID3D11ShaderResourceView* OutputTexture() const;
		ID3D11RenderTargetView* RenderTargetView() const;
		ID3D11DepthStencilView* DepthStencilView() const;

		void Begin();
		void End();

	private:
		FullScreenRenderTarget();
		FullScreenRenderTarget(const FullScreenRenderTarget& rhs);
		FullScreenRenderTarget& operator=(const FullScreenRenderTarget& rhs);

		Game* mGame;
		ID3D11RenderTargetView* mRenderTargetView;
		ID3D11DepthStencilView* mDepthStencilView;
		ID3D11ShaderResourceView* mOutputTexture;
	};
}