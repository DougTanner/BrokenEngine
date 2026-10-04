#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/TerrainWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gTerrainRegistrar
{
	{"Ambient Occlusion", &gIslandAmbientOcclusion},
	{"Detail Normals Multiplier Start Height", &gTerrainDetailNormalsMultiplier.startHeight},
	{"Detail Normals Multiplier End Height", &gTerrainDetailNormalsMultiplier.endHeight},
	{"Detail Normals Multiplier Low", &gTerrainDetailNormalsMultiplier.low},
	{"Detail Normals Multiplier High", &gTerrainDetailNormalsMultiplier.high},
	{"Snow Blend", &gTerrainSnowBlend},
	{"Snow AO Exclusion", &gTerrainSnowAmbientOcclusionExclusion},
	{"Beach Sand Size", &gTerrainBeachSandSize},
	{"Beach Sand Blend", &gTerrainBeachSandBlend},
	{"Beach Normals Size 1", &gTerrainBeachNormalsSizeOne},
	{"Beach Normals Size 2", &gTerrainBeachNormalsSizeTwo},
	{"Beach Normals Size 3", &gTerrainBeachNormalsSizeThree},
	{"Beach Normals Blend", &gTerrainBeachNormalsBlend},
	{"Rock Size", &gTerrainRockSize},
	{"Rock Blend", &gTerrainRockBlend},
	{"Rock Normals Size 1", &gTerrainRockNormalsSizeOne},
	{"Rock Normals Size 2", &gTerrainRockNormalsSizeTwo},
	{"Rock Normals Size 3", &gTerrainRockNormalsSizeThree},
	{"Rock Normals Blend", &gTerrainRockNormalsBlend},
};

void RenderTerrainSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionTerrain;

	rScreen.WrapperSeparatorText("Ambient Occlusion");
	rScreen.WrapperSlider("Ambient Occlusion", iSection);

	rScreen.WrapperSeparatorText("Terrain Detail");
	rScreen.WrapperSlider("Detail Normals Multiplier Start Height", iSection);
	rScreen.WrapperSlider("Detail Normals Multiplier End Height", iSection);
	rScreen.WrapperSlider("Detail Normals Multiplier Low", iSection);
	rScreen.WrapperSlider("Detail Normals Multiplier High", iSection);

	rScreen.WrapperSeparatorText("Beach");
	rScreen.WrapperSlider("Snow Blend", iSection);
	rScreen.WrapperSlider("Snow AO Exclusion", iSection);
	rScreen.WrapperSlider("Beach Sand Size", iSection);
	rScreen.WrapperSlider("Beach Sand Blend", iSection);
	rScreen.WrapperSlider("Beach Normals Size 1", iSection);
	rScreen.WrapperSlider("Beach Normals Size 2", iSection);
	rScreen.WrapperSlider("Beach Normals Size 3", iSection);
	rScreen.WrapperSlider("Beach Normals Blend", iSection);

	rScreen.WrapperSeparatorText("Rock");
	rScreen.WrapperSlider("Rock Size", iSection);
	rScreen.WrapperSlider("Rock Blend", iSection);
	rScreen.WrapperSlider("Rock Normals Size 1", iSection);
	rScreen.WrapperSlider("Rock Normals Size 2", iSection);
	rScreen.WrapperSlider("Rock Normals Size 3", iSection);
	rScreen.WrapperSlider("Rock Normals Blend", iSection);
}

} // namespace engine

#endif // BT_CLIENT
