#pragma once

#include "Common.h"
#include "DrawableGameComponent.h"
#include "ER_GPUTexture.h"
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

		ER_GPUTexture* GetAlbedo() { return mAlbedoBuffer; }
		ER_GPUTexture* GetNormals() { return mNormalBuffer; }
		ER_GPUTexture* GetPositions() { return mPositionsBuffer; }
		ER_GPUTexture* GetExtraBuffer() { return mExtraBuffer; }
		DepthTarget* GetDepth() { return mDepthBuffer; }

	private:
	
		DepthTarget* mDepthBuffer = nullptr;
		ER_GPUTexture* mAlbedoBuffer= nullptr;
		ER_GPUTexture* mNormalBuffer= nullptr;
		ER_GPUTexture* mPositionsBuffer = nullptr;
		ER_GPUTexture* mExtraBuffer = nullptr;
		ID3D11RasterizerState* mRS = nullptr;

		int mWidth;
		int mHeight;
	};
}