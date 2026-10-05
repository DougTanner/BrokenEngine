#include "BakeIslandIntermediatesInternal.h"
#include "FileManager.h"
#include "GaeaArchetype.h"
#include "SubdivideBeachBand.h"


// One route cache under FileManager::mGaeaCacheDirectory holds Gaea's raw texturePixels-resolution
// outputs, checked after baking and by IsGaeaRawDirty. Elevation.r32 is headerless IEEE FloatRaw32
// normalized to [0,1]; ProcessBakedRegion reads it without rewriting it and writes leaf elevation
// downsampled by kiElevationDivisor. AO uses
// UshortRaw16 matched to BC4_UNORM. Color/masks use 8-bit sRGB PNGs matched to BC7; Gaea's sRGB-in-EXR
// convention conflicts with color decoding. Normals remain multi-channel EXR, alongside Mesher outputs.
constexpr const char* kpcIntermediateFiles[] =
{
	"AmbientOcclusion.r16",
	"Color.png",
	"Elevation.r32",
	"Normals.exr",
	"Flow.png",
	"Rock.png",
	"Sand.png",
	"Snow.png",
	"Mesh.gltf",  // Gaea Mesher output: glTF JSON manifest (separate-format)
	"Mesh.bin",   // Gaea Mesher output: binary buffer referenced by Mesh.gltf
};

// MeshProcessed.bin (positions + indices in island-local meters, XY-centered, sea-level Z=0) is
// derived per chunk leaf, not in the route's raw cache root — the per-leaf dirty check in
// AreLeavesDirty verifies it (and Elevation.r32 / AmbientOcclusion.r16 / BakedDimensions.json)
// exists in each chunk folder.

// Route-level BakeVersion.meta tracks slow raw Gaea output; bump kiBakeVersion for archetype
// dimensions, seed, Route Choice, Mesher resolution, or invocation changes, and IsGaeaRawDirty reruns
// Gaea. SplitVersion.meta tracks fast post-Gaea splitting; bump kiSplitVersion for ProcessBakedRegion
// crop/edge-taper or route column/row changes. AreLeavesDirty reruns only the split from existing raw
// output.
constexpr int64_t kiBakeVersion = 28;
constexpr int64_t kiSplitVersion = 8;

// The band straddles beach Z=0 independently of elevationMeters; its lower and upper bounds
// separately control underwater and above-water coverage.
constexpr float kfBeachSubdivisionMinMeters = -0.25f;
constexpr float kfBeachSubdivisionMaxMeters = 0.5f;
constexpr float kfBeachSubdivisionMaxEdgeMeters = 1.0f;
constexpr int64_t kiBeachSubdivisionMaxDepth = 12;

constexpr const char* kpcBakeVersionFile = "BakeVersion.meta";
constexpr const char* kpcSplitVersionFile = "SplitVersion.meta";
constexpr const char* kpcPatchedArchetypeFile = "PatchedArchetype.terrain";
constexpr const char* kpcGaeaStagingDirectory = "GaeaStaging";

static size_t CheckedTexturePixelCount(const std::filesystem::path& rTextureFile, int64_t iTexturePixels)
{
	if (iTexturePixels <= 0)
	{
		throw std::runtime_error(std::format("Gaea output \"{}\" has invalid texture dimensions {}x{}.", rTextureFile.string(), iTexturePixels, iTexturePixels));
	}

	size_t uiTexturePixels = static_cast<size_t>(iTexturePixels);
	if (uiTexturePixels > std::numeric_limits<size_t>::max() / uiTexturePixels)
	{
		throw std::runtime_error(std::format("Gaea output \"{}\" has texture dimensions whose pixel count overflows size_t ({}x{}).", rTextureFile.string(), iTexturePixels, iTexturePixels));
	}
	return uiTexturePixels * uiTexturePixels;
}

