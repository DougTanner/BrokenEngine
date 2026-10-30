#ifndef LIGHTING_COMMON_H
#define LIGHTING_COMMON_H

#if defined(BT_ENGINE)
namespace shaders
{
#endif

CONSTEXPR int kiLightingBindingSources = 20;
CONSTEXPR int kiLightingBindingPublication = 21;
CONSTEXPR int kiLightingBindingScene = 22;
CONSTEXPR int kiLightingBindingInstances = 23;
CONSTEXPR int kiLightingBindingGeometry = 24;
CONSTEXPR int kiLightingBindingMaterials = 25;
CONSTEXPR int kiLightingBindingSamples = 26;
CONSTEXPR int kiLightingSourcePoint = 0;
CONSTEXPR int kiLightingSourceRectangle = 1;
CONSTEXPR int kiLightingGeometryModel = 0;
CONSTEXPR int kiLightingGeometryTerrain = 1;
CONSTEXPR int kiLightingGeometryFixture = 2;
CONSTEXPR float kfLightingRayOffset = 0.05f;
CONSTEXPR float kfLightingDistanceFloor = 0.01f;
CONSTEXPR float kfLightingSourceRange = 150.0f;
CONSTEXPR int kiLightingMaxSources = 1000;
CONSTEXPR int kiLightingMaxSamples = 512;
CONSTEXPR int kiLightingBounceRays = 256;
CONSTEXPR int kiLightingBounceAreaSamples = 16;

struct LightingSource
{
	vec3 f3Center;
	float fRange;
	vec3 f3HalfEdgeU;
	uint32_t uiType;
	vec3 f3HalfEdgeV;
	uint32_t uiPadding0;
	vec3 f3Power;
	uint32_t uiPadding1;
};

struct LightingPublication
{
	uint32_t uiSourceCount;
	uint32_t uiInstanceCount;
	uint32_t uiTriangleCount;
	uint32_t uiMaterialCount;
};

struct LightingInstance
{
	// Rows map local positions and normals into the camera cell; normal rows contain the CPU-computed inverse transpose.
	vec4 pf4ObjectToWorld[3];
	vec4 pf4NormalToWorld[3];
	uint32_t uiFirstGeometry;
	uint32_t uiGeometryCount;
	uint32_t uiIdentity;
	uint32_t uiPadding;
};

struct LightingGeometry
{
	// Addresses name resident local-space streams; uiFirstIndex counts index elements, not bytes.
	uint64_t uiPositionAddress;
	uint64_t uiAttributeAddress;
	uint64_t uiIndexAddress;
	uint64_t uiMaterialAddress;
	uint32_t uiPositionStride;
	uint32_t uiAttributeStride;
	uint32_t uiIndexBytes;
	uint32_t uiFirstIndex;
	uint32_t uiPrimitiveCount;
	uint32_t uiMaterial;
	uint32_t uiKind;
	// Index into the geometry kind's material/placement data.
	uint32_t uiPayload;
};

struct LightingHit
{
	vec3 f3Position;
	float fDistance;
	vec3 f3GeometricNormal;
	uint32_t uiInstance;
	vec3 f3ShadingNormal;
	uint32_t uiPrimitive;
	vec3 f3Albedo;
	uint32_t uiMaterial;
	vec2 f2Barycentrics;
};

#if defined(BT_ENGINE)
static_assert(sizeof(LightingSource) == 64);
static_assert(offsetof(LightingSource, fRange) == 12);
static_assert(offsetof(LightingSource, uiType) == 28);
static_assert(offsetof(LightingSource, f3Power) == 48);
static_assert(sizeof(LightingPublication) == 16);
static_assert(sizeof(LightingInstance) == 112);
static_assert(offsetof(LightingInstance, uiFirstGeometry) == 96);
static_assert(sizeof(LightingGeometry) == 64);
static_assert(offsetof(LightingGeometry, uiPositionStride) == 32);
static_assert(sizeof(LightingHit) == 72);
// LightingIndirect.comp reads these counts of samples-buffer points; the buffer is filled with only the selected 256 or 512.
static_assert(kiLightingBounceRays <= 256 && kiLightingBounceAreaSamples <= 256);
} // namespace shaders
#endif

#endif // LIGHTING_COMMON_H
