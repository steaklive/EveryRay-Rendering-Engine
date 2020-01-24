#include "stdafx.h"

#include "FpsComponent.h"
#include "ColorHelper.cpp"
#include <sstream>
#include <iomanip>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include "Game.h"
#include "Utility.h"

namespace Library
{
	RTTI_DEFINITIONS(FpsComponent)

		FpsComponent::FpsComponent(Game& game) : DrawableGameComponent(game),
		mSpriteBatch(nullptr), mSpriteFont(nullptr), mTextPosition(0.0f, 20.0f),
		mFrameCount(0), mFrameRate(0), mLastTotalElapsedTime(0.0), mTextColor(ColorHelper::White)
	{
	}

	FpsComponent::~FpsComponent()
	{
		DeleteObject(mSpriteFont);
		DeleteObject(mSpriteBatch);
	}

	XMFLOAT2& FpsComponent::TextPosition()
	{
		return mTextPosition;
	}

	int FpsComponent::FrameRate() const
	{
		return mFrameCount;
	}

	void FpsComponent::SetColor(XMVECTORF32 color)
	{
		mTextColor = color;
	}

	void FpsComponent::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		mSpriteBatch = new SpriteBatch(mGame->Direct3DDeviceContext());
		mSpriteFont = new SpriteFont(mGame->Direct3DDevice(), L"Content\\Fonts\\couriersmall.spritefont");
	
	}

	void FpsComponent::Update(const GameTime& gameTime)
	{
		if (gameTime.TotalGameTime() - mLastTotalElapsedTime >= 1)
		{
			mLastTotalElapsedTime = gameTime.TotalGameTime();
			mFrameRate = mFrameCount;
			mFrameCount = 0;
		}

		mFrameCount++;
	}

	void FpsComponent::Draw(const GameTime& gameTime)
	{
		mSpriteBatch->Begin();

		std::wostringstream fpsLabel;
		std::wostringstream timeLabel;
		fpsLabel << std::setprecision(4) << L"FPS: " << mFrameRate;
		timeLabel << std::setprecision(4) << L"Elapsed Time: " << gameTime.TotalGameTime();
		mSpriteFont->DrawString(mSpriteBatch, fpsLabel.str().c_str(), XMFLOAT2(mTextPosition.x, mTextPosition.y - 20), mTextColor);
		mSpriteFont->DrawString(mSpriteBatch, timeLabel.str().c_str(), XMFLOAT2(mTextPosition.x, mTextPosition.y - 10), mTextColor);

		mSpriteBatch->End();
	}
}