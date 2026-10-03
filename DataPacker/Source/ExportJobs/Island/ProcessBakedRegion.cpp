#include "BakeIslandIntermediatesInternal.h"

// Crop the half-open route region into per-leaf Elevation.r32, AmbientOcclusion.r16, MeshProcessed.bin,
// and BakedDimensions.json in the Gaea cache. The source leaf owns tracked BC outputs and ExportIsland's
// stable chunk path; the recorded crop rect lets ExportIsland crop shared full-resolution
// color/normal/mask images in memory. A 1x1 route supplies the whole texture, 2x1 each Y half, and 2x2
// each quadrant, with auto-crop and underwater recoloring once per chunk. Each call mutates its own
// post-subdivision mesh copy. Return true when written, false below kfMinIslandMaxHeightMeters.


// Widest edge-taper band: about one sixteenth of a 400 m island footprint (Island.json widthMeters). The
// quarter-dimension clamp keeps small leaves from deepening their entire shoal with this fixed-width band.
constexpr float kfEdgeTaperMaxMeters = 25.0f;

// Aligned crop rect into the full bake, in full-bake pixel coords. Shared output of the auto-crop
// stages and input to every later crop / downsample / dimension stage.
struct CropRect
{
	int64_t iX = 0;
	int64_t iY = 0;
	int64_t iWidth = 0;
	int64_t iHeight = 0;
};

// Auto-crop bbox of pixels above the sea-floor cut line, in full-bake pixel coords. iMaxX < 0 marks
// an empty bbox (the region produced no terrain) — the caller turns that into a configuration error.
struct RegionBbox
{
	int64_t iMinX = 0;
	int64_t iMinY = 0;
	int64_t iMaxX = 0;
	int64_t iMaxY = 0;
};

// Expand bbox span [iLow, iHigh] symmetrically to a multiple of kiCropAlignment, clamped to
// [iClampLow, iClampHigh]; when clamping at one edge consumes that side's padding, push the remainder
// onto the opposite side. Outputs the aligned start and size.
static void ExpandSpan(int64_t iLow, int64_t iHigh, int64_t iClampLow, int64_t iClampHigh, int64_t& riStart, int64_t& riSize)
{
	int64_t iSpan = iHigh - iLow + 1;
	int64_t iRequired = ((iSpan + kiCropAlignment - 1) / kiCropAlignment) * kiCropAlignment;
	int64_t iPadding = iRequired - iSpan;
	int64_t iPaddingLow = iPadding / 2;
	int64_t iPaddingHigh = iPadding - iPaddingLow;
	int64_t iStart = iLow - iPaddingLow;
	int64_t iEnd = iHigh + iPaddingHigh;
	if (iStart < iClampLow)
	{
		iEnd += iClampLow - iStart;
		iStart = iClampLow;
	}
	if (iEnd > iClampHigh)
	{
		iStart -= iEnd - iClampHigh;
		iEnd = iClampHigh;
	}
	riStart = iStart;
	riSize = iEnd - iStart + 1;
}

// Auto-crop to the bbox of pixels above the sea-floor cut line, restricted to this region so a
// 2x1 half never pulls land across the split seam. Cut line = -fBeachOffsetMeters +
// kfCropEpsilonAboveSeaFloorMeters (epsilon meters up from the per-island sea floor). An empty
// bbox (iMaxX < 0) is left for the caller to report as a configuration error.
static RegionBbox FindRegionBoundingBox(const std::vector<float>& rFullElevationMeters, int64_t iTexturePixels, const RegionBounds& rRegion, float fBeachOffsetMeters)
{
	float fCropCutLineMeters = -fBeachOffsetMeters + kfCropEpsilonAboveSeaFloorMeters;
	RegionBbox boundingBox {.iMinX = rRegion.iEndX, .iMinY = rRegion.iEndY, .iMaxX = -1, .iMaxY = -1};
	for (int64_t iY = rRegion.iStartY; iY < rRegion.iEndY; ++iY)
	{
		const float* pfRow = &rFullElevationMeters.at(iY * iTexturePixels);
		for (int64_t iX = rRegion.iStartX; iX < rRegion.iEndX; ++iX)
		{
			if (pfRow[iX] > fCropCutLineMeters)
			{
				boundingBox.iMinX = std::min(boundingBox.iMinX, iX);
				boundingBox.iMaxX = std::max(boundingBox.iMaxX, iX);
				boundingBox.iMinY = std::min(boundingBox.iMinY, iY);
				boundingBox.iMaxY = std::max(boundingBox.iMaxY, iY);
			}
		}
	}
	return boundingBox;
}

