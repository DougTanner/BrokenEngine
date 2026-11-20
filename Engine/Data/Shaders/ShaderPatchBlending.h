// Randomized patch blending: up to three reads of one tiled texture, each shifted by the UV offset hashed from one vertex
// of the stationary lattice triangle around the fragment, weighted by that vertex's normalized cubed barycentric less
// GlobalLayout::fPatchWeightThreshold capped just below the largest normalized cube, floored at 0 and renormalized. A
// vertex's weight falls continuously to 0 at or before the triangle edge opposite it, so no seam forms, and a vertex at
// weight 0 skips its hash and read; its offset stays unset.
struct PatchTriangle
{
	vec3 f3Weights;
	vec2 pf2Offsets[3];
};

// i2Vertex counts lattice steps from the anchor cell's origin vertex. With k = iExponent and k >= 0, a vertex's
// identity is the anchor cell plus the floor quotient of i2Vertex by 2^k, added unsigned so extreme cells wrap, and the
// subcell remainder in [0, 2^k); for k < 0 the anchor is already the coarse cell, so the identity is the anchor plus
// i2Vertex and the subcell is 0. The identity and the read's seed feed one pcg3d integer hash (Jarzynski and Olano
// 2020, its unused last z step omitted), and the upper 24 bits of two outputs keep each offset component below 1 after
// float conversion.
vec2 PatchVertexOffset(PatchLatticeLayout lattice, ivec2 i2Vertex)
{
	int iShift = max(lattice.iExponent, 0);
	uvec2 ui2Cell = uvec2(lattice.puiAnchor[0], lattice.puiAnchor[1]) + uvec2(i2Vertex >> iShift);
	uvec2 ui2Subcell = uvec2(i2Vertex) & uvec2((1u << uint(iShift)) - 1u);
	uvec3 ui3Hash = uvec3(ui2Cell, lattice.uiSeed ^ (ui2Subcell.x | (ui2Subcell.y << 16u))) * 1664525u + 1013904223u;
	ui3Hash.x += ui3Hash.y * ui3Hash.z;
	ui3Hash.y += ui3Hash.z * ui3Hash.x;
	ui3Hash.z += ui3Hash.x * ui3Hash.y;
	ui3Hash ^= ui3Hash >> 16u;
	ui3Hash.x += ui3Hash.y * ui3Hash.z;
	ui3Hash.y += ui3Hash.z * ui3Hash.x;
	return vec2(ui3Hash.xy >> 8u) * (1.0f / 16777216.0f);
}

// f2LatticePosition is a camera-cell position. Each lattice square splits on its f.x + f.y = 1 diagonal.
PatchTriangle MakePatchTriangle(GlobalLayout globalLayout, PatchLatticeLayout lattice, vec2 f2LatticePosition)
{
	vec2 f2Lattice = ldexp(mat2(globalLayout.f4PatchInverseBasis) * f2LatticePosition, ivec2(lattice.iExponent)) + lattice.f2Remainder;
	vec2 f2Floor = floor(f2Lattice);
	vec2 f2Fraction = f2Lattice - f2Floor;
	ivec2 i2Floor = ivec2(f2Floor);
	ivec2 pi2Vertices[3];
	vec3 f3Barycentrics;
	if (f2Fraction.x + f2Fraction.y <= 1.0f)
	{
		pi2Vertices = ivec2[3](i2Floor, i2Floor + ivec2(1, 0), i2Floor + ivec2(0, 1));
		f3Barycentrics = vec3(max(1.0f - f2Fraction.x - f2Fraction.y, 0.0f), f2Fraction);
	}
	else
	{
		pi2Vertices = ivec2[3](i2Floor + ivec2(1, 1), i2Floor + ivec2(0, 1), i2Floor + ivec2(1, 0));
		f3Barycentrics = vec3(f2Fraction.x + f2Fraction.y - 1.0f, 1.0f - f2Fraction);
	}
	// max(cube / sum - threshold, 0) renormalized equals max(cube - threshold * sum, 0) renormalized. Barycentrics sum to
	// 1, so the largest cube is at least 1/27 and at least a third of the sum. Capping the subtracted term at 0.999 of the
	// largest cube leaves that vertex a positive excess for every threshold, and the cap is not reached while the
	// threshold stays at or below 0.333.
	vec3 f3Cubes = f3Barycentrics * f3Barycentrics * f3Barycentrics;
	float fLargestCube = max(max(f3Cubes.x, f3Cubes.y), f3Cubes.z);
	vec3 f3Excess = max(f3Cubes - min(globalLayout.fPatchWeightThreshold * (f3Cubes.x + f3Cubes.y + f3Cubes.z), 0.999f * fLargestCube), 0.0f);
	PatchTriangle patchTriangle;
	patchTriangle.f3Weights = f3Excess / (f3Excess.x + f3Excess.y + f3Excess.z);
	for (int i = 0; i < 3; ++i)
	{
		if (patchTriangle.f3Weights[i] > 0.0f)
		{
			patchTriangle.pf2Offsets[i] = PatchVertexOffset(lattice, pi2Vertices[i]);
		}
	}
	return patchTriangle;
}

