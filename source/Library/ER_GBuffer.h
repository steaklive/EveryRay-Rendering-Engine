#pragma once

#include "Common.h"
#include "DrawableGameComponent.h"
#include "ER_GPUTexture.h"
#include "DepthTarget.h"

namespace Library
{
	class ER_Scene;
	class ER_GBuffer: public DrawableGameComponent
	{
	public:
		ER_GBuffer(Game& game, Camera& camera, int width, int height);
		~ER_GBuffer();
	
		void Initialize();
		void Update(const GameTime& time);

		void Start();
		void End();
		void Draw(const ER_Scene* scene);

		ER_GPUTexture* GetAlbedo() { return mAlbedoBuffer; }
		ER_GPUTexture* GetNormals() { return mNormalBuffer; }
		ER_GPUTexture* GetPositions() { return mPositionsBuffer; }
		ER_GPUTexture* GetExtraBuffer() { return mExtraBuffer; } // [reflection mask, roughness, metalness, foliage mask]
		ER_GPUTexture* GetExtra2Buffer() { return mExtra2Buffer; } // [global diffuse probe mask, height (for POM, etc.), empty, skip deferred lighting]
		DepthTarget* GetDepth() { return mDepthBuffer; }

	private:
	
		DepthTarget* mDepthBuffer = nullptr;
		ER_GPUTexture* mAlbedoBuffer= nullptr;
		ER_GPUTexture* mNormalBuffer= nullptr;
		ER_GPUTexture* mPositionsBuffer = nullptr;
		ER_GPUTexture* mExtraBuffer = nullptr;
		ER_GPUTexture* mExtra2Buffer = nullptr;
		ID3D11RasterizerState* mRS = nullptr;

		int mWidth;
		int mHeight;
	};
}