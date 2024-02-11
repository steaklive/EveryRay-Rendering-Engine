# Intro

# Frame - GPU culling
Our main goal is to render triangles which are formed from the vertices of ```ER_Mesh```es that are part of every ```ER_RenderingObject```. Doing that can be a bottleneck which is why we want to minimize the amount of unnecessary drawcalls in the engine as early as possible. Normally, this is achieved by _culling_ various data in the engine both on CPU and on GPU. Culling is a big topic which is still not ideal in _EveryRay_, however, some significant time has been invested into it.

Firstly, all ```ER_RenderingObject```s are CPU-culled against the camera view: everything outside the view frustum is ignored for the next passes. This works well for singular objects and simple scenes and can even work for instanced objects to some extent: traversing through all instances on CPU, culling (even with multi-threading) and then updating the instanced buffers on GPU with a readback to CPU can get really expensive for big number of instances. And if you add differnt LODs/meshes/objects, this gets even slower.

What we can do instead and what _EveryRay_ does is processing such objects indirectly on GPU without any CPU overhead. For now it only works for static objects (so for dynamic prefer the method above) which have huge amount of instances (its not worth doing that for low count). In modern APIs it is possible to prepare instanced data on GPU (in other words, cull the instances in a compute shader) and then pass that info to the rendering passes with indirect draw commands of your API without any readbacks.  _Although its not yet implemented in _EveryRay_, but if your API supports indirect multi-draw command, you can even draw multiple different objects in one call!_

In _EveryRay_ ```ER_GPUCuller``` is responsible for the process mentioned above: the system simply runs a GPU compute pass (```IndirectCulling.hlsl```) for every object where it frustum-culls its instances, prepares their LODs and writes everything in one GPU buffer for future processing in the frame. This already makes the workflow more efficient and modern for some scenarios than simple old-school CPU frustum culling.

Potentially, ```ER_GPUCuller``` can be extended for dealing with other data, such as foliage (more in "Foliage" section), lights per screen tile (more in "Direct Lighting"), voxels for cone traced objects (more in "Indirect Lighting"), etc. You can also be creative and combine the GPU culling passes asynchronously with rendering passes (such as shadows), re-use the results of a previous frame and do many other things which will be described here once implemented.

Although at this point we have only prepared the objects that are visible on screen, their triangles can be not fully visible (i.e. some might be outside the view or be overlapped by the triangles in front). This can also lead to significant wasted performance and, although geometry has never been a bottleneck in _EveryRay_ so far, I still consider implementing a _HiZ culling_ as an update to the system in the future.



# Frame - GBuffer

Since _EveryRay_ can use _deferred rendering_ (see "Direct Lighting"), we might need a set of GBuffers. ```ER_Gbuffer``` class finds all objects in the scene which have ```ER_GBufferMaterial``` assigned and renders them in several textures (```Gbuffer.hlsl```):
- Albedo
- Normals
- Extra target #1 (roughness, metalness, height, etc.)
- Extra target #2 ("RenderingObjectFlags" bitmasks)

So far I have not optimized any of the targets above for bandwidth. In theory, it is trivial to pack our data smarter (i.e. not waste unnecessary bits, reduce precision, pack normals into 2 instead of 3 channels, etc.) and perhaps those optimizations will come one day. _Note: a separate depth-prepass option is considered for future updates._


# Frame - Shadows

_Shadows rendering_ was one of the first systems implemented in _EveryRay_ which is why it has been carrying some legacy code and is far from perfect _(I have refactored parts of it though)_. 

In abstract, similarly to ```ER_GBuffer```, shadows are being handled in ```ER_ShadowMapper``` which finds objects with ```ER_ShadowMaterial``` and renders those into several depth targets, aka "shadow maps" (```ShadpowMap.hlsl```).

For direct light source a classic _cascaded shadow mapping_ approach is used with ```NUM_SHADOW_CASCADES``` (by default equal to 3). For other sources there is nothing implemented yet, however, there is plenty of room for ideas: atlas-based approaches, dual-paraboloid mapping, static shadow mapping, etc.

```ER_ShadowMapper``` already provides some useful advancements: 2 ways of calculating projected matrix (bounding sphere or volume), texel-size increment updates (for fixing jittering), LOD-skipping, graphics quality presets (more in "Extra - Graphics config") etc.

# Frame - Illumination

Illumination is a complex subject that requires a certain level of creativity and compromise in real-time graphics. This section focuses on how we currently shade our pixels in _EveryRay_.