static std::string ReadTextFile(const std::filesystem::path& rFile)
{
	std::ifstream stream(rFile, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

static void WriteTextFile(const std::filesystem::path& rFile, std::string_view text)
{
	std::ofstream stream(rFile, std::ios::binary | std::ios::trunc);
	stream.write(text.data(), static_cast<std::streamsize>(text.size()));
	stream.close();
	VERIFY_SUCCESS(stream.good());
}

static std::string BakeFingerprint(const IslandBakeContext& rContext, const RouteSubdivision& rRoute)
{
	nlohmann::json metadata;
	metadata["version"] = kiBakeVersion;
	metadata["island"] = gpFileManager->mpInputFingerprintCache->Get(rContext.rIslandJsonFile, InputFingerprintMode::kTextCrLf);
	metadata["archetype"] = gpFileManager->mpInputFingerprintCache->Get(rContext.rArchetypeFile, InputFingerprintMode::kTextCrLf);
	metadata["route"] = rRoute.pcLabel;
	metadata["choice"] = rRoute.iGaeaChoice;
	return metadata.dump();
}

static std::string SplitFingerprint(const RouteSubdivision& rRoute)
{
	nlohmann::json metadata;
	metadata["version"] = kiSplitVersion;
	metadata["route"] = rRoute.pcLabel;
	metadata["columns"] = rRoute.iColumns;
	metadata["rows"] = rRoute.iRows;
	return metadata.dump();
}

// True if the route's RAW Gaea outputs are missing or stale — forces a (slow) Gaea.Swarm re-export.
// Checks only Gaea-output concerns: the raw intermediate files + the patched archetype (needed for
// the split's sea-level read) are present, the Gaea-bake-version sentinel matches, and the raw
// metadata matches the content fingerprints of Island.json / the archetype and the route identity.
// The post-Gaea split is checked separately
// by AreLeavesDirty so a split-only change never trips this.
static bool IsGaeaRawDirty(const std::filesystem::path& rIntermediatesDirectory, std::string_view expectedFingerprint)
{
	for (const char* pcFile : kpcIntermediateFiles)
	{
		if (!std::filesystem::exists(rIntermediatesDirectory / pcFile))
		{
			return true;
		}
	}
	if (!std::filesystem::exists(rIntermediatesDirectory / kpcPatchedArchetypeFile))
	{
		return true;
	}

	std::filesystem::path versionFile = rIntermediatesDirectory / kpcBakeVersionFile;
	if (!std::filesystem::exists(versionFile))
	{
		return true;
	}
	return ReadTextFile(versionFile) != expectedFingerprint;
}

// True if any chunk leaf's derived split outputs are missing or stale — forces a re-split from the
// (assumed fresh) raw Gaea output, NOT a Gaea re-export. Checks the split-version sentinel (catches
// a kRouteSubdivisions columns/rows or ProcessBakedRegion change) and every leaf's per-region files.
static bool AreLeavesDirty(const std::filesystem::path& rRouteDirectory, const std::filesystem::path& rCacheRouteDirectory, int64_t iLeafCount, std::string_view expectedFingerprint)
{
	std::filesystem::path splitVersionFile = rCacheRouteDirectory / kpcSplitVersionFile;
	if (!std::filesystem::exists(splitVersionFile))
	{
		return true;
	}
	if (ReadTextFile(splitVersionFile) != expectedFingerprint)
	{
		return true;
	}

	for (int64_t i = 0; i < iLeafCount; ++i)
	{
		// An absent leaf folder is an intentionally-rejected (too-low) leaf, not a dirty one -- skip it.
		// ProcessBakedRegion deletes rejected leaves, and the SplitVersion sentinel (checked above,
		// stamped last) keeps a crash mid-split from looking clean. Existing folders must be complete.
		std::filesystem::path sourceLeafDirectory = rRouteDirectory / std::to_string(i);
		std::filesystem::path cacheLeafDirectory = rCacheRouteDirectory / std::to_string(i);
		if (!std::filesystem::exists(sourceLeafDirectory) && !std::filesystem::exists(cacheLeafDirectory))
		{
			continue;
		}
		if (!std::filesystem::exists(cacheLeafDirectory / kpcBakedDimensionsFile)
		 || !std::filesystem::exists(cacheLeafDirectory / "MeshProcessed.bin")
		 || !std::filesystem::exists(cacheLeafDirectory / "Elevation.r32")
		 || !std::filesystem::exists(cacheLeafDirectory / "AmbientOcclusion.r16"))
		{
			return true;
		}
		std::filesystem::create_directories(sourceLeafDirectory);
	}

	return false;
}

// Remove numeric leaves with index >= iLeafCount before the dirty early return: neither the split loop
// nor AreLeavesDirty revisits them, so stale BakedDimensions.json becomes extra kIsland chunks through
// ExportIsland::Handles. A split-version bump only rewrites retained leaves. As in BakeOne's whole-route
// prune, collect paths before remove_all to avoid directory-iterator invalidation; preserve nonnumeric
// metadata and indices below iLeafCount.
static void RemoveOrphanedLeafFolders(const std::filesystem::path& rRouteDirectory, int64_t iLeafCount)
{
	if (!std::filesystem::exists(rRouteDirectory))
	{
		return;
	}

	std::vector<std::filesystem::path> orphanedLeafFolders;
	for (const std::filesystem::directory_entry& rEntry : std::filesystem::directory_iterator(rRouteDirectory))
	{
		if (!rEntry.is_directory())
		{
			continue;
		}
		std::string name = rEntry.path().filename().string();
		if (name.empty() || !std::ranges::all_of(name, [](char cChar) { return cChar >= '0' && cChar <= '9'; }))
		{
			continue;
		}
		// std::stoll can throw when an all-digit name exceeds int64_t's range. Names longer than
		// digits10 (18) are skipped before parsing so the sweep removes only confidently classified folders.
		if (name.size() > static_cast<size_t>(std::numeric_limits<int64_t>::digits10))
		{
			continue;
		}
		if (std::stoll(name) >= iLeafCount)
		{
			orphanedLeafFolders.push_back(rEntry.path());
		}
	}
	for (const std::filesystem::path& rOrphanedLeafFolder : orphanedLeafFolders)
	{
		std::filesystem::remove_all(rOrphanedLeafFolder);
		LOG(kDefault, kDebug, "Removed orphaned island leaf folder: \"{}\"", rOrphanedLeafFolder.string());
	}
}

// STAGE 1 — Gaea raw export (slow). Strips DataPacker-owned keys to vars, copies + patches the
// archetype, runs Gaea.Swarm once at full texturePixels into a staging directory, then verifies the
// staged raw outputs exist. The caller publishes them and stamps the sentinel. Called only when
// IsGaeaRawDirty.
static void RunGaeaExport(const IslandBakeContext& rContext, const RouteSubdivision& rRoute, const std::filesystem::path& rGaeaExecutable, const std::filesystem::path& rRouteDirectory, const std::filesystem::path& rStagingDirectory, const std::filesystem::path& rPatchedArchetypeFile)
{
	LOG(kDefault, kDebug, "Baking island route \"{}\" (Gaea export; archetype: \"{}\", seed: {}, texturePixels: {}, Route Choice: {})", rRouteDirectory.string(), rContext.rArchetypeFile.string(), rContext.iSeed, rContext.iTexturePixels, rRoute.iGaeaChoice);

	// Strip DataPacker-owned keys; remaining keys become Gaea graph variables. routes /
	// widthMeters / elevationMeters / seed / texturePixels are DataPacker-consumed: Gaea's
	// --vars can't reach Terrain.{Width,Height}, the Route Choice, or per-node Seed fields.
	nlohmann::json variablesJson = rContext.rIslandJson;
	for (const char* pcKey : kpcRequiredIslandJsonKeys)
	{
		variablesJson.erase(pcKey);
	}
	variablesJson.erase("meshResolution");  // DataPacker-consumed; patched into the Mesher node directly.

	// Gaea.Swarm.exe trips on `--vars` pointing to an empty JSON object ("{}") with an opaque
	// "System.IO.IOException: The handle is invalid" during variable load. Only emit the vars
	// file and pass --vars when there are user variables left. Per-route temp name so concurrent
	// routes / islands don't collide.
	bool bHasVariables = !variablesJson.empty();
	std::filesystem::path variablesFile = gpFileManager->mCacheDirectory / std::format("{}-{}.gaea-vars.json", rContext.rIslandFolder.filename().string(), rRoute.pcLabel);
	common::ScopedLambda variablesFileCleanup([&variablesFile, bHasVariables]()
	{
		if (bHasVariables)
		{
			std::filesystem::remove(variablesFile);
		}
	});
	if (bHasVariables)
	{
		std::ofstream variablesStream(variablesFile);
		variablesStream << variablesJson.dump();
		variablesStream.close();
		VERIFY_SUCCESS(variablesStream.good());
	}

	// Copy the source archetype into this route's cache and patch the copy — the on-disk
	// source .terrain is never mutated. PatchArchetype sets this route's Route Choice along with
	// dims / seeds / Mesher resolution. PatchedArchetype.terrain doubles as a debug artifact (open
	// in Gaea to inspect exactly what was baked, including the patched Route Choice).
	std::filesystem::copy_file(rContext.rArchetypeFile, rPatchedArchetypeFile, std::filesystem::copy_options::overwrite_existing);
	PatchArchetype(rPatchedArchetypeFile, rContext.rDimensions, rContext.iSeed, rContext.oiMeshResolution, rRoute.iGaeaChoice);

	// Gaea.Swarm.exe requires a real console for stdin/stdout/stderr — invoke via the new-console
	// helper. argv[0] is the executable's own path. --seed is dropped (per-node seeds were patched
	// into the archetype). Gaea bakes the patched copy in this route's staging directory.
	std::wstring commandLine;
	commandLine += L"\"" + rGaeaExecutable.native() + L"\"";
	commandLine += L" --silent";
	commandLine += L" --Filename \"" + rPatchedArchetypeFile.native() + L"\"";
	commandLine += L" --buildpath \"" + rStagingDirectory.native() + L"\"";
	commandLine += std::format(L" --resolution {}", rContext.iTexturePixels);
	if (bHasVariables)
	{
		commandLine += L" --vars \"" + variablesFile.native() + L"\"";
	}

	LOG(kDefault, kDebug, "Running: {}", commandLine);
	if (gpFileManager->mbForbidExpensiveExport)
	{
		throw std::runtime_error("Gaea.Swarm export blocked by BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1");
	}
	if (gpFileManager->mbForbidGaeaExport)
	{
		throw std::runtime_error("Gaea.Swarm export blocked by BT_DATAPACKER_FORBID_GAEA_EXPORT=1");
	}
	common::ExecutableResult result = common::RunExecutableInNewConsole(rGaeaExecutable, commandLine);

	if (result.iExitCode != 0)
	{
		throw std::runtime_error(std::format("Gaea.Swarm.exe exited with code {} for \"{}\". Use /gaea2-diagnose to examine the log file for failures.", result.iExitCode, rRouteDirectory.string()));
	}

	for (const char* pcFile : kpcIntermediateFiles)
	{
		if (!std::filesystem::exists(rStagingDirectory / pcFile))
		{
			throw std::runtime_error(std::format("Gaea bake for \"{}\" did not produce \"{}\". Verify the archetype graph has an Export node named \"{}\" writing to Build Folder, and that the Elevation Export node uses FloatRaw32, AmbientOcclusion uses UshortRaw16, Color uses PNG8, and Normals uses Exr.", rRouteDirectory.string(), pcFile, std::filesystem::path(pcFile).stem().string()));
		}
	}

}

// Read raw elevation and convert to engine-meters: pixel_m = (pixel_normalized -
// fSeaLevelNormalized) × elevationMeters. Beach = 0, ocean negative (to the sea floor), land
// positive. A non-finite or outside-[0, 1] raw sample is rejected (R32_SFLOAT is unbounded, so
// such a pixel is a broken bake, not data to salvage; it would poison the elevation G-buffer and
// vertex displacement). The full-res buffer feeds every chunk's ProcessBakedRegion crop; the raw
// Elevation.r32 stays on disk as the bake source.
static std::vector<float> LoadElevationMeters(const std::filesystem::path& rIntermediatesDirectory, int64_t iTexturePixels, float fSeaLevelNormalized, float fElevationMeters)
{
	std::filesystem::path elevationFile = rIntermediatesDirectory / "Elevation.r32";
	size_t uiPixelCount = CheckedTexturePixelCount(elevationFile, iTexturePixels);
	if (uiPixelCount > std::numeric_limits<size_t>::max() / sizeof(float))
	{
		throw std::runtime_error(std::format("Gaea output \"{}\" has an elevation extent that overflows size_t ({}x{} float32).", elevationFile.string(), iTexturePixels, iTexturePixels));
	}
	size_t uiExpectedBytes = uiPixelCount * sizeof(float);
	if (uiExpectedBytes > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max()))
	{
		throw std::runtime_error(std::format("Gaea output \"{}\" has an elevation extent too large for stream reads ({}x{} float32).", elevationFile.string(), iTexturePixels, iTexturePixels));
	}
	std::ifstream readStream(elevationFile, std::ios::binary);
	if (!readStream)
	{
		throw std::runtime_error(std::format("Failed to open Gaea elevation output \"{}\".", elevationFile.string()));
	}
	uintmax_t uiActualBytes = std::filesystem::file_size(elevationFile);
	if (uiActualBytes != static_cast<uintmax_t>(uiExpectedBytes))
	{
		throw std::runtime_error(std::format("Gaea produced \"{}\" at {} bytes; expected {} bytes ({}x{} float32). Verify the archetype's Elevation Export node uses FloatRaw32 format and is unconstrained by an internal resolution override.", elevationFile.string(), uiActualBytes, uiExpectedBytes, iTexturePixels, iTexturePixels));
	}
	std::vector<float> fullElevationMeters(uiPixelCount);
	if (!readStream.read(reinterpret_cast<char*>(fullElevationMeters.data()), static_cast<std::streamsize>(uiExpectedBytes)) || readStream.gcount() != static_cast<std::streamsize>(uiExpectedBytes))
	{
		throw std::runtime_error(std::format("Failed to read complete Gaea elevation output \"{}\".", elevationFile.string()));
	}
	for (int64_t i = 0; float& fRaw : fullElevationMeters)
	{
		if (!std::isfinite(fRaw) || fRaw < 0.0f || fRaw > 1.0f)
		{
			throw std::runtime_error(std::format("Gaea produced \"{}\" with elevation pixel {} at {}, outside the normalized [0, 1] range. Verify the archetype's Elevation Export node uses FloatRaw32 format and that the graph feeding it is clamped to [0, 1].", elevationFile.string(), i, fRaw));
		}
		fRaw = (fRaw - fSeaLevelNormalized) * fElevationMeters;
		++i;
	}
	return fullElevationMeters;
}

// Read raw full-resolution AmbientOcclusion (cropped per chunk in ProcessBakedRegion).
static std::vector<uint16_t> LoadAmbientOcclusion(const std::filesystem::path& rIntermediatesDirectory, int64_t iTexturePixels)
{
	std::filesystem::path ambientOcclusionFile = rIntermediatesDirectory / "AmbientOcclusion.r16";
	size_t uiPixelCount = CheckedTexturePixelCount(ambientOcclusionFile, iTexturePixels);
	std::vector<uint16_t> fullAmbientOcclusion;
	if (uiPixelCount > std::numeric_limits<size_t>::max() / sizeof(uint16_t))
	{
		throw std::runtime_error(std::format("Gaea output \"{}\" has an ambient-occlusion extent that overflows size_t ({}x{} uint16).", ambientOcclusionFile.string(), iTexturePixels, iTexturePixels));
	}
	size_t uiExpectedAmbientOcclusionBytes = uiPixelCount * sizeof(uint16_t);
	if (uiExpectedAmbientOcclusionBytes > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max()))
	{
		throw std::runtime_error(std::format("Gaea output \"{}\" has an ambient-occlusion extent too large for stream reads ({}x{} uint16).", ambientOcclusionFile.string(), iTexturePixels, iTexturePixels));
	}
	std::ifstream readStream(ambientOcclusionFile, std::ios::binary);
	if (!readStream)
	{
		throw std::runtime_error(std::format("Failed to open Gaea ambient-occlusion output \"{}\".", ambientOcclusionFile.string()));
	}
	uintmax_t uiActualAmbientOcclusionBytes = std::filesystem::file_size(ambientOcclusionFile);
	if (uiActualAmbientOcclusionBytes != static_cast<uintmax_t>(uiExpectedAmbientOcclusionBytes))
	{
		throw std::runtime_error(std::format("Gaea produced \"{}\" at {} bytes; expected {} bytes ({}x{} uint16). Verify the archetype's AmbientOcclusion Export node uses UshortRaw16 format.", ambientOcclusionFile.string(), uiActualAmbientOcclusionBytes, uiExpectedAmbientOcclusionBytes, iTexturePixels, iTexturePixels));
	}
	fullAmbientOcclusion.resize(uiPixelCount);
	if (!readStream.read(reinterpret_cast<char*>(fullAmbientOcclusion.data()), static_cast<std::streamsize>(uiExpectedAmbientOcclusionBytes)) || readStream.gcount() != static_cast<std::streamsize>(uiExpectedAmbientOcclusionBytes))
	{
		throw std::runtime_error(std::format("Failed to read complete Gaea ambient-occlusion output \"{}\".", ambientOcclusionFile.string()));
	}
	readStream.close();
	return fullAmbientOcclusion;
}

