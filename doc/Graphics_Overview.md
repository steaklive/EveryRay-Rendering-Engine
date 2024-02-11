# Intro

# Frame - GPU culling
Our main goal is to render triangles which are formed from the vertices of ```ER_Mesh```es that are part of every ```ER_RenderingObject```. Doing that can be a bottleneck which is why we want to minimize the amount of unnecessary drawcalls in the engine as early as possible. Normally, this is achieved by _culling_ various data in the engine both on CPU and on GPU. Culling is a big topic which is still not ideal in _EveryRay_, however, some significant time has been invested into it.

Firstly, all ```ER_RenderingObject```s are CPU-culled against the camera view: everything outside the view frustum is ignored for the next passes. This works well for singular objects and simple scenes and can even work for instanced objects to some extent: traversing through all instances on CPU, culling (even with multi-threading) and then updating the instanced buffers on GPU with a readback to CPU can get really expensive for big number of instances. And if you add differnt LODs/meshes/objects, this gets even slower.

What we can do instead and what _EveryRay_ does is processing such objects indirectly on GPU without any CPU overhead. For now it only works for static objects (so for dynamic prefer the method above) which have huge amount of instances (its not worth doing that for low count). In modern APIs it is possible to prepare instanced data on GPU (in other words, cull the instances in a compute shader) and then pass that info to the rendering passes with indirect draw commands of your API without any readbacks.  _Although its not yet implemented in _EveryRay_, but if your API supports indirect multi-draw command, you can even draw multiple different objects in one call!_

In _EveryRay_ ```ER_GPUCuller``` is responsible for the process mentioned above: the system simply runs a GPU compute pass (```IndirectCulling.hlsl```) for every object where it frustum-culls its instances, prepares their LODs and writes everything in one GPU buffer for future processing in the frame. This already makes the workflow more efficient and modern for some scenarios than simple old-school CPU frustum culling.

Potentially, ```ER_GPUCuller``` can be extended for dealing with other data, such as foliage (more in "Foliage" section), lights per screen tile (more in "Direct Lighting"), voxels for cone traced objects (more in "Indirect Lighting"), etc. You can also be creative and combine the GPU culling passes asynchronously with rendering passes (such as shadows), re-use the results of a previous frame and do many other things which will be described here once implemented.

Although at this point we have only prepared the objects that are visible on screen, their triangles can be not fully visible (i.e. some might be outside the view or be overlapped by the triangles in front). This can also lead to significant wasted performance and, although geometry was never a bottleneck in _EveryRay_, I still consider implementing a _HiZ culling_ as an update to the system in the future.



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
