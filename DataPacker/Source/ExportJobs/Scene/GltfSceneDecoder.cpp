#include "GltfSceneDecoder.h"

#include "ExportJobs/SourceReadValidation.h"
#include "SceneAnimationLoader.h"
#include "SceneSkeletonLoader.h"
#include "SceneVerticesLoader.h"

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
			std::unreachable();
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
			std::unreachable();
	}
}

static std::vector<VkFormat> ComputeTextureFormats(const tinygltf::Model& rModel)
{
	std::vector<VkFormat> textureFormats;
	textureFormats.reserve(static_cast<size_t>(std::ssize(rModel.textures)));
	for (const tinygltf::Texture& rTexture : rModel.textures)
	{
		// Textures sharing a source image share one generated intermediate, so a normal use through any of them
		// forces the two-channel format: the shader samples .rg and reconstructs Z from them. The single-channel
		// format likewise requires that no texture in the group carries a use needing more channels.
		bool bNormal = false;
		bool bOcclusion = false;
		bool bNonOcclusionUse = false;
		for (int64_t j = 0; j < std::ssize(rModel.textures); ++j)
		{
			if (rModel.textures.at(static_cast<size_t>(j)).source != rTexture.source)
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

// Uses the same loader as DecodeAnimationSection to detect whether a clip survives filtering.
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

std::optional<std::string> ReadGlbJson(const std::filesystem::path& rPath)
{
	try
	{
		int64_t iFileSize = SourceFileSize(rPath, kpcContext);
		uint32_t puiHeader[5] {};
		RequireSourceExtent(static_cast<uintmax_t>(iFileSize), 0, sizeof(puiHeader), kpcContext);
		std::fstream fileStreamIn(rPath, std::ios::in | std::ios::binary);
		ReadSourceBytes(fileStreamIn, std::span<char>(reinterpret_cast<char*>(puiHeader), sizeof(puiHeader)), kpcContext);
		if (puiHeader[0] != 0x46546C67 || puiHeader[1] != 2 || puiHeader[2] != static_cast<uintmax_t>(iFileSize) || puiHeader[4] != 0x4E4F534A)
		{
			return std::nullopt;
		}
		RequireSourceExtent(static_cast<uintmax_t>(iFileSize), sizeof(puiHeader), puiHeader[3], kpcContext);
		std::string json(static_cast<size_t>(puiHeader[3]), '\0');
		ReadSourceBytes(fileStreamIn, std::span<char>(json.data(), json.size()), kpcContext);
		return json;
	}
	catch (const std::runtime_error&)
	{
		// CheckDirty has no per-asset catch; defer binary read failures to the export-time loader.
		return std::nullopt;
	}
}

static tinygltf::Model LoadGltfModel(const std::filesystem::path& rPath, bool bBinary)
{
	tinygltf::Model gltfModel;

	std::string filename = rPath.string();
	if (bBinary && !ReadGlbJson(rPath).has_value())
	{
		throw std::runtime_error(std::format("Failed to load GLTF model '{}': invalid GLB framing", filename));
	}

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

// Inserts node 0 as the default scene's only root, translating and uniformly scaling the default-pose bounds of every
// drawn vertex to center (0, 0, 0) with longest extent 2. Node 0 is unanimated: a static export bakes it into its
// vertices and a skeleton export ships it in its hierarchy. Every existing node reference shifts by one.
static void InsertNormalizationRootNode(tinygltf::Model& rGltfModel, std::string_view sourcePath)
{
	tinygltf::Scene& rDefaultScene = rGltfModel.scenes.at(static_cast<size_t>(rGltfModel.defaultScene > -1 ? rGltfModel.defaultScene : 0));

	// With a skeleton, LoadVertices leaves every vertex mesh-local and records each material's mesh world, so the
	// default pose below is evaluated the way the runtime draws it.
	std::vector<common::ModelVertex> vertices;
	std::vector<Material> materials(static_cast<size_t>(std::ssize(rGltfModel.materials)));
	std::vector<MaterialNodeInfo> materialNodeInfos(static_cast<size_t>(std::ssize(rGltfModel.materials)));
	MaterialNodeMap materialNodeMap;
	LoadVerticesContext loadContext {.rVertices = vertices, .rMaterials = materials, .rMaterialNodeInfos = materialNodeInfos, .rMaterialNodeMap = materialNodeMap, .sourcePath = sourcePath, .bHasSkeleton = true, };
	for (int64_t iNodeIndex : rDefaultScene.nodes)
	{
		Parent parent {.pParent = nullptr, .matNode = XMMatrixIdentity(), .iNodeIndex = -1};
		LoadVertices(&parent, iNodeIndex, rGltfModel.nodes.at(static_cast<size_t>(iNodeIndex)), rGltfModel, loadContext);
	}

	// The runtime cancels a skinned material's mesh world (AnimationData::EvaluateMaterial), so a skinned vertex
	// lands at its weighted inverse-bind × joint-world blend.
	std::vector<XMMATRIX> jointMatrices;
	if (std::ranges::any_of(materialNodeInfos, [](const MaterialNodeInfo& rInfo)
	{
		return rInfo.bHasSkinning;
	}))
	{
		SkeletonData skeletonData = LoadSkeletonData(rGltfModel);
		std::unordered_map<int64_t, int64_t> nodeParentMap = BuildNodeParentMap(rGltfModel);
		jointMatrices.reserve(static_cast<size_t>(std::ssize(skeletonData.skinJointToNode)));
		for (int64_t i = 0; i < std::ssize(skeletonData.skinJointToNode); ++i)
		{
			jointMatrices.push_back(XMLoadFloat4x4(&skeletonData.inverseBindMatrices.at(i)) * ComputeNodeWorldTransform(skeletonData.skinJointToNode.at(i), rGltfModel, nodeParentMap));
		}
	}

	XMVECTOR vecMin = XMVectorReplicate(std::numeric_limits<float>::infinity());
	XMVECTOR vecMax = XMVectorReplicate(-std::numeric_limits<float>::infinity());
	AccumulateDrawnBounds(vertices, materials, materialNodeInfos, jointMatrices, true, vecMin, vecMax);

	if (std::ranges::all_of(materials, [](const Material& rMaterial)
	{
		return rMaterial.indexBuffer.empty();
	}))
	{
		throw std::runtime_error("ExportScene default scene draws no vertices, so its model space cannot be normalized");
	}

	if (XMVector3IsNaN(vecMin) || XMVector3IsInfinite(vecMin) || XMVector3IsNaN(vecMax) || XMVector3IsInfinite(vecMax))
	{
		throw std::runtime_error("ExportScene default-pose bounds are not finite, so its model space cannot be normalized");
	}

	float fLongestExtent = BoundsLongestExtent(vecMin, vecMax);
	if (fLongestExtent == 0.0f)
	{
		throw std::runtime_error("ExportScene default-pose bounds have zero size, so its model space cannot be normalized");
	}

	XMFLOAT3A f3Center {};
	XMStoreFloat3A(&f3Center, BoundsCenter(vecMin, vecMax));
	double fScale = 2.0 / fLongestExtent;

	// Negative references remain negative, and out-of-range references remain out of range after the node shift.
	auto ShiftNodeReference = [](int& riNodeIndex)
	{
		if (riNodeIndex >= 0)
		{
			++riNodeIndex;
		}
	};
	for (tinygltf::Node& rNode : rGltfModel.nodes)
	{
		std::ranges::for_each(rNode.children, ShiftNodeReference);
	}
	for (tinygltf::Scene& rScene : rGltfModel.scenes)
	{
		std::ranges::for_each(rScene.nodes, ShiftNodeReference);
	}
	for (tinygltf::Skin& rSkin : rGltfModel.skins)
	{
		std::ranges::for_each(rSkin.joints, ShiftNodeReference);
		ShiftNodeReference(rSkin.skeleton);
	}
	for (tinygltf::Animation& rAnimation : rGltfModel.animations)
	{
		for (tinygltf::AnimationChannel& rChannel : rAnimation.channels)
		{
			ShiftNodeReference(rChannel.target_node);
		}
	}

	// NodeLocalMatrix composes scale before translation, so node 0 maps v to (v - center) * scale.
	tinygltf::Node normalizationNode;
	normalizationNode.scale = {fScale, fScale, fScale};
	normalizationNode.translation = {-f3Center.x * fScale, -f3Center.y * fScale, -f3Center.z * fScale};
	normalizationNode.children = rDefaultScene.nodes;
	rDefaultScene.nodes = {0};
	rGltfModel.nodes.insert(rGltfModel.nodes.begin(), std::move(normalizationNode));
}

GltfSceneDecoder::GltfSceneDecoder(const std::filesystem::path& rPath, bool bBinary, SceneExportData& rData)
: mpGltfModel(std::make_unique<tinygltf::Model>(LoadGltfModel(rPath, bBinary)))
, mSourcePath(rPath.string())
{
	CanonicalizeSceneSkin(*mpGltfModel);
	InsertNormalizationRootNode(*mpGltfModel, mSourcePath);

	rData.iSourceMaterialCount = std::ssize(mpGltfModel->materials);
	rData.iSourceAnimationCount = std::ssize(mpGltfModel->animations);
	rData.textures.reserve(static_cast<size_t>(std::ssize(mpGltfModel->textures)));
	for (const tinygltf::Texture& rTexture : mpGltfModel->textures)
	{
		rData.textures.push_back({.iSource = rTexture.source});
	}
	rData.images.reserve(static_cast<size_t>(std::ssize(mpGltfModel->images)));
	for (tinygltf::Image& rImage : mpGltfModel->images)
	{
		rData.images.push_back({.pixels = std::move(rImage.image), .uri = std::move(rImage.uri), .iWidth = rImage.width, .iHeight = rImage.height, .iComponents = rImage.component});
	}
}

GltfSceneDecoder::~GltfSceneDecoder() = default;

void GltfSceneDecoder::ValidateSource(const std::filesystem::path& rPath, bool bBinary)
{
	LoadGltfModel(rPath, bBinary);
}

void GltfSceneDecoder::DecodeTextureFormats(SceneExportData& rData) const
{
	std::vector<VkFormat> textureFormats = ComputeTextureFormats(*mpGltfModel);
	for (int64_t i = 0; i < std::ssize(rData.textures); ++i)
	{
		rData.textures.at(i).vkFormat = textureFormats.at(i);
	}
}

bool GltfSceneDecoder::SetupSkeletonAndMaterials() const
{
	const tinygltf::Model& rGltfModel = *mpGltfModel;
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

void GltfSceneDecoder::ValidateSkinAnimation() const
{
	const tinygltf::Model& rGltfModel = *mpGltfModel;
	// A skin alone makes bHasSkeleton true, but MainExport emits the skeleton only inside WriteAnimationSection,
	// which runs only for a surviving clip. Without one the vertices stay mesh-local and the material transforms
	// node-relative with no node matrices at runtime, so the model would render mispositioned in silence.
	if (!rGltfModel.skins.empty() && !HasSurvivingAnimationClip(rGltfModel))
	{
		throw std::runtime_error("ExportScene model declares a skin but no animation clip survives export");
	}
}

void GltfSceneDecoder::DecodeVertices(bool bHasSkeleton, SceneExportData& rData) const
{
	const tinygltf::Model& rGltfModel = *mpGltfModel;
	std::vector<Material>& rMaterials = rData.materials;
	std::vector<MaterialNodeInfo>& rMaterialNodeInfos = rData.materialNodeInfos;
	std::vector<common::ModelVertex>& rVertices = rData.vertices;
	const tinygltf::Scene& rScene = rGltfModel.scenes.at(rGltfModel.defaultScene > -1 ? rGltfModel.defaultScene : 0);
	MaterialNodeMap materialNodeMap;
	LoadVerticesContext loadContext {.rVertices = rVertices, .rMaterials = rMaterials, .rMaterialNodeInfos = rMaterialNodeInfos, .rMaterialNodeMap = materialNodeMap, .sourcePath = mSourcePath, .bHasSkeleton = bHasSkeleton, };
	for (int64_t iNodeIndex : rScene.nodes)
	{
		const tinygltf::Node& rNode = rGltfModel.nodes.at(static_cast<size_t>(iNodeIndex));
		Parent parent {.pParent = nullptr, .matNode = XMMatrixIdentity(), .iNodeIndex = -1};
		LoadVertices(&parent, iNodeIndex, rNode, rGltfModel, loadContext);
	}
	if (std::ssize(rMaterials) > std::ssize(rGltfModel.materials))
	{
		LOG(kDefault, kDebug, "  Split {} materials into {} to handle primitives from different mesh nodes or deformation modes", std::ssize(rGltfModel.materials), std::ssize(rMaterials));
	}
}

void GltfSceneDecoder::DecodeMaterialInfos(bool bHasSkeleton, SceneExportData& rData) const
{
	const tinygltf::Model& rGltfModel = *mpGltfModel;
	const std::vector<Material>& rMaterials = rData.materials;
	const std::vector<MaterialNodeInfo>& rMaterialNodeInfos = rData.materialNodeInfos;
	rData.materialInfos.resize(static_cast<size_t>(std::ssize(rMaterials)));
	std::vector<common::MaterialInfo>& rMaterialInfos = rData.materialInfos;
	// Build parent map for node hierarchy traversal (used for both skeletal and node-based)
	std::unordered_map<int64_t, int64_t> nodeParentMap = BuildNodeParentMap(rGltfModel);

	for (int64_t i = 0; i < std::ssize(rMaterialNodeInfos); ++i)
	{
		const MaterialNodeInfo& rInfo = rMaterialNodeInfos.at(i);

		XMMATRIX matNodeWorld = XMMatrixIdentity();
		if (!rInfo.bHasSkinning && rInfo.iNodeIndex >= 0 && bHasSkeleton)
		{
			matNodeWorld = ComputeNodeWorldTransform(rInfo.iNodeIndex, rGltfModel, nodeParentMap);
		}
		SetMaterialTransform(rInfo, bHasSkeleton, matNodeWorld, rMaterialInfos.at(i));
		if (rInfo.bHasSkinning)
		{
			LOG(kDefault, kVerbose, "  Material {}: skinned, mesh node {}", i, rMaterialInfos.at(i).iParentNodeIndex);
		}
		else if (rInfo.iNodeIndex >= 0 && bHasSkeleton)
		{
			LOG(kDefault, kVerbose, "  Material {}: non-skinned, parent node {}, mesh node {}", i, rInfo.iNodeIndex, rInfo.iNodeIndex);
		}
	}

	for (int64_t i = 0; i < std::ssize(rMaterials); ++i)
	{
		int64_t iOriginalMaterialIndex = rMaterialNodeInfos.at(i).iOriginalMaterialIndex >= 0 ? rMaterialNodeInfos.at(i).iOriginalMaterialIndex : i;
		const tinygltf::Material& rTinygltfMaterial = rGltfModel.materials.at(static_cast<size_t>(iOriginalMaterialIndex));
		LOG(kDefault, kVerbose, "  {}: \"{}\"{}; {} {} {} {} {} textures, {} indices{}", i, rTinygltfMaterial.name, (iOriginalMaterialIndex != i ? std::format(" (split from {})", iOriginalMaterialIndex) : ""), rTinygltfMaterial.pbrMetallicRoughness.baseColorTexture.index, rTinygltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index, rTinygltfMaterial.normalTexture.index, rTinygltfMaterial.occlusionTexture.index, rTinygltfMaterial.emissiveTexture.index, std::ssize(rMaterials.at(i).indexBuffer), (rMaterialInfos.at(i).flags & common::MaterialFlags::kSkinned) ? " (skinned)" : "");
	}
}

void GltfSceneDecoder::ValidateSamplers() const
{
	const tinygltf::Model& rGltfModel = *mpGltfModel;
	LOG(kDefault, kDebug, "Samplers: {}", std::ssize(rGltfModel.samplers));
	for (const tinygltf::Sampler& rSampler : rGltfModel.samplers)
	{
		LOG(kDefault, kVerbose, "  {} {} {} {}", ToVkFilter(rSampler.minFilter), ToVkFilter(rSampler.magFilter), ToVkSamplerAddressMode(rSampler.wrapS), ToVkSamplerAddressMode(rSampler.wrapT));
		ASSERT(ToVkSamplerAddressMode(rSampler.wrapS) == VK_SAMPLER_ADDRESS_MODE_REPEAT);
	}
}

void GltfSceneDecoder::DecodeMaterialShaderDatas(SceneExportData& rData) const
{
	const tinygltf::Model& rGltfModel = *mpGltfModel;
	const std::vector<common::MaterialInfo>& rMaterialInfos = rData.materialInfos;
	rData.materialShaderDatas.resize(static_cast<size_t>(std::ssize(rMaterialInfos)));
	std::span<common::MaterialShaderData> materialShaderDatas(rData.materialShaderDatas);
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

		materialShaderData.f3EmissiveFactor = XMFLOAT3(static_cast<float>(rMaterial.emissiveFactor.at(0)), static_cast<float>(rMaterial.emissiveFactor.at(1)), static_cast<float>(rMaterial.emissiveFactor.at(2)));

		// Only metallic-roughness workflow is supported
		ASSERT(!rMaterial.extensions.contains("KHR_materials_pbrSpecularGlossiness"));

		if (rMaterial.values.contains("baseColorFactor"))
		{
			const tinygltf::ColorValue& rColorFactor = rMaterial.values.at("baseColorFactor").ColorFactor();
			materialShaderData.f4BaseColorFactor = XMFLOAT4(static_cast<float>(rColorFactor[0]), static_cast<float>(rColorFactor[1]), static_cast<float>(rColorFactor[2]), static_cast<float>(rColorFactor[3]));
		}

		if (rMaterial.values.contains("baseColorTexture"))
		{
			materialShaderData.uiColorTextureIndex = static_cast<uint8_t>(rMaterial.values.at("baseColorTexture").TextureIndex());
			LOG(kDefault, kVerbose, "  baseColorTexture: {}", materialShaderData.uiColorTextureIndex);
			materialShaderData.iColorTextureSet = rMaterial.values.at("baseColorTexture").TextureTexCoord();
		}

		if (rMaterial.values.contains("metallicRoughnessTexture"))
		{
			materialShaderData.uiPhysicalDescriptorTextureIndex = static_cast<uint8_t>(rMaterial.values.at("metallicRoughnessTexture").TextureIndex());
			LOG(kDefault, kVerbose, "  metallicRoughnessTexture: {}", materialShaderData.uiPhysicalDescriptorTextureIndex);
			materialShaderData.iPhysicalDescriptorTextureSet = rMaterial.values.at("metallicRoughnessTexture").TextureTexCoord();
		}

		if (rMaterial.values.contains("metallicFactor"))
		{
			materialShaderData.fMetallicFactor = static_cast<float>(rMaterial.values.at("metallicFactor").Factor());
		}

		if (rMaterial.values.contains("roughnessFactor"))
		{
			materialShaderData.fRoughnessFactor = static_cast<float>(rMaterial.values.at("roughnessFactor").Factor());
		}

		if (rMaterial.additionalValues.contains("normalTexture"))
		{
			materialShaderData.uiNormalTextureIndex = static_cast<uint8_t>(rMaterial.additionalValues.at("normalTexture").TextureIndex());
			LOG(kDefault, kVerbose, "  normalTexture: {}", materialShaderData.uiNormalTextureIndex);
			materialShaderData.iNormalTextureSet = rMaterial.additionalValues.at("normalTexture").TextureTexCoord();
		}

		if (rMaterial.additionalValues.contains("occlusionTexture"))
		{
			materialShaderData.uiOcclusionTextureIndex = static_cast<uint8_t>(rMaterial.additionalValues.at("occlusionTexture").TextureIndex());
			LOG(kDefault, kVerbose, "  occlusionTexture: {}", materialShaderData.uiOcclusionTextureIndex);
			materialShaderData.iOcclusionTextureSet = rMaterial.additionalValues.at("occlusionTexture").TextureTexCoord();
			materialShaderData.fOcclusionStrength = static_cast<float>(rMaterial.occlusionTexture.strength);
		}

		if (rMaterial.additionalValues.contains("emissiveTexture"))
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

void GltfSceneDecoder::DecodeAnimationSection(SceneExportData& rData) const
{
	const tinygltf::Model& rGltfModel = *mpGltfModel;
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

	rData.skeletonData = LoadSkeletonData(rGltfModel);
	const SkeletonData& skeletonData = rData.skeletonData;

	LOG(kDefault, kDebug, "  {} nodes in skeleton, {} skin joints", skeletonData.skeleton.uiNodeCount, skeletonData.skeleton.uiSkinJointCount);

	std::vector<common::AnimationClip>& animations = rData.animations;
	std::vector<common::AnimationChannel>& channels = rData.channels;
	std::vector<common::AnimationKeyframe>& keyframes = rData.keyframes;
	std::vector<common::AnimationKeyframeCubic>& cubicKeyframes = rData.cubicKeyframes;
	AnimationOutput animationOutput {.rAnimations = animations, .rChannels = channels, .rKeyframes = keyframes, .rCubicKeyframes = cubicKeyframes};
	LoadAnimations(rGltfModel, animationOutput);
	LOG(kDefault, kDebug, "  {} animations, {} channels, {} keyframes, {} cubic keyframes", std::ssize(animations), std::ssize(channels), std::ssize(keyframes), std::ssize(cubicKeyframes));

	// Every animation channel was filtered out: export as a static scene rather than an empty clip section
	if (animations.empty())
	{
		LogFilteredChannelDiagnostics(rGltfModel);
		return;
	}
}
