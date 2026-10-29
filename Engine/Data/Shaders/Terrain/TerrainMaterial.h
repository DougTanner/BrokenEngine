#ifndef TERRAIN_MATERIAL_H
#define TERRAIN_MATERIAL_H

#ifdef LIGHTING_TERRAIN_RAY_MATERIAL
#define TerrainMaterialTexture(s, uv) textureLod(s, uv, 0.0f)
#define TerrainMaterialTextureGrad(s, uv, dx, dy) textureLod(s, uv, 0.0f)
vec3 TerrainMaterialNormal(GlobalLayout globalLayout, sampler2D normalSampler, vec2 position, float size, float speed, vec2 offset, vec2 dx, vec2 dy)
{
	vec2 xy = 2.0f * textureLod(normalSampler, offset + size * position + speed * vec2(globalLayout.fElapsedTime), 0.0f).rg - 1.0f;
	return vec3(xy, sqrt(clamp(1.0f - dot(xy, xy), 0.0f, 1.0f)));
}
#else
#define TerrainMaterialTexture(s, uv) texture(s, uv)
#define TerrainMaterialTextureGrad(s, uv, dx, dy) textureGrad(s, uv, dx, dy)
#define TerrainMaterialNormal SampleNormal
#endif

void TerrainMaterial(GlobalLayout globalLayout, vec3 f3InPosition, vec2 f2InIslandTexcoord, vec2 f2InRotationCosSin,
	                 sampler2D colorSampler, sampler2D normalSampler, sampler2D maskSampler,
	                 sampler2D rockSampler, sampler2D sandSampler,
	                 sampler2D rockNormalsSampler0, sampler2D rockNormalsSampler1, sampler2D rockNormalsSampler2,
	                 sampler2D sandNormalsSampler0, sampler2D sandNormalsSampler1, sampler2D sandNormalsSampler2,
	                 out vec3 f3Color, out vec3 f3Normal, out float fSnowPercent)
{
	f3Color = TerrainMaterialTexture(colorSampler, f2InIslandTexcoord).xyz;

	// BC5 normal: source stores RG (tangent X, Y) only. Decode `2x-1` to [-1, +1], rotate by the
	// per-island (cos, sin), reconstruct Z.
	vec2 f2NormalRG = TerrainMaterialTexture(normalSampler, f2InIslandTexcoord).rg;
	vec2 f2NormalXY = 2.0f * f2NormalRG - 1.0f;
	float fNormalCos = f2InRotationCosSin.x;
	float fNormalSin = f2InRotationCosSin.y;
	vec2 f2NormalRot = vec2(f2NormalXY.x * fNormalCos - f2NormalXY.y * fNormalSin,
	                        f2NormalXY.x * fNormalSin + f2NormalXY.y * fNormalCos);
	float fNormalZ = sqrt(clamp(1.0f - dot(f2NormalRot, f2NormalRot), 0.0f, 1.0f));
	f3Normal = normalize(vec3(f2NormalRot, fNormalZ));

	// Material masks: R=Rock, G=Sand, B=Snow, A=Flow (reserved). DataPacker packs four Gaea-authored
	// grayscale masks into a single BC7 RGBA texture per island, driving material detail blending.
	// Snow takes priority over rock/sand — snow-painted pixels suppress those blends proportionally.
	vec4 f4Masks = TerrainMaterialTexture(maskSampler, f2InIslandTexcoord);
	fSnowPercent  = f4Masks.b;
	float fRockPercent  = f4Masks.r * (1.0f - fSnowPercent);
	float fBeachPercent = f4Masks.g * (1.0f - fSnowPercent);
#ifdef LIGHTING_TERRAIN_RAY_MATERIAL
	vec2 f2PositionGradX = vec2(0.0f);
	vec2 f2PositionGradY = vec2(0.0f);
	vec2 f2RockPositionGradX = vec2(0.0f);
	vec2 f2RockPositionGradY = vec2(0.0f);
#else
	vec2 f2PositionGradX = dFdx(f3InPosition.xy);
	vec2 f2PositionGradY = dFdy(f3InPosition.xy);
	vec2 f2RockPositionGradX = dFdx(f3InPosition.xy + f3InPosition.z);
	vec2 f2RockPositionGradY = dFdy(f3InPosition.xy + f3InPosition.z);
#endif

	if (fRockPercent > 0.001f)
	{
		if (globalLayout.fTerrainRockNormalsBlend > 0.0f)
		{
			vec3 f3RockNormalSum = TerrainMaterialNormal(globalLayout, rockNormalsSampler0, f3InPosition.xy + f3InPosition.z, globalLayout.fTerrainRockNormalsSizeOne, 0.0f, vec2(0.0f, 0.0f), f2RockPositionGradX, f2RockPositionGradY) + TerrainMaterialNormal(globalLayout, rockNormalsSampler1, f3InPosition.xy + f3InPosition.z, globalLayout.fTerrainRockNormalsSizeTwo, 0.0f, vec2(0.0f, 0.0f), f2RockPositionGradX, f2RockPositionGradY) + TerrainMaterialNormal(globalLayout, rockNormalsSampler2, f3InPosition.xy + f3InPosition.z, globalLayout.fTerrainRockNormalsSizeThree, 0.0f, vec2(0.0f, 0.0f), f2RockPositionGradX, f2RockPositionGradY);
			vec3 f3RockNormal = normalize(f3RockNormalSum);
			f3Normal = normalize(f3Normal + globalLayout.fTerrainRockNormalsBlend * fRockPercent * f3RockNormal);
		}

		f3Color = mix(f3Color, TerrainMaterialTextureGrad(rockSampler, globalLayout.fTerrainRockSize * f3InPosition.xy, globalLayout.fTerrainRockSize * f2PositionGradX, globalLayout.fTerrainRockSize * f2PositionGradY).xyz, globalLayout.fTerrainRockBlend * fRockPercent);
	}

	if (fBeachPercent > 0.001f)
	{
		if (globalLayout.fTerrainBeachNormalsBlend > 0.0f)
		{
			vec3 f3BeachNormalSum = 2.0f * TerrainMaterialNormal(globalLayout, sandNormalsSampler0, f3InPosition.xy, globalLayout.fTerrainBeachNormalsSizeOne, 0.0f, vec2(0.0f, 0.0f), f2PositionGradX, f2PositionGradY) +
			                            0.5f * TerrainMaterialNormal(globalLayout, sandNormalsSampler1, f3InPosition.yx, globalLayout.fTerrainBeachNormalsSizeTwo, 0.0f, vec2(0.0f, 0.0f), f2PositionGradX.yx, f2PositionGradY.yx) +
			                            1.0f * TerrainMaterialNormal(globalLayout, sandNormalsSampler2, f3InPosition.yx, globalLayout.fTerrainBeachNormalsSizeThree, 0.0f, vec2(0.0f, 0.0f), f2PositionGradX.yx, f2PositionGradY.yx);
			vec3 f3BeachNormal = f3BeachNormalSum;
			f3BeachNormal.z = 0.0f;
			f3Normal = normalize(f3Normal + globalLayout.fTerrainBeachNormalsBlend * fBeachPercent * f3BeachNormal);
		}

		f3Color = mix(f3Color, TerrainMaterialTextureGrad(sandSampler, globalLayout.fTerrainBeachSandSize * f3InPosition.xy, globalLayout.fTerrainBeachSandSize * f2PositionGradX, globalLayout.fTerrainBeachSandSize * f2PositionGradY).xyz, globalLayout.fTerrainBeachSandBlend * fBeachPercent);
	}

	// f4TerrainSnowSunNormal = fTerrainSnowBlend * f4SunMoonNormal (uniform product folded CPU-side).
	f3Normal = normalize(f3Normal + fSnowPercent * globalLayout.f4TerrainSnowSunNormal.xyz);

}

#undef TerrainMaterialTexture
#undef TerrainMaterialTextureGrad
#undef TerrainMaterialNormal
#endif
