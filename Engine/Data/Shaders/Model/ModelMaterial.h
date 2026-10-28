#ifndef MODEL_MATERIAL_H
#define MODEL_MATERIAL_H

#include "Model/ModelDiffuse.h"

const float kMinRoughness = 0.04;

// Select UV based on texture set index
vec2 getUV(int textureSet)
{
	if (textureSet <= 0) return f2InUV;
	if (textureSet == 1) return f2InUV1;
	if (textureSet == 2) return f2InUV2;
	if (textureSet == 3) return f2InUV3;
	return f2InUV4;
}

// sRGB to linear color space conversion
vec4 SRGBtoLinear(vec4 srgb)
{
	return vec4(pow(srgb.rgb, vec3(2.2)), srgb.a);
}

vec3 SRGBtoLinear(vec3 srgb)
{
	return pow(srgb, vec3(2.2));
}

// Compute perturbed normal from normal map using screen-space derivatives
vec3 GetNormal(PbrMaterialLayout material)
{
	vec3 N = normalize(f3InNormal);

	if (material.iNormalTextureSet < 0)
	{
		return N;
	}

	vec3 pos_dx = dFdx(f3InWorldPosition);
	vec3 pos_dy = dFdy(f3InWorldPosition);
	vec2 tex_dx = dFdx(f2InUV);
	vec2 tex_dy = dFdy(f2InUV);

	float det = tex_dx.s * tex_dy.t - tex_dy.s * tex_dx.t;
	if (abs(det) < 1e-8)
	{
		return N;
	}

	vec3 T = (tex_dy.t * pos_dx - tex_dx.t * pos_dy) / det;
	vec3 Tperp = T - N * dot(N, T);
	if (dot(Tperp, Tperp) < kfEpsilon * kfEpsilon)
	{
		return N;
	}
	T = normalize(Tperp);
	// cross of two perpendicular unit vectors is already unit (N is unit; T is unit and Gram-Schmidt-orthogonalized to N above)
	vec3 B = cross(N, T);
	mat3 TBN = mat3(T, B, N);

	// BC5 normal map: only XY stored, reconstruct Z = sqrt(1 - X^2 - Y^2).
	vec2 nXY = texture(sampler2D(pTextures[nonuniformEXT(int(material.fNormalTextureIndex))], samplerRepeatModelData), getUV(material.iNormalTextureSet)).rg * 2.0 - 1.0;
	vec3 tangentNormal = vec3(nXY, sqrt(clamp(1.0 - dot(nXY, nXY), 0.0, 1.0)));
	return normalize(TBN * tangentNormal);
}

void ModelMaterial(PbrMaterialLayout material, out vec4 baseColor, out float metallic, out float perceptualRoughness,
	               out float alphaRoughness, out vec3 f0, out vec3 diffuseColor)
{
	// Sample base color
	baseColor = material.f4BaseColorFactor;
	if (material.iColorTextureSet > -1)
	{
		baseColor *= SRGBtoLinear(texture(sampler2D(pTextures[nonuniformEXT(int(material.fColorTextureIndex))], samplerRepeat), getUV(material.iColorTextureSet)));
	}

	// Metallic-Roughness workflow
	metallic = material.fMetallicFactor;
	perceptualRoughness = material.fRoughnessFactor;
	if (material.iPhysicalDescriptorTextureSet > -1)
	{
		vec4 mrSample = texture(sampler2D(pTextures[nonuniformEXT(int(material.fPhysicalDescriptorTextureIndex))], samplerRepeatModelData), getUV(material.iPhysicalDescriptorTextureSet));
		perceptualRoughness *= mrSample.g;
		metallic *= mrSample.b;
	}
	metallic = clamp(metallic, 0.0, 1.0);
	perceptualRoughness = clamp(perceptualRoughness, kMinRoughness, 1.0);

	alphaRoughness = perceptualRoughness * perceptualRoughness;

	// Energy-conserving diffuse (accounts for light reflected as specular)
	f0 = kModelDielectricReflectance;
	diffuseColor = ModelDiffuse(baseColor.rgb, metallic);

}

#endif
