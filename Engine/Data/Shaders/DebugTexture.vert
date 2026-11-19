#version 460

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"

layout (set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform
{
	GlobalLayout globalLayout;
};

layout (set = 0, binding = kiGlobalBindingMainUniform) uniform mainUniform
{
	MainLayout mainLayout;
};

layout (location = 0) in vec2 f2InQuadVertex;

layout (location = 0) out flat int iOutInstanceIndex;
layout (location = 1) out vec2 f2OutTexcoord;

void main()
{
	iOutInstanceIndex = gl_InstanceIndex;
	f2OutTexcoord = f2InQuadVertex;

	int iFormat = int(globalLayout.fDebugTextureFormat);
	vec4 f4Area;
	float fHeight;
	if (iFormat == kiDebugTextureFormatTerrainElevation)
	{
		f4Area = globalLayout.f4VisibleArea;
		fHeight = 0.0f;
	}
	else if (iFormat == kiDebugTextureFormatFloat16LinearVisibleArea || iFormat == kiDebugTextureFormatFloat16DepositDirectionCombined || iFormat == kiDebugTextureFormatFloat16SpreadDirectionCombined)
	{
		f4Area = globalLayout.f4LightingArea;
		fHeight = globalLayout.pfFlightPlaneHeights[globalLayout.uiLightingRefreshLayer];
	}
	else if (iFormat == kiDebugTextureFormatUnormLightingDirectional)
	{
		f4Area = globalLayout.pf4LightingAreas[0];
		fHeight = globalLayout.pfFlightPlaneHeights[0];
	}
	else
	{
		gl_Position = vec4(-1.0f + 2.0f * f2InQuadVertex.x, 1.0f - 2.0f * f2InQuadVertex.y, 0.0f, 1.0f);
		return;
	}

	vec2 f2WorldPosition = VisibleAreaToWorld(f2InQuadVertex, f4Area);
	gl_Position = Transform(vec4(f2WorldPosition, fHeight, 1.0f), mainLayout.f4x4ViewProjection);
}
