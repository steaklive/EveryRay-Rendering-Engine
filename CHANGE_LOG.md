# EveryRay - Change Log

# 1.1.2
Fixed:
- AMD crash in `ER_Illumination::PrepareResourcesForForwardLighting` with uninitialized `LightProbesCBuffer` (PR: https://github.com/steaklive/EveryRay-Rendering-Engine/pull/100)
  
# 1.1.1
Fixed:
- on terrain light probes placement on DX12

# 1.1
Added:
- support for terrain in light probes
- support for GPU indirect objects in light probes (w/ GPU culling)
- documentation page "Graphics Overview"

Fixed:
- bug when loading textures that are missing on disk for a specific quality preset

# 1.0

Initial release
