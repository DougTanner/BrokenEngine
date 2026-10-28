#version 460

layout(location = 0) in vec3 f3InWorldPosition;
layout(location = 1) in vec3 f3InNormal;

#include "Lighting/LightingReceiver.h"

void main()
{
	LightingReceiver(f3InWorldPosition, f3InNormal, f3InNormal, vec3(0.5f));
}
