// DT: TEMP
// #define DT_LIGHTING_ONLY

// #define ENABLE_SHADER_REALTIME_CLOCK_EXT
// #define ENABLE_DEBUG_PRINTF_EXT
#if defined(ENABLE_DEBUG_PRINTF_EXT)
	#define ENABLE_VULKAN_DEBUG_LAYERS
#endif

#if defined(BT_ENGINE)

#pragma once

#define CONSTEXPR inline constexpr
#define INLINE inline
#define INIT {}
#define STD std::

// Constexpr bool equivalents of shader defines for C++ code
#if defined(ENABLE_DEBUG_PRINTF_EXT)
inline constexpr bool kbDebugPrintf = true;
#else
inline constexpr bool kbDebugPrintf = false;
#endif

#if defined(ENABLE_SHADER_REALTIME_CLOCK_EXT)
inline constexpr bool kbShaderRealtimeClock = true;
#else
inline constexpr bool kbShaderRealtimeClock = false;
#endif

namespace shaders
{

inline constexpr VkFormat keElevationFormat = VK_FORMAT_R16_SFLOAT;

inline constexpr VkFormat keLightingFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

inline constexpr VkFormat keSmokeFormat = VK_FORMAT_R16_SFLOAT;
inline constexpr VkFormat keWindFormat = VK_FORMAT_R16G16_SFLOAT;
inline constexpr VkFormat keShadowFormat = VK_FORMAT_R16_UNORM;
inline constexpr VkFormat keCombineFormat = VK_FORMAT_R8G8B8A8_UNORM;
inline constexpr VkFormat keWaterDisplacementFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

struct vec2 : public XMFLOAT2
{
};

struct vec3 : public XMFLOAT3
{
};

struct vec4 : public XMFLOAT4
{
	vec4() = default;

	constexpr vec4(float fX, float fY, float fZ, float fW)
	: XMFLOAT4(fX, fY, fZ, fW)
	{
	}

