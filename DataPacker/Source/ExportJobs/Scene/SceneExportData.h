#pragma once

struct SceneImage
{
	std::vector<uint8_t> pixels;
	std::string uri;
	int64_t iWidth = 0;
	int64_t iHeight = 0;
	int64_t iComponents = 0;
};

struct SceneTexture
{
	int64_t iSource = -1;
	VkFormat vkFormat = VK_FORMAT_UNDEFINED;
};

struct Material
{
	std::vector<uint32_t> indexBuffer;
};

// Tracks per-material skinning metadata during export
struct MaterialNodeInfo
{
	bool bHasSkinning = false;  // Deformation mode of every primitive routed to this material
	int64_t iNodeIndex = -1;        // Node index of the mesh contributing to this material
	XMMATRIX matMeshWorld = XMMatrixIdentity();  // World transform of mesh at bind pose (accumulated matLocal)
	int64_t iOriginalMaterialIndex = -1;  // Source material index; the material's own index when not split
};

inline std::vector<MaterialNodeInfo> SourceMaterialNodeInfos(int64_t iSourceMaterialCount)
{
	std::vector<MaterialNodeInfo> materialNodeInfos(static_cast<size_t>(iSourceMaterialCount));
	// std::views::enumerate cannot be used here: MSVC then reports C2872 on iter_move inside std::ranges algorithms, between
	// the enumerate iterator's friend and the customization point, in any translation unit that includes this header and
	// calls a std::ranges algorithm.
	for (int64_t i = 0; MaterialNodeInfo& rMaterialNodeInfo : materialNodeInfos)
	{
		rMaterialNodeInfo.iOriginalMaterialIndex = i;
		++i;
	}
	return materialNodeInfos;
}

struct SkeletonData
{
	common::Skeleton skeleton;  // counts only
	std::vector<common::ModelNode> nodes;
	std::vector<uint16_t> skinJointToNode;
	std::vector<XMFLOAT4X4> inverseBindMatrices;
};

struct SceneExportData
{
	std::vector<SceneImage> images;
	std::vector<SceneTexture> textures;
	int64_t iSourceMaterialCount = 0;
	int64_t iSourceAnimationCount = 0;
	std::vector<Material> materials;
	std::vector<MaterialNodeInfo> materialNodeInfos;
	std::vector<common::ModelVertex> vertices;
	std::vector<common::MaterialInfo> materialInfos;
	std::vector<common::MaterialShaderData> materialShaderDatas;
	SkeletonData skeletonData;
	std::vector<common::AnimationClip> animations;
	std::vector<common::AnimationChannel> channels;
	std::vector<common::AnimationKeyframe> keyframes;
	std::vector<common::AnimationKeyframeCubic> cubicKeyframes;
};
