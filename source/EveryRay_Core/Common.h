#pragma once

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

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

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <dinput.h>

//#include "TGATextureLoader.h"
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

#include "DirectXTex.h"

#include "imgui.h"
#include "imgui_impl_win32.h"

#include "ImGuizmo.h"

#define ER_PLATFORM_WIN64_DX11 1

#define DeleteObject(object) if((object) != NULL) { delete object; object = NULL; }
#define DeleteObjects(objects) if((objects) != NULL) { delete[] objects; objects = NULL; }
#define ReleaseObject(object) if((object) != NULL) { object->Release(); object = NULL; }
#define DeletePointerCollection(objects) {for (auto &it: objects) delete it; objects.clear();}
#define ReleasePointerCollection(objects) {for (auto &it: objects) it->Release(); objects.clear();}

#define NUM_SHADOW_CASCADES 3 //TODO remove from here
#define MAX_LOD 3

#define INT_CEIL(n,d) (int)ceil((float)n/d)

#define ER_DEBUG_OUTPUT_LOG( s )            \
{                             \
   std::ostringstream os_;    \
   os_ << s;                   \
   OutputDebugString( os_.str().c_str() );  \
}

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