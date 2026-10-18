#pragma once

// Prepass validates published source geometry or publishes a proven author cache.
// Explicit authoring may run dirty Gaea/split stages. Throws on failure.
void BakeIslandIntermediates();

struct WorldDimensions
{
	float fFootprintMeters = 0.0f;   // Isotropic horizontal extent (Gaea Terrain.Width)
	float fElevationMeters = 0.0f;   // Total vertical span in meters (Gaea Terrain.Height); per-island sea floor sits at -(Level × elevationMeters), where Level is read from the archetype's Sea node (fallback kfGaeaSeaLevelDefault if absent)
};

// Post-crop dimensions written into each chunk leaf's BakedDimensions.json by the Gaea bake.
// `fWidthMeters` / `fHeightMeters` are anisotropic (cropped chunk extent in world meters);
// `iCropX/Y/Width/Height` describe the sub-rect of the original `iFullTexturePixels`-square
// Gaea bake that this chunk kept (for a 2x1 route, the left/right half's crop). Used by
// ExportIsland to size AO / Elevation Texture ctors and to crop in-memory the PNG-loaded Color
// and EXR-loaded Normals before BC encoding. The route's shared full-resolution texture sources
// are derived from the leaf's location in the Gaea cache.
struct BakedDimensions
{
	float fWidthMeters = 0.0f;
	float fHeightMeters = 0.0f;
	float fElevationMeters = 0.0f;
	int64_t iCropX = 0;
	int64_t iCropY = 0;
	int64_t iCropWidth = 0;
	int64_t iCropHeight = 0;
	int64_t iFullTexturePixels = 0;
};

inline constexpr char kpcBakedDimensionsFile[] = "BakedDimensions.json";

// Reads the canonical source leaf dimensions after route validation; throws if missing or malformed.
BakedDimensions ReadBakedDimensions(const std::filesystem::path& rLeafFolder);
std::string GetBakedDimensionsFingerprint(const std::filesystem::path& rLeafFolder);

// Maps a source-tree island path (`.../Islands/<island>/...`) into the single mutable Gaea cache.
// Raw and author split intermediates live in the cache; delivered geometry lives in the source tree.
std::filesystem::path GetIslandCachePath(const std::filesystem::path& rSourcePath);

// Maps a source-tree island path into the parallel diagnostics tree used for JPEG sidecars.
std::filesystem::path GetIslandDiagnosticsPath(const std::filesystem::path& rSourcePath);
