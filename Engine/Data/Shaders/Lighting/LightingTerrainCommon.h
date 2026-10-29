#ifndef LIGHTING_TERRAIN_COMMON_H
#define LIGHTING_TERRAIN_COMMON_H

#if defined(BT_ENGINE)
namespace shaders
{
#endif

CONSTEXPR int kiLightingTerrainGroup = 64;

struct LightingTerrainWork
{
	uint64_t uiXYAddress;
	uint64_t uiXYZAddress;
	uint32_t uiVertexCount;
	uint32_t uiPadding;
};

struct LightingTerrainMaterial
{
	uint32_t uiPlacement;
	uint32_t uiTextureSlot;
	vec2 f2Footprint;
	float fRotation;
	uint32_t uiPadding;
};

#if defined(BT_ENGINE)
static_assert(std::cmp_equal(sizeof(LightingTerrainWork), 24));
static_assert(std::cmp_equal(offsetof(LightingTerrainWork, uiVertexCount), 16));
static_assert(std::cmp_equal(sizeof(LightingTerrainMaterial), 24));
} // namespace shaders
#endif

#endif
