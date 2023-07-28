
#include "stdafx.h"
#include "ER_Utility.h"
#include <algorithm>
#include <exception>
#include <Shlwapi.h>
#include <fstream>

namespace EveryRay_Core
{
	bool ER_Utility::IsEditorMode = false;
	bool ER_Utility::IsLightEditor = false;
	bool ER_Utility::IsFoliageEditor = false;
	bool ER_Utility::IsMainCameraCPUFrustumCulling = true;
	bool ER_Utility::StopDrawingRenderingObjects = false;
	float ER_Utility::DistancesLOD[MAX_LOD] = { 100.0f, 300.0f, 2000.0f };

	std::string ER_Utility::CurrentDirectory()
	{
		WCHAR buffer[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, buffer);
		std::wstring currentDirectoryW(buffer);

		return std::string(currentDirectoryW.begin(), currentDirectoryW.end());
	}

	std::wstring ER_Utility::ExecutableDirectory()
	{
		WCHAR buffer[MAX_PATH];
		GetModuleFileName(nullptr, buffer, MAX_PATH);
		PathRemoveFileSpec(buffer);

		return std::wstring(buffer);
	}

	std::wstring ER_Utility::GetFilePath(const std::wstring& input)
	{
		std::wstring exeDir = ExecutableDirectory();
		std::wstring key(L"\\");
		exeDir = exeDir.substr(0, exeDir.rfind(key));
		exeDir = exeDir.substr(0, exeDir.rfind(key));
		exeDir = exeDir.substr(0, exeDir.rfind(key));
		exeDir = exeDir.substr(0, exeDir.rfind(key));
		exeDir.append(key);
		exeDir.append(input);

		return exeDir;
	}
	std::string ER_Utility::GetFilePath(const std::string& input)
	{
		std::wstring exeDirL = ExecutableDirectory();
		std::string exeDir = std::string(exeDirL.begin(), exeDirL.end());

		std::string key("\\");
		exeDir = exeDir.substr(0, exeDir.rfind(key));
		exeDir = exeDir.substr(0, exeDir.rfind(key));
		exeDir = exeDir.substr(0, exeDir.rfind(key));
		exeDir = exeDir.substr(0, exeDir.rfind(key));
		exeDir.append(key);
		exeDir.append(input);

		return exeDir;
	}

	void ER_Utility::GetFileName(const std::string& inputPath, std::string& filename)
	{
		std::string fullPath(inputPath);
		std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

		std::string::size_type lastSlashIndex = fullPath.find_last_of('/');

		if (lastSlashIndex == std::string::npos)
		{
			filename = fullPath;
		}
		else
		{
			filename = fullPath.substr(lastSlashIndex + 1, fullPath.size() - lastSlashIndex - 1);
		}
	}

	void ER_Utility::GetDirectory(const std::string& inputPath, std::string& directory)
	{
		std::string fullPath(inputPath);
		std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

		std::string::size_type lastSlashIndex = fullPath.find_last_of('/');

		if (lastSlashIndex == std::string::npos)
		{
			directory = "";
		}
		else
		{
			directory = fullPath.substr(0, lastSlashIndex);
		}
	}

	void ER_Utility::GetFileNameAndDirectory(const std::string& inputPath, std::string& directory, std::string& filename)
	{
		std::string fullPath(inputPath);
		std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

		std::string::size_type lastSlashIndex = fullPath.find_last_of('/');

		if (lastSlashIndex == std::string::npos)
		{
			directory = "";
			filename = fullPath;
		}
		else
		{
			directory = fullPath.substr(0, lastSlashIndex);
			filename = fullPath.substr(lastSlashIndex + 1, fullPath.size() - lastSlashIndex - 1);
		}
	}

	void ER_Utility::LoadBinaryFile(const std::wstring& filename, std::vector<char>& data)
	{
		std::ifstream file(filename.c_str(), std::ios::binary);
		if (file.bad())
		{
			throw std::exception("Could not open file.");
		}

		file.seekg(0, std::ios::end);
		UINT size = (UINT)file.tellg();

		if (size > 0)
		{
			data.resize(size);
			file.seekg(0, std::ios::beg);
			file.read(&data.front(), size);
		}

		file.close();
	}

	void ER_Utility::ToWideString(const std::string& source, std::wstring& dest)
	{
		dest.assign(source.begin(), source.end());
	}

	std::wstring ER_Utility::ToWideString(const std::string& source)
	{
		std::wstring dest;
		dest.assign(source.begin(), source.end());

		return dest;
	}

	void ER_Utility::PathJoin(std::wstring& dest, const std::wstring& sourceDirectory, const std::wstring& sourceFile)
	{
		WCHAR buffer[MAX_PATH];

		PathCombine(buffer, sourceDirectory.c_str(), sourceFile.c_str());
		dest = buffer;
	}

	void ER_Utility::GetPathExtension(const std::wstring& source, std::wstring& dest)
	{
		dest = PathFindExtension(source.c_str());
	}

	float ER_Utility::RandomFloat(float a, float b) {
		float random = ((float)rand()) / (float)RAND_MAX;
		float diff = b - a;
		float r = random * diff;
		return a + r;
	}
}