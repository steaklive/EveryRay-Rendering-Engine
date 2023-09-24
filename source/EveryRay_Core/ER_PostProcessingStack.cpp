#include "ER_PostProcessingStack.h"
#include "ER_CoreException.h"
#include "ER_Utility.h"
#include "ER_ColorHelper.h"
#include "ER_MatrixHelper.h"
#include "ER_QuadRenderer.h"
#include "ER_GBuffer.h"
#include "ER_Camera.h"
#include "ER_VolumetricClouds.h"
#include "ER_VolumetricFog.h"
#include "ER_Illumination.h"
#include "ER_Settings.h"
#include "ER_RenderableAABB.h"
#include "ER_Scene.h"

#define LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define SSS_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define SSS_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define SSR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define SSR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define TONEMAP_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0

#define COLORGRADING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0

#define VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define FXAA_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define FXAA_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define FINALRESOLVE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0

const XMFLOAT4 DebugPostEffectsVolumeColor = { 1.0, 0.0, 0.5, 1.0 };

namespace EveryRay_Core {

	ER_PostProcessingStack::ER_PostProcessingStack(ER_Core& pCore, ER_Camera& pCamera)
		: mCore(pCore), mCamera(pCamera)
	{
	}

	ER_PostProcessingStack::~ER_PostProcessingStack()
	{
		DeleteObject(mTonemappingRT);
		DeleteObject(mSSRRT);
		DeleteObject(mSSSRT);
		DeleteObject(mColorGradingRT);
		DeleteObject(mVignetteRT);
		DeleteObject(mFXAART);
		DeleteObject(mLinearFogRT);
		DeleteObject(mVolumetricFogRT);

		DeleteObject(mTonemappingPS);
		DeleteObject(mSSRPS);
		DeleteObject(mSSSPS);
		DeleteObject(mColorGradingPS);
		DeleteObject(mVignettePS);
		DeleteObject(mFXAAPS);
		DeleteObject(mLinearFogPS);
		DeleteObject(mFinalResolvePS);

		DeleteObject(mTonemapRS);
		DeleteObject(mSSRRS);
		DeleteObject(mSSSRS);
		DeleteObject(mColorGradingRS);
		DeleteObject(mVignetteRS);
		DeleteObject(mFXAARS);
		DeleteObject(mLinearFogRS);
		DeleteObject(mFinalResolveRS);

		mPostEffectsVolumes.clear();

		mSSRConstantBuffer.Release();
		mSSSConstantBuffer.Release();
		mFXAAConstantBuffer.Release();
		mVignetteConstantBuffer.Release();
		mLinearFogConstantBuffer.Release();
	}

