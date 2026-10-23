#pragma once

#include "SceneExportData.h"

namespace tinygltf { class Model; }

std::optional<std::string> ReadGlbJson(const std::filesystem::path& rPath);

class GltfSceneDecoder
{
public:
	GltfSceneDecoder(const std::filesystem::path& rPath, bool bBinary, SceneExportData& rData);
	~GltfSceneDecoder();

	static void ValidateSource(const std::filesystem::path& rPath, bool bBinary);
	void DecodeTextureFormats(SceneExportData& rData) const;
	bool SetupSkeletonAndMaterials() const;
	void ValidateSkinAnimation() const;
	void DecodeVertices(bool bHasSkeleton, SceneExportData& rData) const;
	void DecodeMaterialInfos(bool bHasSkeleton, SceneExportData& rData) const;
	void ValidateSamplers() const;
	void DecodeMaterialShaderDatas(SceneExportData& rData) const;
	void DecodeAnimationSection(SceneExportData& rData) const;

private:
	std::unique_ptr<tinygltf::Model> mpGltfModel;
	std::string mSourcePath;
};
