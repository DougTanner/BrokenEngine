#pragma once

#include "SceneExportData.h"

class AssimpSceneDecoder
{
public:

	AssimpSceneDecoder(const std::filesystem::path& rPath, SceneExportData& rData);
	void DecodeMaterialShaderDatas(SceneExportData& rData) const;

	std::vector<std::filesystem::path> mDependencies;

private:

	std::vector<common::MaterialShaderData> mSourceMaterialShaderDatas;
};
