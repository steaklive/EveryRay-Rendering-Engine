#include "stdafx.h"
#include "ER_CoreException.h"

namespace Library
{
	ER_CoreException::ER_CoreException(const char* const& message, HRESULT hr)
		: exception(message), mHR(hr)
	{
	}

	HRESULT ER_CoreException::HR() const
	{
		return mHR;
	}

	std::wstring ER_CoreException::whatw() const
	{
		std::string whatString(what());
		std::wstring whatw;
		whatw.assign(whatString.begin(), whatString.end());

		return whatw;
	}
}