// A tiled read at patch blend strength fStrength (a uniform): 0 is the plain read with no patch construction, 1 the patch
// blend alone, and between them their mix. bFract wraps each UV with fract() after its offset, as the repeat-sampled
// water reads require; mirrored reads pass false.
vec4 TextureGradPatched(GlobalLayout globalLayout, sampler2D textureSampler, vec2 f2Uv, vec2 f2Dx, vec2 f2Dy, bool bFract, PatchLatticeLayout lattice, vec2 f2LatticePosition, float fStrength)
{
	if (fStrength <= 0.0f)
	{
		return textureGrad(textureSampler, bFract ? fract(f2Uv) : f2Uv, f2Dx, f2Dy);
	}

	PatchTriangle patchTriangle = MakePatchTriangle(globalLayout, lattice, f2LatticePosition);
	vec4 f4Blend = vec4(0.0f);
	for (int i = 0; i < 3; ++i)
	{
		if (patchTriangle.f3Weights[i] > 0.0f)
		{
			vec2 f2OffsetUv = f2Uv + patchTriangle.pf2Offsets[i];
			f4Blend += patchTriangle.f3Weights[i] * textureGrad(textureSampler, bFract ? fract(f2OffsetUv) : f2OffsetUv, f2Dx, f2Dy);
		}
	}
	if (fStrength >= 1.0f)
	{
		return f4Blend;
	}

	return mix(textureGrad(textureSampler, bFract ? fract(f2Uv) : f2Uv, f2Dx, f2Dy), f4Blend, fStrength);
}

// TextureGradPatched for a fract()-wrapped water normal read, blending decoded normals rather than encoded XY. The caller
// builds patchTriangle with MakePatchTriangle whenever fStrength is positive, once per fragment, and reuses it for nearby
// sample positions; at strength 0 it is not read.
vec3 WaterNormalGradPatched(sampler2D normalSampler, vec2 f2Uv, vec2 f2Dx, vec2 f2Dy, PatchTriangle patchTriangle, float fStrength)
{
	if (fStrength <= 0.0f)
	{
		return DecodeWaterNormal(textureGrad(normalSampler, fract(f2Uv), f2Dx, f2Dy).rg);
	}

	vec3 f3Blend = vec3(0.0f);
	for (int i = 0; i < 3; ++i)
	{
		if (patchTriangle.f3Weights[i] > 0.0f)
		{
			f3Blend += patchTriangle.f3Weights[i] * DecodeWaterNormal(textureGrad(normalSampler, fract(f2Uv + patchTriangle.pf2Offsets[i]), f2Dx, f2Dy).rg);
		}
	}
	if (fStrength >= 1.0f)
	{
		return f3Blend;
	}

	return mix(DecodeWaterNormal(textureGrad(normalSampler, fract(f2Uv), f2Dx, f2Dy).rg), f3Blend, fStrength);
}

// TextureGradPatched at a fixed level of detail, for compute reads through a mirrored sampler (no wrap).
vec4 TextureLodPatched(GlobalLayout globalLayout, sampler2D textureSampler, vec2 f2Uv, float fLod, PatchLatticeLayout lattice, vec2 f2LatticePosition, float fStrength)
{
	if (fStrength <= 0.0f)
	{
		return textureLod(textureSampler, f2Uv, fLod);
	}

	PatchTriangle patchTriangle = MakePatchTriangle(globalLayout, lattice, f2LatticePosition);
	vec4 f4Blend = vec4(0.0f);
	for (int i = 0; i < 3; ++i)
	{
		if (patchTriangle.f3Weights[i] > 0.0f)
		{
			f4Blend += patchTriangle.f3Weights[i] * textureLod(textureSampler, f2Uv + patchTriangle.pf2Offsets[i], fLod);
		}
	}
	if (fStrength >= 1.0f)
	{
		return f4Blend;
	}

	return mix(textureLod(textureSampler, f2Uv, fLod), f4Blend, fStrength);
}
