#ifndef LIGHTING_SCENE_DIAGNOSTICS_H
#define LIGHTING_SCENE_DIAGNOSTICS_H

#if defined(BT_ENGINE)
namespace shaders
{
#endif

struct LightingSceneDiagnosticParameters
{
	uint64_t uiPublication;
	uint64_t uiGeneration;
	uint32_t uiSampleCount;
	uint32_t uiPadding;
};

struct LightingSceneProbe
{
	LightingHit hit;
	vec3 f3Receiver;
	uint32_t uiHit;
	vec3 f3Radiance;
	float fVisible;
};

struct LightingSceneDiagnosticResult
{
	uint64_t uiPublication;
	uint64_t uiGeneration;
	uint32_t uiCheckedSources;
	uint32_t uiBadSources;
	uint32_t uiCheckedSamples;
	uint32_t uiBadSamples;
	LightingSource pSources[3];
	vec2 pf2Samples[4];
	LightingSceneProbe pProbes[8];
};

#if defined(BT_ENGINE)
static_assert(sizeof(LightingSceneDiagnosticParameters) == 24);
static_assert(sizeof(LightingSceneProbe) == 104);
static_assert(sizeof(LightingSceneDiagnosticResult) == 1088);
} // namespace shaders
#endif

#endif // LIGHTING_SCENE_DIAGNOSTICS_H
