#pragma once

#include "Common.h"
#include "DrawableGameComponent.h"
#include "CustomRenderTarget.h"
#include "DepthTarget.h"

namespace Library
{
	class GBuffer: public DrawableGameComponent
	{
	public:
		GBuffer(Game& game, Camera& camera, int width, int height);
		~GBuffer();
	
		void Initialize();
		void Update(const GameTime& time);

		void Start();
		void End();

		CustomRenderTarget* GetAlbedo() { return mAlbedoBuffer; }
		CustomRenderTarget* GetNormals() { return mNormalBuffer; }
		CustomRenderTarget* GetPositions() { return mPositionsBuffer; }
		CustomRenderTarget* GetExtraBuffer() { return mExtraBuffer; }
		DepthTarget* GetDepth() { return mDepthBuffer; }

	private:
	
		DepthTarget* mDepthBuffer;
		CustomRenderTarget* mAlbedoBuffer;
		CustomRenderTarget* mNormalBuffer;
		CustomRenderTarget* mPositionsBuffer;
		CustomRenderTarget* mExtraBuffer;
		ID3D11RasterizerState* mRS;

		int mWidth;
		int mHeight;
	};
}