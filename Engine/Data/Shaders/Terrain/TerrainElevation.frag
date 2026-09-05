#version 460

#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"

// Uniforms
layout (set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform
{
    GlobalLayout globalLayout;
};

layout (set = 1, binding = 2) uniform sampler2D textureSampler[kiMaxIslands];

// Input
layout (location = 0) in flat int iInInstanceIndex;
layout (location = 1) in vec4 f4InMisc;
layout (location = 2) in vec2 f2InTexcoord;
layout (location = 7) in flat uint uiInTextureSlot;

// Output
layout (location = 0) out float fOutElevation;

void main()
{
    // DataPacker shifts heightmaps by the archetype Sea Level * elevationMeters: zero is beach, negative water,
    // positive land. Islands.cpp forces unused f4InMisc.x to zero, so subtraction is a no-op. Underwater depth uses
    // a sea-floor-anchored power curve with beach/floor fixed points; gWaterUnderseaCompression=1 is identity, and
    // smaller values upload a larger reciprocal exponent to raise shallow water. All mTerrainElevationTexture
    // consumers inherit this one compression step; land passes through unchanged.
    float fRaw = texture(textureSampler[nonuniformEXT(uiInTextureSlot)], f2InTexcoord).r - f4InMisc.x;
    // Undersea depth compression only applies below sea level; land pixels (fRaw >= 0) pass through and skip the pow
    if (fRaw < 0.0)
    {
        float fT = clamp(fRaw * globalLayout.fSeaFloorElevationInv, 0.0, 1.0);
        fOutElevation = pow(fT, globalLayout.fWaterUnderseaCompressionInv) * globalLayout.fSeaFloorElevation;
    }
    else
    {
        fOutElevation = fRaw;
    }
}
