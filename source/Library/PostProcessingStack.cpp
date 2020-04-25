#include "PostProcessingStack.h"
#include "VignetteMaterial.h"
#include "MotionBlurMaterial.h"
#include "ColorGradingMaterial.h"
#include "ScreenSpaceReflectionsMaterial.h"
#include "FXAAMaterial.h"
#include "FullScreenQuad.h"
#include "FullScreenRenderTarget.h"
#include "ShaderCompiler.h"
#include "GameException.h"
#include <WICTextureLoader.h>
#include "Utility.h"
#include "ColorHelper.h"

#include "MatrixHelper.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

namespace Rendering {
	const XMVECTORF32 ClearBackgroundColor = ColorHelper::Black;

	PostProcessingStack::PostProcessingStack(Game& pGame, Camera& pCamera)
		:
		mVignetteEffect(nullptr),
		mColorGradingEffect(nullptr),
		mMotionBlurEffect(nullptr),
		mFXAAEffect(nullptr),
		mSSREffect(nullptr),
		game(pGame), camera(pCamera)
	{
	}

	PostProcessingStack::~PostProcessingStack()
	{
		DeleteObject(mVignetteEffect);
		DeleteObject(mFXAAEffect);
		DeleteObject(mColorGradingEffect);
		DeleteObject(mMotionBlurEffect);
		DeleteObject(mTonemapEffect);
		DeleteObject(mSSREffect);

		DeleteObject(mMainRenderTarget);
		DeleteObject(mColorGradingRenderTarget);
		DeleteObject(mMotionBlurRenderTarget);
		DeleteObject(mVignetteRenderTarget);
		DeleteObject(mFXAARenderTarget);
		DeleteObject(mSSRRenderTarget);

		ReleaseObject(mQuadVB);
		ReleaseObject(mFullScreenQuadVS);
		ReleaseObject(mFullScreenQuadLayout);
	}

	void PostProcessingStack::Initialize(bool pTonemap, bool pMotionBlur, bool pColorGrading, bool pVignette, bool pFXAA, bool pSSR)
	{
		mMainRenderTarget = new FullScreenRenderTarget(game);

		Effect* vignetteEffectFX = new Effect(game);
		vignetteEffectFX->CompileFromFile(Utility::GetFilePath(L"content\\effects\\Vignette.fx"));
		mVignetteEffect = new EffectElements::VignetteEffect();
		mVignetteEffect->Material = new VignetteMaterial();
		mVignetteEffect->Material->Initialize(vignetteEffectFX);
		mVignetteEffect->Quad = new FullScreenQuad(game, *mVignetteEffect->Material);
		mVignetteEffect->Quad->Initialize();
		mVignetteEffect->Quad->SetActiveTechnique("vignette_filter", "p0");
		mVignetteEffect->Quad->SetCustomUpdateMaterial(std::bind(&PostProcessingStack::UpdateVignetteMaterial, this));
		mVignetteLoaded = true;
		mVignetteRenderTarget = new FullScreenRenderTarget(game);
		mVignetteEffect->OutputTexture = mVignetteRenderTarget->OutputColorTexture();
		mVignetteEffect->isActive = pVignette;

		Effect* ssrEffectFX = new Effect(game);
		ssrEffectFX->CompileFromFile(Utility::GetFilePath(L"content\\effects\\SSR.fx"));
		mSSREffect = new EffectElements::SSREffect();
		mSSREffect->Material = new ScreenSpaceReflectionsMaterial();
		mSSREffect->Material->Initialize(ssrEffectFX);
		mSSREffect->Quad = new FullScreenQuad(game, *mSSREffect->Material);
		mSSREffect->Quad->Initialize();
		mSSREffect->Quad->SetActiveTechnique("ssr", "p0");
		//mSSREffect->Quad->SetCustomUpdateMaterial(std::bind(&PostProcessingStack::UpdateSSRMaterial, this));
		mSSRRenderTarget = new FullScreenRenderTarget(game);
		mSSREffect->InputColor = mSSRRenderTarget->OutputColorTexture();
		mSSREffect->isActive = pSSR;
		mSSRLoaded = true;

		Effect* colorgradingEffectFX = new Effect(game);
		colorgradingEffectFX->CompileFromFile(Utility::GetFilePath(L"content\\effects\\ColorGrading.fx"));
		mColorGradingEffect = new EffectElements::ColorGradingEffect();
		mColorGradingEffect->Material = new ColorGradingMaterial();
		mColorGradingEffect->Material->Initialize(colorgradingEffectFX);
		mColorGradingEffect->Quad = new FullScreenQuad(game, *mColorGradingEffect->Material);
		mColorGradingEffect->Quad->Initialize();
		mColorGradingEffect->Quad->SetActiveTechnique("color_grading_filter", "p0");
		mColorGradingEffect->Quad->SetCustomUpdateMaterial(std::bind(&PostProcessingStack::UpdateColorGradingMaterial, this));
		mColorGradingRenderTarget = new FullScreenRenderTarget(game);
		mColorGradingEffect->OutputTexture = mColorGradingRenderTarget->OutputColorTexture();
		mColorGradingEffect->isActive = pColorGrading;
		mColorGradingEffect->currentLUT = 2;

		std::wstring LUTtexture1 = Utility::GetFilePath(L"content\\effects\\LUT_1.png");
		if (FAILED(DirectX::CreateWICTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(), LUTtexture1.c_str(), nullptr, &mColorGradingEffect->LUT1)))
		{
			throw GameException("Failed to load LUT1 texture.");
		}

