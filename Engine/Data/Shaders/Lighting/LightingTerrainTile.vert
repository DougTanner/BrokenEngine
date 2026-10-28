#version 460

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"
#include "Quads/QuadsAxisAligned.h"
#include "Lighting/LightingTerrainCommon.h"

layout(push_constant) uniform pushConstants { PushConstantsLayout pushConstantsLayout; };
layout(set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform { GlobalLayout globalLayout; };
layout(scalar, set = 1, binding = 1) readonly buffer quadsBuffer { AxisAlignedQuadLayout pQuads[]; };
layout(set = 1, binding = 3) uniform sampler2D visibleElevation;

layout(location = 0) in vec2 f2InQuadVertex;
layout(location = 0) out flat int iOutInstanceIndex;
layout(location = 1) out vec4 f4OutParams;
layout(location = 2) out vec2 f2OutTexcoord;
layout(location = 7) out flat uint uiOutTextureSlot;

void main()
{
	iOutInstanceIndex = gl_InstanceIndex;
	f4OutParams = pQuads[gl_InstanceIndex].f4Parameters;
	uiOutTextureSlot = pQuads[gl_InstanceIndex].uiTextureSlot;
	f2OutTexcoord = AxisAlignedQuadTexcoord(pQuads[gl_InstanceIndex], f2InQuadVertex);
	vec4 f4World = AxisAlignedQuadWorld(pQuads[gl_InstanceIndex], f2InQuadVertex);
	vec2 f2Pixel = (f4World.xy - globalLayout.f4VisibleArea.xy) /
	               (globalLayout.f4VisibleArea.zw - globalLayout.f4VisibleArea.xy) * vec2(textureSize(visibleElevation, 0));
	ivec2 i2Origin = floatBitsToInt(pushConstantsLayout.f4Pipeline.xy);
	vec2 f2ScratchPixel = f2Pixel - vec2(i2Origin) + float(kiLightingTerrainHalo);
	gl_Position = vec4(-1.0f + 2.0f * f2ScratchPixel.x / float(kiLightingTerrainScratch),
	                    1.0f - 2.0f * f2ScratchPixel.y / float(kiLightingTerrainScratch), 0.0f, 1.0f);
}
