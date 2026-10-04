#pragma once

#include "HeightLerpWrapperQuartet.h"
#include "WrapperBase.h"

namespace engine
{

extern Wrapper gIslandAmbientOcclusion;

extern HeightLerpWrapperQuartet gTerrainDetailNormalsMultiplier;

extern Wrapper gTerrainSnowBlend;
extern Wrapper gTerrainSnowAmbientOcclusionExclusion;
extern Wrapper gTerrainBeachSandSize;
extern Wrapper gTerrainBeachSandBlend;
extern Wrapper gTerrainBeachNormalsSizeOne;
extern Wrapper gTerrainBeachNormalsSizeTwo;
extern Wrapper gTerrainBeachNormalsSizeThree;
extern Wrapper gTerrainBeachNormalsBlend;

extern Wrapper gTerrainRockSize;
extern Wrapper gTerrainRockBlend;
extern Wrapper gTerrainRockNormalsSizeOne;
extern Wrapper gTerrainRockNormalsSizeTwo;
extern Wrapper gTerrainRockNormalsSizeThree;
extern Wrapper gTerrainRockNormalsBlend;

} // namespace engine
