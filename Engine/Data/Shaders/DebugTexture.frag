#version 460

#include "ShaderLayouts.h"

// Uniforms
layout (set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform
{
	GlobalLayout globalLayout;
};

// Texture array
layout (set = 1, binding = 1) uniform sampler2D debugTextures[kiMaxDebugTextures];

// Input
layout (location = 0) in flat int iInInstanceIndex;
layout (location = 1) in vec2 f2InTexcoord;

// Output
layout (location = 0) out vec4 f4OutColor;

void main()
{
	f4OutColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	int iIndex = int(globalLayout.fDebugTextureIndex);
	int iFormat = int(globalLayout.fDebugTextureFormat);
	vec4 f4Sample = texture(debugTextures[iIndex], f2InTexcoord);

	if (iFormat == kiDebugTextureFormatFloat16Linear)
	{
		float fTotal = f4Sample.r + f4Sample.g + f4Sample.b + f4Sample.a;
		float fValue = clamp(fTotal / globalLayout.fDebugTextureLinearRange, 0.0f, 1.0f);
		f4OutColor = vec4(fValue, fValue, fValue, 1.0f);
	}
	else if (iFormat == kiDebugTextureFormatRgb)
	{
		f4OutColor = vec4(f4Sample.rgb, 1.0f);
	}
	else if (iFormat == kiDebugTextureFormatGrayscaleR)
	{
		f4OutColor = vec4(f4Sample.rrr, 1.0f);
	}
	else if (iFormat == kiDebugTextureFormatTerrainElevation)
	{
		float fLow = globalLayout.fSeaFloorElevation;
		float fHigh = globalLayout.fDebugTerrainElevationHigh;
		float fRange = max(fHigh - fLow, kfEpsilon);
		float fValue = clamp((f4Sample.r - fLow) / fRange, 0.0f, 1.0f);
		f4OutColor = vec4(fValue, fValue, fValue, 1.0f);
	}
}