// Expand bbox symmetrically to a multiple of kiCropAlignment per axis, clamped to the FULL bake
// [0, iTexturePixels) -- NOT the region. When a region edge isn't 64-aligned (e.g. a 3-way split
// of a power-of-two bake), the alignment padding borrows neighbour pixels across the seam to reach
// the crop alignment; the borrowed strip is the inter-landmass gap (the bbox search already
// confined this chunk's landmass to its own region). When clamping at the bake edge consumes one
// side's padding, push the remainder onto the opposite side. iTexturePixels is itself a multiple of
// kiCropAlignment, so the worst case (a 1x1 island spanning the whole bake) needs no padding and
// never overflows.
static CropRect ComputeCropRect(const RegionBbox& rBoundingBox, int64_t iTexturePixels)
{
	CropRect crop {.iX = 0, .iY = 0, .iWidth = 0, .iHeight = 0};
	ExpandSpan(rBoundingBox.iMinX, rBoundingBox.iMaxX, 0, iTexturePixels - 1, crop.iX, crop.iWidth);
	ExpandSpan(rBoundingBox.iMinY, rBoundingBox.iMaxY, 0, iTexturePixels - 1, crop.iY, crop.iHeight);
	return crop;
}

// Crop elevation to iCropWidth × iCropHeight, then box-filter downsample by kiElevationDivisor.
// Downsampled dims are multiples of 4 because crop dims are multiples of 4 × kiElevationDivisor.
static std::vector<float> CropAndDownsampleElevation(const std::vector<float>& rFullElevationMeters, int64_t iTexturePixels, const CropRect& rCrop)
{
	int64_t iElevationWidth = rCrop.iWidth / kiElevationDivisor;
	int64_t iElevationHeight = rCrop.iHeight / kiElevationDivisor;
	float fOneOverBoxSize = 1.0f / static_cast<float>(kiElevationDivisor * kiElevationDivisor);
	std::vector<float> downsampledPixels(static_cast<size_t>(iElevationWidth) * static_cast<size_t>(iElevationHeight));
	for (int64_t iOutY = 0; iOutY < iElevationHeight; ++iOutY)
	{
		for (int64_t iOutX = 0; iOutX < iElevationWidth; ++iOutX)
		{
			float fSum = 0.0f;
			for (int64_t iDy = 0; iDy < kiElevationDivisor; ++iDy)
			{
				const float* pfRow = &rFullElevationMeters.at((rCrop.iY + iOutY * kiElevationDivisor + iDy) * iTexturePixels + rCrop.iX + iOutX * kiElevationDivisor);
				for (int64_t iDx = 0; iDx < kiElevationDivisor; ++iDx)
				{
					fSum += pfRow[iDx];
				}
			}
			downsampledPixels.at(iOutY * iElevationWidth + iOutX) = fSum * fOneOverBoxSize;
		}
	}
	return downsampledPixels;
}

