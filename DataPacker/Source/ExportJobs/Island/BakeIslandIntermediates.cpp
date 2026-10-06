#include "BakeIslandIntermediates.h"

#include "BakeIslandIntermediatesInternal.h"
#include "FileManager.h"


static std::filesystem::path IslandRelativePath(const std::filesystem::path& rSourcePath)
{
	bool bUnderInputRoot = false;
	for (const std::filesystem::path& rInputRoot : gpFileManager->mpInputDirectories)
	{
		std::error_code error;
		std::filesystem::path relativePath = std::filesystem::relative(rSourcePath, rInputRoot, error);
		if (error)
		{
			continue;
		}
		if (relativePath.empty())
		{
			continue;
		}
		if (*relativePath.begin() == "..")
		{
			continue;
		}
		bUnderInputRoot = true;
		if (common::ToLower(relativePath.begin()->string()) != "islands")
		{
			continue;
		}
		return relativePath;
	}
	if (bUnderInputRoot)
	{
		throw std::runtime_error(std::format("Island path \"{}\" is not under an input root's Islands directory", rSourcePath.string()));
	}
	throw std::runtime_error(std::format("Island path \"{}\" is outside DataPacker input roots", rSourcePath.string()));
}

// pcLabel is both the Island.json route and cache subfolder; iGaeaChoice is the zero-based Gaea Route
// input (0=In, 1=Input2, through 9=Input10). iColumns/iRows split X/Y into independent kIsland chunks.
// Labels are rows-by-columns: the 2x1 dual band separates Y-offset landmasses, and 2x2 separates both
// axes at quadrant boundaries. Split axes must cut between the Route's landmasses. Uneven regions are
// valid: ProcessBakedRegion borrows neighboring pixels to meet crop alignment, including three-way
// splits of power-of-two bakes. Column/row changes bump kiSplitVersion, reuse raw Gaea output, and rerun
// only the split.
constexpr RouteSubdivision kRouteSubdivisions[] =
{
	{ .pcLabel = "1x1", .iGaeaChoice = 0, .iColumns = 1, .iRows = 1, },
	{ .pcLabel = "2x1", .iGaeaChoice = 1, .iColumns = 1, .iRows = 2, },
	{ .pcLabel = "2x2", .iGaeaChoice = 2, .iColumns = 2, .iRows = 2, },
	{ .pcLabel = "3x1", .iGaeaChoice = 3, .iColumns = 1, .iRows = 3, },
	{ .pcLabel = "3x2", .iGaeaChoice = 4, .iColumns = 2, .iRows = 3, },
	{ .pcLabel = "3x3", .iGaeaChoice = 5, .iColumns = 3, .iRows = 3, },
	{ .pcLabel = "3x4", .iGaeaChoice = 6, .iColumns = 4, .iRows = 3, },
	{ .pcLabel = "8x4", .iGaeaChoice = 7, .iColumns = 4, .iRows = 8, },
	{ .pcLabel = "4x2", .iGaeaChoice = 8, .iColumns = 2, .iRows = 4, },
	{ .pcLabel = "4x4", .iGaeaChoice = 9, .iColumns = 4, .iRows = 4, },
};

static const RouteSubdivision& LookupRouteSubdivision(std::string_view label, const std::filesystem::path& rIslandJsonFile)
{
	for (const RouteSubdivision& rRoute : kRouteSubdivisions)
	{
		if (label == rRoute.pcLabel)
		{
			return rRoute;
		}
	}

	std::string validLabels;
	for (const RouteSubdivision& rRoute : kRouteSubdivisions)
	{
		if (!validLabels.empty())
		{
			validLabels += ", ";
		}
		validLabels += rRoute.pcLabel;
	}
	throw std::runtime_error(std::format("\"{}\" lists unknown route \"{}\". Valid routes: {}.", rIslandJsonFile.string(), label, validLabels));
}