	constexpr vec4(const XMFLOAT4& rOther)
	{
		x = rOther.x;
		y = rOther.y;
		z = rOther.z;
		w = rOther.w;
	}
};

struct ivec4
{
	int32_t x = 0, y = 0, z = 0, w = 0;
};

struct uvec4
{
	uint32_t x = 0, y = 0, z = 0, w = 0;
};

#else

#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_scalar_block_layout : require
layout(scalar) uniform;
#if defined(ENABLE_DEBUG_PRINTF_EXT)
	#extension GL_EXT_debug_printf : require
#endif
#if defined(ENABLE_SHADER_REALTIME_CLOCK_EXT)
	#extension GL_EXT_shader_realtime_clock : require
#endif

#define UINT_MAX 0xFFFFFFFF
#define INT_MAX 2147483647
#define CONSTEXPR const
#define INLINE
#define INIT
#define STD

#define ELEVATION_FORMAT r16f

struct VkDrawIndexedIndirectCommand
{
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
	uint32_t firstInstance;
};

struct VkDispatchIndirectCommand
{
	uint32_t x;
	uint32_t y;
	uint32_t z; 
};

#endif // BT_ENGINE

CONSTEXPR float kfPi = 3.141592654f;
CONSTEXPR float kfEpsilon = 1e-6f;

// Global Set-0 descriptor binding numbers. Single-sourced here (dual-language) so the C++ descriptor
// layout/writes (TextureDescriptors) and every shader's layout(set = 0, binding = N) qualifier stay in
// lockstep — a binding-number change in one place no longer silently mismatches the other.
CONSTEXPR int kiGlobalBindingGlobalUniform = 0;
CONSTEXPR int kiGlobalBindingMainUniform = 1;
CONSTEXPR int kiGlobalBindingSamplerRepeatModelData = 13;
CONSTEXPR int kiGlobalBindingSamplerRepeat = 3;
CONSTEXPR int kiGlobalBindingBindlessTextures = 4;
CONSTEXPR int kiGlobalBindingSamplerClamp = 12;

// Set-1 per-pipeline descriptor binding numbers. Single-sourced here (dual-language) like the Set-0
// block above, so the C++ pipeline setup (ModelPipeline kModel descriptor append, BufferManager mesh/joint
// storage-buffer updates, PipelineManager water displacement descriptors) and the GLSL
// layout(set = 1, binding = N) qualifiers (ModelCommon.h, Water.vert) stay in lockstep.
CONSTEXPR int kiWaterBindingDisplacement = 13;
CONSTEXPR int kiWaterBindingDisplacementNormal = 14;
CONSTEXPR int kiModelBindingMeshData = 15;
CONSTEXPR int kiModelBindingJointMatrix = 16;

// Water normal map atlas size. Shared by C++ (PipelineManager mppWaterNormalTextures array,
// TextureManager kpWaterNormalCrcs/kpWaterNormalNames tables) and the water shader
// (Water.frag pWaterNormalSamplers[] sampler array).
CONSTEXPR int kiWaterNormalCount = 17;

// Length of each per-octave-group mip-variance table uploaded for Water.frag's
// WATER_SPEC_AA_MIP_HANDOFF kernel. Must equal common::TextureHeader::kiMipVarianceCount
// (static_assert in TextureManager.cpp) — the tables are copied verbatim from texture chunk headers.
CONSTEXPR int kiWaterSpecAAMipTableSize = 10;

// Island bindless-slot ceiling: shared by C++ and the terrain shaders, it sizes the bindless
// sampler arrays (Terrain.frag / TerrainElevation.frag), their descriptor counts (PipelineManager),
// and the TextureManager render-target pointer vectors. Raised 64 -> 128 so island 01's 10 routes
// (65 kIsland chunks) fit. Bumping this only grows descriptor-array capacity + pointer vectors, not
// resident GPU textures (LRU-managed). Every kIsland template stays permanently resident (mesh +
// heightmap) regardless of this cap or LRU; that memory scaling is tracked in
// Documents/Investigations/Graphics/IslandResidentMemoryScaling_Overview.md.
CONSTEXPR int kiMaxIslands = 128;

CONSTEXPR int kiMaxSpreadPasses = 40;
CONSTEXPR int kiMaxDebugTextures = kiMaxSpreadPasses + 16;

CONSTEXPR int kiDebugTextureFormatFloat16LightingDirectional = 0;
CONSTEXPR int kiDebugTextureFormatUnormLightingDirectional = 1;
CONSTEXPR int kiDebugTextureFormatFloat16Linear = 2;
CONSTEXPR int kiDebugTextureFormatFloat16LinearVisibleArea = 3;
CONSTEXPR int kiDebugTextureFormatFloat16DepositDirectionCombined = 4;
CONSTEXPR int kiDebugTextureFormatFloat16SpreadDirectionCombined = 5;
CONSTEXPR int kiDebugTextureFormatRgb = 6;
CONSTEXPR int kiDebugTextureFormatGrayscaleR = 7;
CONSTEXPR int kiDebugTextureFormatTerrainElevation = 8;

CONSTEXPR int kiShadowTextureExecutionSize = 64;

CONSTEXPR int kiShadowBlurRadius = 5;

CONSTEXPR int kiComputeTileSize = 8;
CONSTEXPR int kiOccupancyDilateGroupSize = 256;
CONSTEXPR int kiParticleUpdateGroupSize = 32;

CONSTEXPR float kfAmbient = 0.05f;

CONSTEXPR vec4 kf4MudColor = {99.0f / 255.0f, 75.0f / 255.0f, 53.0f / 255.0f, 0.0f};

struct PushConstantsLayout
{
	vec4 f4Pipeline INIT; // Per-pipeline push payload; .w carries the model material index (read by Model.frag / ModelSkinned.vert)
};

#include "ShaderGlobalLayout.h"

#include "ShaderMainLayout.h"

struct AxisAlignedQuadLayout
{
	vec4 f4VertexRect INIT;
	vec4 f4TextureRect INIT;
	vec4 f4Params INIT;
	float fRotation INIT; // radians; 0 = identity (no rotation). Vertex shader rotates corners around quad center.
	uint32_t uiTextureSlot INIT; // per-instance bindless texture-array index; 0 for non-island quads (templates assign at AcquireTextureSlot).
	uint32_t uiColor INIT;
};

struct BillboardLayout
{
	vec4 f4Position INIT;
	float fSize INIT;
	float fTextureIndex INIT;
	float fRotation INIT;
	float fAlpha INIT;
};

struct DebugRenderLayout
{
	vec4 f3x4Transform[3] INIT;
	vec4 f4Color INIT;
};

struct QuadLayout
{
	vec4 pf4VerticesTexcoords[4] INIT;
	vec4 pf4Params[4] INIT;
	vec4 f4Params INIT;
	uint32_t uiColor INIT;
};

struct VisibleLightQuadLayout
{
	vec4 pf4Vertices[4] INIT;
	vec4 pf4Texcoords[4] INIT;
	uint32_t puiColors[4] INIT;

	float fIntensity INIT;
	float fRotation INIT;

