#pragma once

#define NOMINMAX
#include <windows.h>
#include <wrl/client.h>
#include <exception>
#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <thread>
#include <mutex>
#include <chrono>

#include "RTTI.h"

#if defined (ER_API_DX11)
#define ER_PLATFORM_WIN64_DX11 1
#elif defined (ER_API_DX12)
#define ER_PLATFORM_WIN64_DX12 1
#endif

#if ER_PLATFORM_WIN64_DX11 || ER_PLATFORM_WIN64_DX12
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include "DirectXTex.h"
#endif

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
using namespace DirectX;

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "ImGuizmo.h"

#define DeleteObject(object) if((object) != NULL) { delete object; object = NULL; }
#define DeleteObjects(objects) if((objects) != NULL) { delete[] objects; objects = NULL; }
#define ReleaseObject(object) if((object) != NULL) { object->Release(); object = NULL; }
#define DeletePointerCollection(objects) {for (auto &it: objects) delete it; objects.clear();}
#define ReleasePointerCollection(objects) {for (auto &it: objects) it->Release(); objects.clear();}

#define ER_CEIL(n,d) (int)ceil((float)n/d)
#define ER_OUTPUT_LOG( s ) { OutputDebugString(s); }

#if defined (ER_COMPILER_VS)
#define ER_ALIGN8 __declspec(align(8))
#define ER_ALIGN16 __declspec(align(16))
#define ER_ALIGN32 __declspec(align(32))
#define ER_ALIGN64 __declspec(align(64))
#elif defined (ER_COMPILER_CLANG)
#define ER_ALIGN8 __attribute__((__aligned__(8)))
#define ER_ALIGN16 __attribute__((__aligned__(16)))
#define ER_ALIGN32 __attribute__((__aligned__(32)))
#define ER_ALIGN64 __attribute__((__aligned__(64)))
#endif

#define NUM_SHADOW_CASCADES 3
#define MAX_LOD 3

using ER_AABB = std::pair<XMFLOAT3, XMFLOAT3>;

template <typename T>
inline T ER_DivideByMultiple(T value, unsigned int alignment)
{
	return (T)((value + alignment - 1) / alignment);
}

inline unsigned int ER_BitmaskAlign(unsigned int value, unsigned int alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}
