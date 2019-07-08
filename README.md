# EveryRay-Rendering-Engine
Rendering engine/framework written in C++ with DirectX 11 support. 

"EveryRay - Rendering Engine" is partially based on the small engine from the book "Real-Time 3D Rendering with DirectX and HLSL: A Practical Guide to Graphics Programming" by Paul Varcholik.

I have been changing and extending it for educational purposes with all kinds of features (see the description below) for quite a while. Stay tuned for more updates :)

<br>![alt text](https://preview.ibb.co/jVodie/Every_Ray_PBR.png")
<br>![alt text](https://preview.ibb.co/h069pK/Every_Ray_CSM.png")
<br>![alt text](https://preview.ibb.co/fqP3uK/Every_Ray_Instancing.png")
<br>![alt text](https://preview.ibb.co/i8mLnq/Every-Ray-Frustum-Culling.png")
<br>![alt text](https://i.ibb.co/zVG7qvL/Every-Ray-SSSS.png")
<br>![alt text](https://i.ibb.co/0ZJmFfh/Every-Ray-Post-Processing.png")

# Features
- 3D model loading (.obj, .fbx and etc.) with Assimp Library
- User Interface with ImGUI
- Different light sources
- FPS camera
- Normal-Mapping, Environment-Mapping
- Post Processing: Motion Blur, Vignette, LUT color grading
- Physically Based Rendering with IBL
- Cascaded Shadow Mapping
- GPU Instancing
- Frustum Culling
- Separable Subsurface Scattering (based on the work of J.Jimenez and D.Gutierrez)
- Volumetric Lighting (based on NVIDIA's "Fast, Flexible, Physically-Based Volumetric Light Scattering") - sorry, can't show all the code here...

# External Dependencies
- DirectX Effects 11 (https://github.com/Microsoft/FX11)
- DirectXTK (https://github.com/Microsoft/DirectXTK)
- ImGui (https://github.com/ocornut/imgui)
- Assimp (https://github.com/assimp/assimp)

# Requirements
- Visual Studio 2017
- Windows 10 + SDK
- DirectX 11 supported hardware
