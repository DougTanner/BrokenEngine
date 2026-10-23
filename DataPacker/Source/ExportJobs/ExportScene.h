#pragma once

class GltfSceneDecoder;
class AssimpSceneDecoder;

#include "Scene/SceneExportData.h"
#include "ExportJob.h"

class ExportScene : public ExportJob
{
public:

	static inline constexpr std::string_view kName = "Scene";

	static std::optional<common::ChunkFlags_t> Handles(const std::filesystem::directory_entry& rDirectoryEntry);

	// Payload-struct sizes fold in so size-changing layout edits auto-dirty cached chunks and the .PreExport marker; same-size layout or semantic changes need the raw version bumped
	static constexpr int64_t kiVersion = Version(70
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

	ExportScene(common::ChunkFlags_t rChunkFlags, const std::filesystem::path& rFile);

	~ExportScene() override = default;

	bool CheckDirty(const std::filesystem::path& rPackFile) override;

protected:

	std::string GetInputFingerprint() const override;
	void Export() override;
	void UpdateCacheMetadata() override;

private:

	std::filesystem::path GetBlenderExportMarkerPath() const;
	std::filesystem::path GetSceneDependencyMarkerPath() const;
	bool SceneDependenciesDirty() const;
	void CaptureSceneDependencies(const std::vector<std::filesystem::path>& rDependencies);
	std::filesystem::path GetPreExportMarkerPath() const;
	std::optional<std::string> GetPreExportFingerprint() const;
	std::filesystem::path GetTextureIntermediatePath(int64_t iTextureIndex, VkFormat vkFormat) const;
	std::filesystem::path GetTextureIntermediateStagePath(int64_t iStageIndex) const;

	void PreExport(const GltfSceneDecoder& rDecoder, SceneExportData& rData);
	void MainExport(const GltfSceneDecoder* pGltfDecoder, const AssimpSceneDecoder* pAssimpDecoder, SceneExportData& rData);

	void ProcessTextures(const SceneExportData& rData);
	void CleanupTextureAttemptFiles();
	void OptimizeMeshes(std::vector<Material>& rMaterials, std::vector<common::ModelVertex>& rVertices);
	void WriteModelFile(const std::vector<Material>& rMaterials, const std::vector<common::MaterialInfo>& rMaterialInfos, std::vector<common::ModelVertex>& rVertices);

	void ReadMaterialInfosFromModel(const std::filesystem::path& rModelPath, std::span<uint32_t> indexStarts, std::vector<common::MaterialInfo>& rMaterialInfos);
	void WriteAnimationSection(const SceneExportData& rData, common::ChunkHeader* pHeader);

	void CleanupOnFailure() override;

	std::filesystem::path mBlenderSourcePath;
	std::string mSceneDependencyMetadata;
	std::vector<std::filesystem::path> mIntermediateFiles;
	std::vector<std::filesystem::path> mTextureAttemptFiles;
	std::vector<std::filesystem::path> mPublishedTextureFiles;
	bool mbNeedsPreExport = false;
};
