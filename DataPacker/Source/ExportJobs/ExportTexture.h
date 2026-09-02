#pragma once

#include "ExportJob.h"

class ExportTexture : public ExportJob
{
public:

	static inline constexpr std::string_view kName = "Texture";

	static std::optional<common::ChunkFlags_t> Handles(const std::filesystem::directory_entry& rDirectoryEntry);

	// The per-job .chunk cache stores the encoded chunk, so a compression change must dirty every cache
	// to force re-export; the type-wide dirty check can otherwise stream a clean per-job chunk verbatim.
	static constexpr int64_t kiVersion = Version(13);

	ExportTexture(common::ChunkFlags_t rChunkFlags, const std::filesystem::path& rFile)
	: ExportJob(rChunkFlags, rFile, kiVersion)
	{
	}

	virtual ~ExportTexture() = default;

protected:

	virtual void Export() override;

private:

	void ProcessKtxCubemap();
	void ProcessRawTexture(VkFormat vkFormat);
	void ProcessLiveCubemap(VkFormat vkFormat);
	void ProcessRegularTexture(VkFormat vkFormat);
};