		std::wstring LUTtexture2 = Utility::GetFilePath(L"content\\effects\\LUT_2.png");
		if (FAILED(DirectX::CreateWICTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(), LUTtexture2.c_str(), nullptr, &mColorGradingEffect->LUT2)))
		{
			throw GameException("Failed to load LUT2 texture.");
		}

		std::wstring LUTtexture3 = Utility::GetFilePath(L"content\\effects\\LUT_3.png");
		if (FAILED(DirectX::CreateWICTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(), LUTtexture3.c_str(), nullptr, &mColorGradingEffect->LUT3)))
		{
			throw GameException("Failed to load LUT3 texture.");
		}
		mColorGradingLoaded = true;

		Effect* motionblurEffectFX = new Effect(game);
		motionblurEffectFX->CompileFromFile(Utility::GetFilePath(L"content\\effects\\MotionBlur.fx"));
		mMotionBlurEffect = new EffectElements::MotionBlurEffect();
		mMotionBlurEffect->Material = new MotionBlurMaterial();
		mMotionBlurEffect->Material->Initialize(motionblurEffectFX);
		mMotionBlurEffect->Quad = new FullScreenQuad(game, *mMotionBlurEffect->Material);
		mMotionBlurEffect->Quad->Initialize();
		mMotionBlurEffect->Quad->SetActiveTechnique("blur_filter", "p0");
		mMotionBlurEffect->Quad->SetCustomUpdateMaterial(std::bind(&PostProcessingStack::UpdateMotionBlurMaterial, this));
		mMotionBlurRenderTarget = new FullScreenRenderTarget(game);
		mMotionBlurEffect->OutputTexture = mMainRenderTarget->OutputColorTexture();
		mMotionBlurEffect->isActive = pMotionBlur;
		mMotionBlurLoaded = true;

		Effect* fxaaFX = new Effect(game);
		fxaaFX->CompileFromFile(Utility::GetFilePath(L"content\\effects\\FXAA.fx"));
		mFXAAEffect = new EffectElements::FXAAEffect();
		mFXAAEffect->Material = new FXAAMaterial();
		mFXAAEffect->Material->Initialize(fxaaFX);
		mFXAAEffect->Quad = new FullScreenQuad(game, *mFXAAEffect->Material);
		mFXAAEffect->Quad->Initialize();
		mFXAAEffect->Quad->SetActiveTechnique("fxaa_filter", "p0");
		mFXAAEffect->Quad->SetCustomUpdateMaterial(std::bind(&PostProcessingStack::UpdateFXAAMaterial, this));
		mFXAARenderTarget = new FullScreenRenderTarget(game);
		mFXAAEffect->isActive = pFXAA;
		mFXAALoaded = true;

		// Tonemapping
		mTonemapEffect = new EffectElements::TonemapEffect();
		mTonemapEffect->isActive = pTonemap;
		mTonemapRenderTarget = new FullScreenRenderTarget(game);
		UINT bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		float minDim = (float)std::min(game.ScreenHeight(), game.ScreenWidth());
		int log2min = std::min((int)floor(log(minDim) / log(2.0f)), 9);
		int lumDim = 1 << log2min;
		mTonemapEffect->LuminanceResource = CustomRenderTarget::Create(game.Direct3DDevice(), lumDim, lumDim, 0, DXGI_FORMAT_R16_FLOAT, bindFlags, log2min + 1);
		mTonemapEffect->AvgLuminanceResource = CustomRenderTarget::Create(game.Direct3DDevice(), 16, 16, 0, DXGI_FORMAT_R16_FLOAT, bindFlags, 1);
		mTonemapEffect->BrightResource = CustomRenderTarget::Create(game.Direct3DDevice(), game.ScreenWidth()/2, game.ScreenHeight()/2, 0, DXGI_FORMAT_R11G11B10_FLOAT, bindFlags, 4);
		mTonemapEffect->BlurSummedResource = CustomRenderTarget::Create(game.Direct3DDevice(), game.ScreenWidth() / 2, game.ScreenHeight() / 2, 0, DXGI_FORMAT_R11G11B10_FLOAT, bindFlags, 4);
		mTonemapEffect->BlurHorizontalResource = CustomRenderTarget::Create(game.Direct3DDevice(), game.ScreenWidth() / 2, game.ScreenHeight() / 2, 0, DXGI_FORMAT_R11G11B10_FLOAT, bindFlags, 4);
		mTonemapEffect->BlurVerticalResource = CustomRenderTarget::Create(game.Direct3DDevice(), game.ScreenWidth() / 2, game.ScreenHeight() / 2, 0, DXGI_FORMAT_R11G11B10_FLOAT, bindFlags, 4);

		D3D11_BUFFER_DESC BufferDesc;
		ZeroMemory(&BufferDesc, sizeof(BufferDesc));
		BufferDesc.Usage = D3D11_USAGE_DEFAULT;
		BufferDesc.CPUAccessFlags = 0;
		BufferDesc.ByteWidth = sizeof(int) * 4;
		BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		game.Direct3DDevice()->CreateBuffer(&BufferDesc, NULL, &mTonemapEffect->ConstBuffer);

		BufferDesc.ByteWidth = sizeof(float) * 4;
		BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		game.Direct3DDevice()->CreateBuffer(&BufferDesc, NULL, &mTonemapEffect->BloomConstants);

		D3D11_SAMPLER_DESC samplerDesc = CD3D11_SAMPLER_DESC();
		ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MaxAnisotropy = 16;
		samplerDesc.MaxLOD = FLT_MAX;
		samplerDesc.MinLOD = FLT_MIN;
		samplerDesc.MipLODBias = 0.0f;
		game.Direct3DDevice()->CreateSamplerState(&samplerDesc, &mTonemapEffect->LinearSampler);		
		
		ID3DBlob* pShaderBlob = nullptr;		
		if(FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Tonemap\\Tonemap.hlsl").c_str(), "ToneMapWithBloom", "ps_4_0", &pShaderBlob)))
			throw GameException("Failed to load ToneMapWithBloom pass from shader: PSPostProcess.hlsl!");
		if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mTonemapEffect->ToneMapWithBloomPS)))
			throw GameException("Failed to create ToneMapWithBloom shader from PSPostProcess.hlsl!");
		pShaderBlob->Release();

		pShaderBlob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Tonemap\\Tonemap.hlsl").c_str(), "Add", "ps_4_0", &pShaderBlob)))
			throw GameException("Failed to load Add pass from shader: PSPostProcess.hlsl!");
		if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mTonemapEffect->AddPS)))
			throw GameException("Failed to create Add shader from PSPostProcess.hlsl!");
		pShaderBlob->Release();

		pShaderBlob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Tonemap\\Tonemap.hlsl").c_str(), "BlurHorizontal", "ps_4_0", &pShaderBlob)))
			throw GameException("Failed to load BlurHorizontal pass from shader: PSPostProcess.hlsl!");
		if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mTonemapEffect->BlurHPS)))
			throw GameException("Failed to create BlurHorizontal shader from PSPostProcess.hlsl!");
		pShaderBlob->Release();

		pShaderBlob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Tonemap\\Tonemap.hlsl").c_str(), "BlurVertical", "ps_4_0", &pShaderBlob)))
			throw GameException("Failed to load BlurVertical pass from shader: PSPostProcess.hlsl!");
		if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mTonemapEffect->BlurVPS)))
			throw GameException("Failed to create BlurVertical shader from PSPostProcess.hlsl!");
		pShaderBlob->Release();

		pShaderBlob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Tonemap\\Tonemap.hlsl").c_str(), "Bright", "ps_4_0", &pShaderBlob)))
			throw GameException("Failed to load Bright pass from shader: PSPostProcess.hlsl!");
		if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mTonemapEffect->BrightPS)))
			throw GameException("Failed to create Bright shader from PSPostProcess.hlsl!");
		pShaderBlob->Release();
	
		pShaderBlob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Tonemap\\Tonemap.hlsl").c_str(), "CalcLuminance", "ps_4_0", &pShaderBlob)))
			throw GameException("Failed to load CalcLuminance pass from shader: PSPostProcess.hlsl!");
		if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mTonemapEffect->CalcLumPS)))
			throw GameException("Failed to create CalcLuminance shader from PSPostProcess.hlsl!");
		pShaderBlob->Release();

		pShaderBlob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Tonemap\\Tonemap.hlsl").c_str(), "AvgLuminance", "ps_4_0", &pShaderBlob)))
			throw GameException("Failed to load AvgLuminance pass from shader: PSPostProcess.hlsl!");
		if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mTonemapEffect->AvgLumPS)))
			throw GameException("Failed to create AvgLuminance shader from PSPostProcess.hlsl!");
		pShaderBlob->Release();

		pShaderBlob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Tonemap\\Tonemap.hlsl").c_str(), "EmptyPass", "ps_4_0", &pShaderBlob)))
			throw GameException("Failed to load EmptyPass pass from shader: PSPostProcess.hlsl!");
		if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mTonemapEffect->EmptyPassPS)))
			throw GameException("Failed to create EmptyPass shader from PSPostProcess.hlsl!");
		pShaderBlob->Release();

		// Quad temp
		ID3DBlob* pBlob = NULL;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Tonemap\\FullScreenQuad.hlsl").c_str(), "VSMain", "vs_4_0", &pBlob)))
			throw GameException("Failed to load VSMain pass from shader: FullScreenQuad.hlsl!");
		if (FAILED(game.Direct3DDevice()->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &mFullScreenQuadVS)))
			throw GameException("Failed to create AvgLuminance shader from PSPostProcess.hlsl!");

		D3D11_INPUT_ELEMENT_DESC InputLayout[] =
		{ { "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		game.Direct3DDevice()->CreateInputLayout(InputLayout, ARRAYSIZE(InputLayout),
			pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
			&mFullScreenQuadLayout);
		pBlob->Release();

		float data[] =
		{ -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f, 0.0f,
			-1.0f, -1.0f, 0.0f,
			 1.0f, -1.0f, 0.0f,
		};

		D3D11_BUFFER_DESC BufferDesc2;
		ZeroMemory(&BufferDesc2, sizeof(BufferDesc2));
		BufferDesc2.ByteWidth = sizeof(float) * 3 * 4;
		BufferDesc2.Usage = D3D11_USAGE_DEFAULT;
		BufferDesc2.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BufferDesc2.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA subresourceData2;
		subresourceData2.pSysMem = data;
		subresourceData2.SysMemPitch = 0;
		subresourceData2.SysMemSlicePitch = 0;
		game.Direct3DDevice()->CreateBuffer(&BufferDesc2, &subresourceData2, &mQuadVB);
	}

	// Only updates Effects
	void PostProcessingStack::Update()
	{
		if (mVignetteLoaded)
		{
			if (mVignetteEffect->isActive) 
				mVignetteEffect->Quad->SetActiveTechnique("vignette_filter", "p0");
			else
				mVignetteEffect->Quad->SetActiveTechnique("no_filter", "p0");
		}

		if (mColorGradingLoaded) 
		{
			if (mColorGradingEffect->isActive) 
				mColorGradingEffect->Quad->SetActiveTechnique("color_grading_filter", "p0");
			else 
				mColorGradingEffect->Quad->SetActiveTechnique("no_filter", "p0");
		}

		if (mMotionBlurLoaded)
		{
			if (mMotionBlurEffect->isActive)
				mMotionBlurEffect->Quad->SetActiveTechnique("blur_filter", "p0");
			else
				mMotionBlurEffect->Quad->SetActiveTechnique("no_filter", "p0");
			mMotionBlurEffect->preWVP = mMotionBlurEffect->WVP;
			mMotionBlurEffect->WVP = XMMatrixIdentity() * camera.ViewMatrix() * camera.ProjectionMatrix();
		}

		if (mFXAALoaded)
		{
			if (mFXAAEffect->isActive)
				mFXAAEffect->Quad->SetActiveTechnique("fxaa_filter", "p0");
			else
				mFXAAEffect->Quad->SetActiveTechnique("no_filter", "p0");
		}		
		
		if (mSSRLoaded)
		{
			if (mSSREffect->isActive)
				mSSREffect->Quad->SetActiveTechnique("ssr", "p0");
			else
				mSSREffect->Quad->SetActiveTechnique("no_ssr", "p0");
		}

	}

	void PostProcessingStack::UpdateTonemapConstantBuffer(ID3D11DeviceContext* pD3DImmediateContext, ID3D11Buffer* buffer, int mipLevel0, int mipLevel1, unsigned int width, unsigned int height)
	{
		int src[] = { mipLevel0, mipLevel1, width, height };
		int rowpitch = sizeof(int) * 4;
		pD3DImmediateContext->UpdateSubresource(buffer, 0, NULL, &src, rowpitch, 0);
		pD3DImmediateContext->PSSetConstantBuffers(0, 1, &buffer);
	}

	void PostProcessingStack::UpdateVignetteMaterial()
	{
		mVignetteEffect->Material->ColorTexture() << mVignetteEffect->OutputTexture;
		mVignetteEffect->Material->radius() << mVignetteEffect->radius;
		mVignetteEffect->Material->softness() << mVignetteEffect->softness;
	}

	void PostProcessingStack::UpdateColorGradingMaterial()
	{

		mColorGradingEffect->Material->ColorTexture() << mColorGradingEffect->OutputTexture;
		switch (mColorGradingEffect->currentLUT)
		{
		case 0:
			mColorGradingEffect->Material->lut() << mColorGradingEffect->LUT1;
			break;
		case 1:
			mColorGradingEffect->Material->lut() << mColorGradingEffect->LUT2;
			break;
		case 2:
			mColorGradingEffect->Material->lut() << mColorGradingEffect->LUT3;
			break;
		default:
			mColorGradingEffect->Material->lut() << mColorGradingEffect->LUT1;
			break;
		}

	}

	void PostProcessingStack::UpdateMotionBlurMaterial()
	{
		mMotionBlurEffect->Material->ColorTexture() << mMotionBlurEffect->OutputTexture;
		mMotionBlurEffect->Material->DepthTexture() << mMotionBlurEffect->DepthMap;

		mMotionBlurEffect->Material->WorldViewProjection()		 << mMotionBlurEffect->WVP;
		mMotionBlurEffect->Material->InvWorldViewProjection()	 << XMMatrixInverse(nullptr, mMotionBlurEffect->WVP);
		mMotionBlurEffect->Material->preWorldViewProjection()	 << mMotionBlurEffect->preWVP;
		mMotionBlurEffect->Material->preInvWorldViewProjection() << XMMatrixInverse(nullptr, mMotionBlurEffect->preWVP);
		mMotionBlurEffect->Material->WorldInverseTranspose()	 << mMotionBlurEffect->WIT;
		mMotionBlurEffect->Material->amount() << mMotionBlurEffect->amount;
	}

	void PostProcessingStack::UpdateSSRMaterial(ID3D11ShaderResourceView* normal, ID3D11ShaderResourceView* depth, ID3D11ShaderResourceView* extra, float time)
	{
		XMMATRIX vp = camera.ViewMatrix() * camera.ProjectionMatrix();

		mSSREffect->Material->ColorTexture() << mSSREffect->InputColor;
		mSSREffect->Material->GBufferDepth() << depth;
		mSSREffect->Material->GBufferNormals() << normal;
		mSSREffect->Material->GBufferExtra() << extra;
		mSSREffect->Material->ViewProjection() << vp;
		mSSREffect->Material->InvProjMatrix() << XMMatrixInverse(nullptr, camera.ProjectionMatrix());
		mSSREffect->Material->InvViewMatrix() << XMMatrixInverse(nullptr, camera.ViewMatrix());
		mSSREffect->Material->ViewMatrix() << camera.ViewMatrix();
		mSSREffect->Material->ProjMatrix() << camera.ProjectionMatrix();
		mSSREffect->Material->CameraPosition() << camera.PositionVector();
		mSSREffect->Material->StepSize() << mSSREffect->stepSize;
		mSSREffect->Material->MaxThickness() << mSSREffect->maxThickness;
		mSSREffect->Material->Time() << time;
		mSSREffect->Material->MaxRayCount() << mSSREffect->rayCount;
	}
	
	void PostProcessingStack::UpdateFXAAMaterial()
	{
		mFXAAEffect->Material->ColorTexture() << mFXAAEffect->OutputTexture;

		mFXAAEffect->Material->Width() << game.ScreenWidth();
		mFXAAEffect->Material->Height() << game.ScreenHeight();

	}
	
	void PostProcessingStack::ShowPostProcessingWindow()
	{
		ImGui::Begin("Post Processing Stack Config");

		if (mSSRLoaded)
		{
			if (ImGui::CollapsingHeader("Screen Space Reflections"))
			{
				ImGui::Checkbox("SSR - On", &mSSREffect->isActive);
				ImGui::SliderInt("Ray count", &mSSREffect->rayCount, 0, 100);
				ImGui::SliderFloat("Step Size", &mSSREffect->stepSize, 0.0f, 10.0f);
				ImGui::SliderFloat("Max Thickness", &mSSREffect->maxThickness, 0.0f, 0.01f);
			}
		}

		if (true)
		{
			if (ImGui::CollapsingHeader("Tonemap and Bloom"))
			{
				ImGui::Checkbox("Tonemap and Bloom - On", &mTonemapEffect->isActive);
				ImGui::SliderFloat("Middle Grey", &mTonemapEffect->middlegrey, 0.0f, 0.2f);
				ImGui::SliderFloat("Bloom Threshold", &mTonemapEffect->bloomthreshold, 0.0f, 1.0f);
				ImGui::SliderFloat("Bloom Multiplier", &mTonemapEffect->bloommultiplier, 0.0f, 1.0f);
			}
		}

		if (mMotionBlurLoaded)
		{
			if (ImGui::CollapsingHeader("Motion Blur"))
			{
				ImGui::Checkbox("Motion Blur - On", &mMotionBlurEffect->isActive);
				ImGui::SliderFloat("Amount", &mMotionBlurEffect->amount, 1.000f, 50.0f);
			}
		}
		
		if (mColorGradingLoaded) 
		{
			if (ImGui::CollapsingHeader("Color Grading"))
			{
				ImGui::Checkbox("Color Grading - On", &mColorGradingEffect->isActive);

				ImGui::TextWrapped("Current LUT");
				ImGui::RadioButton("LUT 1", &mColorGradingEffect->currentLUT, 0);
				ImGui::RadioButton("LUT 2", &mColorGradingEffect->currentLUT, 1);
				ImGui::RadioButton("LUT 3", &mColorGradingEffect->currentLUT, 2);
			}
		}

		if (mVignetteLoaded)
		{
			if (ImGui::CollapsingHeader("Vignette"))
			{
				ImGui::Checkbox("Vignette - On", &mVignetteEffect->isActive);
				ImGui::SliderFloat("Radius", &mVignetteEffect->radius, 0.000f, 1.0f);
				ImGui::SliderFloat("Softness", &mVignetteEffect->softness, 0.000f, 1.0f);
			}
		}	
		
		if (mFXAALoaded)
		{
			if (ImGui::CollapsingHeader("FXAA"))
			{
				ImGui::Checkbox("FXAA - On", &mFXAAEffect->isActive);
			}
		}

		ImGui::End();
	}

	ID3D11ShaderResourceView * PostProcessingStack::GetDepthOutputTexture()
	{
		return mMainRenderTarget->OutputDepthTexture(); 
	}

	//ID3D11ShaderResourceView * PostProcessingStack::GetPrepassColorOutputTexture()
	//{
	//	return mMainRenderTarget->OutputColorTexture();
	//}

	void PostProcessingStack::ComputeLuminance(ID3D11DeviceContext * pContext, ID3D11ShaderResourceView * pInput, ID3D11RenderTargetView* pOutput)
	{
		pContext->OMSetRenderTargets(1, &pOutput, NULL);
		pContext->PSSetShaderResources(0, 1, &pInput);
		pContext->PSSetShader(mTonemapEffect->CalcLumPS, NULL, NULL);
		DrawFullscreenQuad(pContext);

		pContext->OMSetRenderTargets(1, mTonemapEffect->AvgLuminanceResource->getRTVs(), NULL);
		ID3D11ShaderResourceView* srv = mTonemapEffect->LuminanceResource->getSRV();
		pContext->GenerateMips(srv);
		pContext->PSSetShaderResources(0, 1, &srv);
		pContext->PSSetShader(mTonemapEffect->AvgLumPS, NULL, NULL);
		DrawFullscreenQuad(pContext);
	}

	void PostProcessingStack::ComputeBrightPass(ID3D11DeviceContext * pContext, ID3D11ShaderResourceView * pInput, ID3D11RenderTargetView * pOutput)
	{
		ID3D11RenderTargetView* pRT[] = { pOutput };
		pContext->PSSetShaderResources(0, 1, &pInput);
		pContext->PSSetShader(mTonemapEffect->BrightPS, NULL, NULL);
		pContext->OMSetRenderTargets(1, pRT, NULL);
		DrawFullscreenQuad(pContext);
	}

	void PostProcessingStack::ComputeHorizontalBlur(ID3D11DeviceContext * pContext, ID3D11ShaderResourceView * pInput, ID3D11RenderTargetView * pOutput)
	{
		pContext->OMSetRenderTargets(1, &pOutput, NULL);
		ID3D11ShaderResourceView* pSR[] = { pInput };
		pContext->PSSetShaderResources(0, 1, pSR);
		pContext->PSSetShader(mTonemapEffect->BlurHPS, NULL, NULL);
		DrawFullscreenQuad(pContext);
	}

	void PostProcessingStack::ComputeVerticalBlur(ID3D11DeviceContext * pContext, ID3D11ShaderResourceView * pInput, ID3D11RenderTargetView * pOutput)
	{
		ID3D11ShaderResourceView* pSR[] = { NULL, NULL };
		pContext->PSSetShaderResources(0, 2, pSR);
		pContext->OMSetRenderTargets(1, &pOutput, NULL);
		pSR[0] = pInput;
		pContext->PSSetShaderResources(0, 1, pSR);
		pContext->PSSetShader(mTonemapEffect->BlurVPS, NULL, NULL);
		DrawFullscreenQuad(pContext);
	}

	void PostProcessingStack::ComputeToneMapWithBloom(ID3D11DeviceContext * pContext, ID3D11ShaderResourceView * pInput, ID3D11ShaderResourceView * pAVG, ID3D11ShaderResourceView * pBloom, ID3D11RenderTargetView * pOutput)
	{
		pContext->OMSetRenderTargets(1, &pOutput, NULL);
		ID3D11ShaderResourceView* pSR[] = { pInput, pBloom, pAVG };
		pContext->PSSetShaderResources(0, 3, pSR);
		pContext->PSSetShader(mTonemapEffect->ToneMapWithBloomPS, NULL, NULL);
		DrawFullscreenQuad(pContext);
	}

	void PostProcessingStack::PerformEmptyPass(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView * pInput, ID3D11RenderTargetView * pOutput)
	{
		pContext->OMSetRenderTargets(1, &pOutput, NULL);
		ID3D11ShaderResourceView* pSR[] = { pInput };
		pContext->PSSetShaderResources(0, 1, pSR);
		pContext->PSSetShader(mTonemapEffect->EmptyPassPS, NULL, NULL);
		DrawFullscreenQuad(pContext);
	}
	
	void PostProcessingStack::Begin()
	{
		mMainRenderTarget->Begin();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mMainRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mMainRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
	}

	void PostProcessingStack::End(const GameTime& gameTime)
	{
		mMainRenderTarget->End();
	}

	void PostProcessingStack::DrawEffects(const GameTime& gameTime)
	{
		ID3D11DeviceContext* context = game.Direct3DDeviceContext();

		// SSR
		mSSRRenderTarget->Begin();
		mSSREffect->InputColor = mMainRenderTarget->OutputColorTexture();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mSSRRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mSSRRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mSSREffect->Quad->Draw(gameTime);
		mSSRRenderTarget->End();


		// TONEMAP
		mTonemapRenderTarget->Begin();
		context->ClearRenderTargetView(mTonemapRenderTarget->RenderTargetView(), ClearBackgroundColor);
		context->ClearDepthStencilView(mTonemapRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);

		if (mTonemapEffect->isActive)
		{
			float pBuffer[] = { mTonemapEffect->middlegrey, mTonemapEffect->bloomthreshold, mTonemapEffect->bloommultiplier, 0.0f };
			context->UpdateSubresource(mTonemapEffect->BloomConstants, 0, NULL, pBuffer, sizeof(float) * 4, 0);
			context->PSSetConstantBuffers(1, 1, &mTonemapEffect->BloomConstants);
			D3D11_VIEWPORT viewport;
			UINT numViewPorts = 1;
			context->RSGetViewports(&numViewPorts, &viewport);

			unsigned int width = 512;
			unsigned int height = 512;

			int gameWidth = game.ScreenWidth();
			int gameHeight = game.ScreenHeight();

			D3D11_VIEWPORT quadViewPort;
			quadViewPort.Height = (float)height;
			quadViewPort.Width = (float)width;
			quadViewPort.MaxDepth = 1.0f;
			quadViewPort.MinDepth = 0.0f;
			quadViewPort.TopLeftX = 0;
			quadViewPort.TopLeftY = 0;

			context->PSSetSamplers(0, 1, &mTonemapEffect->LinearSampler);
			context->RSSetViewports(1, &quadViewPort);

			UpdateTonemapConstantBuffer(context, mTonemapEffect->ConstBuffer, 0, 0, width, height);
			ComputeLuminance(context, mSSRRenderTarget->OutputColorTexture(), *mTonemapEffect->LuminanceResource->getRTVs());

			quadViewPort.Height = (float)(gameHeight / 2);
			quadViewPort.Width = (float)(gameWidth / 2);
			context->RSSetViewports(1, &quadViewPort);

			ComputeBrightPass(context, mSSRRenderTarget->OutputColorTexture(), *mTonemapEffect->BrightResource->getRTVs());
			ID3D11RenderTargetView* pNULLRT[] = { NULL };
			context->OMSetRenderTargets(1, pNULLRT, NULL);
			context->GenerateMips(mTonemapEffect->BrightResource->getSRV());

			int mipLevel0 = 3;
			int mipLevel1 = 4;
			width = gameWidth >> (mipLevel0 + 1);
			height = gameHeight >> (mipLevel0 + 1);
			quadViewPort.Height = (float)height;
			quadViewPort.Width = (float)width;
			context->RSSetViewports(1, &quadViewPort);
			UpdateTonemapConstantBuffer(context, mTonemapEffect->ConstBuffer, mipLevel0, mipLevel1, width, height);
			ComputeHorizontalBlur(context, mTonemapEffect->BrightResource->getSRV(), mTonemapEffect->BlurHorizontalResource->getRTVs()[mipLevel0]);
			ComputeVerticalBlur(context, mTonemapEffect->BlurHorizontalResource->getSRV(), mTonemapEffect->BlurVerticalResource->getRTVs()[mipLevel0]);

			mipLevel0--;
			mipLevel1--;
			width = gameWidth >> (mipLevel0 + 1);;
			height = gameHeight >> (mipLevel0 + 1);
			quadViewPort.Height = (float)height;
			quadViewPort.Width = (float)width;
			context->RSSetViewports(1, &quadViewPort);

			UpdateTonemapConstantBuffer(context, mTonemapEffect->ConstBuffer, mipLevel0, mipLevel1, width, height);
			context->OMSetRenderTargets(1, &mTonemapEffect->BlurSummedResource->getRTVs()[mipLevel0], NULL);
			ID3D11ShaderResourceView* pSR[] = { mTonemapEffect->BrightResource->getSRV(),  mTonemapEffect->BlurVerticalResource->getSRV() };
			context->PSSetShaderResources(0, 2, pSR);
			context->PSSetShader(mTonemapEffect->AddPS, NULL, NULL);
			DrawFullscreenQuad(context);
			ComputeHorizontalBlur(context, mTonemapEffect->BlurSummedResource->getSRV(), mTonemapEffect->BlurHorizontalResource->getRTVs()[mipLevel0]);
			ComputeVerticalBlur(context, mTonemapEffect->BlurHorizontalResource->getSRV(), mTonemapEffect->BlurVerticalResource->getRTVs()[mipLevel0]);

			mipLevel0--;
			mipLevel1--;
			width = gameWidth >> (mipLevel0 + 1);
			height = gameHeight >> (mipLevel0 + 1);
			quadViewPort.Height = (float)height;
			quadViewPort.Width = (float)width;
			context->RSSetViewports(1, &quadViewPort);

			UpdateTonemapConstantBuffer(context, mTonemapEffect->ConstBuffer, mipLevel0, mipLevel1, width, height);
			context->OMSetRenderTargets(1, &mTonemapEffect->BlurSummedResource->getRTVs()[mipLevel0], NULL);
			ID3D11ShaderResourceView* pSR2[] = { mTonemapEffect->BrightResource->getSRV(), mTonemapEffect->BlurVerticalResource->getSRV() };
			context->PSSetShaderResources(0, 2, pSR2);
			context->PSSetShader(mTonemapEffect->AddPS, NULL, NULL);
			DrawFullscreenQuad(context);
			ComputeHorizontalBlur(context, mTonemapEffect->BlurSummedResource->getSRV(), mTonemapEffect->BlurHorizontalResource->getRTVs()[mipLevel0]);
			ComputeVerticalBlur(context, mTonemapEffect->BlurHorizontalResource->getSRV(), mTonemapEffect->BlurVerticalResource->getRTVs()[mipLevel0]);

			mipLevel0--;
			mipLevel1--;
			width = gameWidth >> (mipLevel0 + 1);
			height = gameHeight >> (mipLevel0 + 1);
			quadViewPort.Height = (float)height;
			quadViewPort.Width = (float)width;
			context->RSSetViewports(1, &quadViewPort);

			UpdateTonemapConstantBuffer(context, mTonemapEffect->ConstBuffer, mipLevel0, mipLevel1, width, height);
			context->OMSetRenderTargets(1, &mTonemapEffect->BlurSummedResource->getRTVs()[mipLevel0], NULL);
			ID3D11ShaderResourceView* pSR3[] = { mTonemapEffect->BrightResource->getSRV(), mTonemapEffect->BlurVerticalResource->getSRV() };
			context->PSSetShaderResources(0, 2, pSR3);
			context->PSSetShader(mTonemapEffect->AddPS, NULL, NULL);
			DrawFullscreenQuad(context);
			ComputeHorizontalBlur(context, mTonemapEffect->BlurSummedResource->getSRV(), mTonemapEffect->BlurHorizontalResource->getRTVs()[mipLevel0]);
			ComputeVerticalBlur(context, mTonemapEffect->BlurHorizontalResource->getSRV(), mTonemapEffect->BlurVerticalResource->getRTVs()[mipLevel0]);

			context->RSSetViewports(1, &viewport);
			ComputeToneMapWithBloom(context, mSSRRenderTarget->OutputColorTexture(), mTonemapEffect->AvgLuminanceResource->getSRV(), mTonemapEffect->BlurVerticalResource->getSRV(), mTonemapRenderTarget->RenderTargetView());
		}
		else
		{
			PerformEmptyPass(context, mSSRRenderTarget->OutputColorTexture(), mTonemapRenderTarget->RenderTargetView());
		}
		mTonemapRenderTarget->End();

		// MOTION BLUR
		mMotionBlurRenderTarget->Begin();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mMotionBlurRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mMotionBlurRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mMotionBlurEffect->OutputTexture = mTonemapRenderTarget->OutputColorTexture();
		mMotionBlurEffect->DepthMap = mMainRenderTarget->OutputDepthTexture();
		mMotionBlurEffect->Quad->Draw(gameTime);
		mMotionBlurRenderTarget->End();

		// COLOR GRADING
		mColorGradingRenderTarget->Begin();
		mColorGradingEffect->OutputTexture = mMotionBlurRenderTarget->OutputColorTexture();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mColorGradingRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mColorGradingRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mColorGradingEffect->Quad->Draw(gameTime);
		mColorGradingRenderTarget->End();
		
		// VIGNETTE
		mVignetteRenderTarget->Begin();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mVignetteRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mVignetteRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mVignetteEffect->OutputTexture = mColorGradingRenderTarget->OutputColorTexture();
		mVignetteEffect->Quad->Draw(gameTime);
		mVignetteRenderTarget->End();

		// FXAA
		/* uncomment if there is an effect after this one */
		//mFXAARenderTarget->Begin();
		//game.Direct3DDeviceContext()->ClearRenderTargetView(mFXAARenderTarget->RenderTargetView(), ClearBackgroundColor);
		//game.Direct3DDeviceContext()->ClearDepthStencilView(mFXAARenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mFXAAEffect->OutputTexture = mVignetteRenderTarget->OutputColorTexture();
		mFXAAEffect->Quad->Draw(gameTime);
		//mFXAARenderTarget->End();
	}

	void PostProcessingStack::DrawFullscreenQuad(ID3D11DeviceContext* pContext)
	{
		pContext->VSSetShader(mFullScreenQuadVS, NULL, 0);
		pContext->IASetInputLayout(mFullScreenQuadLayout);
		ID3D11Buffer* pVBs[] = { mQuadVB };
		UINT strides[] = { sizeof(float) * 3 };
		UINT offsets[] = { 0 };
		pContext->IASetVertexBuffers(0, 1, pVBs, strides, offsets);
		pContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->Draw(4, 0);
	}
}