To make it short, let's divide our illumination into two parts: _direct_ and _indirect_. ```ER_Illumination``` aims to cover both with the help of some other systems, such as ```ER_Material```s. 

If we look at _direct_ lighting, there are a few keypoints to mention:
1) The engine uses both _deferred_ (```DeferredLighting.hlsl```) and _forward_ (```ForwardLighting.hlsl```) rendering paths which mostly share the same code from ```Lighting.hlsli```. Deferred rendering is done first and is used if the object has ```ER_GBufferMaterial``` assigned to it. Forward rendering is done after deferred and is used if the object has one of the _standard_ materials assigned to it. Those _standard_ materials can be rendered on top of each other (in the order they are assigned) and are used for special features, such as _fur, snow, effects, transparency,_ etc. which require special shading models and shaders.
2) Our default shading model is _physically based_ (Cook-Torrance, GGX, Schlick) and is coded in ```Lighting.hlsli```; if you want to modify something, it will be applied to both rendering paths.
3) In forward rendering it is also possible to use _parallax occlusion mapping_ (hopefully, support for deferred mode will be added in the future).
4) Currently, directional and non-directional lights' contributions are included in lighting, however, no optimizations have been made yet for the non-directional shading. It is a big topic but worth investigating once we want to support hundreds and thousands of light sources in a frame (_tiled deferred_ and _forward+_ techniques can be implemented).

If we look at _indirect_ lighting, it also divides into 2 parts: _static_ and _dynamic_.

_Static_ indirect lighting uses ```ER_LightProbe```s (for diffuse and specular) that capture radiance in defined points in space and are managed inside ```ER_LightProbeManager```. There are some interesting details to mention that are specific to _EveryRay_:
1) Every scene can have local light probes assigned either in a uniform 3D grid or 2D-scattered on top of the terrain's surface (if it exists in the scene). A user must specify the bounds (```light_probes_volume_bounds_min```, ```light_probes_volume_bounds_max```) and can specify a few other extra parameters, such as distances between probes of each type, etc.
2) If the scene has no probes assigned, a set of 2 global probes (diffuse and specular) is always generated for the level and is used as a fallback for _static_ lighting. Usually, it makes sense to include big objects (such as terrain, buildings, etc.) that contribute to the radiance in the global probes, which is possible to do with ```use_in_global_lightprobe_rendering``` field in the scene's file.
3) Probes (both local and global) are generated in the first frame if the user does not have probe data on disk in the root of the scene (```/diffuse_probes/``` and ```/specular_probes/```). Any object with ```ER_LightProbeMaterial``` assigned will be rendered to local probes (in _forward_-style) both in _diffuse_ (rendered to low-res cubemap, convoluted and encoded with _spherical harmonics_ and stored as text data) and in _specular_ (stored as 128x128 mipped cubemap ```.dds``` textures). Each file contains a probe's position in its name which makes it easier to debug and replace if needed: _if you delete the file, it will be regenerated upon the next launch of the scene_.
4) ```Lighting.hlsi``` also deals with retrieving data from the probes and applying it to the shading of the current pixel during ```DeferredLighting.hlsl``` or ```ForwardLighting.hlsl```. For _diffuse_ probes, we send a set of GPU buffers for probes _sperical harmonics_ coefficients, positions and _cells_ with probe indices (in the case of a 3D grid, a cell is 8 probes with each probe covering the space of a vertex in a uniform 3D volume). Then, during shading, a world-space position that we want to shade finds an appropriate _cell_ with a fast uniform grid approach and finally interpolates the radiance data from the neighbouring probes (in the case of a 3D grid, _trilinearly_). For _specular_ probes, we send a similar set of GPU buffers, but instead of interpolating, we just find the closest probe to our world position. If we have a lot of specular probes in the scene, it gets expensive to carry all of them in GPU memory, which is why we only keep several ones that are next to the camera and constantly unload/cull the ones that are far from the camera's current position (in the future it might be worth changing specular probes loading to _bindless_ resources in order to mitigate hardware limits).
5) The probe system does not support re-loading and updates during the frame, however, it might be extended for such purposes and modern ray-tracing pipelines.

	# Direct Lighting
		# Standard and Special Materials (+ POM)
	# Indirect Lighting
		# Static - Light Probe System
		# Dynamic - Cascaded Voxel Cone Tracing
# Frame - Terrain
# Frame - Foliage
# Frame - Volumetric Fog
# Frame - Volumetric Clouds
# Frame - Post Effects
# Extra - Graphics config
# Extra - Texture and model cache
