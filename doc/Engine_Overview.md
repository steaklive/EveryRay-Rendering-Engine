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

TODO

# EveryRay - Engine Overview - RHI

TODO
