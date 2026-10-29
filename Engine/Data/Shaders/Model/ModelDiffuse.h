#ifndef MODEL_DIFFUSE_H
#define MODEL_DIFFUSE_H

const vec3 kModelDielectricReflectance = vec3(0.04);

vec3 ModelDiffuse(vec3 baseColor, float metallic)
{
	vec3 diffuseColor = baseColor * (vec3(1.0) - kModelDielectricReflectance);
	diffuseColor *= 1.0 - metallic;
	return diffuseColor;
}

#endif
