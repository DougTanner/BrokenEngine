#include "ExportScene.h"

#include "Scene/AssimpSceneDecoder.h"
#include "Scene/GltfSceneDecoder.h"
#include "Texture/Texture.h"
#include "BlenderExport.h"
#include "FileManager.h"
#include "SourceReadValidation.h"

static bool IsAssimpScene(const std::filesystem::path& rPath)
{
	std::string extension = rPath.extension().string();
	return extension == ".fbx" || extension == ".obj" || extension == ".dae" || extension == ".ply" || extension == ".stl" || extension == ".3ds";
}

ExportScene::ExportScene(common::ChunkFlags_t rChunkFlags, const std::filesystem::path& rFile)
: ExportJob(rChunkFlags, rFile.extension() == ".blend" ? std::filesystem::path(rFile).replace_extension(".glb") : rFile, kiVersion)
, mBlenderSourcePath(rFile.extension() == ".blend" ? rFile : std::filesystem::path())
{
}

std::optional<common::ChunkFlags_t> ExportScene::Handles(const std::filesystem::directory_entry& rDirectoryEntry)
{
	if (!IsAssimpScene(rDirectoryEntry.path()) && rDirectoryEntry.path().extension() != ".gltf" && rDirectoryEntry.path().extension() != ".glb" && rDirectoryEntry.path().extension() != ".blend")
	{
		return std::nullopt;
	}
	// Skip glTF files inside any Intermediates/ directory: those are bake artifacts (e.g., the
	// Gaea Mesher's Mesh.gltf alongside Mesh.bin) consumed by ExportIsland, not standalone scenes.
	for (const std::filesystem::path& rPart : rDirectoryEntry.path())
	{
		if (rPart == "Intermediates")
		{
			return std::nullopt;
		}
	}
	if (rDirectoryEntry.path().extension() == ".glb" && std::filesystem::is_regular_file(std::filesystem::path(rDirectoryEntry.path()).replace_extension(".blend")))
	{
		return std::nullopt;
	}
	return std::optional<common::ChunkFlags_t>(common::ChunkFlags::kScene);
}

bool ExportScene::CheckDirty(const std::filesystem::path& rPackFile)
{
	if (!mBlenderSourcePath.empty() && BlenderExportDirty(mBlenderSourcePath, mInputPath, ReadMarkerFile(GetBlenderExportMarkerPath())))
	{
		mbDirty = true;
		mbNeedsPreExport = true;
		return true;
	}

	bool bDirty = ExportJob::CheckDirty(rPackFile);
	if (IsAssimpScene(mInputPath) && SceneDependenciesDirty())
	{
		mbDirty = true;
		bDirty = true;
	}

	if (bDirty)
	{
		// The pre-export flag defers marker deletion until Export, immediately before generated outputs are written.
		LOG(kDefault, kDebug, "Main file dirty, forcing pre-export");
		mbNeedsPreExport = true;
	}
	else
	{
		// Main file is clean, but the generated .MODEL and texture intermediates are tracked in the checkout, so a
		// deleted, reverted, or edited one leaves the chunk cache clean while the scene's published path CRCs name
		// files that no longer exist or no longer match. The marker records exactly those outputs, so any difference
		// - including a missing or unreadable marker - re-runs pre-export before the scene is published again.
		std::optional<std::string> optionalStoredFingerprint = ReadMarkerFile(GetPreExportMarkerPath());
		std::optional<std::string> optionalCurrentFingerprint = GetPreExportFingerprint();
		if (!optionalStoredFingerprint.has_value() || !optionalCurrentFingerprint.has_value() || optionalStoredFingerprint.value() != optionalCurrentFingerprint.value())
		{
			LOG(kDefault, kDebug, "Pre-export outputs stale for \"{}\", forcing pre-export", mInputPath.string());
			mbDirty = true;
			bDirty = true;
			mbNeedsPreExport = true;
		}
	}

	return bDirty;
}


constexpr const char* kpcContext = "ExportScene";

// Collects the glTF's external file references (images[].uri and buffers[].uri) by reading the JSON directly:
// going through tinygltf would re-read every .bin and stb-decode every texture on each dirty check. Reading
// "uri" is exact, because tinygltf only treats it as an external file here too - bufferView-backed and
// data-URI images leave the loaded image's uri empty.
static std::vector<std::string> ReadExternalUris(const std::filesystem::path& rGltfPath)
{
	std::vector<std::string> uris;

	// Non-throwing parse: CheckDirty runs outside the per-asset try/catch, so a throw here would abort the
	// whole run instead of failing one asset. An unreadable or malformed scene contributes no dependencies
	// and still fails per-asset later through Export().
	nlohmann::json gltfJson;
	if (rGltfPath.extension() == ".glb")
	{
		std::optional<std::string> optionalJson = ReadGlbJson(rGltfPath);
		if (!optionalJson.has_value())
		{
			return uris;
		}
		gltfJson = nlohmann::json::parse(optionalJson.value(), nullptr, false);
	}
	else
	{
		std::fstream fileStreamIn(rGltfPath, std::ios::in | std::ios::binary);
		gltfJson = nlohmann::json::parse(fileStreamIn, nullptr, false);
	}
	if (gltfJson.is_discarded())
	{
		return uris;
	}

	for (const char* pcArrayName : {"images", "buffers"})
	{
		auto arrayIterator = gltfJson.find(pcArrayName);
		if (arrayIterator == gltfJson.end() || !arrayIterator->is_array())
		{
			continue;
		}

		for (const nlohmann::json& rElement : *arrayIterator)
		{
			if (!rElement.is_object())
			{
				continue;
			}

			auto uriIterator = rElement.find("uri");
			if (uriIterator == rElement.end() || !uriIterator->is_string())
			{
				continue;
			}

			const std::string& rUri = uriIterator->get_ref<const std::string&>();
			if (!rUri.empty() && !tinygltf::IsDataURI(rUri))
			{
				uris.push_back(rUri);
			}
		}
	}

	return uris;
}

