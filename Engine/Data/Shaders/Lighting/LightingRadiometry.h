#ifndef LIGHTING_RADIOMETRY_H
#define LIGHTING_RADIOMETRY_H

#include "Lighting/LightingQuery.h"

vec3 LightingPointRadiance(LightingSource source, vec3 position, vec3 geometricNormal, vec3 shadingNormal, vec3 albedo)
{
	vec3 offset = source.f3Center - position;
	float distanceSquared = dot(offset, offset);
	float distance = sqrt(distanceSquared);
	if (distance >= source.fRange || distance == 0.0f) return vec3(0.0f);
	vec3 direction = offset / distance;
	float cosine = max(dot(shadingNormal, direction), 0.0f);
	if (cosine == 0.0f) return vec3(0.0f);
	vec3 origin = LightingRayOrigin(position, geometricNormal, direction);
	vec3 segment = source.f3Center - origin;
	float segmentLength = length(segment);
	if (segmentLength > kfLightingRayOffset && LightingOccluded(origin, segment / segmentLength, 0.0f, segmentLength - kfLightingRayOffset)) return vec3(0.0f);
	return (albedo / kfPi) * (source.f3Power / (4.0f * kfPi)) * cosine / max(distanceSquared, kfLightingDistanceFloor * kfLightingDistanceFloor);
}

vec3 LightingRectangleRadiance(LightingSource source, vec2 sampleUv, uint sampleCount, vec3 position, vec3 geometricNormal, vec3 shadingNormal, vec3 albedo)
{
	vec3 emitter = source.f3Center + (2.0f * sampleUv.x - 1.0f) * source.f3HalfEdgeU + (2.0f * sampleUv.y - 1.0f) * source.f3HalfEdgeV;
	vec3 offset = emitter - position;
	float distanceSquared = dot(offset, offset);
	float distance = sqrt(distanceSquared);
	if (distance >= source.fRange || distance == 0.0f) return vec3(0.0f);
	vec3 direction = offset / distance;
	vec3 crossEdges = cross(source.f3HalfEdgeU, source.f3HalfEdgeV);
	float crossLength = length(crossEdges);
	float area = 4.0f * crossLength;
	float cosine = max(dot(shadingNormal, direction), 0.0f) * max(dot(crossEdges / crossLength, -direction), 0.0f);
	if (cosine == 0.0f) return vec3(0.0f);
	vec3 origin = LightingRayOrigin(position, geometricNormal, direction);
	vec3 segment = emitter - origin;
	float segmentLength = length(segment);
	if (segmentLength > kfLightingRayOffset && LightingOccluded(origin, segment / segmentLength, 0.0f, segmentLength - kfLightingRayOffset)) return vec3(0.0f);
	return (albedo / kfPi) * (source.f3Power / (kfPi * area)) * (area / float(sampleCount)) * cosine / max(distanceSquared, kfLightingDistanceFloor * kfLightingDistanceFloor);
}

#endif // LIGHTING_RADIOMETRY_H
