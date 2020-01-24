#pragma once

#include "DrawableGameComponent.h"

namespace DirectX
{
	class SpriteBatch;
	class SpriteFont;
}

namespace Library
{
	class FpsComponent : public DrawableGameComponent
	{
		RTTI_DECLARATIONS(FpsComponent, DrawableGameComponent)

	public:
		FpsComponent(Game& game);
		~FpsComponent();

		XMFLOAT2& TextPosition();

		int FrameRate() const;
		void SetColor(XMVECTORF32);

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

	private:
		FpsComponent();
		FpsComponent(const FpsComponent& rhs);
		FpsComponent& operator=(const FpsComponent& rhs);

		SpriteBatch* mSpriteBatch;
		SpriteFont* mSpriteFont;
		XMFLOAT2 mTextPosition;

		int mFrameCount;
		int mFrameRate;
		XMVECTORF32 mTextColor;

		double mLastTotalElapsedTime;
	};
}