#ifndef LIGHTING_COMPOSE_H
#define LIGHTING_COMPOSE_H

layout(set = 0, binding = kiGlobalBindingLightingReference) uniform sampler2D lightingReferenceImage;
layout(set = 0, binding = kiGlobalBindingLightingControl, scalar) readonly buffer LightingControl { uint lightingReferenceEnabled; };

vec3 LightingReferenceDiffuse()
{
	if (lightingReferenceEnabled == 0u) return vec3(0.0);
	return texelFetch(lightingReferenceImage, ivec2(gl_FragCoord.xy), 0).rgb;
}

#endif
