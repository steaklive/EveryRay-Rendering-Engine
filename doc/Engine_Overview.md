# EveryRay - Engine Overview - Intro

"EveryRay" is a simple engine by design. As mentioned before, it lacks your usual complexity of a game engine. Nevertheless, it can be still difficult to get a high-level overview by looking at code without any explanation. 

The sections of this page were created to help a new user get a grasp of the engine's base architecture:
- EveryRay's Core
- EveryRay's Scene
- EveryRay's Editor
- EveryRay's RHI

Each section will be described below.

# EveryRay - Engine Overview - Core

Let's start with the setup. For legacy reasons the Visual Studio solution contains 2 projects: _EveryRay Runtime_ and _EveryRay Core_. The first one is very simple and does nothing except generating an executable and calling ```WinMain()``` which creates an instance of _EveryRay Runtime_ (it is possible to adapt that for non-Windows platforms, too). The second one contains all the code of the engine and generates a static library (.lib) file. Everything mentioned in this document applies to _EveryRay Runtime_.

"EveryRay" uses a classic ```Update()``` and ```Draw()``` approach which is executed in a while loop until the user quits or the crash happens. For the time being it is just simple and single-threaded... This is by no means efficient or the "right" way to do but enough for our current scenarios: no complex interactions between objects, just culling and position updates which are followed by rendering. There are plans for multi-buffering and a job system though which are more in line with modern CPU architectures.

The core loop, clock/time, window and RHI creation are processed in ```ER_Core``` class. It has a child class ```ER_RuntimeCore``` which deals with higher level systems, such as input, camera, editor, current sandbox (explained later) and others. Here is a very simple UML diagram of the engine's core:

![picture](images/er_uml.png)

You can think of ```ER_Sandbox``` as a collection of all "visible" data of the engine: graphics systems + ```ER_Scene``` (explained in the next section). Everything else which is more abstract is hidden inside ```ER_Core```/```ER_RuntimeCore```. You can also see that there is ```ER_CoreServicesContainer``` which is a collection of systems/services that can be requested in any place of the code using RTTI. For example:

```ER_Camera* camera = (ER_Camera*)(ER_Material::GetCore()->GetServices().FindService(ER_Camera::TypeIdClass()));```

Such system must be derived from a ```ER_CoreComponent```, declared with ```RTTI_DECLARATIONS()``` macro and defined with ```RTTI_DEFINITIONS()``` macro. In theory, it is also possible to create other containers for non-core systems, such as graphics and use RTTI with them. 

# EveryRay - Engine Overview - Scene

As mentioned in a previous section, ```ER_Scene``` plays an essential part in "EveryRay". It stores a _root_ of the ```.json``` scene (aka "level" of "EveryRay") which can be read and written to from ```content/levels/```. The ```.json``` file contains a list of ```ER_RenderingObject```s with their properties (transforms, materials, etc.) and additional data for the scene, such as camera properties, light probes setup, sun, sky and many others. That info is then parsed in ```ER_``` graphics systems upon their creation. Unfortunately, for now you have to add most of the things by hand and not via code/UI which only supports saving data to a certain extent. Here is an example:

```
{
	"camera_position" : 
	[
		0.0,
		6.7999999999999998,
		37.0
	],
	"camera_plane_far" : 100000.0,
	"camera_plane_near" : 0.5,
	"rendering_objects" : 
	[
		{
			"instanced" : false,
			"model_path" : "content\\models\\sphere.fbx",
			"name" : "Dynamic sphere",
			"new_materials" : 
			[
				{
					"name" : "ShadowMapMaterial"
				},
				{
					"name" : "GBufferMaterial"
				},
				{
					"name" : "VoxelizationMaterial"
				}
			],
			"textures" : 
			[
				{
					"albedo" : "content\\textures\\colors\\white.png"
				}
			],
			"transform" : 
			[
				0.46295559406280518,
				0.0,
				0.0,
				0.0,
				0.0,
				0.46295559406280518,
				0.0,
				-0.50061535835266113,
				0.0,
				0.0,
				0.46295559406280518,
				7.026115894317627,
				0.0,
				0.0,
				0.0,
				1.0
			]
		}
	],
	"sky_color_top" : 
	[
		0.0,
		0.447,
		0.643,
		1.0
	],
	"sun_color" : 
	[
		1.0,
		0.94999999999999996,
		0.94999999999999996
	],
	"sun_direction" : 
	[
		-54.0,
		-24.0,
		-24.0
	]
}
```
You can also load other scenes via ImGui's drop down menu or reload the current scene after you have modified the ```.json``` file.

# EveryRay - Engine Overview - Editor

A lot of graphics systems can be toggled right when you start the engine, however, "EveryRay" also has a simple in-built scene _editor mode_ done via ImGui and ImGuizmo. 

<p align="center">
 <img src="../screenshots/EveryRay_testScene_simple.png" width="500"/>
</p>

 At the moment of writing this documentation, it doesn't do much except for showing or changing some properties of the following systems:
- Rendering Objects (**transforms** _(also per instance)_, custom material settings, activation/deactivation, showing AABB gizmos and stats)
- Foliage zones (**transforms**, showing AABB gizmo and stats)
- Post effects volumes (**transforms**, custom effects settings, activation/deactivation, showing AABB gizmos)
- Directional & non-directional lights editor (**transforms**, custom properties, etc.)
- Sky (color data)
- Camera (speed, FOV, near/far planes, frustum culling toggles)

Highlighted values from above can even be saved back to ```.json``` of the scene after modifications with ImGuizmo or ImGui _(by pressing the "Save" buttons in the editor)_. More data available for saving is planned in the future releases of the engine. Ideally, it would be nice to have every single non-debug toggle "saveable" via UI.


# EveryRay - Engine Overview - RHI

RHI stands for "Rendering Hardware Interface" which is a common concept in game engines. Most of the engines use graphics APIs in one way or another for 2D or 3D rendering. Often that code is lower-level than your system's code as you are directly communicating with a specific API/hardware. For example, when writing code for your graphics system you might want to clear render targets, set resources and other things via API, such as DirectX, Vulkan, OpenGL, or something else. That code will serve the same purpose on all APIs/platforms, however, the implementation will differ drastically. It is a good idea to wrap the functionality into an abstract RHI with abstract methods (done in ```ER_RHI.h```) and then add API-specific implementation in the separate classes which will not affect higher-level systems of the engine (i.e. calls to ```SetDepthStencilState()``` and methods alike will be handled automatically inside RHI). I have also created abstract classes for GPU resources, such as ```ER_RHI_GPUTexture```, ```ER_RHI_GPUBuffer```, ```ER_RHI_GPUShader``` and others.

It is not always easy to combine all APIs in one abstraction but I have attempted to at least do it in ```ER_RHI_DX11.cpp/h``` and ```ER_RHI_DX12.cpp/h```. The whole RHI is shifted towards DirectX but it is possible to add support for other APIs which can be both a great and painful experience.

_Note on DX12_ : Years ago I originally started developing this project on DX11 with the mindset of DX11/OpenGL-era APIs (single-threaded renderer, immediate context, etc.). However, only much later did I add support for DX12 into "EveryRay" and, unfortunately, I only had time for the "1 to 1" port from DX11. DX12 is currently underused in the engine and is not bringing any improvements yet (in performance, for example). This will likely change in the future, as I start refactoring/adding support to many new DX12-era concepts (i.e. async compute, multithreaded command list submission, bindless, etc.). That will take time and serious changes in the architecture of "EveryRay", but it would be worth it alongside with other novel features, like DirectX Raytracing or mesh shaders.