	uint32_t uiTextureIndex INIT;
};

struct ObjectLayout
{
	vec4 f4Position INIT;
	uint32_t uiColor INIT;
	vec4 f3x4Transform[3] INIT;
	vec4 f3x4TransformNormal[3] INIT;
};

struct PbrMaterialLayout
{
	// First 9 fields must exactly match MaterialShaderData (from f4BaseColorFactor onward)
	// PBR material properties per glTF 2.0 metallic-roughness specification
	vec4 f4BaseColorFactor INIT;
	vec4 f4EmissiveFactor INIT;
	int32_t iColorTextureSet INIT;
	int32_t iPhysicalDescriptorTextureSet INIT;
	int32_t iNormalTextureSet INIT;
	int32_t iOcclusionTextureSet INIT;
	int32_t iEmissiveTextureSet INIT;
	float fMetallicFactor INIT;
	float fRoughnessFactor INIT;
	float fAlphaMask INIT;
	float fAlphaMaskCutoff INIT;

	// Bindless texture array indices (populated by CrcToIndex() on CPU)
	float fColorTextureIndex INIT;
	float fPhysicalDescriptorTextureIndex INIT;
	float fNormalTextureIndex INIT;
	float fOcclusionTextureIndex INIT;
	float fEmissiveTextureIndex INIT;
};

struct ModelLayout
{
	vec4 f4Position INIT;
	vec4 f3x4Transform[3] INIT;
	vec4 f3x4TransformNormal[3] INIT;
	vec4 f4ColorAdd INIT;
	uint32_t uiMeshDataBase INIT;     // Base index into meshData[] for this object
	uint32_t uiMaterialCount INIT;    // Number of materials (slots in meshData)
};

struct ModelCustomLayout
{
	vec4 f4Position INIT;
	vec4 f3x4Transform[3] INIT;
	vec4 f3x4TransformNormal[3] INIT;
	vec4 f3x4TransformCustom[3] INIT;
	vec4 f3x4TransformCustomNormal[3] INIT;
	vec4 f4ColorAdd INIT;
};

CONSTEXPR int32_t kiHexShieldDirections = 4 * 4;
#if defined(BT_ENGINE)
static_assert((kiHexShieldDirections % 4) == 0);
#endif

struct HexShieldLayout
{
#if defined(BT_ENGINE)
	bool operator==(const HexShieldLayout&) const = default;
#endif

	vec4 f4Position INIT;
	vec4 f3x4Transform[3] INIT;
	vec4 f3x4TransformNormal[3] INIT;
	vec4 f4Color INIT;
	vec4 f4LightingColor INIT;
	vec4 pf4Directions[kiHexShieldDirections] INIT;
	float pfVertIntensities[kiHexShieldDirections] INIT;
	float pfFragIntensities[kiHexShieldDirections] INIT;

	float fLightingIntensity INIT;
	float fSize INIT;
	float fColorMix INIT;
	float fMinimumIntensity INIT;
};

// Particles
CONSTEXPR int kiMaxParticlesSpawn = 8 * 1024;
CONSTEXPR int kiMaxParticles = 16 * 1024;

struct ParticleLayout
{
	int32_t iColor INIT;
	int32_t iCookie INIT;

	float fVelocityDecay INIT;
	float fGravity INIT;
	float fIntensityDecay INIT;

	float fSize INIT;
	float fLength INIT;
	float fVisibleIntensity INIT;
	float fIntensityPower INIT;

	float fSizeDecay INIT;
	float fRotationDelta INIT;
	float fRotation INIT;
	float fRotationDeltaDecay INIT;

	vec4 f4Position INIT;
	vec4 f4Velocity INIT;
};

struct ParticlesSpawnLayout
{
	int32_t iCount INIT;
	int32_t iReset INIT;
	ParticleLayout pParticles[kiMaxParticlesSpawn] INIT;
};

#define ENABLE_32_BIT_BOOL

struct ParticlesLayout
{
	int32_t iLastCount INIT;
	int32_t iMinFreeIndex INIT;

	float fLastUpdateTime INIT;
	float fDeltaTime INIT;

	ParticleLayout pParticles[kiMaxParticles] INIT;
#if defined(ENABLE_32_BIT_BOOL)
	uint32_t puiAllocated[kiMaxParticles / 32 + 1] INIT;
#else
	uint16_t pbAllocated[kiMaxParticles] INIT;
#endif
};

#if defined(BT_ENGINE)

// NOTE: Uniform buffer structs (GlobalLayout, MainLayout) use scalar layout (no padding requirements)
static_assert(sizeof(GlobalLayout) <= 65536);

} // namespace shaders

#endif
