#pragma once

#include "Common.h"
#include "DrawableGameComponent.h"
#include "CustomRenderTarget.h"

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

	private:
	
		CustomRenderTarget* mAlbedoBuffer;
		CustomRenderTarget* mNormalBuffer;
		ID3D11RasterizerState* mRS;
		int mWidth;
		int mHeight;
	};
}