// Parse Mesher's separate Mesh.gltf/Mesh.bin into island-local meter float3 positions and uint32
// indices. Map glTF (east,height,south) to engine (X,-Z,Y): determinant +1 preserves handedness/CCW,
// engine Y points north and Z up. Subtract the beach offset for sea level zero; discard TEXCOORD_0
// because runtime derives visible-area UVs from world XY. Subdivide the full mesh once;
// ProcessBakedRegion reuses it for each crop, recenter, and write.
static void LoadMesherMesh(const std::filesystem::path& rIntermediatesDirectory, float fBeachOffsetMeters, const std::filesystem::path& rRouteDirectory, std::vector<float>& rMeshPositions, std::vector<uint32_t>& rMeshIndices)
{
	std::filesystem::path meshGltfFile = rIntermediatesDirectory / "Mesh.gltf";
	tinygltf::Model gltfModel;
	std::string error;
	std::string warning;
	tinygltf::TinyGLTF gltfContext;
	bool bLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, meshGltfFile.string());
	if (!bLoaded)
	{
		throw std::runtime_error(std::format("Failed to parse Gaea Mesher output \"{}\": {} (warning: {}). Verify the archetype has a Mesher node with Format=GLTF and that Gaea wrote both Mesh.gltf and Mesh.bin.", meshGltfFile.string(), error, warning));
	}
	if (gltfModel.meshes.empty() || gltfModel.meshes.at(0).primitives.empty())
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" contains no mesh primitives.", meshGltfFile.string()));
	}

	const tinygltf::Primitive& rPrimitive = gltfModel.meshes.at(0).primitives.at(0);
	auto positionIt = rPrimitive.attributes.find("POSITION");
	if (positionIt == rPrimitive.attributes.end() || rPrimitive.indices < 0)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" primitive missing POSITION attribute or indices accessor.", meshGltfFile.string()));
	}
	if (rPrimitive.mode != TINYGLTF_MODE_TRIANGLES)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" primitive mode is {}, not triangles ({}). Set the Mesher node's topology to triangles.", meshGltfFile.string(), rPrimitive.mode, TINYGLTF_MODE_TRIANGLES));
	}

	const tinygltf::Accessor& rPositionAccessor = gltfModel.accessors.at(static_cast<size_t>(positionIt->second));
	const tinygltf::BufferView& rPositionView = gltfModel.bufferViews.at(static_cast<size_t>(rPositionAccessor.bufferView));
	const tinygltf::Buffer& rPositionBuffer = gltfModel.buffers.at(static_cast<size_t>(rPositionView.buffer));
	if (rPositionAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || rPositionAccessor.type != TINYGLTF_TYPE_VEC3)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" POSITION accessor is not float3.", meshGltfFile.string()));
	}

	// Every span check below is written as a subtraction/division against the container size rather
	// than an addition compared to it, so no size_t sum can wrap past the limit it is tested against.
	static constexpr size_t kuiPositionBytes = sizeof(float) * 3;
	if (rPositionView.byteOffset > rPositionBuffer.data.size() || rPositionView.byteLength > rPositionBuffer.data.size() - rPositionView.byteOffset)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" POSITION buffer view (byteOffset {}, byteLength {}) does not fit its {}-byte buffer.", meshGltfFile.string(), rPositionView.byteOffset, rPositionView.byteLength, rPositionBuffer.data.size()));
	}
	if (rPositionAccessor.count < 1)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" POSITION accessor has no vertices (count {}).", meshGltfFile.string(), rPositionAccessor.count));
	}
	if (rPositionAccessor.byteOffset > rPositionView.byteLength || kuiPositionBytes > rPositionView.byteLength - rPositionAccessor.byteOffset)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" POSITION accessor byteOffset {} leaves no room for a float3 in its {}-byte buffer view.", meshGltfFile.string(), rPositionAccessor.byteOffset, rPositionView.byteLength));
	}
	// ByteStride() is never -1 here: the float3 check above passed and tinygltf rejects a byteStride
	// that is not a multiple of 4 while parsing.
	size_t uiStride = static_cast<size_t>(rPositionAccessor.ByteStride(rPositionView));
	if (rPositionAccessor.count - 1 > (rPositionView.byteLength - rPositionAccessor.byteOffset - kuiPositionBytes) / uiStride)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" POSITION accessor count {} at stride {} runs past its buffer view (byteOffset {}, byteLength {}).", meshGltfFile.string(), rPositionAccessor.count, uiStride, rPositionAccessor.byteOffset, rPositionView.byteLength));
	}
	if ((rPositionView.byteOffset + rPositionAccessor.byteOffset) % alignof(float) != 0)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" POSITION data starts at byte {}, which is not float-aligned.", meshGltfFile.string(), rPositionView.byteOffset + rPositionAccessor.byteOffset));
	}

	int64_t iVertexCount = static_cast<int64_t>(rPositionAccessor.count);
	rMeshPositions.resize(static_cast<size_t>(iVertexCount) * 3);
	{
		const std::byte* pSource = reinterpret_cast<const std::byte*>(rPositionBuffer.data.data()) + rPositionView.byteOffset + rPositionAccessor.byteOffset;
		for (int64_t i = 0; i < iVertexCount; ++i)
		{
			const float* pfXyz = reinterpret_cast<const float*>(pSource + static_cast<size_t>(i) * uiStride);
			if (!std::isfinite(pfXyz[0]) || !std::isfinite(pfXyz[1]) || !std::isfinite(pfXyz[2]))
			{
				throw std::runtime_error(std::format("Gaea Mesher output \"{}\" vertex {} has a non-finite position ({}, {}, {}).", meshGltfFile.string(), i, pfXyz[0], pfXyz[1], pfXyz[2]));
			}
			float fX = pfXyz[0];
			float fY = -pfXyz[2];
			float fZ = pfXyz[1] - fBeachOffsetMeters;
			rMeshPositions.at(static_cast<size_t>(i) * 3 + 0) = fX;
			rMeshPositions.at(static_cast<size_t>(i) * 3 + 1) = fY;
			rMeshPositions.at(static_cast<size_t>(i) * 3 + 2) = fZ;
		}
	}

	const tinygltf::Accessor& rIndexAccessor = gltfModel.accessors.at(static_cast<size_t>(rPrimitive.indices));
	const tinygltf::BufferView& rIndexView = gltfModel.bufferViews.at(static_cast<size_t>(rIndexAccessor.bufferView));
	const tinygltf::Buffer& rIndexBuffer = gltfModel.buffers.at(static_cast<size_t>(rIndexView.buffer));
	int64_t iIndexComponentBytes = 0;
	if (rIndexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
	{
		iIndexComponentBytes = sizeof(uint32_t);
	}
	else if (rIndexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
	{
		iIndexComponentBytes = sizeof(uint16_t);
	}
	else if (rIndexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
	{
		iIndexComponentBytes = sizeof(uint8_t);
	}
	else
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" indices accessor has unsupported componentType {}.", meshGltfFile.string(), rIndexAccessor.componentType));
	}
	if (rIndexView.byteOffset > rIndexBuffer.data.size() || rIndexView.byteLength > rIndexBuffer.data.size() - rIndexView.byteOffset)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" indices buffer view (byteOffset {}, byteLength {}) does not fit its {}-byte buffer.", meshGltfFile.string(), rIndexView.byteOffset, rIndexView.byteLength, rIndexBuffer.data.size()));
	}
	if (rIndexAccessor.byteOffset > rIndexView.byteLength || rIndexAccessor.count > (rIndexView.byteLength - rIndexAccessor.byteOffset) / iIndexComponentBytes)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" indices accessor (byteOffset {}, count {}, {} byte(s) per index) runs past its buffer view (byteLength {}).", meshGltfFile.string(), rIndexAccessor.byteOffset, rIndexAccessor.count, iIndexComponentBytes, rIndexView.byteLength));
	}
	if (rIndexAccessor.count % 3 != 0)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" indices accessor count {} is not a multiple of three.", meshGltfFile.string(), rIndexAccessor.count));
	}
	if ((rIndexView.byteOffset + rIndexAccessor.byteOffset) % iIndexComponentBytes != 0)
	{
		throw std::runtime_error(std::format("Gaea Mesher output \"{}\" indices start at byte {}, which is not aligned to the {}-byte index component.", meshGltfFile.string(), rIndexView.byteOffset + rIndexAccessor.byteOffset, iIndexComponentBytes));
	}

	int64_t iIndexCount = static_cast<int64_t>(rIndexAccessor.count);
	rMeshIndices.resize(static_cast<size_t>(iIndexCount));
	{
		const std::byte* pSource = reinterpret_cast<const std::byte*>(rIndexBuffer.data.data()) + rIndexView.byteOffset + rIndexAccessor.byteOffset;
		switch (rIndexAccessor.componentType)
		{
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				std::memcpy(rMeshIndices.data(), pSource, static_cast<size_t>(iIndexCount) * sizeof(uint32_t));
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			{
				const uint16_t* puiSource = reinterpret_cast<const uint16_t*>(pSource);
				for (int64_t i = 0; i < iIndexCount; ++i)
				{
					rMeshIndices.at(static_cast<size_t>(i)) = puiSource[i];
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			{
				const uint8_t* puiSource = reinterpret_cast<const uint8_t*>(pSource);
				for (int64_t i = 0; i < iIndexCount; ++i)
				{
					rMeshIndices.at(static_cast<size_t>(i)) = puiSource[i];
				}
				break;
			}
		}
	}

	for (int64_t i = 0; int64_t iIndex : rMeshIndices)
	{
		if (iIndex >= static_cast<uint32_t>(iVertexCount))
		{
			throw std::runtime_error(std::format("Gaea Mesher output \"{}\" index {} references vertex {}, past the {} vertices in the mesh.", meshGltfFile.string(), i, iIndex, iVertexCount));
		}
		++i;
	}

	int64_t iInitialVertexCount = iVertexCount;
	int64_t iInitialTriangleCount = iIndexCount / 3;

	// Beach-band subdivision gives shore silhouettes and material blends enough vertices. In-band
	// triangles split 1-to-4 to the XY edge limit; out-of-band neighbors absorb existing midpoints with
	// minimal 1-to-2/3/4 splits, adding no midpoints beyond one ring. Interpolated midpoint Z stays within
	// the parent's range and serves only the band test; Terrain.vert derives rendered Z from the
	// heightmap. Subdivide once over the full mesh and reuse it for chunk crops.
	SubdivisionConfig subdivisionConfig
	{
		.fBandMinimumMeters = kfBeachSubdivisionMinMeters,
		.fBandMaximumMeters = kfBeachSubdivisionMaxMeters,
		.fMaximumEdgeMeters = kfBeachSubdivisionMaxEdgeMeters,
		.iMaximumDepth = kiBeachSubdivisionMaxDepth,
	};
	int64_t iDepthCapHits = 0;
	SubdivideBeachBand(rMeshPositions, rMeshIndices, subdivisionConfig, iDepthCapHits);
	iVertexCount = std::ssize(rMeshPositions) / 3;
	iIndexCount = std::ssize(rMeshIndices);

	if (iDepthCapHits > 0)
	{
		LOG(kDefault, kWarning, "Mesh \"{}\": beach subdivision hit depth cap ({}) on {} triangle(s); largest input triangles may still exceed {:.2f}m edge target AND the mesh may contain T-junction cracks where capped absorption-needing triangles were skipped (raise kiBeachSubdivisionMaxDepth or split it into separate in-band / absorption caps if observed)", rRouteDirectory.string(), kiBeachSubdivisionMaxDepth, iDepthCapHits, kfBeachSubdivisionMaxEdgeMeters);
	}
	LOG(kDefault, kDebug, "Mesh \"{}\": {} -> {} vertices, {} -> {} triangles after beach subdivision (band Z=[{:.2f}, {:.2f}]m, edge target {:.2f}m)", rRouteDirectory.string(), iInitialVertexCount, iVertexCount, iInitialTriangleCount, iIndexCount / 3, subdivisionConfig.fBandMinimumMeters, subdivisionConfig.fBandMaximumMeters, kfBeachSubdivisionMaxEdgeMeters);
}


// Chunks below kfMinIslandMaxHeightMeters are rejected, so leaf indices can be sparse.
void BakeRoute(const IslandBakeContext& rContext, const RouteSubdivision& rRoute)
{
	std::filesystem::path routeDirectory = rContext.rIslandFolder / rRoute.pcLabel;
	std::filesystem::path intermediatesDirectory = rContext.rCacheIslandFolder / rRoute.pcLabel;
	int64_t iLeafCount = rRoute.iColumns * rRoute.iRows;
	std::string bakeFingerprint = BakeFingerprint(rContext, rRoute);
	std::string splitFingerprint = SplitFingerprint(rRoute);

	// Sweep leaf folders orphaned by a route leaf-count shrink before anything else: this must run even
	// on the otherwise-clean early-return path, since a kRouteSubdivisions edit need not bump kiSplitVersion.
	RemoveOrphanedLeafFolders(routeDirectory, iLeafCount);
	RemoveOrphanedLeafFolders(intermediatesDirectory, iLeafCount);
	RemoveOrphanedLeafFolders(GetIslandDiagnosticsPath(routeDirectory), iLeafCount);

	bool bGaeaDirty = IsGaeaRawDirty(intermediatesDirectory, bakeFingerprint);
	bool bLeavesDirty = AreLeavesDirty(routeDirectory, intermediatesDirectory, iLeafCount, splitFingerprint);
	if (!bGaeaDirty && !bLeavesDirty)
	{
		return;
	}

	// Resolve Gaea only for a route whose raw bake is dirty, and ahead of the dirty-stage mutations
	// below, so a missing executable leaves this route's version markers and staging untouched.
	std::filesystem::path gaeaExecutable;
	if (bGaeaDirty)
	{
		gaeaExecutable = ResolveGaeaExecutable();
	}

	std::filesystem::create_directories(intermediatesDirectory);
	// Invalidate split completion before either stage mutates its inputs. A crash after a raw re-bake
	// or midway through overwriting existing leaves must force the split to run again next launch.
	std::filesystem::remove(intermediatesDirectory / kpcSplitVersionFile);
	std::filesystem::path patchedArchetypeFile = intermediatesDirectory / kpcPatchedArchetypeFile;

	// STAGE 1 — Gaea raw export (slow). Skipped when the raw outputs are already present and fresh,
	// so a split-only change re-splits the existing bake without re-running Gaea.
	if (bGaeaDirty)
	{
		// Keep the last complete raw bake available while Gaea runs. Only after every staged output
		// exists do we invalidate completion and publish the new set. A failed Gaea invocation leaves
		// GaeaStaging for diagnosis; the next attempt replaces it before running.
		std::filesystem::path stagingDirectory = intermediatesDirectory / kpcGaeaStagingDirectory;
		std::filesystem::remove_all(stagingDirectory);
		std::filesystem::create_directories(stagingDirectory);
		std::filesystem::path stagingPatchedArchetypeFile = stagingDirectory / kpcPatchedArchetypeFile;
		RunGaeaExport(rContext, rRoute, gaeaExecutable, routeDirectory, stagingDirectory, stagingPatchedArchetypeFile);

		std::filesystem::remove(intermediatesDirectory / kpcBakeVersionFile);
		std::filesystem::remove(intermediatesDirectory / kpcSplitVersionFile);
		for (const char* pcFile : kpcIntermediateFiles)
		{
			std::filesystem::path source = stagingDirectory / pcFile;
			std::filesystem::path destination = intermediatesDirectory / pcFile;
			VERIFY_SUCCESS(MoveFileExW(source.native().c_str(), destination.native().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
		}
		VERIFY_SUCCESS(MoveFileExW(stagingPatchedArchetypeFile.native().c_str(), patchedArchetypeFile.native().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
		std::filesystem::remove_all(stagingDirectory);
		WriteTextFile(intermediatesDirectory / kpcBakeVersionFile, bakeFingerprint);
	}
	else
	{
		LOG(kDefault, kDebug, "Reusing Gaea bake for route \"{}\"; re-splitting only (no Gaea export)", routeDirectory.string());
	}

	// STAGE 2 — split (fast). Raw outputs are present here (just baked, or reused fresh).
	// PatchArchetype doesn't touch the Sea node, so the patched copy keeps the authored Level (or
	// kfGaeaSeaLevelDefault); the per-island beach offset drives the elevation transform, auto-crop
	// cut line, and mesh-Z offset.
	float fSeaLevelNormalized = ReadArchetypeSeaLevel(patchedArchetypeFile);
	float fBeachOffsetMeters = fSeaLevelNormalized * rContext.rDimensions.fElevationMeters;
	LOG(kDefault, kDebug, "Archetype Sea Level (read, not patched): {} → beach offset {} m for elevationMeters {} m", common::Wb(fSeaLevelNormalized, 4), common::Wb(fBeachOffsetMeters, 2), common::Wb(rContext.rDimensions.fElevationMeters, 2));

	// Auto-crop cut line = -fBeachOffsetMeters + kfCropEpsilonAboveSeaFloorMeters. If the beach
	// offset is at or below the epsilon, the cut line lands at or above sea level and silently
	// strips the entire shoreline halo. Catch the authoring mistake here rather than shipping a
	// halo-less island.
	if (fBeachOffsetMeters <= kfCropEpsilonAboveSeaFloorMeters)
	{
		throw std::runtime_error(std::format("Island \"{}\" beach offset ({:.2f} m, = Sea Level {:.4f} × elevationMeters {:.2f} m) is at or below the crop epsilon ({:.2f} m): the auto-crop would land at or above sea level and strip the shoreline halo. Raise elevationMeters, raise the archetype Sea node's Level, or lower kfCropEpsilonAboveSeaFloorMeters.", rContext.rIslandFolder.string(), fBeachOffsetMeters, fSeaLevelNormalized, rContext.rDimensions.fElevationMeters, kfCropEpsilonAboveSeaFloorMeters));
	}

	// Per-island sea floor must match the engine-wide kfSeaBottomMeters so the elevation RTT clear
	// (Engine/Source/Graphics/Managers/RenderTargetTextures.cpp) blends seamlessly with edge texels.
	ASSERT(std::abs(-fBeachOffsetMeters - common::kfSeaBottomMeters) < 0.01f);

	std::vector<float> fullElevationMeters = LoadElevationMeters(intermediatesDirectory, rContext.iTexturePixels, fSeaLevelNormalized, rContext.rDimensions.fElevationMeters);

	std::vector<uint16_t> fullAmbientOcclusion = LoadAmbientOcclusion(intermediatesDirectory, rContext.iTexturePixels);

	std::vector<float> meshPositions;
	std::vector<uint32_t> meshIndices;
	LoadMesherMesh(intermediatesDirectory, fBeachOffsetMeters, routeDirectory, meshPositions, meshIndices);

	// Split into chunks (up to 1 for 1x1, iColumns × iRows otherwise) and write each leaf that clears
	// the minimum-height threshold (ProcessBakedRegion rejects too-low / underwater chunks). The X / Y
	// region boundaries partition the full texture (uneven when columns/rows don't divide it evenly);
	// ProcessBakedRegion auto-crops within each, borrowing neighbour pixels across a seam for alignment.
	BakeOutput bakeOutput {.rFullElevationMeters = fullElevationMeters, .rFullAmbientOcclusion = fullAmbientOcclusion, .fBeachOffsetMeters = fBeachOffsetMeters};
	int64_t iWrittenLeaves = 0;
	for (int64_t i = 0; i < rRoute.iColumns; ++i)
	{
		for (int64_t j = 0; j < rRoute.iRows; ++j)
		{
			int64_t iChunkIndex = i * rRoute.iRows + j;
			RegionBounds region
			{
				.iStartX = i * rContext.iTexturePixels / rRoute.iColumns,
				.iEndX = (i + 1) * rContext.iTexturePixels / rRoute.iColumns,
				.iStartY = j * rContext.iTexturePixels / rRoute.iRows,
				.iEndY = (j + 1) * rContext.iTexturePixels / rRoute.iRows,
			};
			std::filesystem::path sourceLeafDirectory = routeDirectory / std::to_string(iChunkIndex);
			std::filesystem::path cacheLeafDirectory = intermediatesDirectory / std::to_string(iChunkIndex);
			LeafTarget leaf {.rSourceLeafDirectory = sourceLeafDirectory, .rCacheLeafDirectory = cacheLeafDirectory};
			if (ProcessBakedRegion(rContext, bakeOutput, region, meshPositions, meshIndices, leaf))
			{
				++iWrittenLeaves;
			}
		}
	}

	if (iWrittenLeaves == 0)
	{
		throw std::runtime_error(std::format("Island route \"{}\": every one of {} chunk(s) was rejected as too low (peak < {:.2f} m). Raise Island.json's elevationMeters, lower kfMinIslandMaxHeightMeters, or remove this route from Island.json.", routeDirectory.string(), iLeafCount, kfMinIslandMaxHeightMeters));
	}

	// Stamp the split sentinel last, after every leaf is written. A crash mid-split leaves it absent
	// (or stale), so AreLeavesDirty re-splits next run — without re-running Gaea (BakeVersion is
	// already stamped above, so IsGaeaRawDirty stays clean).
	{
		WriteTextFile(intermediatesDirectory / kpcSplitVersionFile, splitFingerprint);
	}

	LOG(kDefault, kDebug, "Island route \"{}\" ready ({} of {} chunk(s) written, {} rejected as too low{})", routeDirectory.string(), iWrittenLeaves, iLeafCount, iLeafCount - iWrittenLeaves, bGaeaDirty ? ", Gaea re-baked" : ", split-only reuse");
}
