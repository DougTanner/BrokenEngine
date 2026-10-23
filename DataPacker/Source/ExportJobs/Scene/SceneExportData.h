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
	int64_t iOriginalMaterialIndex = -1;  // Original glTF material index (for split materials)
};

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
