#include "stdafx.h"

#include "RenderingGame.h"

#include <sstream>
#include <SpriteBatch.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <SpriteFont.h>

#include "..\Library\GameException.h"
#include "..\Library\Keyboard.h"
#include "..\Library\Mouse.h"
#include "..\Library\FpsComponent.h"
#include "..\Library\Utility.h"
#include "..\Library\FirstPersonCamera.h"
#include "..\Library\ColorHelper.h"
#include "..\Library\Grid.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\RasterizerStates.h"
#include "..\Library\SamplerStates.h"
#include "..\Library\Skybox.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\PointLightingMaterial.h"
#include "..\Library\FullScreenRenderTarget.h"
#include "..\Library\FullScreenQuad.h"
#include "..\Library\Effect.h"
#include "..\Library\ColorFilteringMaterial.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\PostProcessingStack.h"


#include "AmbientLightingDemo.h"
#include "ShadowMappingDemo.h"
#include "PhysicallyBasedRenderingDemo.h"
#include "InstancingDemo.h"
#include "FrustumCullingDemo.h"
#include "SubsurfaceScatteringDemo.h"
#include "VolumetricLightingDemo.h"
#include "CollisionTestDemo.h"



#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

namespace Rendering
{
	const XMVECTORF32 RenderingGame::BackgroundColor = ColorHelper::Black;
	const float RenderingGame::BrightnessModulationRate = 1.0f;
	
	const char* displayedLevelNames[] =
	{
		"GPU Instancing",
		"Cascaded Shadow Mapping",
		"Physically Based Rendering",
		"Frustum Culling", 
		"Separable Subsurface Scattering",
		"Volumetric Lighting",
		"Collision Detection"
	};

	// we will store our demo scenes here:
	std::vector<DemoLevel*> demoLevels;
	static int currentLevel = 0;

	static float fov = 60.0f;
	

	bool showImGuiDemo = false;

	RenderingGame::RenderingGame(HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand)
		: Game(instance, windowClass, windowTitle, showCommand),
		
		mFpsComponent(nullptr),
		mSpriteBatch(nullptr),
		mSpriteFont(nullptr),
		mDirectInput(nullptr),
		mKeyboard(nullptr),
		mMouse(nullptr),
		mMouseTextPosition(0.0f, 40.0f),
		mGrid(nullptr), mShowGrid(false), 

		//scenes
		mAmbientLightingDemo(nullptr),
		mShadowMappingDemo(nullptr),
		mPBRDemo(nullptr),
		mInstancingDemo(nullptr),
		mFrustumCullingDemo(nullptr),
		mSubsurfaceScatteringDemo(nullptr),
		mVolumetricLightingDemo(nullptr),
		mCollisionTestDemo(nullptr),

		mRenderStateHelper(nullptr),
		mRenderTarget(nullptr), mFullScreenQuad(nullptr)

	{
		mDepthStencilBufferEnabled = true;
		mMultiSamplingEnabled = true;
	}

	RenderingGame::~RenderingGame()
	{
	}

	void RenderingGame::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		RasterizerStates::Initialize(mDirect3DDevice);
		SamplerStates::BorderColor = ColorHelper::Black;
		SamplerStates::Initialize(mDirect3DDevice);

		if (FAILED(DirectInput8Create(mInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&mDirectInput, nullptr)))
		{
			throw GameException("DirectInput8Create() failed");
		}
		
		mKeyboard = new Keyboard(*this, mDirectInput);
		components.push_back(mKeyboard);
		mServices.AddService(Keyboard::TypeIdClass(), mKeyboard);

		mMouse = new Mouse(*this, mDirectInput);
		components.push_back(mMouse);
		mServices.AddService(Mouse::TypeIdClass(), mMouse);

		mCamera = new FirstPersonCamera(*this, 1.5708f, this->AspectRatio(), 0.5f, 600.0f );
		mCamera->SetPosition(0.0f, 20.0f, 65.0f);
		components.push_back(mCamera);
		mServices.AddService(Camera::TypeIdClass(), mCamera);

		mGrid = new Grid(*this, *mCamera, 100, 64, XMFLOAT4(0.961f, 0.871f, 0.702f, 1.0f));
		mGrid->SetColor((XMFLOAT4)ColorHelper::LightGray);
		components.push_back(mGrid);

		// Adding empty Levels
		demoLevels.push_back(mInstancingDemo);
		demoLevels.push_back(mShadowMappingDemo);
		demoLevels.push_back(mPBRDemo);
		demoLevels.push_back(mFrustumCullingDemo);
		demoLevels.push_back(mSubsurfaceScatteringDemo);
		demoLevels.push_back(mVolumetricLightingDemo);
		demoLevels.push_back(mCollisionTestDemo);


		//Render State Helper
		mRenderStateHelper = new RenderStateHelper(*this);



		#pragma region INITIALIZE_IMGUI

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;

		ImGui_ImplWin32_Init(mWindowHandle);
		ImGui_ImplDX11_Init(mDirect3DDevice,mDirect3DDeviceContext);
		ImGui::StyleEveryRayColor();


#pragma endregion

