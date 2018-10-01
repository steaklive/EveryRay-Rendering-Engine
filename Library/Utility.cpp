
#include "stdafx.h"
#include "Utility.h"
#include <algorithm>
#include <exception>
#include <Shlwapi.h>
#include <fstream>

namespace Library
{
	std::string Utility::CurrentDirectory()
	{
		WCHAR buffer[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, buffer);
		std::wstring currentDirectoryW(buffer);

		return std::string(currentDirectoryW.begin(), currentDirectoryW.end());
	}

	std::wstring Utility::ExecutableDirectory()
	{
		WCHAR buffer[MAX_PATH];
		GetModuleFileName(nullptr, buffer, MAX_PATH);
		PathRemoveFileSpec(buffer);

		return std::wstring(buffer);
	}

	void Utility::GetFileName(const std::string& inputPath, std::string& filename)
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

	void Utility::GetDirectory(const std::string& inputPath, std::string& directory)
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

	void Utility::GetFileNameAndDirectory(const std::string& inputPath, std::string& directory, std::string& filename)
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

	void Utility::LoadBinaryFile(const std::wstring& filename, std::vector<char>& data)
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

	void Utility::ToWideString(const std::string& source, std::wstring& dest)
	{
		dest.assign(source.begin(), source.end());
	}

	std::wstring Utility::ToWideString(const std::string& source)
	{
		std::wstring dest;
		dest.assign(source.begin(), source.end());

		return dest;
	}

	void Utility::PathJoin(std::wstring& dest, const std::wstring& sourceDirectory, const std::wstring& sourceFile)
	{
		WCHAR buffer[MAX_PATH];

		PathCombine(buffer, sourceDirectory.c_str(), sourceFile.c_str());
		dest = buffer;
	}

	void Utility::GetPathExtension(const std::wstring& source, std::wstring& dest)
	{
		dest = PathFindExtension(source.c_str());
	}
}