// Independent island leaves need border tapering, including interior split edges, toward their
// per-island sea floor -fBeachOffsetMeters. At the edge, underwater pixels at or below halfway depth
// receive the full lowering weight; shallower water changes little to preserve the visible sand apron.
// Only underwater pixels change and none is raised, preserving land and detail already below the taper
// ceiling.
static void TaperLeafElevationEdgesToSeaFloor(std::vector<float>& rDownsampledPixels, int64_t iWidth, int64_t iHeight, float fMetersPerPixel, float fBeachOffsetMeters)
{
	float fBandMeters = std::min(kfEdgeTaperMaxMeters, 0.25f * fMetersPerPixel * static_cast<float>(std::min(iWidth, iHeight)));
	if (fBandMeters <= 0.0f)
	{
		return;
	}

	float fSeaFloorMeters = -fBeachOffsetMeters;
	float fHalfwayMeters = 0.5f * fSeaFloorMeters;
	for (int64_t iY = 0; iY < iHeight; ++iY)
	{
		for (int64_t iX = 0; iX < iWidth; ++iX)
		{
			float fEdgeDistanceMeters = static_cast<float>(std::min({iX, iY, iWidth - 1 - iX, iHeight - 1 - iY})) * fMetersPerPixel;
			if (fEdgeDistanceMeters >= fBandMeters)
			{
				continue;
			}

			float fS = fEdgeDistanceMeters / fBandMeters;
			float fT = fS * fS * (3.0f - 2.0f * fS);
			float fCeiling = fSeaFloorMeters + fT * (0.0f - fSeaFloorMeters);

			float& rfPixel = rDownsampledPixels[iY * iWidth + iX];
			if (rfPixel < 0.0f)
			{
				float fDepthWeight = std::clamp(rfPixel / fHalfwayMeters, 0.0f, 1.0f);
				fDepthWeight = fDepthWeight * fDepthWeight * (3.0f - 2.0f * fDepthWeight);
				rfPixel += fDepthWeight * (std::min(rfPixel, fCeiling) - rfPixel);
			}
		}
	}
}

static void WriteElevation(const std::filesystem::path& rLeafIntermediatesDirectory, const std::vector<float>& rDownsampledPixels)
{
	std::ofstream writeStream(rLeafIntermediatesDirectory / "Elevation.r32", std::ios::binary | std::ios::trunc);
	writeStream.write(reinterpret_cast<const char*>(rDownsampledPixels.data()), static_cast<std::streamsize>(std::ssize(rDownsampledPixels) * static_cast<int64_t>(sizeof(float))));
	writeStream.close();
	VERIFY_SUCCESS(writeStream.good());
}

// Crop AmbientOcclusion to the same bbox and write the leaf AmbientOcclusion.r16. Color.png and
// Normals.exr stay full-res in the route cache (no writer in Texture.cpp for either
// format); ExportIsland crops their pixel data in-memory via this leaf's crop rect.
static void CropAndWriteAmbientOcclusion(const std::filesystem::path& rLeafIntermediatesDirectory, const std::vector<uint16_t>& rFullAmbientOcclusion, int64_t iTexturePixels, const CropRect& rCrop)
{
	std::vector<uint16_t> ambientOcclusionCropped(static_cast<size_t>(rCrop.iWidth) * static_cast<size_t>(rCrop.iHeight));
	for (int64_t iY = 0; iY < rCrop.iHeight; ++iY)
	{
		const uint16_t* puiSource = &rFullAmbientOcclusion.at((rCrop.iY + iY) * iTexturePixels + rCrop.iX);
		uint16_t* puiDestination = &ambientOcclusionCropped.at(iY * rCrop.iWidth);
		std::memcpy(puiDestination, puiSource, static_cast<size_t>(rCrop.iWidth) * sizeof(uint16_t));
	}
	std::ofstream writeStream(rLeafIntermediatesDirectory / "AmbientOcclusion.r16", std::ios::binary | std::ios::trunc);
	writeStream.write(reinterpret_cast<const char*>(ambientOcclusionCropped.data()), static_cast<std::streamsize>(std::ssize(ambientOcclusionCropped) * static_cast<int64_t>(sizeof(uint16_t))));
	writeStream.close();
	VERIFY_SUCCESS(writeStream.good());
}

