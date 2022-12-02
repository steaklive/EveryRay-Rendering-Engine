#define ER_PLATFORM_WIN64_DX11 1

#include "stdafx.h"
#include <memory>

#include "..\EveryRay_Core\ER_RuntimeCore.h"
#include "..\EveryRay_Core\ER_CoreException.h"
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
	//#if defined(DEBUG) || defined(_DEBUG)
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
	//#endif

#if defined(DEBUG) || defined(_DEBUG)
	std::unique_ptr<ER_RuntimeCore> game(new ER_RuntimeCore(new ER_RHI_DX11(), instance, L"EveryRay Main Window Class", L"EveryRay - Rendering Engine | Win64 DX11 (Debug)", showCommand, false));
#else
	std::unique_ptr<ER_RuntimeCore> game(new ER_RuntimeCore(new ER_RHI_DX11(), instance, L"EveryRay Main Window Class", L"EveryRay - Rendering Engine | Win64 DX11 (Release)", showCommand, false));
#endif
	try {
		game->Run();
	}
	catch (ER_CoreException ex)
	{
		MessageBox(game->WindowHandle(), ex.whatw().c_str(), game->WindowTitle().c_str(), MB_ABORTRETRYIGNORE);
	}

	return 0;

}