// A generated scene texture intermediate is a "<scene>.Texture<imageIndex>.<BCn>" sibling of the source glTF.
// One definition serves both the orphan sweep and the pre-export marker's output membership, so what the sweep
// leaves on disk and what the marker records can never disagree.
static bool IsSceneTextureIntermediate(std::string_view name, std::string_view intermediatePrefix)
{
	if (!name.starts_with(intermediatePrefix))
	{
		return false;
	}

	std::string_view indexAndSuffix(name);
	indexAndSuffix.remove_prefix(intermediatePrefix.size());
	int64_t iSuffixStart = static_cast<int64_t>(indexAndSuffix.find('.'));
	if (iSuffixStart == -1)
	{
		return false;
	}
	if (iSuffixStart == 0)
	{
		return false;
	}
	if (std::ranges::any_of(indexAndSuffix.substr(0, static_cast<size_t>(iSuffixStart)), [](const char& rcCharacter)
	{
		return rcCharacter < '0' || rcCharacter > '9';
	}))
	{
		return false;
	}

	std::string_view suffix = indexAndSuffix.substr(static_cast<size_t>(iSuffixStart));
	return suffix == TextureIntermediateSuffix(VK_FORMAT_BC4_UNORM_BLOCK) || suffix == TextureIntermediateSuffix(VK_FORMAT_BC5_UNORM_BLOCK) || suffix == TextureIntermediateSuffix(VK_FORMAT_BC7_UNORM_BLOCK);
}


std::string ExportScene::GetInputFingerprint() const
{
	if (IsAssimpScene(mInputPath))
	{
		return ExportJob::GetInputFingerprint();
	}
	if (!mBlenderSourcePath.empty())
	{
		// RunExport snapshots this before Export replaces the generated GLB.
		return gpFileManager->mpInputFingerprintCache->Get(mBlenderSourcePath);
	}

	nlohmann::json fingerprint;
	fingerprint["gltf"] = gpFileManager->mpInputFingerprintCache->Get(mInputPath);
	for (const std::string& rUri : ReadExternalUris(mInputPath))
	{
		std::string decodedUri;
		tinygltf::URIDecode(rUri, &decodedUri, nullptr);
		std::filesystem::path dependencyPath = mInputPath.parent_path() / decodedUri;
		// A missing dependency records null rather than looking up the fingerprint cache, which throws on a missing file
		// and would kill the run from CheckDirty. Null never equals a stored hash, so the scene stays dirty and
		// the failure surfaces per-asset in Export().
		fingerprint["uris"][rUri] = std::filesystem::exists(dependencyPath) ? nlohmann::json(gpFileManager->mpInputFingerprintCache->Get(dependencyPath)) : nlohmann::json();
	}
	return fingerprint.dump();
}

std::filesystem::path ExportScene::GetSceneDependencyMarkerPath() const
{
	std::filesystem::path path = mCacheMetadataFile;
	path += ".scene-dependencies";
	return path;
}

bool ExportScene::SceneDependenciesDirty() const
{
	std::optional<std::string> optionalMarker = ReadMarkerFile(GetSceneDependencyMarkerPath());
	if (!optionalMarker.has_value())
	{
		return true;
	}
	nlohmann::json metadata = nlohmann::json::parse(optionalMarker.value(), nullptr, false);
	if (!metadata.is_object())
	{
		return true;
	}
	if (metadata.value("version", nlohmann::json()) != 1)
	{
		return true;
	}
	if (!metadata.contains("dependencies"))
	{
		return true;
	}
	if (!metadata["dependencies"].is_array())
	{
		return true;
	}
	try
	{
		for (const nlohmann::json& rDependency : metadata["dependencies"])
		{
			if (!rDependency.is_object())
			{
				return true;
			}
			if (!rDependency.contains("root"))
			{
				return true;
			}
			if (!rDependency["root"].is_number_integer())
			{
				return true;
			}
			if (!rDependency.contains("path"))
			{
				return true;
			}
			if (!rDependency["path"].is_string())
			{
				return true;
			}
			if (!rDependency.contains("hash"))
			{
				return true;
			}
			if (!rDependency["hash"].is_string())
			{
				return true;
			}
			int64_t iRoot = rDependency["root"].get<int64_t>();
			std::filesystem::path relativePath(rDependency["path"].get<std::string>());
			if (iRoot < 0 || iRoot >= std::ssize(gpFileManager->mpInputDirectories))
			{
				return true;
			}
			if (relativePath.empty())
			{
				return true;
			}
			if (relativePath.is_absolute())
			{
				return true;
			}
			if (std::ranges::contains(relativePath, std::filesystem::path("..")))
			{
				return true;
			}
			std::filesystem::path dependencyPath = gpFileManager->mpInputDirectories[iRoot] / relativePath;
			if (!std::filesystem::is_regular_file(dependencyPath))
			{
				return true;
			}
			if (gpFileManager->mpInputFingerprintCache->Get(dependencyPath) != rDependency["hash"].get<std::string>())
			{
				return true;
			}
		}
	}
	catch (const std::exception&)
	{
		return true;
	}
	return false;
}

void ExportScene::CaptureSceneDependencies(const std::vector<std::filesystem::path>& rDependencies)
{
	nlohmann::json metadata;
	metadata["version"] = 1;
	metadata["dependencies"] = nlohmann::json::array();
	for (const std::filesystem::path& rDependency : rDependencies)
	{
		bool bFound = false;
		for (int64_t i = 0; i < std::ssize(gpFileManager->mpInputDirectories); ++i)
		{
			std::filesystem::path relativePath = std::filesystem::relative(rDependency, gpFileManager->mpInputDirectories[i]);
			if (!relativePath.empty() && *relativePath.begin() != "..")
			{
				metadata["dependencies"].push_back({{"root", i,}, {"path", relativePath.generic_string(),}, {"hash", gpFileManager->mpInputFingerprintCache->Get(rDependency),}, });
				bFound = true;
				break;
			}
		}
		ASSERT(bFound);
	}
	mSceneDependencyMetadata = metadata.dump();
}