// Crop the (already beach-subdivided) mesh to this chunk's bbox and re-center its local origin
// on the post-crop center. iCropX/iCropWidth map to engine X (Gaea X-east, no flip);
// iCropY/iCropHeight map to engine Y with a sign flip (engine Y north-positive, heightmap row 0
// = north edge). Discard triangles fully outside the bbox; keep partial-cross triangles for
// silhouette quality (for a 2x1 split, a triangle straddling the seam is kept by both halves —
// the same partial-cross behavior the single-island crop already uses at every edge). After the
// index buffer is compacted, repack the vertex buffer to drop orphans, then re-center. Mutates the
// caller's by-value mesh copies in place.
static void CropAndRepackMesh(std::vector<float>& rMeshPositions, std::vector<uint32_t>& rMeshIndices, const WorldDimensions& rDimensions, int64_t iTexturePixels, const CropRect& rCrop, const RegionBounds& rRegion, const std::filesystem::path& rLeafDirectory)
{
	int64_t iDiscardedTriangles = 0;
	double fPixelsToMeters = static_cast<double>(rDimensions.fFootprintMeters) / static_cast<double>(iTexturePixels);
	float fCropMinXMeters = static_cast<float>(static_cast<double>(rCrop.iX)                          * fPixelsToMeters - 0.5 * rDimensions.fFootprintMeters);
	float fCropMaxXMeters = static_cast<float>(static_cast<double>(rCrop.iX + rCrop.iWidth)           * fPixelsToMeters - 0.5 * rDimensions.fFootprintMeters);
	float fCropMaxYMeters = static_cast<float>(0.5 * rDimensions.fFootprintMeters - static_cast<double>(rCrop.iY)               * fPixelsToMeters);
	float fCropMinYMeters = static_cast<float>(0.5 * rDimensions.fFootprintMeters - static_cast<double>(rCrop.iY + rCrop.iHeight) * fPixelsToMeters);
	float fCropCenterXMeters = 0.5f * (fCropMinXMeters + fCropMaxXMeters);
	float fCropCenterYMeters = 0.5f * (fCropMinYMeters + fCropMaxYMeters);

	auto VertexOutside = [&rMeshPositions, fCropMinXMeters, fCropMaxXMeters, fCropMinYMeters, fCropMaxYMeters](int64_t iVertexIndex) -> bool
	{
		float fX = rMeshPositions[iVertexIndex * 3 + 0];
		float fY = rMeshPositions[iVertexIndex * 3 + 1];
		return fX < fCropMinXMeters || fX > fCropMaxXMeters || fY < fCropMinYMeters || fY > fCropMaxYMeters;
	};

	std::vector<uint32_t> survivingIndices;
	survivingIndices.reserve(rMeshIndices.size());
	for (int64_t i = 0; i + 2 < std::ssize(rMeshIndices); i += 3)
	{
		uint32_t iA = rMeshIndices[i + 0];
		uint32_t iB = rMeshIndices[i + 1];
		uint32_t iC = rMeshIndices[i + 2];
		if (VertexOutside(iA) && VertexOutside(iB) && VertexOutside(iC))
		{
			++iDiscardedTriangles;
			continue;
		}
		survivingIndices.push_back(iA);
		survivingIndices.push_back(iB);
		survivingIndices.push_back(iC);
	}
	rMeshIndices = std::move(survivingIndices);

	// Reject an empty cropped mesh after the pixel-bbox check so no invisible island chunk is written.
	if (rMeshIndices.empty())
	{
		throw std::runtime_error(std::format("Island chunk \"{}\" region [{}..{}, {}..{}] has zero surviving triangles after mesh crop ({} discarded): the Route subdivision produced no mesh inside this chunk's bbox. Check the archetype's Mesher resolution or Route shape.", rLeafDirectory.string(), rRegion.iStartX, rRegion.iEndX - 1, rRegion.iStartY, rRegion.iEndY - 1, iDiscardedTriangles));
	}

	// Compact + cache-optimize the vertex buffer: meshopt_optimizeVertexFetch reorders surviving
	// vertices into index-access order (GPU fetch efficiency) and drops orphans left by the crop,
	// rewriting rMeshIndices in place. Positions are bare float XYZ triples (12-byte stride).
	size_t uiOldVertexCount = rMeshPositions.size() / 3;
	std::vector<float> packedPositions(rMeshPositions.size());
	size_t uiNewVertexCount = meshopt_optimizeVertexFetch(packedPositions.data(), rMeshIndices.data(), rMeshIndices.size(), rMeshPositions.data(), uiOldVertexCount, sizeof(float) * 3);
	packedPositions.resize(uiNewVertexCount * 3);
	rMeshPositions = std::move(packedPositions);

	// Re-center XY of every surviving vertex on the post-crop center. Z is unchanged
	// (Z=0 is sea level globally, independent of horizontal crop).
	for (int64_t i = 0; i < std::ssize(rMeshPositions) / 3; ++i)
	{
		rMeshPositions[i * 3 + 0] -= fCropCenterXMeters;
		rMeshPositions[i * 3 + 1] -= fCropCenterYMeters;
	}
	LOG(kDefault, kDebug, "Mesh chunk \"{}\": cropped {} triangles outside bbox, re-centered XY by ({:.2f}, {:.2f})m, {} -> {} vertices", rLeafDirectory.string(), iDiscardedTriangles, fCropCenterXMeters, fCropCenterYMeters, uiOldVertexCount, std::ssize(rMeshPositions) / 3);
}

