#pragma once

#include <windows.h>
#include <string>
#include "stdafx.h"

#include "Common.h"
#include "GameClock.h"
#include "GameTime.h"
#include "GameComponent.h"
#include "ServiceContainer.h"
#include "RenderTarget.h"
#include "DemoLevel.h"

using namespace Library;

namespace Library
{
	class Game : public RenderTarget
	{
	public:
		Game(HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand);
		virtual ~Game();

		HINSTANCE Instance() const;
		HWND WindowHandle() const;
		const WNDCLASSEX& Window() const;
		const std::wstring& WindowClass() const;
		const std::wstring& WindowTitle() const;
		int ScreenWidth() const;
		int ScreenHeight() const;

		ID3D11Device1* Direct3DDevice() const;
		ID3D11DeviceContext1* Direct3DDeviceContext() const;

		bool DepthStencilBufferEnabled() const;
		bool DepthBufferEnabled() const;
		ID3D11RenderTargetView* RenderTargetView() const;
		ID3D11DepthStencilView* DepthStencilView() const;
		ID3D11ShaderResourceView* OutputTexture() const;

		UINT GetMultisampleCount() { return mMultiSamplingCount; }


		float AspectRatio() const;
		bool IsFullScreen() const;

		template<typename T>
		std::pair<bool, int> FindInGameComponents(std::vector<GameComponent*>& components, const T& component)
		{
			std::pair<bool, int> result;

			auto it = std::find(components.begin(), components.end(), component);

			if (it != components.end())
			{
				result.second = distance(components.begin(), it);
				result.first = true;
			}
			else
			{
				result.second = -1;
				result.first = false;
			}

			return result;
		}

		template<typename T>
		bool IsInGameComponents(std::vector<GameComponent*>& components, const T& component)
		{
			std::pair<bool, int> result;

			auto it = std::find(components.begin(), components.end(), component);

			if (it != components.end())
			{
				return true;
			}
			else
			{
				return false;
			}

		}

		template<typename T>
		std::pair<bool, int> FindInGameLevels(std::vector<DemoLevel*>& levels, const T& level)
		{
			std::pair<bool, int> result;

			auto it = std::find(levels.begin(), levels.end(), level);

			if (it != levels.end())
			{
				result.second = distance(levels.begin(), it);
				result.first = true;
			}
			else
			{
				result.second = -1;
				result.first = false;
			}

			return result;
		}

		template<typename T>
		bool IsInGameLevels(std::vector<DemoLevel*>& components, const T& level)
		{
			std::pair<bool, int> result;

			auto it = std::find(levels.begin(), levels.end(), level);

			if (it != levels.end())
			{
				return true;
			}
			else
			{
				return false;
			}

		}

		const D3D11_TEXTURE2D_DESC& BackBufferDesc() const;
		const D3D11_VIEWPORT& Viewport() const;

		//const std::vector<GameComponent*>& Components() const;
		//const std::vector<DemoLevel*>& Levels() const;
		const ServiceContainer& Services() const;

		virtual void Run();
		virtual void Exit();
		virtual void Initialize();
		virtual void Update(const GameTime& gameTime);
		virtual void Draw(const GameTime& gameTime);
		virtual void ResetRenderTargets();
		virtual void UnbindPixelShaderResources(UINT startSlot, UINT count);
		virtual void SetBackBuffer();

		std::vector<GameComponent*> components;
		std::vector<DemoLevel*> levels;

	protected:
		virtual void Begin() override;
		virtual void End() override;
		virtual void InitializeWindow();
		virtual void InitializeDirectX();
		virtual void Shutdown();

		static const UINT DefaultScreenWidth;
		static const UINT DefaultScreenHeight;
		static const UINT DefaultFrameRate;
		static const UINT DefaultMultiSamplingCount;

		ServiceContainer mServices;

		HINSTANCE mInstance;
		std::wstring mWindowClass;
		std::wstring mWindowTitle;
		int mShowCommand;

		HWND mWindowHandle;
		WNDCLASSEX mWindow;

		UINT mScreenWidth;
		UINT mScreenHeight;

		GameClock mGameClock;
		GameTime mGameTime;

		D3D_FEATURE_LEVEL mFeatureLevel;
		ID3D11Device1* mDirect3DDevice;
		ID3D11DeviceContext1* mDirect3DDeviceContext;
		IDXGISwapChain1* mSwapChain;

		UINT mFrameRate;
		bool mIsFullScreen;
		bool mDepthStencilBufferEnabled;
		bool mMultiSamplingEnabled;
		UINT mMultiSamplingCount;
		UINT mMultiSamplingQualityLevels;

		ID3D11Texture2D* mDepthStencilBuffer;
		D3D11_TEXTURE2D_DESC mBackBufferDesc;
		ID3D11RenderTargetView* mRenderTargetView;
		ID3D11DepthStencilView* mDepthStencilView;
		ID3D11ShaderResourceView* mOutputTexture;

		D3D11_VIEWPORT mViewport;

	private:
		Game(const Game& rhs);
		Game& operator=(const Game& rhs);

		POINT CenterWindow(int windowWidth, int windowHeight);
		static LRESULT WINAPI WndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
	};
}