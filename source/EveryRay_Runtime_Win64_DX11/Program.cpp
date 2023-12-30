#define ER_PLATFORM_WIN64_DX11 1

#include "..\EveryRay_Core\ER_RuntimeCore.h"
#include "..\EveryRay_Core\ER_CoreException.h"
#include "..\EveryRay_Core\ER_Utility.h"
#include "..\EveryRay_Core\RHI\ER_RHI.h"
#include "..\EveryRay_Core\RHI\DX11\ER_RHI_DX11.h"

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

using namespace EveryRay_Core;

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand)
{
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
		throw ER_CoreException("Failed to call CoInitializeEx");

	//#if defined(DEBUG) || defined(_DEBUG)
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
	//#endif

	const std::string platformName = "Win64 DX11";
	const std::string windowClassName = "EveryRay Main Window Class";
	std::string windowMainName = "EveryRay - Rendering Engine";
	windowMainName += " " + engineVersionString + " | ";
	windowMainName += platformName;

#if defined(DEBUG) || defined(_DEBUG)
	windowMainName += " (Debug)";
#else
	windowMainName += " (Release)";
#endif
	std::unique_ptr<ER_RuntimeCore> game(new ER_RuntimeCore(new ER_RHI_DX11(), instance, ER_Utility::ToWideString(windowClassName).c_str(), ER_Utility::ToWideString(windowMainName).c_str(), showCommand, false));
	try {
		game->Run();
	}
	catch (ER_CoreException ex)
	{
		MessageBox(game->WindowHandle(), ex.whatw().c_str(), game->WindowTitle().c_str(), MB_ABORTRETRYIGNORE);
	}

	return 0;

}