static void WriteMeshProcessed(const std::filesystem::path& rLeafIntermediatesDirectory, const std::vector<float>& rMeshPositions, const std::vector<uint32_t>& rMeshIndices)
{
	std::ofstream meshOutput(rLeafIntermediatesDirectory / "MeshProcessed.bin", std::ios::binary | std::ios::trunc);
	int32_t iVertexCount32 = static_cast<int32_t>(rMeshPositions.size() / 3);
	int32_t iIndexCount32 = static_cast<int32_t>(rMeshIndices.size());
	meshOutput.write(reinterpret_cast<const char*>(&iVertexCount32), sizeof(int32_t));
	meshOutput.write(reinterpret_cast<const char*>(&iIndexCount32), sizeof(int32_t));
	meshOutput.write(reinterpret_cast<const char*>(rMeshPositions.data()), static_cast<std::streamsize>(std::ssize(rMeshPositions) * static_cast<int64_t>(sizeof(float))));
	meshOutput.write(reinterpret_cast<const char*>(rMeshIndices.data()), static_cast<std::streamsize>(std::ssize(rMeshIndices) * static_cast<int64_t>(sizeof(uint32_t))));
	meshOutput.close();
	VERIFY_SUCCESS(meshOutput.good());
}

// BakedDimensions.json is written last in the leaf; ExportIsland::Handles treats its presence as the
// leaf-complete marker. The post-crop world width and height use the global meters-per-pixel scale,
// and the crop rect maps the leaf into the full bake. The shared texture source path follows the
// cache layout.
static void WriteBakedDimensions(const std::filesystem::path& rLeafIntermediatesDirectory, const WorldDimensions& rDimensions, int64_t iTexturePixels, const CropRect& rCrop)
{
	nlohmann::json bakedJson;
	bakedJson["widthMeters"] = rDimensions.fFootprintMeters * static_cast<float>(rCrop.iWidth) / static_cast<float>(iTexturePixels);
	bakedJson["heightMeters"] = rDimensions.fFootprintMeters * static_cast<float>(rCrop.iHeight) / static_cast<float>(iTexturePixels);
	bakedJson["elevationMeters"] = rDimensions.fElevationMeters;
	bakedJson["cropX"] = rCrop.iX;
	bakedJson["cropY"] = rCrop.iY;
	bakedJson["cropWidth"] = rCrop.iWidth;
	bakedJson["cropHeight"] = rCrop.iHeight;
	bakedJson["fullTexturePixels"] = iTexturePixels;
	std::ofstream bakedStream(rLeafIntermediatesDirectory / kpcBakedDimensionsFile);
	bakedStream << bakedJson.dump(4);
	bakedStream.close();
	VERIFY_SUCCESS(bakedStream.good());
}


