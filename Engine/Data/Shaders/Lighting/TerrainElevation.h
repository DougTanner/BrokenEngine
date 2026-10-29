#ifndef LIGHTING_TERRAIN_ELEVATION_H
#define LIGHTING_TERRAIN_ELEVATION_H

float ShapeTerrainElevation(float fRaw, float fSeaFloorElevation, float fSeaFloorElevationInverse, float fWaterUnderseaCompressionInverse)
{
	if (fRaw < 0.0f)
	{
		float fT = clamp(fRaw * fSeaFloorElevationInverse, 0.0f, 1.0f);
		return pow(fT, fWaterUnderseaCompressionInverse) * fSeaFloorElevation;
	}
	return fRaw;
}

#endif // LIGHTING_TERRAIN_ELEVATION_H
