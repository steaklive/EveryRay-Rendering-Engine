#pragma once
#include "stdafx.h"
#include <exception>
#include <Windows.h>
#include <string>

namespace Library
{
	class ER_CoreException : public std::exception
	{
	public:
		ER_CoreException(const char* const& message, HRESULT hr = S_OK);

		HRESULT HR() const;
		std::wstring whatw() const;

	private:
		HRESULT mHR;
	};
}