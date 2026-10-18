#include "PublishedIslandBake.h"

#include "BakeIslandIntermediatesInternal.h"
#include "FileManager.h"
#include "GaeaArchetype.h"


constexpr const char* kpcPublicationFile = "BakedIsland.json";
constexpr const char* kpcGeometryFiles[] = {"BakedDimensions.json", "Elevation.r32", "MeshProcessed.bin"};
constexpr const char* kpcDeliveredFiles[] =
{
	"BakedDimensions.json", "Elevation.r32", "MeshProcessed.bin",
	kpcIslandAmbientOcclusion, kpcIslandColor, kpcIslandMasks, kpcIslandNormals,
};

static std::map<std::filesystem::path, nlohmann::json> sPendingPublications;
static std::unordered_set<std::filesystem::path> sPublishedLeaves;

static nlohmann::json PublicationIdentity(const IslandBakeContext& rContext, const RouteSubdivision& rRoute, int64_t iBakeVersion, int64_t iSplitVersion)
{
	nlohmann::json identity;
	identity["magic"] = "BrokenEngineIslandBake";
	identity["version"] = 2;
	identity["bakeVersion"] = iBakeVersion;
	identity["splitVersion"] = iSplitVersion;
	identity["textureVersion"] = ExportIsland::kiTextureVersion;
	identity["island"] = gpFileManager->mpInputFingerprintCache->Get(rContext.rIslandJsonFile, InputFingerprintMode::kTextCrLf);
	identity["archetype"] = GetArchetypeIdentity(rContext.rArchetypeFile, rContext.rDimensions, rContext.iSeed, rContext.optionalMeshResolution, rRoute.iGaeaChoice);
	identity["route"] = rRoute.pcLabel;
	identity["choice"] = rRoute.iGaeaChoice;
	identity["columns"] = rRoute.iColumns;
	identity["rows"] = rRoute.iRows;
	return identity;
}

static std::unordered_set<int64_t> NumericLeaves(const std::filesystem::path& rRoute, int64_t iLeafCount)
{
	std::unordered_set<int64_t> leaves;
	if (!std::filesystem::exists(rRoute))
	{
		return leaves;
	}
	for (const std::filesystem::directory_entry& rEntry : std::filesystem::directory_iterator(rRoute))
	{
		std::string name = rEntry.path().filename().string();
		if (name.empty())
		{
			continue;
		}
		if (!std::ranges::all_of(name, [](char c) { return c >= '0' && c <= '9'; }))
		{
			continue;
		}
		if (!rEntry.is_directory() || std::cmp_greater(name.size(), 18))
		{
			throw std::runtime_error("Invalid numeric island leaf: " + rEntry.path().string());
		}
		int64_t iIndex = std::stoll(name);
		if (iIndex >= iLeafCount || name != std::to_string(iIndex))
		{
			throw std::runtime_error("Unexpected numeric island leaf: " + rEntry.path().string());
		}
		leaves.insert(iIndex);
	}
	return leaves;
}

static nlohmann::json DeliveredHashes(const std::filesystem::path& rLeaf)
{
	nlohmann::json hashes;
	for (const char* pcFile : kpcDeliveredFiles)
	{
		if (!std::filesystem::is_regular_file(rLeaf / pcFile))
		{
			throw std::runtime_error("Missing delivered island file: " + (rLeaf / pcFile).string());
		}
		hashes[pcFile] = std::string_view(pcFile) == kpcBakedDimensionsFile ? GetBakedDimensionsFingerprint(rLeaf) : gpFileManager->mpInputFingerprintCache->Get(rLeaf / pcFile);
	}
	return hashes;
}