		Game::Initialize();
		
	}


	// ...*sniff-sniff*
	// ...why does it smell like shit here??...
	// ...oh, amazing level management... multiple inheritance, switch... mhm...
	// ...also have you heard of Data-Oriented Design?..
	void RenderingGame::SetLevel(int level)
	{
		if (demoLevels[level] == nullptr)
		{
			switch (level)
			{
			case 0:
				demoLevels[level] = new InstancingDemo(*this, *mCamera);
				break;
			case 1:
				demoLevels[level] = new ShadowMappingDemo(*this, *mCamera);
				break;
			case 2:
				demoLevels[level] = new PhysicallyBasedRenderingDemo(*this, *mCamera);
				break;
			case 3:
				demoLevels[level] = new FrustumCullingDemo(*this, *mCamera);
				break;
			case 4:
				demoLevels[level] = new SubsurfaceScatteringDemo(*this, *mCamera);
				break;
			case 5:
				demoLevels[level] = new VolumetricLightingDemo(*this, *mCamera);
				break;
			case 6:
				demoLevels[level] = new CollisionTestDemo(*this, *mCamera);
				break;
			}
		}

		bool isComponent = demoLevels[level]->IsComponent();

		if (isComponent)
		{
			return;
		}
		else
		{
			demoLevels[level]->Create();

			int i = 0;
			std::for_each(demoLevels.begin(), demoLevels.end(), [&](DemoLevel* lvl)
			{
				if (i != level && lvl!=nullptr)
				{
					lvl->Destroy();
					demoLevels[i] = nullptr;
				}
				i++;
			});
		}
		
	}

	void RenderingGame::Update(const GameTime& gameTime)
	{
		SetLevel(currentLevel);

		if (mKeyboard->WasKeyPressedThisFrame(DIK_ESCAPE))
		{
			Exit();
		}

		UpdateImGui();

	
		Game::Update(gameTime);
	}
	
	void RenderingGame::UpdateImGui()
	{
		#pragma region ENGINE_SPECIFIC_IMGUI

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();


		ImGui::SetNextWindowBgAlpha(0.9f); // Transparent background
		if (ImGui::Begin("EveryRay - Rendering Engine: Configuration"))
		{
			ImGui::TextColored(ImVec4(0.52f, 0.78f, 0.04f, 1), "Welcome to EveryRay!");
			ImGui::Separator();
			
			ImGui::TextColored(ImVec4(0.95f, 0.5f, 0.0f, 1), "FPS: (%.1f FPS), %.3f ms/frame", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
			
			if (ImGui::IsMousePosValid())
				ImGui::Text("Mouse Position: (%.1f,%.1f)", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
			else
				ImGui::Text("Mouse Position: <invalid>");
			
			ImGui::Text("Camera Position: (%.1f,%.1f,%.1f)", mCamera->Position().x, mCamera->Position().y, mCamera->Position().z);
			
			
			ImGui::SliderFloat("Camera FOV", &fov, 1.0f, 90.0f);
			mCamera->SetFOV(fov*XM_PI/180.0f);
			ImGui::Separator();

			ImGui::Checkbox("Show grid", &mShowGrid);
			mGrid->SetVisible(mShowGrid);
			ImGui::Separator();

			ImGui::Checkbox("Show ImGUI demo window", &showImGuiDemo);
			if (showImGuiDemo) ImGui::ShowDemoWindow(false);
			ImGui::Separator();

			if (ImGui::CollapsingHeader("Load Demo Level"))
			{
				ImGui::Combo("Level", &currentLevel, displayedLevelNames, IM_ARRAYSIZE(displayedLevelNames));
			}


		}
		ImGui::End();
		#pragma endregion
	}
	
	void RenderingGame::Shutdown()
	{
		DeleteObject(mAmbientLightingDemo);
		DeleteObject(mShadowMappingDemo);
		DeleteObject(mPBRDemo);
		DeleteObject(mInstancingDemo);
		DeleteObject(mFrustumCullingDemo);
		DeleteObject(mSubsurfaceScatteringDemo);
		DeleteObject(mVolumetricLightingDemo);
		DeleteObject(mCollisionTestDemo);

		DeleteObject(mFullScreenQuad);
		DeleteObject(mRenderTarget);

		DeleteObject(mKeyboard);
		DeleteObject(mMouse);
		DeleteObject(mFpsComponent);
		DeleteObject(mSpriteFont);
		DeleteObject(mSpriteBatch);
		DeleteObject(mCamera);
		DeleteObject(mGrid);
		DeleteObject(mRenderStateHelper);

		demoLevels.clear();
		DeletePointerCollection(demoLevels);

		RasterizerStates::Release();
		SamplerStates::Release();

		//destroy imgui
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		Game::Shutdown();
	}
	
	void RenderingGame::Draw(const GameTime& gameTime)
	{



		mDirect3DDeviceContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&BackgroundColor));
		mDirect3DDeviceContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		Game::Draw(gameTime);



		
		mRenderStateHelper->SaveAll();
		mRenderStateHelper->RestoreAll();
		
		
		HRESULT hr = mSwapChain->Present(0, 0);
		if (FAILED(hr))
		{
			throw GameException("IDXGISwapChain::Present() failed.", hr);
		}
	}


	
}

