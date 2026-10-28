#ifndef LIGHTING_TERRAIN_VALIDATION_H
#define LIGHTING_TERRAIN_VALIDATION_H

layout(buffer_reference, scalar, buffer_reference_align = 4) buffer LightingTerrainWrites { uint pCounts[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer LightingTerrainOriginals { LightingTerrainOriginal pOriginals[]; };
layout(buffer_reference, scalar, buffer_reference_align = 8) buffer LightingTerrainSummary { LightingTerrainValidation result; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer LightingTerrainRawTile { uint pWords[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer LightingTerrainTileResults { LightingTerrainTileResult pTiles[]; };
// TEMP START bounded terrain evidence
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer TerrainEvidence { LightingTerrainEvidence data; };
// TEMP END bounded terrain evidence

void LightingTerrainFailure(LightingTerrainWork work, uint placement, uint vertex, uint index, vec2 local, vec3 position, vec2 pixel, ivec2 origin, int shift, ivec2 tap, uint baseBits, uint otherBits, float baseZ, float otherZ, float own, float cutoff)
{
	LightingTerrainSummary summary = LightingTerrainSummary(work.uiSummaryAddress);
	if (atomicCompSwap(summary.result.uiFirstClaim, 0u, 1u) != 0u) return;
	summary.result.uiFirstPlacement = placement;
	summary.result.uiFirstVertex = vertex;
	summary.result.uiFirstIndex = index;
	summary.result.uiFirstTemplate = work.uiTemplate;
	summary.result.f2FirstLocal = local;
	summary.result.f3FirstPosition = position;
	summary.result.fFirstOwn = own;
	summary.result.f2FirstPixel = pixel;
	summary.result.iFirstOriginX = origin.x;
	summary.result.iFirstOriginY = origin.y;
	summary.result.iFirstShift = shift;
	summary.result.iFirstTapX = tap.x;
	summary.result.iFirstTapY = tap.y;
	summary.result.uiFirstBaseBits = baseBits;
	summary.result.uiFirstOtherBits = otherBits;
	summary.result.fFirstBaseZ = baseZ;
	summary.result.fFirstOtherZ = otherZ;
	summary.result.fFirstCutoff = cutoff;
	memoryBarrierBuffer();
	atomicExchange(summary.result.uiFirstClaim, 2u);
}

uint LightingTerrainHalf(float value)
{
	return packHalf2x16(vec2(value, 0.0f)) & 0xffffu;
}

uint LightingTerrainSavedHalf(LightingTerrainWork work, ivec2 pixel)
{
	uint index = uint(pixel.y * kiLightingTerrainScratch + pixel.x);
	uint word = LightingTerrainRawTile(work.uiBaseTileAddress).pWords[index / 2u];
	return (word >> ((index & 1u) * 16u)) & 0xffffu;
}

#endif // LIGHTING_TERRAIN_VALIDATION_H