bool UsePublishedIslandBake(const IslandBakeContext& rContext, const RouteSubdivision& rRoute, int64_t iBakeVersion, int64_t iSplitVersion)
{
	std::filesystem::path route = rContext.rIslandFolder / rRoute.pcLabel;
	nlohmann::json identity = PublicationIdentity(rContext, rRoute, iBakeVersion, iSplitVersion);
	std::filesystem::path publicationFile = route / kpcPublicationFile;
	if (std::filesystem::exists(publicationFile))
	{
		try
		{
			std::ifstream stream(publicationFile);
			nlohmann::json publication = nlohmann::json::parse(stream);
			nlohmann::json leaves = publication.at("leaves");
			publication.erase("leaves");
			if (publication != identity || !leaves.is_array() || leaves.empty())
			{
				throw std::runtime_error("source identity, stage version, or accepted leaf set differs");
			}
			std::unordered_set<int64_t> indices;
			for (const nlohmann::json& rLeaf : leaves)
			{
				if (!rLeaf.at("index").is_number_integer())
				{
					throw std::runtime_error("leaf index is not an integer");
				}
				int64_t iIndex = rLeaf.at("index").get<int64_t>();
				if (iIndex < 0 || iIndex >= rRoute.iColumns * rRoute.iRows || !indices.insert(iIndex).second)
				{
					throw std::runtime_error("duplicate or out-of-range leaf index");
				}
				if (rLeaf.at("files") != DeliveredHashes(route / std::to_string(iIndex)))
				{
					throw std::runtime_error("delivered file hash mismatch for leaf " + std::to_string(iIndex));
				}
			}
			if (NumericLeaves(route, rRoute.iColumns * rRoute.iRows) != indices)
			{
				throw std::runtime_error("unexpected numeric source leaves");
			}
			for (int64_t iIndex : indices)
			{
				sPublishedLeaves.insert(route / std::to_string(iIndex));
			}
			return true;
		}
		catch (const std::exception& rException)
		{
			if (!(gpFileManager->mExportFlags & FileManager::ExportFlags::kAllowGaeaExport))
			{
				throw std::runtime_error(std::format("Invalid published island route \"{}\": {}. Restore its delivered assets or explicitly authorize authoring with BT_DATAPACKER_ALLOW_GAEA_EXPORT=1.", route.string(), rException.what()));
			}
			std::filesystem::remove(publicationFile);
		}
	}
	sPendingPublications.insert_or_assign(route, std::move(identity));
	return false;
}

void CopyIslandBakeGeometry(const std::filesystem::path& rRouteDirectory, int64_t iLeafCount)
{
	std::filesystem::path cacheRoute = GetIslandCachePath(rRouteDirectory);
	std::unordered_set<int64_t> indices = NumericLeaves(cacheRoute, iLeafCount);
	if (indices.empty() || NumericLeaves(rRouteDirectory, iLeafCount) != indices)
	{
		throw std::runtime_error("Incomplete accepted island leaf set: " + rRouteDirectory.string());
	}
	std::filesystem::remove(rRouteDirectory / kpcPublicationFile);
	for (int64_t iIndex : indices)
	{
		for (const char* pcFile : kpcGeometryFiles)
		{
			std::filesystem::path destination = rRouteDirectory / std::to_string(iIndex) / pcFile;
			std::filesystem::path temporary = destination;
			temporary += ".tmp";
			std::filesystem::copy_file(cacheRoute / std::to_string(iIndex) / pcFile, temporary, std::filesystem::copy_options::overwrite_existing);
			VERIFY_SUCCESS(MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING));
		}
	}
}

void ResetPublishedIslandBakes()
{
	sPendingPublications.clear();
	sPublishedLeaves.clear();
}

static void WritePublication(const std::filesystem::path& rRoute, nlohmann::json identity)
{
	int64_t iLeafCount = identity.at("columns").get<int64_t>() * identity.at("rows").get<int64_t>();
	identity["leaves"] = nlohmann::json::array();
	std::unordered_set<int64_t> indices = NumericLeaves(rRoute, iLeafCount);
	std::vector<int64_t> sortedIndices(indices.begin(), indices.end());
	std::ranges::sort(sortedIndices);
	if (indices.empty())
	{
		throw std::runtime_error("No accepted island leaves: " + rRoute.string());
	}
	for (int64_t iIndex : sortedIndices)
	{
		std::filesystem::path leaf = rRoute / std::to_string(iIndex);
		if (!ExportIsland::HasAuthorTextureProof(leaf))
		{
			throw std::runtime_error("Missing current author texture proof: " + leaf.string());
		}
		identity["leaves"].push_back({{"index", iIndex}, {"files", DeliveredHashes(leaf)}});
	}
	std::filesystem::path publicationFile = rRoute / kpcPublicationFile;
	std::filesystem::path temporary = publicationFile;
	temporary += ".tmp";
	WriteEntireFile(temporary, identity.dump(2));
	VERIFY_SUCCESS(MoveFileExW(temporary.c_str(), publicationFile.c_str(), MOVEFILE_REPLACE_EXISTING));
	for (int64_t iIndex : sortedIndices)
	{
		sPublishedLeaves.insert(rRoute / std::to_string(iIndex));
	}
}

void PublishIslandBake(const IslandBakeContext& rContext, const RouteSubdivision& rRoute, int64_t iBakeVersion, int64_t iSplitVersion)
{
	std::filesystem::path route = rContext.rIslandFolder / rRoute.pcLabel;
	WritePublication(route, PublicationIdentity(rContext, rRoute, iBakeVersion, iSplitVersion));
	sPendingPublications.erase(route);
}

void FinalizePublishedIslandBakes()
{
	for (const auto& [rRoute, rIdentity] : sPendingPublications)
	{
		WritePublication(rRoute, rIdentity);
	}
	sPendingPublications.clear();
	sPublishedLeaves.clear();
}

bool IsPublishedIslandLeaf(const std::filesystem::path& rLeaf)
{
	return sPublishedLeaves.contains(rLeaf);
}
