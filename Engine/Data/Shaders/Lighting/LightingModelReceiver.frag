#version 460

#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"

layout(push_constant) uniform pushConstants { PushConstantsLayout pushConstantsLayout; };
layout(set = 0, binding = kiGlobalBindingSamplerRepeatModelData) uniform sampler samplerRepeatModelData;
layout(set = 0, binding = kiGlobalBindingSamplerRepeat) uniform sampler samplerRepeat;
layout(set = 0, binding = kiGlobalBindingBindlessTextures) uniform texture2D pTextures[];
layout(scalar, set = 2, binding = 8) readonly buffer pbrMaterialsUniform { PbrMaterialLayout pMaterials[]; };

layout(location = 0) in vec3 f3InWorldPosition;
layout(location = 1) in vec3 f3InNormal;
layout(location = 2) in vec2 f2InUV;
layout(location = 3) in vec2 f2InUV1;
layout(location = 4) in vec2 f2InUV2;
layout(location = 5) in vec2 f2InUV3;
layout(location = 6) in vec2 f2InUV4;

#include "Model/ModelMaterial.h"
#include "Lighting/LightingReceiver.h"

void main()
{
	PbrMaterialLayout material = pMaterials[int32_t(pushConstantsLayout.f4Pipeline.w)];
	vec4 baseColor;
	float metallic;
	float perceptualRoughness;
	float alphaRoughness;
	vec3 f0;
	vec3 diffuseColor;
	ModelMaterial(material, baseColor, metallic, perceptualRoughness, alphaRoughness, f0, diffuseColor);
	LightingReceiver(f3InWorldPosition, GetNormal(material), f3InNormal, diffuseColor);
}
