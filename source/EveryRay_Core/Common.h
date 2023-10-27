#pragma once

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
#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
using namespace Microsoft::WRL;

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
using namespace DirectX;

#if ER_PLATFORM_WIN64_DX11 || ER_PLATFORM_WIN64_DX12
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#define USE_XINPUT // comment if you do not want to use XInput
#ifdef USE_XINPUT
#include <XInput.h>
#endif
#endif

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
#define ER_ALIGN128 __declspec(align(128))
#define ER_ALIGN256 __declspec(align(256))
#elif defined (ER_COMPILER_CLANG)
#define ER_ALIGN8 __attribute__((__aligned__(8)))
#define ER_ALIGN16 __attribute__((__aligned__(16)))
#define ER_ALIGN32 __attribute__((__aligned__(32)))
#define ER_ALIGN64 __attribute__((__aligned__(64)))
#define ER_ALIGN128 __attribute__((__aligned__(128)))
#define ER_ALIGN256 __attribute__((__aligned__(256)))
#endif

#if ER_PLATFORM_WIN64_DX11
#define ER_ALIGN_GPU_BUFFER ER_ALIGN16
#define ER_GPU_BUFFER_ALIGNMENT 16
#elif ER_PLATFORM_WIN64_DX12
#define ER_ALIGN_GPU_BUFFER ER_ALIGN256
#define ER_GPU_BUFFER_ALIGNMENT 256
#endif

#define NUM_SHADOW_CASCADES 3
#define MAX_LOD 3
#define MAX_MESH_COUNT 32 // should match with IndirectCulling.hlsli
#define MAX_NUM_POINT_LIGHTS 64 // keep in sync with Lighting.hlsli; deprecate or bump once tiled rendering is implemented

template <typename T>
inline T ER_DivideByMultiple(T value, unsigned int alignment) {	return (T)((value + alignment - 1) / alignment); }

inline unsigned int ER_BitmaskAlign(unsigned int value, unsigned int alignment) { return (value + alignment - 1) & ~(alignment - 1); }
inline bool ER_IsPowerOfTwo(int v) { return v != 0 && (v & (v - 1)) == 0; }
inline float ER_Lerp(const float& a, const float& b, const float& t) { return a + t * (b - a); }

using ER_AABB = std::pair<XMFLOAT3, XMFLOAT3>;
