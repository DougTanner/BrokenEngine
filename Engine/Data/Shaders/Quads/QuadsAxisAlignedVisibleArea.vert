#version 460

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"
#include "Quads/QuadsAxisAligned.h"

// Push constants
layout(push_constant) uniform pushConstants
{
	PushConstantsLayout pushConstantsLayout;
};

// Uniforms
layout (set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform
{
	GlobalLayout globalLayout;
};

layout (scalar, set = 1, binding = 1) buffer readonly quadsUniform
{
	AxisAlignedQuadLayout pQuads[];
};

// Input
layout (location = 0) in vec2 f2InQuadVertex;

// Output
layout (location = 0) out flat int iOutInstanceIndex;
layout (location = 1) out vec4 f4OutParams;
layout (location = 2) out vec2 f2OutTexcoord;
layout (location = 4) out vec2 f2OutWorldPosition;
layout (location = 5) out flat vec2 f2OutWorldCenter;
layout (location = 7) out flat uint uiOutTextureSlot;

void main()
{
	iOutInstanceIndex = gl_InstanceIndex;
	f4OutParams = pQuads[gl_InstanceIndex].f4Parameters;
	uiOutTextureSlot = pQuads[gl_InstanceIndex].uiTextureSlot;

	f2OutTexcoord = AxisAlignedQuadTexcoord(pQuads[gl_InstanceIndex], f2InQuadVertex);

	vec4 f4VisibleArea;
	if (int(pushConstantsLayout.f4Pipeline.x) == 0)
	{
		f4VisibleArea = globalLayout.f4VisibleArea;
	}
	else if (int(pushConstantsLayout.f4Pipeline.x) == 1)
	{
		f4VisibleArea = globalLayout.f4ShadowAreaExtra;
	}
	else
	{
		f4VisibleArea = globalLayout.pf4SmokeAreas[int(pushConstantsLayout.f4Pipeline.y)];
	}
	// fRotation is per-quad angle in radians; 0 (default) = identity (no rotation).
	vec4 f4World = AxisAlignedQuadWorld(pQuads[gl_InstanceIndex], f2InQuadVertex);
	float fWorldX = f4World.x;
	float fWorldY = f4World.y;
	float fCenterX = f4World.z;
	float fCenterY = f4World.w;
	gl_Position = vec4(-1.0f + 2.0f * (fWorldX - f4VisibleArea.x) / (f4VisibleArea.z - f4VisibleArea.x),
	                    1.0f - 2.0f * (fWorldY - f4VisibleArea.y) / (f4VisibleArea.w - f4VisibleArea.y),
	                    0.0f,
	                    1.0f);

	f2OutWorldPosition = vec2(fWorldX, fWorldY);
	f2OutWorldCenter = vec2(fCenterX, fCenterY);
}