static std::filesystem::path ResolveTerrain(const std::filesystem::path& rIslandFolder, const nlohmann::json& rIslandJson)
{
	// Named archetype: two-tier lookup — any input dir's shared Islands/ folder, then sibling in
	// the island folder. Iterates all input directories so archetypes can live anywhere on the
	// search path.
	std::string archetype = rIslandJson.at("archetype").get<std::string>();

	for (const std::filesystem::path& rInputDirectory : gpFileManager->mpInputDirectories)
	{
		std::filesystem::path sharedPath = rInputDirectory / "Islands" / (archetype + ".terrain");
		if (std::filesystem::exists(sharedPath))
		{
			return sharedPath;
		}
	}

	std::filesystem::path siblingPath = rIslandFolder / (archetype + ".terrain");
	if (std::filesystem::exists(siblingPath))
	{
		return siblingPath;
	}

	throw std::runtime_error(std::format("Archetype '{}' not found in any input directory's Islands/ folder or as a sibling of \"{}\".", archetype, rIslandFolder.string()));
}

// Prune stale source sub-folders: any directory whose name isn't a current route label. This
// drops folders for routes removed from the list, so no orphaned leaf produces a stale chunk.
// Island.json and the archetype .terrain are files, so this never touches them. An empty route
// list therefore removes every generated sub-folder of the island folder.
static void RemoveNonRouteSubFolders(const std::filesystem::path& rIslandFolder, const std::vector<const RouteSubdivision*>& rRoutes)
{
	// Collect stale sub-folders first, then delete — mutating the directory mid-iteration via
	// remove_all is unspecified behavior for directory_iterator.
	std::vector<std::filesystem::path> staleSubFolders;
	for (const std::filesystem::directory_entry& rEntry : std::filesystem::directory_iterator(rIslandFolder))
	{
		if (!rEntry.is_directory())
		{
			continue;
		}
		std::string name = rEntry.path().filename().string();
		bool bIsRouteLabel = std::ranges::any_of(rRoutes, [&name](const RouteSubdivision* const& pRoute)
		{
			return name == pRoute->pcLabel;
		});
		if (!bIsRouteLabel)
		{
			staleSubFolders.push_back(rEntry.path());
		}
	}
	for (const std::filesystem::path& rStaleSubFolder : staleSubFolders)
	{
		std::filesystem::remove_all(rStaleSubFolder);
		LOG(kDefault, kDebug, "Removed stale island sub-folder: \"{}\"", rStaleSubFolder.string());
	}
}

