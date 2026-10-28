#ifndef LIGHTING_RECEIVER_H
#define LIGHTING_RECEIVER_H

layout(location = 0) out vec4 f4OutReceiverPosition;
layout(location = 1) out vec4 f4OutReceiverShadingNormal;
layout(location = 2) out vec4 f4OutReceiverGeometricNormal;
layout(location = 3) out vec4 f4OutReceiverAlbedo;

void LightingReceiver(vec3 position, vec3 shadingNormal, vec3 geometryOrientation, vec3 albedo)
{
	vec3 geometricNormal = normalize(cross(dFdx(position), dFdy(position)));
	// Derivatives recover the triangle plane; the unperturbed normal fixes its sign across viewport conventions.
	if (dot(geometricNormal, geometryOrientation) < 0.0f)
	{
		geometricNormal = -geometricNormal;
	}
	f4OutReceiverPosition = vec4(position, 1.0f);
	f4OutReceiverShadingNormal = vec4(normalize(shadingNormal), 0.0f);
	f4OutReceiverGeometricNormal = vec4(geometricNormal, 0.0f);
	f4OutReceiverAlbedo = vec4(albedo, 1.0f);
}

#endif
