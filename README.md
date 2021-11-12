# EveryRay-Rendering-Engine
Rendering engine/framework written in C++/DirectX 11. 
https://youtu.be/_htQNxesr9U

# Some of the rendering features
- Hybrid Rendering (Deferred Prepass with GBuffer + Forward Pass)
- Physically Based Rendering with IBL
- Cascaded Shadow Mapping
- Post Processing: SSR, Motion Blur, Vignette, LUT color grading, FXAA, Tonemap, Bloom, Fog, Light Shafts
- Separable Subsurface Scattering
- Terrain with GPU tessellation
- Foliage
- Volumetric clouds
- Cascaded Voxel Cone Tracing GI (WIP)

Other features based on NVIDIA techniques (no code provided here)
- Volumetric Lighting (from "Fast, Flexible, Physically-Based Volumetric Light Scattering")
- Ocean Simulation (from DX11 archive samples)

# Some of the engine features
- 3D model loading (.obj, .fbx and etc.) with Assimp Library
- Loading/saving to JSON scene files
- Simple objects editor
- AABB, OBB, collision detection
- ImGUI, ImGuizmo
 
# Screenshots

![picture](screenshots/EveryRayTerrain.png)
![picture](screenshots/EveryRayEditor.png)
![picture](screenshots/EveryRayPBR.png)
![picture](screenshots/EveryRayInstancing.png)
![picture](screenshots/EveryRaySSSS.png)
![picture](screenshots/EveryRayWater.png)
![picture](screenshots/EveryRayCollisionDetection.png)
![picture](screenshots/EveryRaySSR.png)
![picture](screenshots/EveryRayTestScene.png)

# Controls
- Mouse + Right Click - camera rotation
- WASD - camera side movement
- E/Q - camera up/down movement
- Backspace - enable editor
- R/T/Y - scale/translate/rotate object in the editor mode

# Notes
Only "Sponza Main Demo", "Test Scene", "Terrain Demo Scene" have almost all the features and are up-to-date with the changes to the codebase. Other levels suffer from legacy issues and might not have, for example, up to date code, such as RenderingObjects, working editor or post processing stack... I will be refactoring those when possible. 

The framework is NOT API-agnostic and, thus, has been tighly bound to DX11 since the beginning of its development. Hopefully, I will be changing that in the future and adding DX12 support for it. It will undeniably improve the perfomance in many scenarios. 

You might increase TDR time of your GPU driver (explained here https://docs.substance3d.com/spdoc/gpu-drivers-crash-with-long-computations-128745489.html).

# External Dependencies
- DirectX Effects 11 (https://github.com/Microsoft/FX11)
- DirectXTK (https://github.com/Microsoft/DirectXTK)
- ImGui (https://github.com/ocornut/imgui)
- Assimp 5.0.1 (https://github.com/assimp/assimp)
- JsonCpp (https://github.com/open-source-parsers/jsoncpp/)

# References
- "Real-Time 3D Rendering with DirectX and HLSL: A Practical Guide to Graphics Programming" by Paul Varcholik.
- "Real-Time Rendering" 3rd/4th ed. by Tomas Möller, Naty Hoffman, Eric Haines
- "GPU Gems" series from NVIDIA
- "GPU Pro" series by Wolfgang Engel
- numerous SIGGRAPH, GDC papers and blogposts by fellow graphics geeks and vendors :)
 
# Requirements
- Visual Studio 2017
- Windows 10 + SDK
- DirectX 11 supported hardware
