# EveryRay-Rendering-Engine
Rendering engine/framework in C++ with DirectX 11 support. 

"EveryRay - Rendering Engine" is partially based on the small engine from the book "Real-Time 3D Rendering with DirectX and HLSL: A Practical Guide to Graphics Programming" by Paul Varcholik.

I have changed and extended it for educational purposes with all kinds of features (see the description below). Stay tuned for more updates :)

<br>![alt text](https://preview.ibb.co/jVodie/Every_Ray_PBR.png")
<br>![alt text](https://preview.ibb.co/h069pK/Every_Ray_CSM.png")
<br>![alt text](https://preview.ibb.co/fqP3uK/Every_Ray_Instancing.png")

# Features
- 3D model loading (.obj, .fbx and etc.) with Assimp Library
- User Interface with ImGUI
- Different light sources support
- FPS camera
- Normal-Mapping, Environment-Mapping
- Physically Based Rendering with IBL
- Cascaded Shadow Mapping

# How to build
This repo only contains the source code of the engine: "Game" & "Library" folders. You should build the contents of "Library" as a .lib file and use "Game" as the main project.

Do not forget to link all the dependencies properly!

# External Dependencies
- DirectX Effects 11 (https://github.com/Microsoft/FX11)
- DirectXTK (https://github.com/Microsoft/DirectXTK)
- ImGui (https://github.com/ocornut/imgui)
- Assimp (https://github.com/assimp/assimp)

# Requirements
- Visual Studio 2017
- Windows 10 + SDK
- DirectX 11 supported hardware
