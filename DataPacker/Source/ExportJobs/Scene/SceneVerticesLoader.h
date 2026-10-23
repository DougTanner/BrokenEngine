#pragma once

#include "SceneExportData.h"

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

struct Parent
{
	const Parent* pParent = nullptr;
	XMMATRIX matNode {};
	int64_t iNodeIndex = -1;
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
	std::string_view sourcePath;
	bool bHasSkeleton = false;
};

void LoadVertices(const Parent* pParent, int64_t iCurrentNodeIndex, const tinygltf::Node& rNode, const tinygltf::Model& rModel, LoadVerticesContext& rContext);

bool IsNonOcclusionUse(int64_t iIndex, const tinygltf::Material& rMaterial);
bool IsOcclusion(int64_t iIndex, const tinygltf::Material& rMaterial);
bool IsNormal(int64_t iIndex, const tinygltf::Material& rMaterial);

int64_t ResolveEffectiveMaterial(std::vector<Material>& rMaterials, std::vector<MaterialNodeInfo>& rMaterialNodeInfos, MaterialNodeMap& rMaterialNodeMap, int64_t iOriginalMaterial, int64_t iCurrentNodeIndex, bool bHasSkinning, const XMMATRIX& rMatMeshWorld);
void AccumulateDrawnBounds(const std::vector<common::ModelVertex>& rVertices, const std::vector<Material>& rMaterials, const std::vector<MaterialNodeInfo>& rMaterialNodeInfos, const std::vector<XMMATRIX>& rJointMatrices, bool bMeshLocal, XMVECTOR& rVecMin, XMVECTOR& rVecMax);
float BoundsLongestExtent(FXMVECTOR vecMin, FXMVECTOR vecMax);
XMVECTOR BoundsCenter(FXMVECTOR vecMin, FXMVECTOR vecMax);
void SetMaterialTransform(const MaterialNodeInfo& rInfo, bool bHasSkeleton, const XMMATRIX& rMatNodeWorld, common::MaterialInfo& rMaterialInfo);
