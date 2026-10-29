#version 460

layout(location = 0) in vec3 f3InWorldPosition;
layout(location = 1) in vec3 f3InNormal;
layout(location = 2) in vec2 f2InUV;
layout(location = 3) in vec2 f2InUV1;
layout(location = 4) in vec2 f2InUV2;
layout(location = 5) in vec2 f2InUV3;
layout(location = 6) in vec2 f2InUV4;
layout(location = 7) in vec4 f4InColorAdd;
layout(location = 8) in float fInHeightAboveBase;

#include "Lighting/LightingReceiver.h"

void main()
{
	LightingReceiver(f3InWorldPosition, f3InNormal, f3InNormal, vec3(0.5f));
}
