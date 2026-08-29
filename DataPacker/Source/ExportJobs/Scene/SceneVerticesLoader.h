#pragma once

namespace tinygltf { class Model; class Node; struct Material; }

// Hash function for the (originalMaterial, nodeIndex, hasSkinning) effective-material key
struct MaterialNodeKeyHash
{
	size_t operator()(const std::tuple<int, int, bool>& rKey) const
	{
		return std::hash<int>()(std::get<0>(rKey)) ^ (std::hash<int>()(std::get<1>(rKey)) << 1) ^ (std::get<2>(rKey) ? 0x9e3779b9u : 0u);
	}
};

using MaterialNodeMap = std::unordered_map<std::tuple<int, int, bool>, int, MaterialNodeKeyHash>;

struct Material
{
	std::vector<uint32_t> indexBuffer;
};

struct Parent
{
	Parent* pParent = nullptr;
	XMMATRIX matNode {};
	int iNodeIndex = -1;
};

// Tracks per-material skinning metadata during export
struct MaterialNodeInfo
{
	bool bHasSkinning = false;  // Deformation mode of every primitive routed to this material
	int iNodeIndex = -1;        // Node index of the mesh contributing to this material
	XMMATRIX matMeshWorld = XMMatrixIdentity();  // World transform of mesh at bind pose (accumulated matLocal)
	int iOriginalMaterialIndex = -1;  // Original glTF material index (for split materials)
};

XMMATRIX ComputeNodeWorldTransform(int iNodeIndex, const tinygltf::Model& rModel, const std::unordered_map<int, int>& rNodeParentMap);

// Per-scene state threaded through every recursive LoadVertices call.
// rMaterialNodeMap: tracks (originalMaterial, nodeIndex, hasSkinning) -> effectiveMaterialIndex for handling primitives that share a material but come from
// different mesh nodes; the deformation bit is part of the identity because one draw must not mix deformation modes
struct LoadVerticesContext
{
	std::vector<common::ModelVertex>& rVertices;
	std::vector<Material>& rMaterials;
	std::vector<MaterialNodeInfo>& rMaterialNodeInfos;
	MaterialNodeMap& rMaterialNodeMap;
	bool bHasSkeleton;
};

void LoadVertices(Parent* pParent, int iCurrentNodeIndex, const tinygltf::Node& rNode, const tinygltf::Model& rModel, LoadVerticesContext& rContext);

bool IsNonOcclusionUse(int64_t iIndex, const tinygltf::Material& rMaterial);
bool IsOcclusion(int64_t iIndex, const tinygltf::Material& rMaterial);
bool IsNormal(int64_t iIndex, const tinygltf::Material& rMaterial);
