#pragma once

#include "HeightLerpWrapperQuartet.h"
#include "WrapperBase.h"

namespace engine
{

inline Wrapper gIslandAmbientOcclusion(0.65f, 0.0f, 1.0f);

inline HeightLerpWrapperQuartet gTerrainDetailNormalsMultiplier
{
	.startHeight = Wrapper(300.0f, 150.0f, 2'000.0f),
	.endHeight = Wrapper(600.0f, 150.0f, 2'000.0f),
	.low = Wrapper(1.0f, 0.0f, 1.0f),
	.high = Wrapper(0.0f, 0.0f, 1.0f),
};

inline Wrapper gTerrainSnowBlend(0.2f, 0.0f, 2.0f);
inline Wrapper gTerrainSnowAmbientOcclusionExclusion(1.0f, 0.0f, 1.0f);
inline Wrapper gTerrainBeachSandSize(0.1f, 0.01f, 0.4f);
inline Wrapper gTerrainBeachSandBlend(0.8f, 0.0f, 1.0f);
inline Wrapper gTerrainBeachNormalsSizeOne(0.06f, 0.001f, 0.2f);
inline Wrapper gTerrainBeachNormalsSizeTwo(0.006f, 0.005f, 0.04f);
inline Wrapper gTerrainBeachNormalsSizeThree(0.06f, 0.01f, 0.2f);
inline Wrapper gTerrainBeachNormalsBlend(0.25f, 0.0f, 1.0f);

inline Wrapper gTerrainRockSize(0.4f, 0.01f, 0.4f);
inline Wrapper gTerrainRockBlend(0.15f, 0.0f, 1.0f);
inline Wrapper gTerrainRockNormalsSizeOne(0.04f, 0.01f, 0.5f);
inline Wrapper gTerrainRockNormalsSizeTwo(0.03f, 0.005f, 0.5f);
inline Wrapper gTerrainRockNormalsSizeThree(0.25f, 0.01f, 0.5f);
inline Wrapper gTerrainRockNormalsBlend(0.7f, 0.0f, 2.0f);

} // namespace engine
