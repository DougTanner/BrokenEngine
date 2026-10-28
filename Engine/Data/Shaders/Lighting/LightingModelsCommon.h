#ifndef LIGHTING_MODELS_COMMON_H
#define LIGHTING_MODELS_COMMON_H

#if defined(BT_ENGINE)
namespace shaders
{
#endif

struct LightingModelVertex
{
	vec3 f3Position;
	vec3 f3Normal;
};

struct LightingModelMaterial
{
	vec4 f4BaseColorFactor;
	float fMetallicFactor;
	uint32_t uiColorTexture;
	uint32_t uiPhysicalTexture;
	int32_t iColorTextureSet;
	int32_t iPhysicalTextureSet;
	uint32_t uiNormalTexture;
	int32_t iNormalTextureSet;
	uint32_t uiPadding;
};

struct LightingModelDeform
{
	uint64_t uiVertexAddress;
	uint64_t uiInstanceAddress;
	uint64_t uiMeshAddress;
	uint64_t uiJointAddress;
	uint64_t uiOutputAddress;
	uint64_t uiVertexMaterialAddress;
	uint32_t uiInstance;
	uint32_t uiVertexCount;
};

#if defined(BT_ENGINE)
static_assert(sizeof(LightingModelVertex) == 24);
static_assert(sizeof(LightingModelMaterial) == 48);
static_assert(sizeof(LightingModelDeform) == 56);
static_assert(offsetof(LightingModelDeform, uiInstance) == 48);
} // namespace shaders
#endif

#endif // LIGHTING_MODELS_COMMON_H