	void ER_PostProcessingStack::Initialize()
	{
		auto rhi = mCore.GetRHI();

		mFinalResolvePS = rhi->CreateGPUShader();
		mFinalResolvePS->CompileShader(rhi, "content\\shaders\\EmptyColorResolve.hlsl", "PSMain", ER_PIXEL);
		mFinalResolveRS = rhi->CreateRootSignature(1, 1);
		if (mFinalResolveRS)
		{
			mFinalResolveRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mFinalResolveRS->InitDescriptorTable(rhi, FINALRESOLVE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mFinalResolveRS->Finalize(rhi, "ER_RHI_GPURootSignature: Final Resolve Pass", true);
		}

		//Linear fog
		{
			mLinearFogPS = rhi->CreateGPUShader();
			mLinearFogPS->CompileShader(rhi, "content\\shaders\\LinearFog.hlsl", "PSMain", ER_PIXEL);
			
			mLinearFogConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Linear Fog CB");

			mLinearFogRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Linear Fog RT");
			mLinearFogRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mLinearFogRS = rhi->CreateRootSignature(2, 1);
			if (mLinearFogRS)
			{
				mLinearFogRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mLinearFogRS->InitDescriptorTable(rhi, LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mLinearFogRS->InitDescriptorTable(rhi, LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mLinearFogRS->Finalize(rhi, "ER_RHI_GPURootSignature: Linear Fog Pass", true);
			}
		}

		mVolumetricFogRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Volumetric Fog RT");
		mVolumetricFogRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
			ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

		//SSR
		{
			mSSRPS = rhi->CreateGPUShader();
			mSSRPS->CompileShader(rhi, "content\\shaders\\SSR.hlsl", "PSMain", ER_PIXEL);

			mSSRConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: SSR CB");
			
			mSSRRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: SSR RT");
			mSSRRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mSSRRS = rhi->CreateRootSignature(2, 1);
			if (mSSRRS)
			{
				mSSRRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSRRS->InitDescriptorTable(rhi, SSR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 5 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSRRS->InitDescriptorTable(rhi, SSR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSRRS->Finalize(rhi, "ER_RHI_GPURootSignature: SSR Pass", true);
			}
		}

		//SSS
		{
			mSSSPS = rhi->CreateGPUShader();
			mSSSPS->CompileShader(rhi, "content\\shaders\\SSS.hlsl", "BlurPS", ER_PIXEL);

			mSSSConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: SSS CB");

			mSSSRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: SSS RT");
			mSSSRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mSSSRS = rhi->CreateRootSignature(2, 1);
			if (mSSSRS)
			{
				mSSSRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSSRS->InitDescriptorTable(rhi, SSS_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 3 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSSRS->InitDescriptorTable(rhi, SSS_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSSRS->Finalize(rhi, "ER_RHI_GPURootSignature: SSS Pass", true);
			}
		}

		//Tonemap
		{		
			mTonemappingPS = rhi->CreateGPUShader();
			mTonemappingPS->CompileShader(rhi, "content\\shaders\\Tonemap.hlsl", "PSMain", ER_PIXEL);

			mTonemappingRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Tonemapping RT");
			mTonemappingRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mTonemapRS = rhi->CreateRootSignature(1, 1);
			if (mTonemapRS)
			{
				mTonemapRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mTonemapRS->InitDescriptorTable(rhi, TONEMAP_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mTonemapRS->Finalize(rhi, "ER_RHI_GPURootSignature: Tonemap Pass", true);
			}
		}

		//Color grading
		{
			mLUTs[0] = rhi->CreateGPUTexture(L"");
			mLUTs[0]->CreateGPUTextureResource(rhi, "content\\shaders\\LUT_1.png");
			mLUTs[1] = rhi->CreateGPUTexture(L"");
			mLUTs[1]->CreateGPUTextureResource(rhi, "content\\shaders\\LUT_2.png");
			mLUTs[2] = rhi->CreateGPUTexture(L"");
			mLUTs[2]->CreateGPUTextureResource(rhi, "content\\shaders\\LUT_3.png");
			//...more
			
			mColorGradingPS = rhi->CreateGPUShader();
			mColorGradingPS->CompileShader(rhi, "content\\shaders\\ColorGrading.hlsl", "PSMain", ER_PIXEL);

			mColorGradingRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Color Grading RT");
			mColorGradingRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mColorGradingRS = rhi->CreateRootSignature(1, 0);
			if (mColorGradingRS)
			{
				mColorGradingRS->InitDescriptorTable(rhi, COLORGRADING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mColorGradingRS->Finalize(rhi, "ER_RHI_GPURootSignature: Color Grading Pass", true);
			}
		}

		//Vignette
		{
			mVignettePS = rhi->CreateGPUShader();
			mVignettePS->CompileShader(rhi, "content\\shaders\\Vignette.hlsl", "PSMain", ER_PIXEL);

			mVignetteConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Vignette CB");
			
			mVignetteRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Vignette RT");
			mVignetteRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mVignetteRS = rhi->CreateRootSignature(2, 1);
			if (mVignetteRS)
			{
				mVignetteRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mVignetteRS->InitDescriptorTable(rhi, VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mVignetteRS->InitDescriptorTable(rhi, VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mVignetteRS->Finalize(rhi, "ER_RHI_GPURootSignature: Vignette Pass", true);
			}
		}	
		
		//FXAA
		{
			mFXAAPS = rhi->CreateGPUShader();
			mFXAAPS->CompileShader(rhi, "content\\shaders\\FXAA.hlsl", "PSMain", ER_PIXEL);

			mFXAAConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: FXAA CB");

			mFXAART = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: FXAA RT");
			mFXAART->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mFXAARS = rhi->CreateRootSignature(2, 1);
			if (mFXAARS)
			{
				mFXAARS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mFXAARS->InitDescriptorTable(rhi, FXAA_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mFXAARS->InitDescriptorTable(rhi, FXAA_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mFXAARS->Finalize(rhi, "ER_RHI_GPURootSignature: FXAA Pass", true);
			}
		}
	}

	void ER_PostProcessingStack::Update()
	{	
		UpdatePostEffectsVolumes();

		// re-adjust some effects if global graphics settings don't allow them
		mUseFXAA &= ER_Settings::AntiAliasingQuality > 0;
		mUseSSS &= ER_Settings::SubsurfaceScatteringQuality > 0;

		ShowPostProcessingWindow();
	}

	void ER_PostProcessingStack::ReservePostEffectsVolumes(int count)
	{
		assert(mPostEffectsVolumes.size() == 0 && count);
		mPostEffectsVolumes.reserve(count);
	}

	bool ER_PostProcessingStack::AddPostEffectsVolume(const XMFLOAT4X4& aTransform, const PostEffectsVolumeValues& aValues, const std::string& aName)
	{
		if (mPostEffectsVolumes.size() < MAX_POST_EFFECT_VOLUMES)
		{
			mPostEffectsVolumes.emplace_back(mCore, aTransform, aValues, aName);

			std::string message = "[ER Logger][ER_PostProcessingStack] Added a new volume: " + aName + "\n";
			ER_OUTPUT_LOG(ER_Utility::ToWideString(message).c_str());

			return true;
		}
		else
			return false;
	}

	void ER_PostProcessingStack::ShowPostProcessingWindow()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("Post Processing Stack Config");
		ImGui::TextWrapped("You can change default values below (visible if outside the volume or when the volume is disabled)");
		
		if (ImGui::CollapsingHeader("Linear Fog"))
		{
			ImGui::Checkbox("Fog - On", &mUseLinearFogDefault);
			ImGui::ColorEdit3("Color", mLinearFogColorDefault);
			ImGui::SliderFloat("Density", &mLinearFogDensityDefault, 1.0f, 10000.0f);
		}

		if (ImGui::CollapsingHeader("Separable Subsurface Scattering"))
		{
			ER_Illumination* illumination = mCore.GetLevel()->mIllumination;

			ImGui::Checkbox("SSS - On", &mUseSSSDefault);
			illumination->SetSSS(mUseSSS);

			float strength = illumination->GetSSSStrength();
			ImGui::SliderFloat("SSS Strength", &strength, 0.0f, 15.0f);
			illumination->SetSSSStrength(strength);

			float translucency = illumination->GetSSSTranslucency();
			ImGui::SliderFloat("SSS Translucency", &translucency, 0.0f, 1.0f);
			illumination->SetSSSTranslucency(translucency);

			float width = illumination->GetSSSWidth();
			ImGui::SliderFloat("SSS Width", &width, 0.0f, 0.1f);
			illumination->SetSSSWidth(width);

			float lightPlaneScale = illumination->GetSSSDirLightPlaneScale();
			ImGui::SliderFloat("Dir light plane scale", &lightPlaneScale, 0.0f, 10.0f);
			illumination->SetSSSDirLightPlaneScale(lightPlaneScale);
		}

		if (ImGui::CollapsingHeader("Screen Space Reflections"))
		{
			ImGui::Checkbox("SSR - On", &mUseSSRDefault);
			ImGui::SliderInt("Ray count", &mSSRRayCount, 0, 100);
			ImGui::SliderFloat("Step Size", &mSSRStepSize, 0.0f, 10.0f);
			ImGui::SliderFloat("Max Thickness", &mSSRMaxThickness, 0.0f, 0.01f);
		}

		if (ImGui::CollapsingHeader("Tonemap"))
		{
			ImGui::Checkbox("Tonemap - On", &mUseTonemapDefault);
		}

		if (ImGui::CollapsingHeader("Color Grading"))
		{
			ImGui::Checkbox("Color Grading - On", &mUseColorGradingDefault);

			ImGui::TextWrapped("Current LUT");
			const int size = sizeof(mLUTs) / sizeof(mLUTs[0]);
			for (int i = 0; i < size; i++)
			{
				std::string name = "LUT " + std::to_string(i);
				ImGui::RadioButton(name.c_str(), &(static_cast<int>(mColorGradingCurrentLUTIndexDefault)), i);
			}
		}

		if (ImGui::CollapsingHeader("Vignette"))
		{
			ImGui::Checkbox("Vignette - On", &mUseVignetteDefault);
			ImGui::SliderFloat("Radius", &mVignetteRadiusDefault, 0.000f, 1.0f);
			ImGui::SliderFloat("Softness", &mVignetteSoftnessDefault, 0.000f, 1.0f);
		}

		if (ImGui::CollapsingHeader("FXAA"))
		{
			ImGui::Checkbox("FXAA - On", &mUseFXAA);
		}

		ImGui::Separator();
		ImGui::TextWrapped("You can transform the volumes below (when editor is enabled)");
		ImGui::TextWrapped("Note: saving the values from above to the volume is not yet supported!");
		ImGui::Checkbox("Show volumes", &mShowDebugVolumes);
		ImGui::Checkbox("Enable volume editor", &ER_Utility::IsPostEffectsVolumeEditor);
		
		if (ImGui::Button("Save volume changes"))
			mCore.GetLevel()->mScene->SavePostProcessingVolumes();

		bool editable = ER_Utility::IsEditorMode && ER_Utility::IsPostEffectsVolumeEditor;
		if (editable)
		{
			//disable all other editors
			ER_Utility::IsFoliageEditor = ER_Utility::IsLightEditor = false;

			for (int i = 0; i < static_cast<int>(mPostEffectsVolumes.size()); i++)
				mEditorPostEffectsVolumesNames[i] = mPostEffectsVolumes[i].name.c_str();

			ImGui::PushItemWidth(-1);
			ImGui::ListBox("##empty", &mSelectedEditorPostEffectsVolumeIndex, mEditorPostEffectsVolumesNames, static_cast<int>(mPostEffectsVolumes.size()), 5);

			//Transform the selected volume
			if (mSelectedEditorPostEffectsVolumeIndex != -1)
			{
				float cameraViewMat[16];
				float cameraProjMat[16];
				ER_MatrixHelper::GetFloatArray(mCamera.ViewMatrix4X4(), cameraViewMat);
				ER_MatrixHelper::GetFloatArray(mCamera.ProjectionMatrix4X4(), cameraProjMat);

				ER_MatrixHelper::GetFloatArray(mPostEffectsVolumes[mSelectedEditorPostEffectsVolumeIndex].GetTransform(), mEditorCurrentPostEffectsVolumeTransformMatrix);

				static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
				static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
				static bool useSnap = false;
				static float snap[3] = { 1.f, 1.f, 1.f };
				static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
				static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
				static bool boundSizing = false;
				static bool boundSizingSnap = false;

				if (ImGui::IsKeyPressed(84))
					mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
				if (ImGui::IsKeyPressed(82))
					mCurrentGizmoOperation = ImGuizmo::SCALE;

				if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
					mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
				ImGui::SameLine();
				if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
					mCurrentGizmoOperation = ImGuizmo::SCALE;

				ImGuizmo::DecomposeMatrixToComponents(mEditorCurrentPostEffectsVolumeTransformMatrix, 
					mEditorPostEffectsVolumeMatrixTranslation, mEditorPostEffectsVolumeMatrixRotation, mEditorPostEffectsVolumeMatrixScale);
				ImGui::InputFloat3("Tr", mEditorPostEffectsVolumeMatrixTranslation, 3);
				ImGui::InputFloat3("Sc", mEditorPostEffectsVolumeMatrixScale, 3);
				ImGuizmo::RecomposeMatrixFromComponents(mEditorPostEffectsVolumeMatrixTranslation,
					mEditorPostEffectsVolumeMatrixRotation, mEditorPostEffectsVolumeMatrixScale, mEditorCurrentPostEffectsVolumeTransformMatrix);
				ImGui::Checkbox("Volume enabled", &(mPostEffectsVolumes[mSelectedEditorPostEffectsVolumeIndex].isEnabled));
				ImGui::End();

				ImGuiIO& io = ImGui::GetIO();
				ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
				ImGuizmo::Manipulate(cameraViewMat, cameraProjMat, mCurrentGizmoOperation, mCurrentGizmoMode, mEditorCurrentPostEffectsVolumeTransformMatrix,
					NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);

				XMFLOAT4X4 mat(mEditorCurrentPostEffectsVolumeTransformMatrix);
				mPostEffectsVolumes[mSelectedEditorPostEffectsVolumeIndex].SetTransform(mat, true);
			}
			else
				ImGui::End();
		}
		else
			ImGui::End();
	}
	
	void ER_PostProcessingStack::Begin(ER_RHI_GPUTexture* aInitialRT, ER_RHI_GPUTexture* aDepthTarget)
	{
		assert(aInitialRT && aDepthTarget);
		mRenderTargetBeforePostProcessingPasses = aInitialRT;
		mDepthTarget = aDepthTarget;

		auto rhi = mCore.GetRHI();
		rhi->SetRenderTargets({ aInitialRT }, aDepthTarget);
	}

	void ER_PostProcessingStack::End(ER_RHI_GPUTexture* aResolveRT)
	{
		auto rhi = mCore.GetRHI();
		rhi->UnbindRenderTargets();

		rhi->SetMainRenderTargets();

		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		rhi->SetRootSignature(mFinalResolveRS);

		//final resolve to main RT (pre-UI)
		{
			ER_QuadRenderer* quad = (ER_QuadRenderer*)mCore.GetServices().FindService(ER_QuadRenderer::TypeIdClass());
			assert(quad);
			assert(mRenderTargetBeforeResolve);

			if (!rhi->IsPSOReady(mFinalResolvePassPSOName))
			{
				rhi->InitializePSO(mFinalResolvePassPSOName);
				rhi->SetShader(mFinalResolvePS);
				rhi->SetMainRenderTargetFormats();
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetDepthStencilState(ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetTopologyTypeToPSO(mFinalResolvePassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				rhi->SetRootSignatureToPSO(mFinalResolvePassPSOName, mFinalResolveRS);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mFinalResolvePassPSOName);
			}
			rhi->SetPSO(mFinalResolvePassPSOName);
			rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
			rhi->SetShaderResources(ER_PIXEL, { aResolveRT ? aResolveRT : mRenderTargetBeforeResolve }, 0, mFinalResolveRS, FINALRESOLVE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
			quad->Draw(rhi);
			rhi->UnsetPSO();
		}

		rhi->UnbindResourcesFromShader(ER_PIXEL);
	}

	void ER_PostProcessingStack::UpdatePostEffectsVolumes()
	{
		mCurrentActivePostEffectsVolumeIndex = -1;
		// TODO: add priority system at some point
		for (int i = 0; i < static_cast<int>(mPostEffectsVolumes.size()); i++)
		{
			if (!mPostEffectsVolumes[i].isEnabled)
				continue;

			ER_AABB& aabb = mPostEffectsVolumes[i].aabb;

			bool isColliding =
				(mCamera.Position().x <= aabb.second.x && mCamera.Position().x >= aabb.first.x) &&
				(mCamera.Position().y <= aabb.second.y && mCamera.Position().y >= aabb.first.y) &&
				(mCamera.Position().z <= aabb.second.z && mCamera.Position().z >= aabb.first.z);

			if (isColliding)
			{
				mCurrentActivePostEffectsVolumeIndex = i;
				break;
			}
		}

		SetPostEffectsValuesFromVolume(mCurrentActivePostEffectsVolumeIndex);
	}

	void ER_PostProcessingStack::SetPostEffectsValuesFromVolume(int index /*= -1*/)
	{
		if (index == -1 || !mPostEffectsVolumes[index].isEnabled)
		{
			//set defaults
			mUseLinearFog = mUseLinearFogDefault;
			mLinearFogColor[0] = mLinearFogColorDefault[0];
			mLinearFogColor[1] = mLinearFogColorDefault[1];
			mLinearFogColor[2] = mLinearFogColorDefault[2];
			mLinearFogDensity = mLinearFogDensityDefault;

			mUseTonemap = mUseTonemapDefault;
			mUseSSR = mUseSSRDefault;
			mUseSSS = mUseSSSDefault;

			mUseVignette = mUseVignetteDefault;
			mVignetteSoftness = mVignetteSoftnessDefault;
			mVignetteRadius = mVignetteRadiusDefault;

			mUseColorGrading = mUseColorGradingDefault;
			mColorGradingCurrentLUTIndex = mColorGradingCurrentLUTIndexDefault;
		}
		else
		{
			PostEffectsVolume& currentVolume = mPostEffectsVolumes[index];

			mUseLinearFog = currentVolume.values.linearFogEnable;
			mLinearFogColor[0] = currentVolume.values.linearFogColor[0];
			mLinearFogColor[1] = currentVolume.values.linearFogColor[1];
			mLinearFogColor[2] = currentVolume.values.linearFogColor[2];
			mLinearFogDensity = currentVolume.values.linearFogDensity;

			mUseTonemap = currentVolume.values.tonemappingEnable;
			mUseSSR = currentVolume.values.ssrEnable;
			mUseSSS = currentVolume.values.sssEnable;

			mUseVignette = currentVolume.values.vignetteEnable;
			mVignetteSoftness = currentVolume.values.vignetteSoftness;
			mVignetteRadius = currentVolume.values.vignetteRadius;

			mUseColorGrading = currentVolume.values.colorGradingEnable;
			mColorGradingCurrentLUTIndex = currentVolume.values.colorGradingLUTIndex;
		}
	}

	void ER_PostProcessingStack::PrepareDrawingTonemapping(ER_RHI_GPUTexture* aInputTexture, ER_GBuffer* gbuffer)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture, gbuffer->GetDepth() }, 0, mTonemapRS, TONEMAP_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingSSR(const ER_CoreTime& gameTime, ER_RHI_GPUTexture* aInputTexture, ER_GBuffer* gbuffer)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		mSSRConstantBuffer.Data.InvProjMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, mCamera.ProjectionMatrix()));
		mSSRConstantBuffer.Data.InvViewMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, mCamera.ViewMatrix()));
		mSSRConstantBuffer.Data.ViewMatrix = XMMatrixTranspose(mCamera.ViewMatrix());
		mSSRConstantBuffer.Data.ProjMatrix = XMMatrixTranspose(mCamera.ProjectionMatrix());
		mSSRConstantBuffer.Data.CameraPosition = XMFLOAT4(mCamera.Position().x,mCamera.Position().y,mCamera.Position().z,1.0f);
		mSSRConstantBuffer.Data.StepSize = mSSRStepSize;
		mSSRConstantBuffer.Data.MaxThickness = mSSRMaxThickness;
		mSSRConstantBuffer.Data.Time = static_cast<float>(gameTime.TotalCoreTime());
		mSSRConstantBuffer.Data.MaxRayCount = mSSRRayCount;
		mSSRConstantBuffer.ApplyChanges(rhi);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture, gbuffer->GetNormals(), gbuffer->GetExtraBuffer(), gbuffer->GetExtra2Buffer(), mDepthTarget }, 0,
			mSSSRS, SSR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL, { mSSRConstantBuffer.Buffer() }, 0, mSSRRS, SSR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingSSS(const ER_CoreTime& gameTime, ER_RHI_GPUTexture* aInputTexture, ER_GBuffer* gbuffer, bool verticalPass)
	{
		auto rhi = mCore.GetRHI();

		ER_Illumination* illumination = mCore.GetLevel()->mIllumination;
		assert(illumination);
		assert(aInputTexture);

		if (verticalPass)
			mSSSConstantBuffer.Data.SSSStrengthWidthDir = XMFLOAT4(illumination->GetSSSStrength(), illumination->GetSSSWidth(), 1.0f, 0.0f);
		else
			mSSSConstantBuffer.Data.SSSStrengthWidthDir = XMFLOAT4(illumination->GetSSSStrength(), illumination->GetSSSWidth(), 0.0f, 1.0f);
		mSSSConstantBuffer.Data.CameraFOV = mCamera.FieldOfView();
		mSSSConstantBuffer.ApplyChanges(rhi);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture, mDepthTarget, gbuffer->GetExtra2Buffer() }, 0, mSSSRS, SSS_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL, { mSSSConstantBuffer.Buffer() }, 0, mSSSRS, SSS_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingLinearFog(ER_RHI_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		mLinearFogConstantBuffer.Data.FogColor = XMFLOAT4{ mLinearFogColor[0], mLinearFogColor[1], mLinearFogColor[2], 1.0f };
		mLinearFogConstantBuffer.Data.FogNear = mCamera.NearPlaneDistance();
		mLinearFogConstantBuffer.Data.FogFar = mCamera.FarPlaneDistance();
		mLinearFogConstantBuffer.Data.FogDensity = mLinearFogDensity;
		mLinearFogConstantBuffer.ApplyChanges(rhi);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture, mDepthTarget }, 0, mLinearFogRS, LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL, { mLinearFogConstantBuffer.Buffer() }, 0, mLinearFogRS, LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingColorGrading(ER_RHI_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		rhi->SetShaderResources(ER_PIXEL, { mLUTs[mColorGradingCurrentLUTIndex], aInputTexture }, 0, mColorGradingRS, COLORGRADING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingVignette(ER_RHI_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		mVignetteConstantBuffer.Data.RadiusSoftness = XMFLOAT2(mVignetteRadius, mVignetteSoftness);
		mVignetteConstantBuffer.ApplyChanges(rhi);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetConstantBuffers(ER_PIXEL, { mVignetteConstantBuffer.Buffer() }, 0, mVignetteRS, VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture }, 0, mVignetteRS, VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingFXAA(ER_RHI_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		mFXAAConstantBuffer.Data.ScreenDimensions = XMFLOAT2(static_cast<float>(mCore.ScreenWidth()),static_cast<float>(mCore.ScreenHeight()));
		mFXAAConstantBuffer.ApplyChanges(rhi);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture }, 0, mFXAARS, FXAA_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL, { mFXAAConstantBuffer.Buffer() }, 0, mFXAARS, FXAA_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
	}

	void ER_PostProcessingStack::DrawEffects(const ER_CoreTime& gameTime, ER_QuadRenderer* quad, ER_GBuffer* gbuffer, ER_VolumetricClouds* aVolumetricClouds, ER_VolumetricFog* aVolumetricFog)
	{
		assert(quad);
		assert(gbuffer);
		auto rhi = mCore.GetRHI();

		mRenderTargetBeforeResolve = mRenderTargetBeforePostProcessingPasses;
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Linear fog
		if (mUseLinearFog)
		{
			rhi->BeginEventTag("EveryRay: Post Processing (Linear Fog)");

			rhi->SetRenderTargets({ mLinearFogRT });
			rhi->SetRootSignature(mLinearFogRS);
			if (!rhi->IsPSOReady(mLinearFogPassPSOName))
			{
				rhi->InitializePSO(mLinearFogPassPSOName);
				rhi->SetShader(mLinearFogPS);
				rhi->SetRenderTargetFormats({ mLinearFogRT });
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRootSignatureToPSO(mLinearFogPassPSOName, mLinearFogRS);
				rhi->SetTopologyTypeToPSO(mLinearFogPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mLinearFogPassPSOName);
			}
			rhi->SetPSO(mLinearFogPassPSOName);
			PrepareDrawingLinearFog(mRenderTargetBeforeResolve);
			quad->Draw(rhi);
			rhi->UnsetPSO();
			
			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mLinearFogRT;

			rhi->EndEventTag();
		}

		// SSS
		if (mUseSSS)
		{
			rhi->BeginEventTag("EveryRay: Post Processing (SSS)");

			ER_Illumination* illumination = mCore.GetLevel()->mIllumination;
			if (illumination->IsSSSBlurring())
			{
				rhi->SetRenderTargets({ mSSSRT });
				rhi->SetRootSignature(mSSSRS);
				if (!rhi->IsPSOReady(mSSSPassPSOName))
				{
					rhi->InitializePSO(mSSSPassPSOName);
					rhi->SetShader(mSSSPS);
					rhi->SetRenderTargetFormats({ mSSSRT });
					rhi->SetBlendState(ER_NO_BLEND);
					rhi->SetRasterizerState(ER_NO_CULLING);
					rhi->SetRootSignatureToPSO(mSSSPassPSOName, mSSSRS);
					rhi->SetTopologyTypeToPSO(mSSSPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					quad->PrepareDraw(rhi);
					rhi->FinalizePSO(mSSSPassPSOName);
				}
				rhi->SetPSO(mSSSPassPSOName);

				//vertical
				{
					PrepareDrawingSSS(gameTime, mRenderTargetBeforeResolve, gbuffer, true);
					quad->Draw(rhi, false);
				}
				//horizontal
				{
					PrepareDrawingSSS(gameTime, mRenderTargetBeforeResolve, gbuffer, false);
					quad->Draw(rhi);
				}
				rhi->UnsetPSO();

				rhi->UnbindRenderTargets();
				//[WARNING] Set from last post processing effect
				mRenderTargetBeforeResolve = mSSSRT;
			}

			rhi->EndEventTag();
		}

		// SSR
		if (mUseSSR)
		{
			rhi->BeginEventTag("EveryRay: Post Processing (SSR)");

			rhi->SetRenderTargets({ mSSRRT });
			rhi->SetRootSignature(mSSRRS);
			if (!rhi->IsPSOReady(mSSRPassPSOName))
			{
				rhi->InitializePSO(mSSRPassPSOName);
				rhi->SetShader(mSSRPS);
				rhi->SetRenderTargetFormats({ mSSRRT });
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRootSignatureToPSO(mSSRPassPSOName, mSSRRS);
				rhi->SetTopologyTypeToPSO(mSSRPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mSSRPassPSOName);
			}
			rhi->SetPSO(mSSRPassPSOName);
			PrepareDrawingSSR(gameTime, mRenderTargetBeforeResolve, gbuffer);
			quad->Draw(rhi);
			rhi->UnsetPSO();

			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mSSRRT;

			rhi->EndEventTag();
		}


		// Composite with volumetric fog (if enabled)
		rhi->BeginEventTag("EveryRay: Post Processing (Volumetric Fog - Composite)");
		{
			if (aVolumetricFog && aVolumetricFog->IsEnabled())
			{
				rhi->SetRenderTargets({ mVolumetricFogRT });
				aVolumetricFog->Composite(mVolumetricFogRT, mRenderTargetBeforeResolve, gbuffer->GetPositions());
				rhi->UnbindRenderTargets();

				//[WARNING] Set from last post processing effect
				mRenderTargetBeforeResolve = mVolumetricFogRT;
			}
		}
		rhi->EndEventTag();

		rhi->BeginEventTag("EveryRay: Post Processing (Volumetric Clouds - Composite)");
		{
			// Composite with volumetric clouds (if enabled)
			if (aVolumetricClouds && aVolumetricClouds->IsEnabled())
				aVolumetricClouds->Composite(mRenderTargetBeforeResolve);
		}
		rhi->EndEventTag();

		// Tonemap
		if (mUseTonemap)
		{
			rhi->BeginEventTag("EveryRay: Post Processing (Tonemap)");

			rhi->SetRenderTargets({ mTonemappingRT });
			rhi->SetRootSignature(mTonemapRS);
			if (!rhi->IsPSOReady(mTonemapPassPSOName))
			{
				rhi->InitializePSO(mTonemapPassPSOName);
				rhi->SetShader(mTonemappingPS);
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRenderTargetFormats({ mTonemappingRT });
				rhi->SetRootSignatureToPSO(mTonemapPassPSOName, mTonemapRS);
				rhi->SetTopologyTypeToPSO(mTonemapPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mTonemapPassPSOName);
			}
			rhi->SetPSO(mTonemapPassPSOName);
			PrepareDrawingTonemapping(mRenderTargetBeforeResolve, gbuffer);
			quad->Draw(rhi);
			rhi->UnsetPSO();

			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mTonemappingRT;

			rhi->EndEventTag();
		}

		// Color grading
		if (mUseColorGrading)
		{
			rhi->BeginEventTag("EveryRay: Post Processing (Color Grading)");

			rhi->SetRenderTargets({ mColorGradingRT });
			rhi->SetRootSignature(mColorGradingRS);
			if (!rhi->IsPSOReady(mColorGradingPassPSOName))
			{
				rhi->InitializePSO(mColorGradingPassPSOName);
				rhi->SetShader(mColorGradingPS);
				rhi->SetRenderTargetFormats({ mColorGradingRT });
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRootSignatureToPSO(mColorGradingPassPSOName, mColorGradingRS);
				rhi->SetTopologyTypeToPSO(mColorGradingPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mColorGradingPassPSOName);
			}
			rhi->SetPSO(mColorGradingPassPSOName);
			PrepareDrawingColorGrading(mRenderTargetBeforeResolve);
			quad->Draw(rhi);
			rhi->UnsetPSO();

			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mColorGradingRT;

			rhi->EndEventTag();
		}

		// Vignette
		if (mUseVignette)
		{
			rhi->BeginEventTag("EveryRay: Post Processing (Vignette)");

			rhi->SetRenderTargets({ mVignetteRT });
			rhi->SetRootSignature(mVignetteRS);
			if (!rhi->IsPSOReady(mVignettePassPSOName))
			{
				rhi->InitializePSO(mVignettePassPSOName);
				rhi->SetShader(mVignettePS);
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRenderTargetFormats({ mVignetteRT });
				rhi->SetRootSignatureToPSO(mVignettePassPSOName, mVignetteRS);
				rhi->SetTopologyTypeToPSO(mVignettePassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mVignettePassPSOName);
			}
			rhi->SetPSO(mVignettePassPSOName);
			PrepareDrawingVignette(mRenderTargetBeforeResolve);
			quad->Draw(rhi);
			rhi->UnsetPSO();

			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mVignetteRT;

			rhi->EndEventTag();
		}

		// FXAA
		if (mUseFXAA)
		{
			rhi->BeginEventTag("EveryRay: Post Processing (FXAA)");

			rhi->SetRenderTargets({ mFXAART });
			rhi->SetRootSignature(mFXAARS);
			if (!rhi->IsPSOReady(mFXAAPassPSOName))
			{
				rhi->InitializePSO(mFXAAPassPSOName);
				rhi->SetShader(mFXAAPS);
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRenderTargetFormats({ mFXAART });
				rhi->SetRootSignatureToPSO(mFXAAPassPSOName, mFXAARS);
				rhi->SetTopologyTypeToPSO(mFXAAPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mFXAAPassPSOName);
			}
			rhi->SetPSO(mFXAAPassPSOName);
			PrepareDrawingFXAA(mRenderTargetBeforeResolve);
			quad->Draw(rhi);
			rhi->UnsetPSO();

			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect 
			mRenderTargetBeforeResolve = mFXAART;

			rhi->EndEventTag();
		}
	}

	void ER_PostProcessingStack::DrawPostEffectsVolumesDebugGizmos(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs)
	{
		if (!mShowDebugVolumes)
			return;

		for (auto& volume : mPostEffectsVolumes)
			volume.DrawDebugVolume(aRenderTarget, aDepth, rs);
	}

	PostEffectsVolume::PostEffectsVolume(ER_Core& pCore, const XMFLOAT4X4& aTransform, const PostEffectsVolumeValues& aValues, const std::string& aName)
		: worldTransform(aTransform), values(aValues), name(aName)
	{
		aabb = { XMFLOAT3(-1.0, -1.0, -1.0), XMFLOAT3(1.0, 1.0, 1.0) };
		UpdateDebugVolumeAABB();

		debugGizmoAABB = new ER_RenderableAABB(pCore, DebugPostEffectsVolumeColor);
		debugGizmoAABB->InitializeGeometry({ aabb.first, aabb.second });
	}

	PostEffectsVolume::~PostEffectsVolume()
	{
		DeleteObject(debugGizmoAABB);
	}

	void PostEffectsVolume::UpdateDebugVolumeAABB()
	{
		ER_AABB defaultAABB(XMFLOAT3(-1.0, -1.0, -1.0),XMFLOAT3(1.0, 1.0, 1.0));

		currentAABBVertices[0] = (XMFLOAT3(defaultAABB.first.x, defaultAABB.second.y, defaultAABB.first.z));
		currentAABBVertices[1] = (XMFLOAT3(defaultAABB.second.x, defaultAABB.second.y, defaultAABB.first.z));
		currentAABBVertices[2] = (XMFLOAT3(defaultAABB.second.x, defaultAABB.first.y, defaultAABB.first.z));
		currentAABBVertices[3] = (XMFLOAT3(defaultAABB.first.x, defaultAABB.first.y, defaultAABB.first.z));
		currentAABBVertices[4] = (XMFLOAT3(defaultAABB.first.x, defaultAABB.second.y, defaultAABB.second.z));
		currentAABBVertices[5] = (XMFLOAT3(defaultAABB.second.x, defaultAABB.second.y, defaultAABB.second.z));
		currentAABBVertices[6] = (XMFLOAT3(defaultAABB.second.x, defaultAABB.first.y, defaultAABB.second.z));
		currentAABBVertices[7] = (XMFLOAT3(defaultAABB.first.x, defaultAABB.first.y, defaultAABB.second.z));

		XMMATRIX tempM = XMLoadFloat4x4(&worldTransform);
		for (int i = 0; i < 8; i++)
		{
			XMVECTOR point = XMVector3Transform(XMLoadFloat3(&(currentAABBVertices[i])), tempM);
			XMStoreFloat3(&(currentAABBVertices[i]), point);
		}

		XMFLOAT3 minVertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
		XMFLOAT3 maxVertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (UINT i = 0; i < 8; i++)
		{
			//Get the smallest vertex 
			minVertex.x = std::min(minVertex.x, currentAABBVertices[i].x); // Find smallest x value in model
			minVertex.y = std::min(minVertex.y, currentAABBVertices[i].y); // Find smallest y value in model
			minVertex.z = std::min(minVertex.z, currentAABBVertices[i].z); // Find smallest z value in model

			//Get the largest vertex 
			maxVertex.x = std::max(maxVertex.x, currentAABBVertices[i].x); // Find largest x value in model
			maxVertex.y = std::max(maxVertex.y, currentAABBVertices[i].y); // Find largest y value in model
			maxVertex.z = std::max(maxVertex.z, currentAABBVertices[i].z); // Find largest z value in model
		}

		aabb = ER_AABB(minVertex, maxVertex);
		if (debugGizmoAABB)
			debugGizmoAABB->Update(aabb);
	}

	void PostEffectsVolume::DrawDebugVolume(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs)
	{
		if (debugGizmoAABB && isEnabled)
			debugGizmoAABB->Draw(aRenderTarget, aDepth, rs);
	}

	void PostEffectsVolume::SetTransform(const XMFLOAT4X4& aTransform, bool updateAABB /*= true*/)
	{
		worldTransform = aTransform;
		if (updateAABB)
			UpdateDebugVolumeAABB();
	}
}