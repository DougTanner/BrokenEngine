#version 460

#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"
#include "Lighting/LightingModelHit.h"

layout(set = 0, binding = kiGlobalBindingMainUniform) uniform mainUniform { MainLayout mainLayout; };
layout(scalar, set = 1, binding = kiLightingBindingInstances) readonly buffer lightingInstancesBuffer { LightingInstance lightingInstances[]; };
layout(scalar, set = 1, binding = kiLightingBindingGeometry) readonly buffer lightingGeometryBuffer { LightingGeometry lightingGeometries[]; };
layout(scalar, set = 1, binding = kiLightingBindingMaterials) readonly buffer lightingFixtureMaterialsBuffer { vec4 lightingFixtureAlbedos[]; };

layout(location = 0) out vec3 f3OutPosition;
layout(location = 1) out vec3 f3OutNormal;
layout(location = 2) out vec3 f3OutAlbedo;
layout(location = 3) out flat uint uiOutInstance;

void main()
{
	uiOutInstance = gl_InstanceIndex;
	LightingInstance instance = lightingInstances[gl_InstanceIndex];
	LightingGeometry geometry = lightingGeometries[instance.uiFirstGeometry];
	uint vertex = LightingModelIndex(geometry, uint(gl_VertexIndex));
	uint64_t address = geometry.uiPositionAddress + uint64_t(vertex) * geometry.uiPositionStride;
	f3OutPosition = LightingModelTransform(vec4(LightingModelFloat3(address), 1.0f), instance.pf4ObjectToWorld);
	f3OutNormal = normalize(LightingModelTransform(vec4(LightingModelFloat3(address + 12ul), 0.0f), instance.pf4NormalToWorld));
	f3OutAlbedo = lightingFixtureAlbedos[geometry.uiMaterial].rgb;
	gl_Position = Transform(vec4(f3OutPosition, 1.0f), mainLayout.f4x4ViewProjection);
}