static void BakeOne(const std::filesystem::path& rIslandFolder)
{
	std::filesystem::path islandJsonFile = rIslandFolder / "Island.json";

	std::ifstream islandStream(islandJsonFile);
	nlohmann::json islandJson = nlohmann::json::parse(islandStream);
	islandStream.close();

	if (!islandJson.is_object())
	{
		throw std::runtime_error(std::format("\"{}\" must be a JSON object", islandJsonFile.string()));
	}

	// All schema fields are strictly required. Catches stale configs (e.g., legacy "mips" key) and
	// avoids silent implicit-zero seeds or unscaled archetype dimensions.
	for (const char* pcRequired : kpcRequiredIslandJsonKeys)
	{
		if (!islandJson.contains(pcRequired))
		{
			throw std::runtime_error(std::format("\"{}\" is missing required key \"{}\". Schema is: archetype (string), seed (int), widthMeters (float), elevationMeters (float), texturePixels (int), routes (array of label strings, e.g. [\"1x1\", \"2x1\"]).", islandJsonFile.string(), pcRequired));
		}
	}

	if (islandJson.contains("mips"))
	{
		throw std::runtime_error(std::format("\"{}\" has legacy \"mips\" key. Islands no longer use mip chains: remove \"mips\" and use \"texturePixels\" instead (single resolution; elevation auto-downsamples to texturePixels / {}).", islandJsonFile.string(), kiElevationDivisor));
	}

	const nlohmann::json& rSeedJson = islandJson.at("seed");
	if (!rSeedJson.is_number_integer())
	{
		throw std::runtime_error(std::format("\"{}\" seed must be an integer", islandJsonFile.string()));
	}
	if (rSeedJson.is_number_unsigned())
	{
		if (!std::in_range<int32_t>(rSeedJson.get<uint64_t>()))
		{
			throw std::runtime_error(std::format("\"{}\" seed must fit in a signed 32-bit integer", islandJsonFile.string()));
		}
	}
	else if (!std::in_range<int32_t>(rSeedJson.get<int64_t>()))
	{
		throw std::runtime_error(std::format("\"{}\" seed must fit in a signed 32-bit integer", islandJsonFile.string()));
	}
	int64_t iSeed = rSeedJson.get<int64_t>();
	int64_t iTexturePixels = islandJson.at("texturePixels").get<int64_t>();
	WorldDimensions dimensions
	{
		.fFootprintMeters = islandJson.at("widthMeters").get<float>(),
		.fElevationMeters = islandJson.at("elevationMeters").get<float>(),
	};

	// Optional: override the Mesher's VerticesPerSide (per-island mesh density). When absent, leave
	// whatever value the archetype's Mesher node already has. Stripped from variablesJson so it doesn't
	// reach Gaea as a graph variable.
	std::optional<int64_t> oiMeshResolution;
	if (islandJson.contains("meshResolution"))
	{
		oiMeshResolution = islandJson.at("meshResolution").get<int64_t>();
	}

	// texturePixels must be >= and a multiple of kiCropAlignment so the per-axis auto-crop dims
	// satisfy both BC block alignment and Vulkan transfer-queue granularity (block-relative for
	// compressed formats) on every BC + elevation upload. See kiCropAlignment's definition.
	if (iTexturePixels < kiCropAlignment || (iTexturePixels % kiCropAlignment) != 0)
	{
		throw std::runtime_error(std::format("\"{}\" texturePixels {} is invalid: must be >= {} and a multiple of {} so the auto-crop dims stay BC-block- and Vulkan-transfer-granularity-aligned.", islandJsonFile.string(), iTexturePixels, kiCropAlignment, kiCropAlignment));
	}

	// Resolve the routes list (one or many labels). Each must exist in kRouteSubdivisions; a split
	// route additionally requires every per-column / per-row pixel span stay >= the crop alignment.
	const nlohmann::json& rRoutesJson = islandJson.at("routes");
	if (!rRoutesJson.is_array() || rRoutesJson.empty())
	{
		throw std::runtime_error(std::format("\"{}\" \"routes\" must be a non-empty array of label strings (e.g. [\"1x1\", \"2x1\"]).", islandJsonFile.string()));
	}
	std::vector<const RouteSubdivision*> routes;
	routes.reserve(static_cast<size_t>(std::ssize(rRoutesJson)));
	for (const nlohmann::json& rRouteJson : rRoutesJson)
	{
		if (!rRouteJson.is_string())
		{
			throw std::runtime_error(std::format("\"{}\" \"routes\" entries must be label strings (e.g. \"1x1\").", islandJsonFile.string()));
		}
		const RouteSubdivision& rRoute = LookupRouteSubdivision(rRouteJson.get<std::string>(), islandJsonFile);
		// Splits need not divide texturePixels evenly: ProcessBakedRegion searches each natural
		// (possibly uneven) region for its landmass, then the per-axis crop borrows neighbour pixels at
		// a non-64-aligned region edge to reach kiCropAlignment. The only requirement is that every
		// natural per-chunk span stays >= kiCropAlignment, so each region is large enough to hold an
		// aligned crop (texturePixels is already a multiple of kiCropAlignment, so 1x1 always passes).
		if ((iTexturePixels / rRoute.iColumns) < kiCropAlignment || (iTexturePixels / rRoute.iRows) < kiCropAlignment)
		{
			throw std::runtime_error(std::format("\"{}\" route \"{}\" splits texturePixels {} into {}x{} chunks, but a per-chunk pixel span ({}x{}) would be smaller than the {}-pixel crop alignment. Lower the subdivision or raise texturePixels.", islandJsonFile.string(), rRoute.pcLabel, iTexturePixels, rRoute.iColumns, rRoute.iRows, iTexturePixels / rRoute.iColumns, iTexturePixels / rRoute.iRows, kiCropAlignment));
		}
		routes.push_back(&rRoute);
	}

	std::filesystem::path archetypeFile = ResolveTerrain(rIslandFolder, islandJson);
	std::filesystem::path cacheIslandFolder = GetIslandCachePath(rIslandFolder);

	RemoveNonRouteSubFolders(rIslandFolder, routes);

	std::function<void(const std::filesystem::path&)> PruneStaleCacheRoutes = [&routes](const std::filesystem::path& rCacheIslandFolder)
	{
		if (!std::filesystem::exists(rCacheIslandFolder))
		{
			return;
		}
		std::vector<std::filesystem::path> staleCacheRoutes;
		for (const std::filesystem::directory_entry& rEntry : std::filesystem::directory_iterator(rCacheIslandFolder))
		{
			if (!rEntry.is_directory())
			{
				continue;
			}
			bool bCurrentRoute = std::ranges::any_of(routes, [&rEntry](const RouteSubdivision* pRoute)
			{
				return rEntry.path().filename() == pRoute->pcLabel;
			});
			if (!bCurrentRoute)
			{
				staleCacheRoutes.push_back(rEntry.path());
			}
		}
		for (const std::filesystem::path& rStaleCacheRoute : staleCacheRoutes)
		{
			std::filesystem::remove_all(rStaleCacheRoute);
			LOG(kDefault, kDebug, "Removed stale Gaea cache route: \"{}\"", rStaleCacheRoute.string());
		}
	};
	PruneStaleCacheRoutes(cacheIslandFolder);
	PruneStaleCacheRoutes(GetIslandDiagnosticsPath(rIslandFolder));

	IslandBakeContext context
	{
		.rIslandFolder = rIslandFolder,
		.rCacheIslandFolder = cacheIslandFolder,
		.rArchetypeFile = archetypeFile,
		.rIslandJsonFile = islandJsonFile,
		.rIslandJson = islandJson,
		.rDimensions = dimensions,
		.iSeed = iSeed,
		.iTexturePixels = iTexturePixels,
		.oiMeshResolution = oiMeshResolution,
	};
	for (const RouteSubdivision* pRoute : routes)
	{
		BakeRoute(context, *pRoute);
	}
}


