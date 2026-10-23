#version 460

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"

#include "ModelCommon.h"

void main()
{
	ModelLayout model = pModels[gl_InstanceIndex];
	int materialIndex = int(pushConstantsLayout.f4Pipeline.w);
	int meshIndex = int(model.uiMeshDataBase) + materialIndex;
	MeshData data = meshData[meshIndex];

	vec3 f3LocalPosition;
	vec3 f3LocalNormal;

	if (data.jointCount > 0)
	{
		// Use offset-based indexing into separate joint matrix buffer
		uint baseOffset = data.jointMatrixOffset;
		JointMatrix zeroJoint = JointMatrix(vec4[3](vec4(0.0), vec4(0.0), vec4(0.0)));
		JointMatrix jm0 = f4Weight0.x != 0.0 ? jointMatrices[baseOffset + int(f4Joint0.x)] : zeroJoint;
		JointMatrix jm1 = f4Weight0.y != 0.0 ? jointMatrices[baseOffset + int(f4Joint0.y)] : zeroJoint;
		JointMatrix jm2 = f4Weight0.z != 0.0 ? jointMatrices[baseOffset + int(f4Joint0.z)] : zeroJoint;
		JointMatrix jm3 = f4Weight0.w != 0.0 ? jointMatrices[baseOffset + int(f4Joint0.w)] : zeroJoint;

		// Weighted blend of 3-row matrices, then reconstruct mat4
		// Each row stores rotation in .xyz and translation component in .w
		vec4 row0 = f4Weight0.x * jm0.pf4Rows[0] + f4Weight0.y * jm1.pf4Rows[0] + f4Weight0.z * jm2.pf4Rows[0] + f4Weight0.w * jm3.pf4Rows[0];
		vec4 row1 = f4Weight0.x * jm0.pf4Rows[1] + f4Weight0.y * jm1.pf4Rows[1] + f4Weight0.z * jm2.pf4Rows[1] + f4Weight0.w * jm3.pf4Rows[1];
		vec4 row2 = f4Weight0.x * jm0.pf4Rows[2] + f4Weight0.y * jm1.pf4Rows[2] + f4Weight0.z * jm2.pf4Rows[2] + f4Weight0.w * jm3.pf4Rows[2];

		// Unpack each blended row with rotation in .xyz and translation in .w.
		mat4 skinMatrix = mat4(vec4(row0.xyz, 0.0), vec4(row1.xyz, 0.0), vec4(row2.xyz, 0.0), vec4(row0.w, row1.w, row2.w, 1.0));
		vec4 skinnedPos = skinMatrix * vec4(f3InPosition, 1.0f);
		f3LocalPosition = (data.matrix * skinnedPos).xyz;
		// Use precomputed mesh normal matrix with skin approximation (rigid transforms)
		mat3 normalMatrix = mat3(skinMatrix) * GetNormalMatrix(data);
		f3LocalNormal = normalize(normalMatrix * f3InNormal);
	}
	else
	{
		f3LocalPosition = (data.matrix * vec4(f3InPosition, 1.0f)).xyz;
		// Use precomputed normal matrix directly
		f3LocalNormal = normalize(GetNormalMatrix(data) * f3InNormal);
	}

	ModelVertexOutput(f3LocalPosition, f3LocalNormal, model);
}
