#include "TweaksScreen.h"

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

void RenderTerrainSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionTerrain;

	rScreen.WrapperSeparatorText("Ambient Occlusion");
	rScreen.WrapperSlider("Island Ambient Occlusion", iSection, 2.0f, "Ambient Occlusion");

	rScreen.WrapperSeparatorText("Terrain Detail");
	rScreen.WrapperSlider("Terrain Detail Normals Multiplier Start Height", iSection, 2.0f, "Detail Normals Multiplier Start Height");
	rScreen.WrapperSlider("Terrain Detail Normals Multiplier End Height", iSection, 2.0f, "Detail Normals Multiplier End Height");
	rScreen.WrapperSlider("Terrain Detail Normals Multiplier Low", iSection, 2.0f, "Detail Normals Multiplier Low");
	rScreen.WrapperSlider("Terrain Detail Normals Multiplier High", iSection, 2.0f, "Detail Normals Multiplier High");

	rScreen.WrapperSeparatorText("Beach");
	rScreen.WrapperSlider("Terrain Snow Blend", iSection, 2.0f, "Snow Blend");
	rScreen.WrapperSlider("Terrain Snow Ambient Occlusion Exclusion", iSection, 2.0f, "Snow AO Exclusion");
	rScreen.WrapperSlider("Terrain Beach Sand Size", iSection, 2.0f, "Beach Sand Size");
	rScreen.WrapperSlider("Terrain Beach Sand Blend", iSection, 2.0f, "Beach Sand Blend");
	rScreen.WrapperSlider("Terrain Beach Normals Size One", iSection, 2.0f, "Beach Normals Size 1");
	rScreen.WrapperSlider("Terrain Beach Normals Size Two", iSection, 2.0f, "Beach Normals Size 2");
	rScreen.WrapperSlider("Terrain Beach Normals Size Three", iSection, 2.0f, "Beach Normals Size 3");
	rScreen.WrapperSlider("Terrain Beach Normals Blend", iSection, 2.0f, "Beach Normals Blend");

	rScreen.WrapperSeparatorText("Rock");
	rScreen.WrapperSlider("Terrain Rock Size", iSection, 2.0f, "Rock Size");
	rScreen.WrapperSlider("Terrain Rock Blend", iSection, 2.0f, "Rock Blend");
	rScreen.WrapperSlider("Terrain Rock Normals Size One", iSection, 2.0f, "Rock Normals Size 1");
	rScreen.WrapperSlider("Terrain Rock Normals Size Two", iSection, 2.0f, "Rock Normals Size 2");
	rScreen.WrapperSlider("Terrain Rock Normals Size Three", iSection, 2.0f, "Rock Normals Size 3");
	rScreen.WrapperSlider("Terrain Rock Normals Blend", iSection, 2.0f, "Rock Normals Blend");
}

} // namespace engine

#endif // BT_CLIENT
