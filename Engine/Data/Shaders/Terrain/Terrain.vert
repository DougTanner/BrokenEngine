#version 460

#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"
#include "Terrain/TerrainVertex.h"

// Uniforms
layout (set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform
{
	GlobalLayout globalLayout;
};

layout (set = 0, binding = kiGlobalBindingMainUniform) uniform mainUniform
{
	MainLayout mainLayout;
};

// Shared with Terrain.frag (set=1 binding=5) — same composite elevation G-buffer the frag samples.
// Used here to optionally override the baked mesh Z below a runtime threshold so vertices conform
// to the per-island heightmap (which Shadow.comp and Water.frag also key off of). Vert+frag sharing
// a sampler binding mirrors Water.vert/Water.frag, which both declare elevationTextureSampler at
// set=1 binding=5; PipelineCreator merges vert/frag stage flags by binding number.
layout (set = 1, binding = 4) uniform sampler2D elevationTextureSampler;

// Per-island AxisAlignedQuadLayout instance buffer; same SSBO that QuadsAxisAlignedVisibleArea.vert
// reads at set=1 binding=1. Here at binding=19 because the pipeline-creator merge in PipelineCreator.cpp
// keys bindings by NUMBER across vert+frag (asserting same descriptor type when both shaders declare it),
// and Terrain.frag occupies set=0/set=1 bindings 0..18. Picking the next free number (19) keeps the
// SSBO disjoint from frag's samplers. gl_InstanceIndex is supplied per-island via firstInstance at
// draw time (Vulkan 1.2 core).
layout (scalar, set = 1, binding = 17) buffer readonly quadsBuffer
{
	AxisAlignedQuadLayout pQuads[];
};

// Per-island heightmap bindless array (R16_SFLOAT), set=1 binding=19 (after the SSBO at 17 and Terrain.frag's
// masks at 18). Sampled at the island-local UV to read THIS island's own elevation, so a vertex whose own
// terrain is below the underwater zero-out drops to the flat sea floor instead of rising to an overlapping
// neighbor's composite (MAX) height.
layout (set = 1, binding = 19) uniform sampler2D ownHeightmapSamplers[kiMaxIslands];

// Input: per-vertex island-local meters XY (origin at island center). DataPacker re-centered
// XY during BakeIslandIntermediates and stripped Z at the ExportIsland write boundary — Z is
// re-derived below from the composite elevation G-buffer (see fVertexZ below).
layout (location = 0) in vec2 f2InPosition;

// Output 0: visible-area UV for Terrain.frag to sample the composite elevation G-buffer plus
//           the shadow / object-shadow / smoke samplers that all live in
//           that coordinate space. Color / normal / AO instead sample location 1.
layout (location = 0) out vec2 f2OutTexcoord;

// Output 1: per-island texture UV used by Terrain.frag to directly sample the per-island
//           color / normal / AO bindless arrays. Derived from the mesh's island-local XY
//           (pre-rotation) and the full island footprint (`f4VertexRect.zw`). Rotation-
//           independent by construction: the per-island textures are baked in island-local
//           axes, so rotation only affects where the textured surface lands in world space.
layout (location = 1) out vec2 f2OutIslandTexcoord;

// Output 2: per-instance bindless texture-slot index for color / normal / AO arrays.
//           Mirrors QuadsAxisAlignedVisibleArea.vert's location-7 forwarding pattern.
layout (location = 2) out flat uint uiOutTextureSlot;

// Output 3: per-instance rotation (cos, sin) so Terrain.frag can rotate the BC5 normal
//           tangent to match the per-island rotation.
layout (location = 3) out flat vec2 f2OutRotationCosSin;
layout (location = 4) out vec3 f3OutLightingPosition;

void main()
{
	// Direct SSBO field reads mirror QuadsAxisAlignedVisibleArea.vert.
	vec4 f4VertexRect = pQuads[gl_InstanceIndex].f4VertexRect;
	float fRotation = pQuads[gl_InstanceIndex].fRotation;
	uiOutTextureSlot = pQuads[gl_InstanceIndex].uiTextureSlot;

	vec2 f2World;
	TerrainVertexCoordinates(f2InPosition, f4VertexRect, fRotation, globalLayout.f4VisibleArea,
	                         f2World, f2OutIslandTexcoord, f2OutTexcoord, f2OutRotationCosSin);

	// Vertex Z starts from the composite elevation G-buffer (same per-island heightmap that Shadow.comp
	// and Water.frag read), then is sunk to the sea floor below the zero-out line (see below). The Gaea
	// mesh's role is purely tessellation: XY layout and
	// triangulation density. Sampling Z from the same source as the water blend guarantees
	// pixel-perfect alignment at the shoreline. textureLod required in vertex shaders (no implicit
	// derivatives).
	float fVertexZ = textureLod(elevationTextureSampler, f2OutTexcoord, 0.0f).x;

	// Sink this island's own submerged verts to the flat sea floor. fVertexZ above is the MAX composite across
	// overlapping islands, so without this an island's underwater mesh would be lifted to a taller neighbor's
	// land height and draw its baked-black ocean terrain there. The own heightmap stores raw meters relative
	// to beach (no undersea curve), so compare the raw sample to the raw zero-out threshold.
	float fOwnElevation = textureLod(ownHeightmapSamplers[nonuniformEXT(uiOutTextureSlot)], f2OutIslandTexcoord, 0.0f).x;
	if (fOwnElevation < globalLayout.fUnderwaterMaskThreshold)
	{
		fVertexZ = globalLayout.fSeaFloorElevation;
	}

	f3OutLightingPosition = vec3(f2World, fVertexZ);
	gl_Position = Transform(vec4(f2World, fVertexZ, 1.0f), mainLayout.f4x4ViewProjection);
}