std::filesystem::path ExportScene::GetBlenderExportMarkerPath() const
{
	std::filesystem::path markerPath = mCacheMetadataFile.parent_path();
	markerPath /= mInputPath.filename();
	markerPath += std::format(".b{}.blender", kiBlenderExportVersion);
	return markerPath;
}

std::filesystem::path ExportScene::GetPreExportMarkerPath() const
{
	std::filesystem::path path(mInputPath);
	path += ".PreExport";
	return path;
}

// The content of every generated output pre-export owns: the .MODEL file and each scene texture intermediate on
// disk. Entries are keyed by bare filename, the same key the orphan sweep compares, so the marker is independent
// of where the checkout lives; nlohmann's object keys sort, so the dumped string is deterministic. Those filenames
// are also the text MainExport builds its texture and model path CRCs from, so a matching marker proves every CRC
// the scene chunk publishes still names a file with the contents it was published for.
// Returns nullopt on any filesystem error so CheckDirty treats an unreadable scene directory as dirty.
std::optional<std::string> ExportScene::GetPreExportFingerprint() const
{
	nlohmann::json fingerprint;
	fingerprint["version"] = miVersion;

	// A missing output records null rather than looking up the fingerprint cache, which throws on a missing file and would
	// kill the run from CheckDirty. Null never equals a stored hash, so the scene stays dirty.
	std::filesystem::path modelPath(mInputPath);
	modelPath += ".MODEL";
	std::error_code modelErrorCode;
	bool bModelExists = std::filesystem::exists(modelPath, modelErrorCode);
	if (modelErrorCode)
	{
		return std::nullopt;
	}
	fingerprint["outputs"][modelPath.filename().string()] = bModelExists ? nlohmann::json(gpFileManager->mpInputFingerprintCache->Get(modelPath)) : nlohmann::json();

	std::string intermediatePrefix = mInputPath.filename().string() + ".Texture";
	std::error_code iterateErrorCode;
	std::filesystem::directory_iterator iterator(mInputPath.parent_path(), iterateErrorCode);
	if (iterateErrorCode)
	{
		return std::nullopt;
	}
	for (; iterator != std::filesystem::directory_iterator(); iterator.increment(iterateErrorCode))
	{
		if (iterateErrorCode)
		{
			return std::nullopt;
		}

		std::string name = iterator->path().filename().string();
		if (!IsSceneTextureIntermediate(name, intermediatePrefix))
		{
			continue;
		}

		std::error_code entryErrorCode;
		bool bRegularFile = iterator->is_regular_file(entryErrorCode);
		if (entryErrorCode)
		{
			return std::nullopt;
		}
		if (!bRegularFile)
		{
			continue;
		}

		// An entry that vanished between the listing and here drops out of membership, which is itself a difference.
		bool bExists = std::filesystem::exists(iterator->path(), entryErrorCode);
		if (entryErrorCode)
		{
			return std::nullopt;
		}
		if (bExists)
		{
			fingerprint["outputs"][name] = gpFileManager->mpInputFingerprintCache->Get(iterator->path());
		}
	}

	return fingerprint.dump();
}

std::filesystem::path ExportScene::GetTextureIntermediatePath(int64_t iTextureIndex, VkFormat vkFormat) const
{
	std::filesystem::path path(mInputPath);
	path += ".Texture";
	path += std::to_string(iTextureIndex);
	path += TextureIntermediateSuffix(vkFormat);
	return path;
}

std::filesystem::path ExportScene::GetTextureIntermediateStagePath(int64_t iStageIndex) const
{
	std::filesystem::path path = mInputPath.parent_path();
	// Keep outer stages out of tag-first texture routing until they are published under the final name.
	path /= L".TextureStage." + std::to_wstring(miId) + L"." + std::to_wstring(iStageIndex) + L".tmp";
	return path;
}

void ExportScene::Export()
{
	if (!mBlenderSourcePath.empty() && BlenderExportDirty(mBlenderSourcePath, mInputPath, ReadMarkerFile(GetBlenderExportMarkerPath())))
	{
		std::string marker = ExportBlenderScene(mBlenderSourcePath, mInputPath, [](const std::filesystem::path& rStagePath)
		{
			GltfSceneDecoder::ValidateSource(rStagePath, true);
		}, [this]()
		{
			std::filesystem::remove(GetBlenderExportMarkerPath());
			std::filesystem::remove(GetPreExportMarkerPath());
		});
		WriteMarkerFile(GetBlenderExportMarkerPath(), marker);
		mbNeedsPreExport = true;
	}

	SceneExportData sceneData;
	if (IsAssimpScene(mInputPath))
	{
		std::filesystem::remove(GetSceneDependencyMarkerPath());
		AssimpSceneDecoder decoder(mInputPath, sceneData);
		CaptureSceneDependencies(decoder.mDependencies);
		if (mbNeedsPreExport)
		{
			std::filesystem::remove(GetPreExportMarkerPath());
			ProcessTextures(sceneData);
			OptimizeMeshes(sceneData.materials, sceneData.vertices);
			WriteModelFile(sceneData.materials, sceneData.materialInfos, sceneData.vertices);
		}
		MainExport(nullptr, &decoder, sceneData);
		return;
	}
	GltfSceneDecoder decoder(mInputPath, mInputPath.extension() == ".glb", sceneData);

	if (mbNeedsPreExport)
	{
		// Model and texture writes are not transactional, and a kill (rather than a throw) mid-export never reaches
		// CleanupOnFailure. Dropping the marker before the first output is touched keeps a half-written set from being
		// vouched for as current by a marker that still matches - the next run would skip pre-export and ship it.
		std::filesystem::remove(GetPreExportMarkerPath());
		PreExport(decoder, sceneData);
	}

	MainExport(&decoder, nullptr, sceneData);
}

