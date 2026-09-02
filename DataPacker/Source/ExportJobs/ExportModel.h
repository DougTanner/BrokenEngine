#pragma once

#include "ExportJob.h"

class ExportModel : public ExportJob
{
public:

	static inline constexpr std::string_view kName = "Model";

	static std::optional<common::ChunkFlags_t> Handles(const std::filesystem::directory_entry& rDirectoryEntry);

	// Payload-struct sizes fold in so size-changing layout edits auto-dirty cached chunks; same-size reorders need the raw version bumped
	static constexpr int64_t kiVersion = Version(2 + sizeof(common::ModelVertex) + sizeof(common::MaterialInfo));

	ExportModel(common::ChunkFlags_t rChunkFlags, const std::filesystem::path& rFile)
	: ExportJob(rChunkFlags, rFile, kiVersion)
	{
	}

	virtual ~ExportModel() = default;

protected:

	virtual void Export() override;
};
