#pragma once

namespace tinygltf { class Model; class Node; struct Material; }

// Hash function for the (originalMaterial, nodeIndex, hasSkinning) effective-material key
struct MaterialNodeKeyHash
{
	size_t operator()(const std::tuple<int64_t, int64_t, bool>& rKey) const
	{
		return std::hash<int64_t>()(std::get<0>(rKey)) ^ (std::hash<int64_t>()(std::get<1>(rKey)) << 1) ^ (std::get<2>(rKey) ? 0x9e3779b9ui32 : 0ui32);
	}
};

using MaterialNodeMap = std::unordered_map<std::tuple<int64_t, int64_t, bool>, int64_t, MaterialNodeKeyHash>;

struct Material
{
	std::vector<uint32_t> indexBuffer;
};

struct Parent
{
	const Parent* pParent = nullptr;
	XMMATRIX matNode {};
	int64_t iNodeIndex = -1;
};

// Tracks per-material skinning metadata during export
struct MaterialNodeInfo
{
	bool bHasSkinning = false;  // Deformation mode of every primitive routed to this material
	int64_t iNodeIndex = -1;        // Node index of the mesh contributing to this material
	XMMATRIX matMeshWorld = XMMatrixIdentity();  // World transform of mesh at bind pose (accumulated matLocal)
	int64_t iOriginalMaterialIndex = -1;  // Original glTF material index (for split materials)
};

XMMATRIX ComputeNodeWorldTransform(int64_t iNodeIndex, const tinygltf::Model& rModel, const std::unordered_map<int64_t, int64_t>& rNodeParentMap);

// Per-scene state threaded through every recursive LoadVertices call.
// rMaterialNodeMap keys entries by glTF material, mesh node, and skinning mode,
// so each draw uses one mesh transform and one deformation path.
struct LoadVerticesContext
{
	std::vector<common::ModelVertex>& rVertices;
	std::vector<Material>& rMaterials;
	std::vector<MaterialNodeInfo>& rMaterialNodeInfos;
	MaterialNodeMap& rMaterialNodeMap;
	bool bHasSkeleton = false;
};

void LoadVertices(const Parent* pParent, int64_t iCurrentNodeIndex, const tinygltf::Node& rNode, const tinygltf::Model& rModel, LoadVerticesContext& rContext);

bool IsNonOcclusionUse(int64_t iIndex, const tinygltf::Material& rMaterial);
bool IsOcclusion(int64_t iIndex, const tinygltf::Material& rMaterial);
bool IsNormal(int64_t iIndex, const tinygltf::Material& rMaterial);