void ExportScene::PreExport(const GltfSceneDecoder& rDecoder, SceneExportData& rData)
{
	LOG(kDefault, kDebug, "PreExport Gltf: {}", mInputPath.string());

	ASSERT(std::ssize(rData.textures) <= common::SceneHeader::kiMaxTextures);
	LOG(kDefault, kDebug, "Pre-processing {} textures", std::ssize(rData.textures));
	rDecoder.DecodeTextureFormats(rData);
	ProcessTextures(rData);

	LOG(kDefault, kDebug, "Loading {} materials", rData.iSourceMaterialCount);
	rData.materials.resize(static_cast<size_t>(rData.iSourceMaterialCount));
	rData.materialNodeInfos.resize(static_cast<size_t>(rData.iSourceMaterialCount));
	bool bHasSkeleton = rDecoder.SetupSkeletonAndMaterials();
	rDecoder.ValidateSkinAnimation();
	rDecoder.DecodeVertices(bHasSkeleton, rData);
	OptimizeMeshes(rData.materials, rData.vertices);
	rDecoder.DecodeMaterialInfos(bHasSkeleton, rData);
	WriteModelFile(rData.materials, rData.materialInfos, rData.vertices);
	rDecoder.ValidateSamplers();
}

void ExportScene::ProcessTextures(const SceneExportData& rData)
{
	struct TextureAttempt
	{
		int64_t iSource = 0;
		VkFormat vkFormat = VK_FORMAT_UNDEFINED;
		std::filesystem::path finalPath;
		std::filesystem::path stagingPath;
	};

	std::vector<TextureAttempt> textureAttempts;
	textureAttempts.reserve(static_cast<size_t>(std::ssize(rData.textures)));
	mTextureAttemptFiles.reserve(static_cast<size_t>(std::ssize(mTextureAttemptFiles) + std::ssize(rData.textures)));
	for (int64_t i = 0; i < std::ssize(rData.textures); ++i)
	{
		int64_t iSource = rData.textures.at(static_cast<size_t>(i)).iSource;
		VkFormat vkFormat = rData.textures.at(static_cast<size_t>(i)).vkFormat;
		int64_t iAttempt = 0;
		for (; iAttempt < std::ssize(textureAttempts); ++iAttempt)
		{
			const TextureAttempt& rAttempt = textureAttempts.at(static_cast<size_t>(iAttempt));
			if (rAttempt.iSource == iSource && rAttempt.vkFormat == vkFormat)
			{
				break;
			}
		}
		if (iAttempt == std::ssize(textureAttempts))
		{
			textureAttempts.push_back(
			{
				.iSource = iSource,
				.vkFormat = vkFormat,
				.finalPath = GetTextureIntermediatePath(iSource, vkFormat),
				.stagingPath = GetTextureIntermediateStagePath(iAttempt),
			});
			mTextureAttemptFiles.push_back(textureAttempts.back().stagingPath);
		}
	}

	// Process each unique source/format pair on this export-job thread. BC encoding dispatches its
	// block rows across the shared worker pool, while the outer stages remain available for cleanup.
	for (const TextureAttempt& rAttempt : textureAttempts)
	{
		const SceneImage& rImage = rData.images.at(rAttempt.iSource);
		std::lock_guard<std::mutex> lock(Texture::sEncodeMutex);
		Texture texture(reinterpret_cast<const std::byte*>(rImage.pixels.data()), rImage.iWidth, rImage.iHeight, rImage.iComponents);
		texture.MakeMipmaps(rAttempt.vkFormat);
		texture.Save(rAttempt.stagingPath, rAttempt.vkFormat, {});
	}

	// Publish every completed outer stage only after all unique workers succeeded. Record each final as soon as it
	// is published, so a mid-loop failure's CleanupOnFailure removes the replaced prefix instead of leaving a mixed
	// generation for the texture pass to pack.
	mPublishedTextureFiles.reserve(static_cast<size_t>(std::ssize(textureAttempts)));
	for (const TextureAttempt& rAttempt : textureAttempts)
	{
		VERIFY_SUCCESS(MoveFileExW(rAttempt.stagingPath.native().c_str(), rAttempt.finalPath.native().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
		mPublishedTextureFiles.push_back(rAttempt.finalPath);
	}
	mTextureAttemptFiles.clear();

	// Log each source slot, including slots sharing one unique worker, in glTF texture order so each logged
	// index matches that slot's texture CRC index in MainExport.
	int64_t iTextureIndex = 0;
	for (int64_t i = 0; i < std::ssize(rData.textures); ++i)
	{
		const SceneImage& rImage = rData.images.at(rData.textures.at(static_cast<size_t>(i)).iSource);
		std::filesystem::path finalPath = GetTextureIntermediatePath(rData.textures.at(static_cast<size_t>(i)).iSource, rData.textures.at(static_cast<size_t>(i)).vkFormat);
		LOG(kDefault, kVerbose, "  {}: Texture {} -> {}", iTextureIndex++, rImage.uri, finalPath.filename().native());
	}

	// Sweep texture intermediates orphaned by a texture being removed, renumbered, or re-formatted in the
	// source glTF; the recursive ExportTexture scan (Main.cpp) claims any .BCn_UNORM_BLOCK file, so a stale
	// leftover would ship as a live texture chunk that no longer matches the scene. textureAttempts is exactly
	// the current unique write set (same directory, full filename incl. format), so any matching sibling not
	// in it is orphaned. Runs whenever PreExport runs, which a source texture edit reliably triggers via the
	// glTF mtime (CheckDirty).
	std::unordered_set<std::string> currentIntermediateNames;
	for (const TextureAttempt& rAttempt : textureAttempts)
	{
		currentIntermediateNames.insert(rAttempt.finalPath.filename().string());
	}

	std::string intermediatePrefix = mInputPath.filename().string() + ".Texture";
	std::vector<std::filesystem::path> orphanedIntermediates;
	for (const std::filesystem::directory_entry& rEntry : std::filesystem::directory_iterator(mInputPath.parent_path()))
	{
		if (!rEntry.is_regular_file())
		{
			continue;
		}
		std::string name = rEntry.path().filename().string();
		if (IsSceneTextureIntermediate(name, intermediatePrefix) && !currentIntermediateNames.contains(name))
		{
			orphanedIntermediates.push_back(rEntry.path());
		}
	}
	for (const std::filesystem::path& rOrphanedIntermediate : orphanedIntermediates)
	{
		std::filesystem::remove(rOrphanedIntermediate);
		LOG(kDefault, kDebug, "Removed orphaned scene texture intermediate: \"{}\"", rOrphanedIntermediate.filename().native());
	}
}

void ExportScene::OptimizeMeshes(std::vector<Material>& rMaterials, std::vector<common::ModelVertex>& rVertices)
{
	for (int64_t i = 0; i < std::ssize(rMaterials); ++i)
	{
		std::vector<uint32_t>& rIndexBuffer = rMaterials.at(i).indexBuffer;
		if (rIndexBuffer.empty())
		{
			continue;
		}
		meshopt_optimizeVertexCache(rIndexBuffer.data(), rIndexBuffer.data(), static_cast<size_t>(std::ssize(rIndexBuffer)), static_cast<size_t>(std::ssize(rVertices)));
		meshopt_optimizeOverdraw(rIndexBuffer.data(), rIndexBuffer.data(), static_cast<size_t>(std::ssize(rIndexBuffer)), &rVertices.at(0).f3Pos.x, static_cast<size_t>(std::ssize(rVertices)), sizeof(common::ModelVertex), 1.05f);
		LOG(kDefault, kVerbose, "  Material {}: optimized {} triangles", i, std::ssize(rIndexBuffer) / 3);
	}
}

void ExportScene::WriteModelFile(const std::vector<Material>& rMaterials, const std::vector<common::MaterialInfo>& rMaterialInfos, std::vector<common::ModelVertex>& rVertices)
{
	std::filesystem::path path(mInputPath);
	path += ".MODEL";

	LOG(kDefault, kDebug, "Total vertices: {}", std::ssize(rVertices));

	std::unordered_map<float, int64_t> jointsMap;
	XMFLOAT3 f3Min = rVertices.at(0).f3Pos;
	XMFLOAT3 f3Max = rVertices.at(0).f3Pos;
	for (const common::ModelVertex& rVertex : rVertices)
	{
		f3Min.x = std::min(f3Min.x, rVertex.f3Pos.x);
		f3Min.y = std::min(f3Min.y, rVertex.f3Pos.y);
		f3Min.z = std::min(f3Min.z, rVertex.f3Pos.z);
		f3Max.x = std::max(f3Max.x, rVertex.f3Pos.x);
		f3Max.y = std::max(f3Max.y, rVertex.f3Pos.y);
		f3Max.z = std::max(f3Max.z, rVertex.f3Pos.z);

		++jointsMap.try_emplace(rVertex.fJoint, 0).first->second;
	}
	LOG(kDefault, kDebug, "f3Min: {} f3Max: {}", f3Min, f3Max);

	LOG(kDefault, kDebug, "Joints:");
	for (const auto& [rfJointId, riCount] : jointsMap)
	{
		LOG(kDefault, kVerbose, "  {}: {}", rfJointId, riCount);
	}

	std::vector<uint32_t> indices32;
	std::vector<uint32_t> materialIndexPositions(static_cast<size_t>(std::ssize(rMaterials)));
	for (int64_t i = 0; i < std::ssize(rMaterials); ++i)
	{
		materialIndexPositions.at(i) = static_cast<uint32_t>(std::ssize(indices32));
		indices32.append_range(rMaterials.at(i).indexBuffer);
	}

	std::vector<common::ModelVertex> optimizedVertices(static_cast<size_t>(std::ssize(rVertices)));
	meshopt_optimizeVertexFetch(optimizedVertices.data(), indices32.data(), static_cast<size_t>(std::ssize(indices32)), rVertices.data(), static_cast<size_t>(std::ssize(rVertices)), sizeof(common::ModelVertex));
	rVertices = std::move(optimizedVertices);
	LOG(kDefault, kDebug, "Mesh optimized: {} vertices, {} indices ({} materials)", std::ssize(rVertices), std::ssize(indices32), std::ssize(rMaterials));

	std::vector<uint16_t> indices16;
	if (common::ModelHeader::UsesU16Indices(std::ssize(rVertices)))
	{
		indices16.reserve(static_cast<size_t>(std::ssize(indices32)));
		for (int64_t iIndex : indices32)
		{
			indices16.push_back(static_cast<uint16_t>(iIndex));
		}
	}

	std::filesystem::remove(path);
	std::fstream fileStreamOut(path, std::ios::out | std::ios::binary);
	size_t uiMaterialCount = rMaterials.size();
	size_t uiIndexCount = indices32.size();
	size_t uiVertexCount = rVertices.size();
	fileStreamOut.write(reinterpret_cast<const char*>(&uiMaterialCount), sizeof(uiMaterialCount));
	fileStreamOut.write(reinterpret_cast<const char*>(materialIndexPositions.data()), common::VectorByteSize(materialIndexPositions));
	fileStreamOut.write(reinterpret_cast<const char*>(rMaterialInfos.data()), common::VectorByteSize(rMaterialInfos));
	fileStreamOut.write(reinterpret_cast<const char*>(&uiIndexCount), sizeof(uiIndexCount));
	fileStreamOut.write(reinterpret_cast<const char*>(&uiVertexCount), sizeof(uiVertexCount));
	if (std::ssize(indices16) > 0)
	{
		fileStreamOut.write(reinterpret_cast<const char*>(indices16.data()), common::VectorByteSize(indices16));
	}
	else
	{
		fileStreamOut.write(reinterpret_cast<const char*>(indices32.data()), common::VectorByteSize(indices32));
	}
	fileStreamOut.write(reinterpret_cast<const char*>(rVertices.data()), common::VectorByteSize(rVertices));
	fileStreamOut.close();
	VERIFY_SUCCESS(fileStreamOut.good());
	mIntermediateFiles.push_back(std::move(path));
}

void ExportScene::MainExport(const GltfSceneDecoder* pGltfDecoder, const AssimpSceneDecoder* pAssimpDecoder, SceneExportData& rData)
{
	// Split materials can make the MODEL material count larger than the source count.
	std::filesystem::path modelPath(mInputPath);
	modelPath += ".MODEL";
	size_t uiMaterialCount = 0;
	int64_t iModelFileSize = SourceFileSize(modelPath, kpcContext);
	std::fstream materialCountFileStream(modelPath, std::ios::in | std::ios::binary);
	if (!materialCountFileStream)
	{
		throw std::runtime_error("ExportScene failed to open model file");
	}
	RequireSourceExtent(static_cast<uintmax_t>(iModelFileSize), 0, sizeof(uiMaterialCount), kpcContext);
	ReadSourceBytes(materialCountFileStream, std::span<char>(reinterpret_cast<char*>(&uiMaterialCount), sizeof(uiMaterialCount)), kpcContext);
	materialCountFileStream.close();
	if (uiMaterialCount > static_cast<size_t>(common::SceneHeader::kiMaxMaterials))
	{
		throw std::runtime_error("ExportScene model material count is invalid");
	}
	int64_t iMaterialIndexBytes = static_cast<int64_t>(MultiplySourceBytes(static_cast<uintmax_t>(uiMaterialCount), sizeof(uint32_t), kpcContext));
	int64_t iMaterialInfoBytes = static_cast<int64_t>(MultiplySourceBytes(static_cast<uintmax_t>(uiMaterialCount), sizeof(common::MaterialInfo), kpcContext));
	int64_t iMaterialBytes = static_cast<int64_t>(AddSourceBytes(static_cast<uintmax_t>(iMaterialIndexBytes), static_cast<uintmax_t>(iMaterialInfoBytes), kpcContext));
	RequireSourceExtent(static_cast<uintmax_t>(iModelFileSize), sizeof(uiMaterialCount), static_cast<uintmax_t>(iMaterialBytes), kpcContext);

	int64_t iTextureCount = std::ssize(rData.textures);
	int64_t iMaterialCount = static_cast<int64_t>(uiMaterialCount);
	int64_t iSceneArraysSize = common::SceneHeader::MaterialDataOffset(iTextureCount, iMaterialCount);
	int64_t iMaterialShaderDataBytes = static_cast<int64_t>(MultiplySourceBytes(static_cast<uintmax_t>(uiMaterialCount), sizeof(common::MaterialShaderData), kpcContext));
	int64_t iDataSize = iSceneArraysSize + iMaterialShaderDataBytes;
	auto [pHeader, dataSpan] = AllocateHeaderAndData(iDataSize);
	common::crc_t* pTextureCrcs = reinterpret_cast<common::crc_t*>(dataSpan.data());
	uint32_t* puiIndexStarts = reinterpret_cast<uint32_t*>(dataSpan.data() + common::SceneHeader::IndexStartsOffset(iTextureCount));
	common::MaterialShaderData* pMaterialShaderDatas = reinterpret_cast<common::MaterialShaderData*>(dataSpan.data() + iSceneArraysSize);

	LOG(kDefault, kDebug, "Textures: {}", std::ssize(rData.textures));
	if (pGltfDecoder != nullptr)
	{
		pGltfDecoder->DecodeTextureFormats(rData);
	}
	pHeader->sceneHeader.uiTextureCount = 0;
	for (int64_t i = 0; i < std::ssize(rData.textures); ++i)
	{
		const SceneTexture& rTexture = rData.textures.at(static_cast<size_t>(i));
		std::filesystem::path relativeFile = mRelativeDirectory;
		relativeFile /= mInputPath.filename();
		relativeFile += ".Texture";
		relativeFile += std::to_string(rTexture.iSource);
		relativeFile += TextureIntermediateSuffix(rData.textures.at(static_cast<size_t>(i)).vkFormat);
		pTextureCrcs[pHeader->sceneHeader.uiTextureCount++] = common::Crc(relativeFile.string());
	}

	LOG(kDefault, kDebug, "Materials: {} (original), {} (after splitting)", rData.iSourceMaterialCount, uiMaterialCount);

	pHeader->sceneHeader.modelCrc = common::Crc(mRelativeFile + ".MODEL");

	rData.materialInfos.resize(uiMaterialCount);
	std::vector<common::MaterialInfo>& materialInfos = rData.materialInfos;
	ReadMaterialInfosFromModel(modelPath, std::span<uint32_t>(puiIndexStarts, uiMaterialCount), materialInfos);
	pHeader->sceneHeader.uiMaterialCount = static_cast<uint32_t>(iMaterialCount);
	ASSERT(pHeader->sceneHeader.uiMaterialCount <= common::SceneHeader::kiMaxMaterials);

	if (pGltfDecoder != nullptr)
	{
		pGltfDecoder->DecodeMaterialShaderDatas(rData);
	}
	else
	{
		pAssimpDecoder->DecodeMaterialShaderDatas(rData);
	}
	std::copy(rData.materialShaderDatas.begin(), rData.materialShaderDatas.end(), pMaterialShaderDatas);

	if (pGltfDecoder != nullptr && rData.iSourceAnimationCount > 0)
	{
		pGltfDecoder->DecodeAnimationSection(rData);
		WriteAnimationSection(rData, pHeader);
	}
	else if (pAssimpDecoder != nullptr && !rData.animations.empty())
	{
		WriteAnimationSection(rData, pHeader);
	}
}

void ExportScene::ReadMaterialInfosFromModel(const std::filesystem::path& rModelPath, std::span<uint32_t> indexStarts, std::vector<common::MaterialInfo>& rMaterialInfos)
{
	int64_t iMaterialCount = std::ssize(indexStarts);
	int64_t iFileSize = SourceFileSize(rModelPath, kpcContext);
	std::fstream fileStream(rModelPath, std::ios::in | std::ios::binary);
	if (!fileStream)
	{
		throw std::runtime_error("ExportScene failed to open model file");
	}
	RequireSourceExtent(static_cast<uintmax_t>(iFileSize), 0, sizeof(size_t), kpcContext);
	size_t uiMaterialCountVerify = 0;
	ReadSourceBytes(fileStream, std::span<char>(reinterpret_cast<char*>(&uiMaterialCountVerify), sizeof(uiMaterialCountVerify)), kpcContext);
	if (uiMaterialCountVerify != static_cast<size_t>(iMaterialCount))
	{
		throw std::runtime_error("ExportScene model material count changed while reading");
	}
	int64_t iMaterialIndexBytes = static_cast<int64_t>(MultiplySourceBytes(static_cast<uintmax_t>(iMaterialCount), sizeof(uint32_t), kpcContext));
	int64_t iMaterialInfoBytes = static_cast<int64_t>(MultiplySourceBytes(static_cast<uintmax_t>(iMaterialCount), sizeof(common::MaterialInfo), kpcContext));
	int64_t iFileOffset = sizeof(size_t);
	RequireSourceExtent(static_cast<uintmax_t>(iFileSize), static_cast<uintmax_t>(iFileOffset), static_cast<uintmax_t>(iMaterialIndexBytes), kpcContext);
	ReadSourceBytes(fileStream, std::span<char>(reinterpret_cast<char*>(indexStarts.data()), static_cast<size_t>(iMaterialIndexBytes)), kpcContext);
	iFileOffset = static_cast<int64_t>(AddSourceBytes(static_cast<uintmax_t>(iFileOffset), static_cast<uintmax_t>(iMaterialIndexBytes), kpcContext));

	RequireSourceExtent(static_cast<uintmax_t>(iFileSize), static_cast<uintmax_t>(iFileOffset), static_cast<uintmax_t>(iMaterialInfoBytes), kpcContext);
	ReadSourceBytes(fileStream, std::span<char>(reinterpret_cast<char*>(rMaterialInfos.data()), static_cast<size_t>(iMaterialInfoBytes)), kpcContext);

	fileStream.close();
}

void ExportScene::WriteAnimationSection(const SceneExportData& rData, common::ChunkHeader* pHeader)
{
	const SkeletonData& skeletonData = rData.skeletonData;
	const std::vector<common::MaterialInfo>& rMaterialInfos = rData.materialInfos;
	const std::vector<common::AnimationClip>& animations = rData.animations;
	const std::vector<common::AnimationChannel>& channels = rData.channels;
	const std::vector<common::AnimationKeyframe>& keyframes = rData.keyframes;
	const std::vector<common::AnimationKeyframeCubic>& cubicKeyframes = rData.cubicKeyframes;
	if (animations.empty())
	{
		return;
	}

	// The marker implies a clip section holding at least one animation: the runtime rejects a count of 0 as a corrupt
	// stream (AnimationData::Load). Must stay before mHeaderAndData grows below, which invalidates pHeader.
	pHeader->sceneHeader.bHasAnimation = true;

	for (const common::AnimationClip& rAnimation : animations)
	{
		LOG(kDefault, kVerbose, "    \"{}\": {} channels, {:.2f}s duration", rAnimation.pcName, rAnimation.uiChannelCount, rAnimation.fDuration);
	}

	common::AnimationHeader animationHeader {};
	animationHeader.uiAnimationCount = static_cast<uint32_t>(std::ssize(animations));
	animationHeader.uiChannelCount = static_cast<uint32_t>(std::ssize(channels));
	animationHeader.uiKeyframeCount = static_cast<uint32_t>(std::ssize(keyframes));
	animationHeader.uiCubicKeyframeCount = static_cast<uint32_t>(std::ssize(cubicKeyframes));
	animationHeader.skeleton = skeletonData.skeleton;
	ASSERT(std::ssize(animations) <= common::AnimationHeader::kiMaxAnimations);

	for (int64_t i = 0; i < std::ssize(rMaterialInfos); ++i)
	{
		if (rMaterialInfos.at(i).iParentNodeIndex >= 0)
		{
			LOG(kDefault, kVerbose, "  Material {}: {}, parent node {}", i, (rMaterialInfos.at(i).flags & common::MaterialFlags::kSkinned) ? "skinned" : "non-skinned", rMaterialInfos.at(i).iParentNodeIndex);
		}
	}

	int64_t iSkinJointToNodeSize = common::RoundUp<int64_t, common::kiAnimationSectionAlignment>(std::ssize(skeletonData.skinJointToNode) * static_cast<int64_t>(sizeof(uint16_t)));
	int64_t iAnimationDataSize = sizeof(common::AnimationHeader)
		+ std::ssize(skeletonData.nodes) * sizeof(common::ModelNode)
		+ iSkinJointToNodeSize
		+ std::ssize(skeletonData.inverseBindMatrices) * sizeof(XMFLOAT4X4)
		+ std::ssize(animations) * sizeof(common::AnimationClip)
		+ std::ssize(rMaterialInfos) * sizeof(common::MaterialInfo)
		+ std::ssize(channels) * sizeof(common::AnimationChannel)
		+ std::ssize(keyframes) * sizeof(common::AnimationKeyframe)
		+ std::ssize(cubicKeyframes) * sizeof(common::AnimationKeyframeCubic);

	int64_t iCurrentSize = std::ssize(mHeaderAndData);
	// Mirrors the reader's math (LoadAnimationDataFromEagerChunks, AnimationData.cpp): the MaterialShaderData block is 16-byte rounded by AllocateHeaderAndData
	int64_t iExpectedOffset = common::kiChunkDataOffset + common::SceneHeader::AnimationSectionOffset(std::ssize(rData.textures), std::ssize(rMaterialInfos));
	LOG(kDefault, kVerbose, "  Animation data: writing at offset {} (buffer size {}), expected runtime offset {} (diff {})", iCurrentSize, std::ssize(mHeaderAndData), iExpectedOffset, iCurrentSize - iExpectedOffset);
	ASSERT(iCurrentSize == iExpectedOffset);
	mHeaderAndData.resize(iCurrentSize + iAnimationDataSize);
	std::byte* pAnimationData = mHeaderAndData.data() + iCurrentSize;

	std::memcpy(pAnimationData, &animationHeader, sizeof(animationHeader));
	pAnimationData += sizeof(animationHeader);

	std::memcpy(pAnimationData, skeletonData.nodes.data(), static_cast<size_t>(std::ssize(skeletonData.nodes)) * sizeof(common::ModelNode));
	pAnimationData += std::ssize(skeletonData.nodes) * sizeof(common::ModelNode);

	if (!skeletonData.skinJointToNode.empty())
	{
		std::memcpy(pAnimationData, skeletonData.skinJointToNode.data(), static_cast<size_t>(std::ssize(skeletonData.skinJointToNode)) * sizeof(uint16_t));
	}
	pAnimationData += iSkinJointToNodeSize;

	if (!skeletonData.inverseBindMatrices.empty())
	{
		std::memcpy(pAnimationData, skeletonData.inverseBindMatrices.data(), static_cast<size_t>(std::ssize(skeletonData.inverseBindMatrices)) * sizeof(XMFLOAT4X4));
	}
	pAnimationData += std::ssize(skeletonData.inverseBindMatrices) * sizeof(XMFLOAT4X4);

	std::memcpy(pAnimationData, animations.data(), static_cast<size_t>(std::ssize(animations)) * sizeof(common::AnimationClip));
	pAnimationData += std::ssize(animations) * sizeof(common::AnimationClip);

	std::memcpy(pAnimationData, rMaterialInfos.data(), static_cast<size_t>(std::ssize(rMaterialInfos)) * sizeof(common::MaterialInfo));
	pAnimationData += std::ssize(rMaterialInfos) * sizeof(common::MaterialInfo);

	std::memcpy(pAnimationData, channels.data(), static_cast<size_t>(std::ssize(channels)) * sizeof(common::AnimationChannel));
	pAnimationData += std::ssize(channels) * sizeof(common::AnimationChannel);

	std::memcpy(pAnimationData, keyframes.data(), static_cast<size_t>(std::ssize(keyframes)) * sizeof(common::AnimationKeyframe));
	pAnimationData += std::ssize(keyframes) * sizeof(common::AnimationKeyframe);

	std::memcpy(pAnimationData, cubicKeyframes.data(), static_cast<size_t>(std::ssize(cubicKeyframes)) * sizeof(common::AnimationKeyframeCubic));
}

void ExportScene::CleanupTextureAttemptFiles()
{
	std::error_code errorCode;
	for (const std::filesystem::path& rPath : mTextureAttemptFiles)
	{
		std::filesystem::remove(rPath, errorCode);
	}
	mTextureAttemptFiles.clear();
}

void ExportScene::UpdateCacheMetadata()
{
	if (IsAssimpScene(mInputPath))
	{
		WriteMarkerFile(GetSceneDependencyMarkerPath(), mSceneDependencyMetadata);
	}
	// RunExport calls this only after Export() and the chunk write both succeeded, so the marker can only ever
	// record a complete generated set. A run that reused the existing outputs rewrites the same fingerprint.
	std::optional<std::string> optionalFingerprint = GetPreExportFingerprint();
	if (!optionalFingerprint.has_value())
	{
		LOG(kDefault, kWarning, "Could not read generated outputs for \"{}\"; leaving no pre-export marker, so the next run re-runs pre-export", mInputPath.string());
		return;
	}
	WriteMarkerFile(GetPreExportMarkerPath(), optionalFingerprint.value());
}

void ExportScene::CleanupOnFailure()
{
	CleanupTextureAttemptFiles();
	std::error_code errorCode;
	// A partially written output set must never be adopted as fresh by a later run.
	std::filesystem::remove(GetPreExportMarkerPath(), errorCode);
	if (IsAssimpScene(mInputPath))
	{
		std::filesystem::remove(GetSceneDependencyMarkerPath(), errorCode);
		std::filesystem::remove(mCacheMetadataFile, errorCode);
	}
	for (const std::filesystem::path& rPath : mPublishedTextureFiles)
	{
		std::filesystem::remove(rPath, errorCode);
	}
	mPublishedTextureFiles.clear();
	for (const std::filesystem::path& rPath : mIntermediateFiles)
	{
		std::filesystem::remove(rPath, errorCode);
	}
	mIntermediateFiles.clear();
}
