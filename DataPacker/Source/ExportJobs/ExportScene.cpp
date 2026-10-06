#include "ExportScene.h"

#include "Scene/SceneAnimationLoader.h"
#include "Scene/SceneSkeletonLoader.h"
#include "Texture/Texture.h"
#include "FileManager.h"
#include "SourceReadValidation.h"

std::optional<common::ChunkFlags_t> ExportScene::Handles(const std::filesystem::directory_entry& rDirectoryEntry)
{
	if (rDirectoryEntry.path().extension() != ".gltf")
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
	return std::optional<common::ChunkFlags_t>(common::ChunkFlags::kScene);
}

bool ExportScene::CheckDirty(const std::filesystem::path& rPackFile)
{
	bool bDirty = ExportJob::CheckDirty(rPackFile);

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
		std::optional<std::string> storedFingerprint = ReadMarkerFile(GetPreExportMarkerPath());
		std::optional<std::string> currentFingerprint = GetPreExportFingerprint();
		if (!storedFingerprint.has_value() || !currentFingerprint.has_value() || storedFingerprint.value() != currentFingerprint.value())
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

static VkFilter ToVkFilter(int64_t iFilterMode)
{
	switch (iFilterMode)
	{
		case TINYGLTF_TEXTURE_FILTER_NEAREST:
			return VK_FILTER_NEAREST;
		case TINYGLTF_TEXTURE_FILTER_LINEAR:
			return VK_FILTER_LINEAR;
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
			return VK_FILTER_NEAREST;
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
			return VK_FILTER_NEAREST;
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
			return VK_FILTER_LINEAR;
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
			return VK_FILTER_LINEAR;
		case -1:
			return VK_FILTER_LINEAR;
		default:
			ASSERT(false);
			return VK_FILTER_LINEAR;
	}
}

static VkSamplerAddressMode ToVkSamplerAddressMode(int64_t iWrapMode)
{
	switch (iWrapMode)
	{
		case TINYGLTF_TEXTURE_WRAP_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case -1:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		default:
			ASSERT(false);
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

static std::vector<VkFormat> ComputeTextureFormats(const tinygltf::Model& rModel)
{
	std::vector<VkFormat> textureFormats;
	textureFormats.reserve(static_cast<size_t>(std::ssize(rModel.textures)));
	for (int64_t i = 0; i < std::ssize(rModel.textures); ++i)
	{
		// Textures sharing a source image share one generated intermediate, so a normal use through any of them
		// forces the two-channel format: the shader samples .rg and reconstructs Z from them. The single-channel
		// format likewise requires that no texture in the group carries a use needing more channels.
		bool bNormal = false;
		bool bOcclusion = false;
		bool bNonOcclusionUse = false;
		for (int64_t j = 0; j < std::ssize(rModel.textures); ++j)
		{
			if (rModel.textures.at(static_cast<size_t>(j)).source != rModel.textures.at(static_cast<size_t>(i)).source)
			{
				continue;
			}

			for (const tinygltf::Material& rMaterial : rModel.materials)
			{
				bNormal |= IsNormal(j, rMaterial);
				bOcclusion |= IsOcclusion(j, rMaterial);
				bNonOcclusionUse |= IsNonOcclusionUse(j, rMaterial);
			}
		}

		if (bNormal)
		{
			textureFormats.push_back(VK_FORMAT_BC5_UNORM_BLOCK);
		}
		else if (bOcclusion && !bNonOcclusionUse)
		{
			textureFormats.push_back(VK_FORMAT_BC4_UNORM_BLOCK);
		}
		else
		{
			textureFormats.push_back(VK_FORMAT_BC7_UNORM_BLOCK);
		}
	}
	return textureFormats;
}

// Diagnostic dump (warning level) for the case where every animation channel was filtered out: logs the
// node count and the first ten source channel targets so a mis-targeted glTF animation can be debugged.
static void LogFilteredChannelDiagnostics(const tinygltf::Model& rGltfModel)
{
	LOG(kDefault, kWarning, "WARNING: All animation channels were filtered out!");
	LOG(kDefault, kWarning, "  Node count: {}", std::ssize(rGltfModel.nodes));
	int64_t iCount = 0;
	for (const tinygltf::Animation& rAnimation : rGltfModel.animations)
	{
		for (const tinygltf::AnimationChannel& rChannel : rAnimation.channels)
		{
			LOG(kDefault, kWarning, "    Animation channel targets node {} (\"{}\")", rChannel.target_node, rChannel.target_node >= 0 && rChannel.target_node < std::ssize(rGltfModel.nodes) ? rGltfModel.nodes.at(rChannel.target_node).name : "invalid");
			if (++iCount >= 10)
			{
				break;
			}
		}
		if (iCount >= 10)
		{
			break;
		}
	}
}

// Mirrors WriteAnimationSection's emission rule (a clip reaches the pack only when a channel survives
// filtering) by running the same loader, so the two cannot drift apart.
static bool HasSurvivingAnimationClip(const tinygltf::Model& rGltfModel)
{
	std::vector<common::AnimationClip> animations;
	std::vector<common::AnimationChannel> channels;
	std::vector<common::AnimationKeyframe> keyframes;
	std::vector<common::AnimationKeyframeCubic> cubicKeyframes;
	AnimationOutput animationOutput {.rAnimations = animations, .rChannels = channels, .rKeyframes = keyframes, .rCubicKeyframes = cubicKeyframes};
	LoadAnimations(rGltfModel, animationOutput);
	return !animations.empty();
}

// Collects the glTF's external file references (images[].uri and buffers[].uri) by reading the JSON directly:
// going through tinygltf would re-read every .bin and stb-decode every texture on each dirty check. Reading
// "uri" is exact, because tinygltf only treats it as an external file here too - bufferView-backed and
// data-URI images leave the loaded image's uri empty.
static std::vector<std::string> ReadExternalUris(const std::filesystem::path& rGltfPath)
{
	std::vector<std::string> uris;

	std::fstream fileStreamIn(rGltfPath, std::ios::in | std::ios::binary);
	// Non-throwing parse: CheckDirty runs outside the per-asset try/catch, so a throw here would abort the
	// whole run instead of failing one asset. An unreadable or malformed .gltf contributes no dependencies
	// and still fails per-asset later through Export().
	nlohmann::json gltfJson = nlohmann::json::parse(fileStreamIn, nullptr, false);
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
	for (char cCharacter : indexAndSuffix.substr(0, static_cast<size_t>(iSuffixStart)))
	{
		if (cCharacter < '0' || cCharacter > '9')
		{
			return false;
		}
	}

	std::string_view suffix = indexAndSuffix.substr(static_cast<size_t>(iSuffixStart));
	return suffix == TextureIntermediateSuffix(VK_FORMAT_BC4_UNORM_BLOCK) || suffix == TextureIntermediateSuffix(VK_FORMAT_BC5_UNORM_BLOCK) || suffix == TextureIntermediateSuffix(VK_FORMAT_BC7_UNORM_BLOCK);
}


std::string ExportScene::GetInputFingerprint() const
{
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
	path /= L".TextureStage." + std::to_wstring(::GetCurrentProcessId()) + L"." + std::to_wstring(miId) + L"." + std::to_wstring(iStageIndex) + L".tmp";
	return path;
}

tinygltf::Model ExportScene::LoadGltfModel()
{
	tinygltf::Model gltfModel;

	std::string filename = mInputPath.string();
	bool bBinary = mInputPath.extension() == ".glb";

	std::string error;
	std::string warning;
	tinygltf::TinyGLTF gltfContext;
	bool bFileLoaded = bBinary ? gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning, filename.c_str()) : gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, filename.c_str());
	if (!bFileLoaded)
	{
		throw std::runtime_error(std::format("Failed to load GLTF model '{}': {} (warning: {})", filename, error, warning));
	}

	return gltfModel;
}

void ExportScene::Export()
{
	tinygltf::Model gltfModel = LoadGltfModel();

	// Both the pre-export and main-export paths read skin 0, so canonicalize before either runs
	CanonicalizeSceneSkin(gltfModel);

	if (mbNeedsPreExport)
	{
		// Model and texture writes are not transactional, and a kill (rather than a throw) mid-export never reaches
		// CleanupOnFailure. Dropping the marker before the first output is touched keeps a half-written set from being
		// vouched for as current by a marker that still matches - the next run would skip pre-export and ship it.
		std::filesystem::remove(GetPreExportMarkerPath());
		PreExport(gltfModel);
	}

	MainExport(gltfModel);
}

void ExportScene::PreExport(const tinygltf::Model& rGltfModel)
{
	LOG(kDefault, kDebug, "PreExport Gltf: {}", mInputPath.string());

	ProcessTextures(rGltfModel);

	LOG(kDefault, kDebug, "Loading {} materials", std::ssize(rGltfModel.materials));
	std::vector<Material> materials(static_cast<size_t>(std::ssize(rGltfModel.materials)));
	std::vector<MaterialNodeInfo> materialNodeInfos(static_cast<size_t>(std::ssize(rGltfModel.materials)));
	bool bHasSkeleton = SetupSkeletonAndMaterials(rGltfModel);

	// A skin alone makes bHasSkeleton true, but MainExport emits the skeleton only inside WriteAnimationSection,
	// which runs only for a surviving clip. Without one the vertices stay mesh-local and the material transforms
	// node-relative with no node matrices at runtime, so the model would render mispositioned in silence.
	if (!rGltfModel.skins.empty() && !HasSurvivingAnimationClip(rGltfModel))
	{
		throw std::runtime_error("ExportScene model declares a skin but no animation clip survives export");
	}

	std::vector<common::ModelVertex> vertices;
	LoadVerticesAndOptimizeMeshes(rGltfModel, bHasSkeleton, materials, materialNodeInfos, vertices);

	std::vector<common::MaterialInfo> materialInfos(static_cast<size_t>(std::ssize(materials)));
	BuildMaterialInfos(rGltfModel, bHasSkeleton, materials, materialNodeInfos, materialInfos);

	WriteModelFile(materials, materialInfos, vertices);

	LOG(kDefault, kDebug, "Samplers: {}", std::ssize(rGltfModel.samplers));
	for (const tinygltf::Sampler& rSampler : rGltfModel.samplers)
	{
		LOG(kDefault, kVerbose, "  {} {} {} {}", ToVkFilter(rSampler.minFilter), ToVkFilter(rSampler.magFilter), ToVkSamplerAddressMode(rSampler.wrapS), ToVkSamplerAddressMode(rSampler.wrapT));
		ASSERT(ToVkSamplerAddressMode(rSampler.wrapS) == VK_SAMPLER_ADDRESS_MODE_REPEAT);
	}
}

void ExportScene::ProcessTextures(const tinygltf::Model& rGltfModel)
{
	ASSERT(std::ssize(rGltfModel.textures) <= common::SceneHeader::kiMaxTextures);
	LOG(kDefault, kDebug, "Pre-processing {} textures", std::ssize(rGltfModel.textures));

	std::vector<VkFormat> textureFormats = ComputeTextureFormats(rGltfModel);
	struct TextureAttempt
	{
		int64_t iSource = 0;
		VkFormat vkFormat = VK_FORMAT_UNDEFINED;
		std::filesystem::path finalPath;
		std::filesystem::path stagingPath;
	};

	std::vector<TextureAttempt> textureAttempts;
	textureAttempts.reserve(static_cast<size_t>(std::ssize(rGltfModel.textures)));
	mTextureAttemptFiles.reserve(static_cast<size_t>(std::ssize(mTextureAttemptFiles) + std::ssize(rGltfModel.textures)));
	for (int64_t i = 0; i < std::ssize(rGltfModel.textures); ++i)
	{
		int64_t iSource = rGltfModel.textures.at(static_cast<size_t>(i)).source;
		VkFormat vkFormat = textureFormats.at(static_cast<size_t>(i));
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
	try
	{
		for (const TextureAttempt& rAttempt : textureAttempts)
		{
			const tinygltf::Image& rImage = rGltfModel.images.at(rAttempt.iSource);
			std::lock_guard<std::mutex> lock(Texture::sEncodeMutex);
			Texture texture(reinterpret_cast<const std::byte*>(rImage.image.data()), rImage.width, rImage.height, rImage.component);
			texture.MakeMipmaps(rAttempt.vkFormat);
			texture.Save(rAttempt.stagingPath, rAttempt.vkFormat, {});
		}
	}
	catch (...)
	{
		CleanupTextureAttemptFiles();
		throw;
	}

	// Publish every completed outer stage only after all unique workers succeeded. Record each final as soon as it
	// is published, so a mid-loop failure's CleanupOnFailure removes the replaced prefix instead of leaving a mixed
	// generation for the texture pass to pack.
	mPublishedTextureFiles.reserve(static_cast<size_t>(std::ssize(textureAttempts)));
	try
	{
		for (const TextureAttempt& rAttempt : textureAttempts)
		{
			VERIFY_SUCCESS(MoveFileExW(rAttempt.stagingPath.native().c_str(), rAttempt.finalPath.native().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
			mPublishedTextureFiles.push_back(rAttempt.finalPath);
		}
	}
	catch (...)
	{
		CleanupTextureAttemptFiles();
		throw;
	}
	mTextureAttemptFiles.clear();

	// Log each source slot, including slots sharing one unique worker, and retain the existing
	// per-slot format/CRC ordering used by MainExport.
	int64_t iTextureIndex = 0;
	for (int64_t i = 0; i < std::ssize(rGltfModel.textures); ++i)
	{
		const tinygltf::Image& rImage = rGltfModel.images.at(rGltfModel.textures.at(static_cast<size_t>(i)).source);
		std::filesystem::path finalPath = GetTextureIntermediatePath(rGltfModel.textures.at(static_cast<size_t>(i)).source, textureFormats.at(static_cast<size_t>(i)));
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

bool ExportScene::SetupSkeletonAndMaterials(const tinygltf::Model& rGltfModel)
{
	// Skeleton data loads whenever a skin exists (skeletal) or at least one channel survives
	// SceneAnimationLoader filtering (node-based); the test must match what WriteAnimationSection emits, or
	// vertices stay unbaked with no skeleton section to interpret them. When neither holds the model is
	// static and bHasSkeleton stays false (load-bearing for the static-model vertex transform in
	// LoadVertices). DetermineAnimationPath's result here only selects the log label.
	bool bHasSkeleton = !rGltfModel.skins.empty() || HasSurvivingAnimationClip(rGltfModel);
	if (bHasSkeleton)
	{
		SkeletonData skeletonData = LoadSkeletonData(rGltfModel);
		if (DetermineAnimationPath(rGltfModel))
		{
			LOG(kDefault, kDebug, "  Skeletal animation detected: {} skin joints, {} total nodes", std::ssize(rGltfModel.skins.at(0).joints), std::ssize(rGltfModel.nodes));
		}
		else
		{
			LOG(kDefault, kDebug, "  Node-based animation detected: {} nodes in skeleton", skeletonData.skeleton.uiNodeCount);
		}
	}
	return bHasSkeleton;
}

void ExportScene::LoadVerticesAndOptimizeMeshes(const tinygltf::Model& rGltfModel, bool bHasSkeleton, std::vector<Material>& rMaterials, std::vector<MaterialNodeInfo>& rMaterialNodeInfos, std::vector<common::ModelVertex>& rVertices)
{
	const tinygltf::Scene& rScene = rGltfModel.scenes.at(rGltfModel.defaultScene > -1 ? rGltfModel.defaultScene : 0);
	MaterialNodeMap materialNodeMap;
	LoadVerticesContext loadContext {.rVertices = rVertices, .rMaterials = rMaterials, .rMaterialNodeInfos = rMaterialNodeInfos, .rMaterialNodeMap = materialNodeMap, .bHasSkeleton = bHasSkeleton};
	for (int64_t i = 0; i < std::ssize(rScene.nodes); ++i)
	{
		int64_t iNodeIndex = rScene.nodes.at(static_cast<size_t>(i));
		const tinygltf::Node& rNode = rGltfModel.nodes.at(static_cast<size_t>(iNodeIndex));
		Parent parent {.pParent = nullptr, .matNode = XMMatrixIdentity(), .iNodeIndex = -1};
		LoadVertices(&parent, iNodeIndex, rNode, rGltfModel, loadContext);
	}
	if (std::ssize(rMaterials) > std::ssize(rGltfModel.materials))
	{
		LOG(kDefault, kDebug, "  Split {} materials into {} to handle primitives from different mesh nodes or deformation modes", std::ssize(rGltfModel.materials), std::ssize(rMaterials));
	}

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

void ExportScene::BuildMaterialInfos(const tinygltf::Model& rGltfModel, bool bHasSkeleton, const std::vector<Material>& rMaterials, const std::vector<MaterialNodeInfo>& rMaterialNodeInfos, std::vector<common::MaterialInfo>& rMaterialInfos)
{
	// Build parent map for node hierarchy traversal (used for both skeletal and node-based)
	std::unordered_map<int64_t, int64_t> nodeParentMap = BuildNodeParentMap(rGltfModel);

	for (int64_t i = 0; i < std::ssize(rMaterialNodeInfos); ++i)
	{
		const MaterialNodeInfo& rInfo = rMaterialNodeInfos.at(i);

		// Skinned materials deform by the skeleton's uiSkinJointCount joints
		rMaterialInfos.at(i).flags.Set(common::MaterialFlags::kSkinned, rInfo.bHasSkinning);

		// Store original material index for split materials (-1 means not split, same as original index)
		rMaterialInfos.at(i).iOriginalMaterialIndex = static_cast<int16_t>(rInfo.iOriginalMaterialIndex);

		if (rInfo.bHasSkinning)
		{
			// Skinned material: use mesh node directly for mesh world matrix computation
			// At runtime: meshWorld = identity * worldMatrices[meshNodeIndex]
			if (rInfo.iNodeIndex >= 0)
			{
				rMaterialInfos.at(i).iParentNodeIndex = static_cast<int16_t>(rInfo.iNodeIndex);
			}
			else
			{
				// Fallback: use node 0 (typically skeleton root) when mesh node is missing
				rMaterialInfos.at(i).iParentNodeIndex = 0;
			}
			XMStoreFloat4x4(&rMaterialInfos.at(i).f4x4RelativeTransform, XMMatrixIdentity());
			LOG(kDefault, kVerbose, "  Material {}: skinned, mesh node {}", i, rMaterialInfos.at(i).iParentNodeIndex);
		}
		else if (rInfo.iNodeIndex >= 0 && bHasSkeleton && rInfo.iNodeIndex < std::ssize(rGltfModel.nodes))
		{
			// Every node is its own joint (the skeleton used an identity node->joint mapping), so the
			// nearest skeleton ancestor of a non-skinned material is its own mesh node.
			int64_t iAncestorNodeIndex = rInfo.iNodeIndex;
			XMMATRIX matAncestorWorld = ComputeNodeWorldTransform(iAncestorNodeIndex, rGltfModel, nodeParentMap);

			rMaterialInfos.at(i).iParentNodeIndex = static_cast<int16_t>(iAncestorNodeIndex);
			// In row-major: v * relativeTransform * nodeAnimated = v_animated
			XMMATRIX matRelative = rInfo.matMeshWorld * XMMatrixInverse(nullptr, matAncestorWorld);
			XMStoreFloat4x4(&rMaterialInfos.at(i).f4x4RelativeTransform, matRelative);
			LOG(kDefault, kVerbose, "  Material {}: non-skinned, parent node {}, mesh node {}", i, iAncestorNodeIndex, rInfo.iNodeIndex);
		}
	}

	for (int64_t i = 0; i < std::ssize(rMaterials); ++i)
	{
		int64_t iOriginalMaterialIndex = rMaterialNodeInfos.at(i).iOriginalMaterialIndex >= 0 ? rMaterialNodeInfos.at(i).iOriginalMaterialIndex : i;
		const tinygltf::Material& rTinygltfMaterial = rGltfModel.materials.at(static_cast<size_t>(iOriginalMaterialIndex));
		LOG(kDefault, kVerbose, "  {}: \"{}\"{}; {} {} {} {} {} textures, {} indices{}", i, rTinygltfMaterial.name, (iOriginalMaterialIndex != i ? std::format(" (split from {})", iOriginalMaterialIndex) : ""), rTinygltfMaterial.pbrMetallicRoughness.baseColorTexture.index, rTinygltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index, rTinygltfMaterial.normalTexture.index, rTinygltfMaterial.occlusionTexture.index, rTinygltfMaterial.emissiveTexture.index, std::ssize(rMaterials.at(i).indexBuffer), (rMaterialInfos.at(i).flags & common::MaterialFlags::kSkinned) ? " (skinned)" : "");
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
		indices32.insert(indices32.end(), rMaterials.at(i).indexBuffer.begin(), rMaterials.at(i).indexBuffer.end());
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
	fileStreamOut.flush();
	fileStreamOut.close();
	VERIFY_SUCCESS(fileStreamOut.good());
	mIntermediateFiles.push_back(std::move(path));
}

void ExportScene::MainExport(const tinygltf::Model& rGltfModel)
{
	// Read material count from .MODEL file first (may be larger than gltfModel.materials.size() due to splitting)
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

	int64_t iTextureCount = std::ssize(rGltfModel.textures);
	int64_t iMaterialCount = static_cast<int64_t>(uiMaterialCount);
	int64_t iSceneArraysSize = common::SceneHeader::MaterialDataOffset(iTextureCount, iMaterialCount);
	int64_t iMaterialShaderDataBytes = static_cast<int64_t>(MultiplySourceBytes(static_cast<uintmax_t>(uiMaterialCount), sizeof(common::MaterialShaderData), kpcContext));
	int64_t iDataSize = iSceneArraysSize + iMaterialShaderDataBytes;
	int64_t iMaximumDataSize = std::numeric_limits<int64_t>::max() - common::kiChunkDataOffset - (common::kiAlignmentBytes - 1);
	if (iDataSize < 0 || iDataSize > iMaximumDataSize)
	{
		throw std::runtime_error("ExportScene chunk data size overflow");
	}
	auto [pHeader, dataSpan] = AllocateHeaderAndData(iDataSize);
	common::crc_t* pTextureCrcs = reinterpret_cast<common::crc_t*>(dataSpan.data());
	uint32_t* puiIndexStarts = reinterpret_cast<uint32_t*>(dataSpan.data() + common::SceneHeader::IndexStartsOffset(iTextureCount));
	common::MaterialShaderData* pMaterialShaderDatas = reinterpret_cast<common::MaterialShaderData*>(dataSpan.data() + iSceneArraysSize);

	LOG(kDefault, kDebug, "Textures: {}", std::ssize(rGltfModel.textures));
	std::vector<VkFormat> textureFormats = ComputeTextureFormats(rGltfModel);
	pHeader->sceneHeader.uiTextureCount = 0;
	for (int64_t i = 0; i < std::ssize(rGltfModel.textures); ++i)
	{
		const tinygltf::Texture& rTexture = rGltfModel.textures.at(static_cast<size_t>(i));
		std::filesystem::path relativeFile = mRelativeDirectory;
		relativeFile /= mInputPath.filename();
		relativeFile += ".Texture";
		relativeFile += std::to_string(rTexture.source);
		relativeFile += TextureIntermediateSuffix(textureFormats.at(static_cast<size_t>(i)));
		pTextureCrcs[pHeader->sceneHeader.uiTextureCount++] = common::Crc(relativeFile.string());
	}

	LOG(kDefault, kDebug, "Materials: {} (original), {} (after splitting)", std::ssize(rGltfModel.materials), uiMaterialCount);

	pHeader->sceneHeader.modelCrc = common::Crc(mRelativeFile + ".MODEL");

	std::vector<common::MaterialInfo> materialInfos(uiMaterialCount);
	ReadMaterialInfosFromModel(modelPath, std::span<uint32_t>(puiIndexStarts, uiMaterialCount), materialInfos);
	pHeader->sceneHeader.uiMaterialCount = static_cast<uint32_t>(iMaterialCount);
	ASSERT(pHeader->sceneHeader.uiMaterialCount <= common::SceneHeader::kiMaxMaterials);

	FillMaterialShaderDatas(rGltfModel, materialInfos, std::span<common::MaterialShaderData>(pMaterialShaderDatas, uiMaterialCount));

	if (std::ssize(rGltfModel.animations) > 0)
	{
		WriteAnimationSection(rGltfModel, materialInfos, pHeader);
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

void ExportScene::FillMaterialShaderDatas(const tinygltf::Model& rGltfModel, const std::vector<common::MaterialInfo>& rMaterialInfos, std::span<common::MaterialShaderData> materialShaderDatas)
{
	for (int64_t i = 0; i < std::ssize(rMaterialInfos); ++i)
	{
		int64_t iOriginalMaterialIndex = rMaterialInfos.at(static_cast<size_t>(i)).iOriginalMaterialIndex >= 0 ? rMaterialInfos.at(static_cast<size_t>(i)).iOriginalMaterialIndex : i;
		const tinygltf::Material& rMaterial = rGltfModel.materials.at(static_cast<size_t>(iOriginalMaterialIndex));
		LOG(kDefault, kVerbose, "  {}: {}{}", i, rMaterial.name, (iOriginalMaterialIndex != i ? std::format(" (split from {})", iOriginalMaterialIndex) : ""));
		if (rMaterial.doubleSided == true)
		{
			LOG(kDefault, kWarning, "  Warning! Material is double sided");
		}

		common::MaterialShaderData materialShaderData {};

		materialShaderData.f4EmissiveFactor = XMFLOAT4(static_cast<float>(rMaterial.emissiveFactor.at(0)), static_cast<float>(rMaterial.emissiveFactor.at(1)), static_cast<float>(rMaterial.emissiveFactor.at(2)), 1.0f);

		// Only metallic-roughness workflow is supported
		ASSERT(rMaterial.extensions.find("KHR_materials_pbrSpecularGlossiness") == rMaterial.extensions.end());

		if (rMaterial.values.find("baseColorFactor") != rMaterial.values.end())
		{
			const tinygltf::ColorValue& rColorFactor = rMaterial.values.at("baseColorFactor").ColorFactor();
			materialShaderData.f4BaseColorFactor = XMFLOAT4(static_cast<float>(rColorFactor[0]), static_cast<float>(rColorFactor[1]), static_cast<float>(rColorFactor[2]), static_cast<float>(rColorFactor[3]));
		}

		if (rMaterial.values.find("baseColorTexture") != rMaterial.values.end())
		{
			materialShaderData.uiColorTextureIndex = static_cast<uint8_t>(rMaterial.values.at("baseColorTexture").TextureIndex());
			LOG(kDefault, kVerbose, "  baseColorTexture: {}", materialShaderData.uiColorTextureIndex);
			materialShaderData.iColorTextureSet = rMaterial.values.at("baseColorTexture").TextureTexCoord();
		}

		if (rMaterial.values.find("metallicRoughnessTexture") != rMaterial.values.end())
		{
			materialShaderData.uiPhysicalDescriptorTextureIndex = static_cast<uint8_t>(rMaterial.values.at("metallicRoughnessTexture").TextureIndex());
			LOG(kDefault, kVerbose, "  metallicRoughnessTexture: {}", materialShaderData.uiPhysicalDescriptorTextureIndex);
			materialShaderData.iPhysicalDescriptorTextureSet = rMaterial.values.at("metallicRoughnessTexture").TextureTexCoord();
		}

		if (rMaterial.values.find("metallicFactor") != rMaterial.values.end())
		{
			materialShaderData.fMetallicFactor = static_cast<float>(rMaterial.values.at("metallicFactor").Factor());
		}

		if (rMaterial.values.find("roughnessFactor") != rMaterial.values.end())
		{
			materialShaderData.fRoughnessFactor = static_cast<float>(rMaterial.values.at("roughnessFactor").Factor());
		}

		if (rMaterial.additionalValues.find("normalTexture") != rMaterial.additionalValues.end())
		{
			materialShaderData.uiNormalTextureIndex = static_cast<uint8_t>(rMaterial.additionalValues.at("normalTexture").TextureIndex());
			LOG(kDefault, kVerbose, "  normalTexture: {}", materialShaderData.uiNormalTextureIndex);
			materialShaderData.iNormalTextureSet = rMaterial.additionalValues.at("normalTexture").TextureTexCoord();
		}

		if (rMaterial.additionalValues.find("occlusionTexture") != rMaterial.additionalValues.end())
		{
			materialShaderData.uiOcclusionTextureIndex = static_cast<uint8_t>(rMaterial.additionalValues.at("occlusionTexture").TextureIndex());
			LOG(kDefault, kVerbose, "  occlusionTexture: {}", materialShaderData.uiOcclusionTextureIndex);
			materialShaderData.iOcclusionTextureSet = rMaterial.additionalValues.at("occlusionTexture").TextureTexCoord();
		}

		if (rMaterial.additionalValues.find("emissiveTexture") != rMaterial.additionalValues.end())
		{
			materialShaderData.uiEmissiveTextureIndex = static_cast<uint8_t>(rMaterial.additionalValues.at("emissiveTexture").TextureIndex());
			LOG(kDefault, kVerbose, "  emissiveTexture: {}", materialShaderData.uiEmissiveTextureIndex);
			materialShaderData.iEmissiveTextureSet = rMaterial.additionalValues.at("emissiveTexture").TextureTexCoord();
		}

		// Mark transparent materials with fAlphaMask >= 2.0 for runtime two-pass rendering
		if (rMaterial.alphaMode == "BLEND")
		{
			materialShaderData.fAlphaMask = 2.0f;
		}
		else
		{
			materialShaderData.fAlphaMask = 0.0f;
		}
		materialShaderData.fAlphaMaskCutoff = static_cast<float>(rMaterial.alphaCutoff);

		materialShaderDatas[static_cast<size_t>(i)] = materialShaderData;
	}
}

void ExportScene::WriteAnimationSection(const tinygltf::Model& rGltfModel, const std::vector<common::MaterialInfo>& rMaterialInfos, common::ChunkHeader* pHeader)
{
	LOG(kDefault, kDebug, "Animation export: {} skins, {} animations", std::ssize(rGltfModel.skins), std::ssize(rGltfModel.animations));
	if (std::ssize(rGltfModel.skins) > 0)
	{
		LOG(kDefault, kDebug, "  Skin 0 has {} joints", std::ssize(rGltfModel.skins.at(0).joints));
	}
	int64_t iTotalChannels = 0;
	for (const tinygltf::Animation& rAnimation : rGltfModel.animations)
	{
		iTotalChannels += std::ssize(rAnimation.channels);
	}
	LOG(kDefault, kDebug, "  Total animation channels in glTF: {}", iTotalChannels);

	bool bUseSkeletalAnimation = DetermineAnimationPath(rGltfModel);

	LOG(kDefault, kDebug, "  Using {} animation path", bUseSkeletalAnimation ? "SKELETAL" : "NODE-BASED");

	SkeletonData skeletonData = LoadSkeletonData(rGltfModel);

	LOG(kDefault, kDebug, "  {} nodes in skeleton, {} skin joints", skeletonData.skeleton.uiNodeCount, skeletonData.skeleton.uiSkinJointCount);

	std::vector<common::AnimationClip> animations;
	std::vector<common::AnimationChannel> channels;
	std::vector<common::AnimationKeyframe> keyframes;
	std::vector<common::AnimationKeyframeCubic> cubicKeyframes;
	AnimationOutput animationOutput {.rAnimations = animations, .rChannels = channels, .rKeyframes = keyframes, .rCubicKeyframes = cubicKeyframes};
	LoadAnimations(rGltfModel, animationOutput);
	LOG(kDefault, kDebug, "  {} animations, {} channels, {} keyframes, {} cubic keyframes", std::ssize(animations), std::ssize(channels), std::ssize(keyframes), std::ssize(cubicKeyframes));

	// Every animation channel was filtered out: export as a static scene rather than an empty clip section
	if (animations.empty())
	{
		LogFilteredChannelDiagnostics(rGltfModel);
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
			LOG(kDefault, kVerbose, "  Material {}: non-skinned, parent node {}", i, rMaterialInfos.at(i).iParentNodeIndex);
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
	int64_t iExpectedOffset = common::kiChunkDataOffset + common::SceneHeader::AnimationSectionOffset(std::ssize(rGltfModel.textures), std::ssize(rMaterialInfos));
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
	// RunExport calls this only after Export() and the chunk write both succeeded, so the marker can only ever
	// record a complete generated set. A run that reused the existing outputs rewrites the same fingerprint.
	std::optional<std::string> fingerprint = GetPreExportFingerprint();
	if (!fingerprint.has_value())
	{
		LOG(kDefault, kWarning, "Could not read generated outputs for \"{}\"; leaving no pre-export marker, so the next run re-runs pre-export", mInputPath.string());
		return;
	}
	WriteMarkerFile(GetPreExportMarkerPath(), fingerprint.value());
}

void ExportScene::CleanupOnFailure()
{
	CleanupTextureAttemptFiles();
	std::error_code errorCode;
	// A partially written output set must never be adopted as fresh by a later run.
	std::filesystem::remove(GetPreExportMarkerPath(), errorCode);
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
