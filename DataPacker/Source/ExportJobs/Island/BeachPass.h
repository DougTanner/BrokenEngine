#pragma once

// To tune these in the primary checkout, build and run DataPacker from Visual Studio in the Release configuration: a game
// build runs that same DataPacker.exe, and it writes its own built-in values back over the chunks of a Debug run.

// Percent of each leaf's shelf the bars cover, as the mean blend weight over its shelf texels.
inline constexpr float kfBeachBarCoveragePercent = 91.6f;

// How far bars sit below the curved height, in raw meters, not the compressed depth the water shader shows.
inline constexpr float kfBeachBarDepthMeters = 0.15f;

// Reshapes one leaf's heights, before the island scale, into a sloped beach with submerged bars. Each texel stays on its source
// side of common::kfUnderwaterMaskThresholdMeters: terrain below it renders sunk and its BC textures are masked to flat values,
// so crossing either way breaks the coast, and the hull stays the source leaf's. The pass is single-threaded with fixed
// iteration counts so output bytes are reproducible.
void ApplyBeachPass(std::span<float> elevation, int64_t iWidth, int64_t iHeight, float fMetersPerTexel);
