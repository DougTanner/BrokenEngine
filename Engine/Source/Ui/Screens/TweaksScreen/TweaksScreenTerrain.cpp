#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/TerrainWrappersBase.h"

namespace engine
{

void RenderTerrainSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionTerrain;

	rScreen.WrapperSeparatorText("Ambient Occlusion");
	rScreen.WrapperSlider("Island Ambient Occlusion", gIslandAmbientOcclusion, iSection);

	rScreen.WrapperSeparatorText("Terrain Detail");
	rScreen.WrapperSlider("Terrain Detail Normals Multiplier Start Height", gTerrainDetailNormalsMultiplier.startHeight, iSection);
	rScreen.WrapperSlider("Terrain Detail Normals Multiplier End Height", gTerrainDetailNormalsMultiplier.endHeight, iSection);
	rScreen.WrapperSlider("Terrain Detail Normals Multiplier Low", gTerrainDetailNormalsMultiplier.low, iSection);
	rScreen.WrapperSlider("Terrain Detail Normals Multiplier High", gTerrainDetailNormalsMultiplier.high, iSection);

	rScreen.WrapperSeparatorText("Beach");
	rScreen.WrapperSlider("Terrain Snow Blend", gTerrainSnowBlend, iSection);
	rScreen.WrapperSlider("Terrain Snow Ambient Occlusion Exclusion", gTerrainSnowAmbientOcclusionExclusion, iSection);
	rScreen.WrapperSlider("Terrain Beach Sand Size", gTerrainBeachSandSize, iSection);
	rScreen.WrapperSlider("Terrain Beach Sand Blend", gTerrainBeachSandBlend, iSection);
	rScreen.WrapperSlider("Terrain Beach Normals Size One", gTerrainBeachNormalsSizeOne, iSection);
	rScreen.WrapperSlider("Terrain Beach Normals Size Two", gTerrainBeachNormalsSizeTwo, iSection);
	rScreen.WrapperSlider("Terrain Beach Normals Size Three", gTerrainBeachNormalsSizeThree, iSection);
	rScreen.WrapperSlider("Terrain Beach Normals Blend", gTerrainBeachNormalsBlend, iSection);

	rScreen.WrapperSeparatorText("Rock");
	rScreen.WrapperSlider("Terrain Rock Size", gTerrainRockSize, iSection);
	rScreen.WrapperSlider("Terrain Rock Blend", gTerrainRockBlend, iSection);
	rScreen.WrapperSlider("Terrain Rock Normals Size One", gTerrainRockNormalsSizeOne, iSection);
	rScreen.WrapperSlider("Terrain Rock Normals Size Two", gTerrainRockNormalsSizeTwo, iSection);
	rScreen.WrapperSlider("Terrain Rock Normals Size Three", gTerrainRockNormalsSizeThree, iSection);
	rScreen.WrapperSlider("Terrain Rock Normals Blend", gTerrainRockNormalsBlend, iSection);
}

} // namespace engine

#endif // BT_CLIENT