bool ProcessBakedRegion(const IslandBakeContext& rContext, const BakeOutput& rBakeOutput, const RegionBounds& rRegion, std::vector<float> meshPositions, std::vector<uint32_t> meshIndices, const LeafTarget& rLeaf)
{
	const std::filesystem::path& rSourceLeafDirectory = rLeaf.rSourceLeafDirectory;
	const std::filesystem::path& rCacheLeafDirectory = rLeaf.rCacheLeafDirectory;

	// Auto-crop bbox of pixels above the sea-floor cut line, confined to this region. An empty bbox
	// is a configuration error: the route produced no terrain in this chunk.
	RegionBbox boundingBox = FindRegionBoundingBox(rBakeOutput.rFullElevationMeters, rContext.iTexturePixels, rRegion, rBakeOutput.fBeachOffsetMeters);
	if (boundingBox.iMaxX < 0)
	{
		float fCropCutLineMeters = -rBakeOutput.fBeachOffsetMeters + kfCropEpsilonAboveSeaFloorMeters;
		throw std::runtime_error(std::format("Island chunk \"{}\" region [{}..{}, {}..{}] has no pixels above the sea-floor cut line ({:.2f} m): the Route subdivision produced no terrain in this chunk. Check the archetype's Route shape or raise Island.json's elevationMeters.", rSourceLeafDirectory.string(), rRegion.iStartX, rRegion.iEndX - 1, rRegion.iStartY, rRegion.iEndY - 1, fCropCutLineMeters));
	}

	// Align and expand the bbox to the crop rect (multiple of kiCropAlignment per axis).
	CropRect crop = ComputeCropRect(boundingBox, rContext.iTexturePixels);
	LOG(kDefault, kDebug, "Cropping island chunk \"{}\": bbox ({}..{},{}..{}) -> ({}+{},{}+{}) [aligned to {}]", rSourceLeafDirectory.string(), boundingBox.iMinX, boundingBox.iMaxX, boundingBox.iMinY, boundingBox.iMaxY, crop.iX, crop.iWidth, crop.iY, crop.iHeight, kiCropAlignment);

	// Crop + box-filter downsample elevation, then reject very low / underwater leaves: the peak is
	// measured on the downsampled elevation this leaf would ship, not the full bake. Below the
	// threshold, delete any prior committed leaf (intermediates AND BC outputs) and write nothing, so
	// no BakedDimensions.json is created -- ExportIsland::Handles never claims it, producing no kIsland
	// chunk and no orphan texture chunks. AreLeavesDirty treats the now-absent leaf folder as
	// intentionally skipped.
	std::vector<float> downsampledPixels = CropAndDownsampleElevation(rBakeOutput.rFullElevationMeters, rContext.iTexturePixels, crop);
	float fMaxHeightMeters = *std::ranges::max_element(downsampledPixels);
	if (fMaxHeightMeters < kfMinIslandMaxHeightMeters)
	{
		std::filesystem::remove_all(rSourceLeafDirectory);
		std::filesystem::remove_all(rCacheLeafDirectory);
		std::filesystem::remove_all(GetIslandDiagnosticsPath(rSourceLeafDirectory));
		LOG(kDefault, kDebug, "Rejected island leaf \"{}\": max height {}m below minimum {}m", rSourceLeafDirectory.string(), common::Wb(fMaxHeightMeters, 2), common::Wb(kfMinIslandMaxHeightMeters, 2));
		return false;
	}

	// Taper after the rejection check (which judges the untapered peak) — the taper only ever lowers
	// pixels, so it cannot change that decision — and before the write, so the leaf Elevation.r32 and
	// everything derived from it (chunk payload, underwater texture masking, valid-area hull) stay in
	// lockstep.
	int64_t iElevationWidth = crop.iWidth / kiElevationDivisor;
	int64_t iElevationHeight = crop.iHeight / kiElevationDivisor;
	float fMetersPerPixel = rContext.rDimensions.fFootprintMeters / static_cast<float>(rContext.iTexturePixels) * static_cast<float>(kiElevationDivisor);
	TaperLeafElevationEdgesToSeaFloor(downsampledPixels, iElevationWidth, iElevationHeight, fMetersPerPixel, rBakeOutput.fBeachOffsetMeters);

	std::filesystem::create_directories(rSourceLeafDirectory);
	std::filesystem::create_directories(rCacheLeafDirectory);
	WriteElevation(rCacheLeafDirectory, downsampledPixels);
	CropAndWriteAmbientOcclusion(rCacheLeafDirectory, rBakeOutput.rFullAmbientOcclusion, rContext.iTexturePixels, crop);

	CropAndRepackMesh(meshPositions, meshIndices, rContext.rDimensions, rContext.iTexturePixels, crop, rRegion, rSourceLeafDirectory);
	WriteMeshProcessed(rCacheLeafDirectory, meshPositions, meshIndices);

	WriteBakedDimensions(rCacheLeafDirectory, rContext.rDimensions, rContext.iTexturePixels, crop);

	return true;
}
