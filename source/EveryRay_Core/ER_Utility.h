#pragma once

#include "Common.h"

#include "ER_VectorHelper.h"
#include "ER_MatrixHelper.h"
#include "ER_ColorHelper.h"
#include "ER_MaterialHelper.h"

static float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static float clearColorWhite[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static float clearColorRed[4]	= { 1.0f, 0.0f, 0.0f, 1.0f };

static const std::string engineVersionString = "v1.1.2";

namespace EveryRay_Core
{
	class ER_Utility
	{
	public:
		static std::string CurrentDirectory();
		static std::wstring ExecutableDirectory();
		static std::wstring GetFilePath(const std::wstring& inputPath);
		static std::string GetFilePath(const std::string& inputPath);
		static void GetFileName(const std::string& inputPath, std::string& filename);
		static void GetDirectory(const std::string& inputPath, std::string& directory);
		static void GetFileNameAndDirectory(const std::string& inputPath, std::string& directory, std::string& filename);
		static void LoadBinaryFile(const std::wstring& filename, std::vector<char>& data);
		static void ToWideString(const std::string& source, std::wstring& dest);
		static std::wstring ToWideString(const std::string& source);
		static void PathJoin(std::wstring& dest, const std::wstring& sourceDirectory, const std::wstring& sourceFile);
		static void GetPathExtension(const std::wstring& source, std::wstring& dest);
		static float RandomFloat(float a, float b);
		static UINT FastHash(const void* aData, int len);
		static void DisableAllEditors();

		static bool IsEditorMode; // global flag for editor mode
		static bool IsRenderingObjectEditor;
		static bool IsSunLightEditor;
		static bool IsWindEditor;
		static bool IsFoliageEditor;
		static bool IsPostEffectsVolumeEditor;

		static bool IsMainCameraCPUCulling;
		static bool IsMainCameraGPUCulling;
		static bool StopDrawingRenderingObjects;
		static bool IsWireframe;
		static float DistancesLOD[MAX_LOD];
		static float ShadowCascadeDistances[NUM_SHADOW_CASCADES];
	private:
		ER_Utility();
		ER_Utility(const ER_Utility& rhs);
		ER_Utility& operator=(const ER_Utility& rhs);
	};
}