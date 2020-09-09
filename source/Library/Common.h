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
#include <memory>

#include "RTTI.h"

#include <d3d11_1.h>
#include <D3DCompiler.h>
#include <d3dx11Effect.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <dinput.h>

#define DeleteObject(object) if((object) != NULL) { delete object; object = NULL; }
#define DeleteObjects(objects) if((objects) != NULL) { delete[] objects; objects = NULL; }
#define ReleaseObject(object) if((object) != NULL) { object->Release(); object = NULL; }
#define DeletePointerCollection(objects) {for (auto &it: objects) delete it; objects.clear();}
#define ReleasePointerCollection(objects) {for (auto &it: objects) it->Release(); objects.clear();}

#define MAX_NUM_OF_CASCADES 3

namespace Library
{
	typedef unsigned char byte;
}

using namespace DirectX;
using namespace DirectX::PackedVector;