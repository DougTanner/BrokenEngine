#version 460

#include "Lighting/LightingReceiver.h"

layout(location = 0) in vec3 f3InPosition;
layout(location = 1) in vec3 f3InNormal;
layout(location = 2) in vec3 f3InAlbedo;
layout(location = 3) in flat uint uiInInstance;

void main()
{
	LightingReceiver(f3InPosition, f3InNormal, f3InNormal, f3InAlbedo);
}