std::filesystem::path GetIslandCachePath(const std::filesystem::path& rSourcePath)
{
	return gpFileManager->mGaeaCacheDirectory / IslandRelativePath(rSourcePath);
}

std::filesystem::path GetIslandDiagnosticsPath(const std::filesystem::path& rSourcePath)
{
	return gpFileManager->mGaeaCacheDirectory / "Diagnostics" / IslandRelativePath(rSourcePath);
}

BakedDimensions ReadBakedDimensions(const std::filesystem::path& rLeafFolder)
{
	std::filesystem::path bakedJsonFile = GetIslandCachePath(rLeafFolder) / kpcBakedDimensionsFile;
	std::ifstream bakedStream(bakedJsonFile);
	nlohmann::json bakedJson = nlohmann::json::parse(bakedStream);
	bakedStream.close();

	return BakedDimensions
	{
		.fWidthMeters = bakedJson.at("widthMeters").get<float>(),
		.fHeightMeters = bakedJson.at("heightMeters").get<float>(),
		.fElevationMeters = bakedJson.at("elevationMeters").get<float>(),
		.iCropX = bakedJson.at("cropX").get<int64_t>(),
		.iCropY = bakedJson.at("cropY").get<int64_t>(),
		.iCropWidth = bakedJson.at("cropWidth").get<int64_t>(),
		.iCropHeight = bakedJson.at("cropHeight").get<int64_t>(),
		.iFullTexturePixels = bakedJson.at("fullTexturePixels").get<int64_t>(),
	};
}

void BakeIslandIntermediates()
{
	std::vector<std::filesystem::path> islandFolders;
	for (const std::filesystem::path& rInputDirectory : gpFileManager->mpInputDirectories)
	{
		std::filesystem::path islandsRoot = rInputDirectory / "Islands";
		if (!std::filesystem::exists(islandsRoot))
		{
			continue;
		}

		for (const std::filesystem::directory_entry& rEntry : std::filesystem::directory_iterator(islandsRoot))
		{
			if (!rEntry.is_directory())
			{
				continue;
			}

			if (std::filesystem::exists(rEntry.path() / "Island.json"))
			{
				islandFolders.push_back(rEntry.path());
			}
			else
			{
				// An island folder without Island.json is a deleted island: its generated route
				// folders must go here, before ExportIsland / ExportTexture discovery claims their
				// leaves and packs chunks for an island that no longer exists. The island folder
				// itself stays — an empty directory claims nothing.
				RemoveNonRouteSubFolders(rEntry.path(), {});
			}
		}
	}

	if (islandFolders.empty())
	{
		return;
	}

	std::sort(islandFolders.begin(), islandFolders.end());

	for (const std::filesystem::path& rIslandFolder : islandFolders)
	{
		BakeOne(rIslandFolder);
	}
}
