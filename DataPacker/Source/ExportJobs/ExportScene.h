#pragma once

namespace tinygltf { class Model; }

#include "Scene/SceneVerticesLoader.h"
#include "ExportJob.h"

class ExportScene : public ExportJob
{
public:

	static inline constexpr std::string_view kName = "Scene";

	static std::optional<common::ChunkFlags_t> Handles(const std::filesystem::directory_entry& rDirectoryEntry);

	// Payload-struct sizes fold in so size-changing layout edits auto-dirty cached chunks and the .PreExport marker; same-size reorders need the raw version bumped
	static constexpr int64_t kiVersion = Version(68
		+ sizeof(common::MaterialShaderData)
		+ sizeof(common::AnimationHeader)
		+ sizeof(common::Skeleton)
		+ sizeof(common::ModelNode)
		+ sizeof(common::AnimationClip)
		+ sizeof(common::MaterialInfo)
		+ sizeof(common::AnimationChannel)
		+ sizeof(common::AnimationKeyframe)
		+ sizeof(common::AnimationKeyframeCubic)
		+ sizeof(common::ModelVertex));

	ExportScene(common::ChunkFlags_t rChunkFlags, const std::filesystem::path& rFile)
	: ExportJob(rChunkFlags, rFile, kiVersion)
	{
	}

	~ExportScene() override = default;

	virtual bool CheckDirty(const std::filesystem::path& rPackFile) override;

protected:

	virtual std::string GetInputFingerprint() const override;
	virtual void Export() override;
	virtual void UpdateCacheMetadata() override;

private:

	tinygltf::Model LoadGltfModel();
	std::filesystem::path GetPreExportMarkerPath() const;
	std::optional<std::string> GetPreExportFingerprint() const;
	std::filesystem::path GetTextureIntermediatePath(int64_t iTextureIndex, VkFormat vkFormat) const;
	std::filesystem::path GetTextureIntermediateStagePath(int64_t iStageIndex) const;

	void PreExport(const tinygltf::Model& rGltfModel);
	void MainExport(const tinygltf::Model& rGltfModel);

	void ProcessTextures(const tinygltf::Model& rGltfModel);
	void CleanupTextureAttemptFiles();
	bool SetupSkeletonAndMaterials(const tinygltf::Model& rGltfModel);
	void LoadVerticesAndOptimizeMeshes(const tinygltf::Model& rGltfModel, bool bHasSkeleton, std::vector<Material>& rMaterials, std::vector<MaterialNodeInfo>& rMaterialNodeInfos, std::vector<common::ModelVertex>& rVertices);
	void BuildMaterialInfos(const tinygltf::Model& rGltfModel, bool bHasSkeleton, const std::vector<Material>& rMaterials, const std::vector<MaterialNodeInfo>& rMaterialNodeInfos, std::vector<common::MaterialInfo>& rMaterialInfos);
	void WriteModelFile(const std::vector<Material>& rMaterials, const std::vector<common::MaterialInfo>& rMaterialInfos, std::vector<common::ModelVertex>& rVertices);

	void ReadMaterialInfosFromModel(const std::filesystem::path& rModelPath, std::span<uint32_t> indexStarts, std::vector<common::MaterialInfo>& rMaterialInfos);
	void FillMaterialShaderDatas(const tinygltf::Model& rGltfModel, const std::vector<common::MaterialInfo>& rMaterialInfos, std::span<common::MaterialShaderData> materialShaderDatas);
	void WriteAnimationSection(const tinygltf::Model& rGltfModel, const std::vector<common::MaterialInfo>& rMaterialInfos, common::ChunkHeader* pHeader);

	void CleanupOnFailure() override;

	std::vector<std::filesystem::path> mIntermediateFiles;
	std::vector<std::filesystem::path> mTextureAttemptFiles;
	std::vector<std::filesystem::path> mPublishedTextureFiles;
	bool mbNeedsPreExport = false;
};
