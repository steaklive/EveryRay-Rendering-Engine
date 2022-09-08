#pragma once

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#define NOMINMAX
#include <windows.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
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
using namespace Microsoft::WRL;

#if defined (ER_API_DX11)
#define ER_PLATFORM_WIN64_DX11 1
#elif defined (ER_API_DX12)
#define ER_PLATFORM_WIN64_DX12 1
#endif

#include "RTTI.h"

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <dinput.h>

#if ER_PLATFORM_WIN64_DX11 || ER_PLATFORM_WIN64_DX12
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include "DirectXTex.h"
#endif

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "ImGuizmo.h"

#define DeleteObject(object) if((object) != NULL) { delete object; object = NULL; }
#define DeleteObjects(objects) if((objects) != NULL) { delete[] objects; objects = NULL; }
#define ReleaseObject(object) if((object) != NULL) { object->Release(); object = NULL; }
#define DeletePointerCollection(objects) {for (auto &it: objects) delete it; objects.clear();}
#define ReleasePointerCollection(objects) {for (auto &it: objects) it->Release(); objects.clear();}

#define NUM_SHADOW_CASCADES 3
#define MAX_LOD 3

#define ER_CEIL(n,d) (int)ceil((float)n/d)
#define ER_ALIGN(value, alignment) (((value + alignment - 1) / alignment) * alignment)
#define ER_OUTPUT_LOG( s ) { OutputDebugString(s); }

template <typename T>
inline T DivideByMultiple(T value, size_t alignment)
{
	return (T)((value + alignment - 1) / alignment);
}

namespace EveryRay_Core
{
	typedef unsigned char byte;
}

using namespace DirectX;
using namespace DirectX::PackedVector;

using ER_AABB = std::pair<XMFLOAT3, XMFLOAT3>;