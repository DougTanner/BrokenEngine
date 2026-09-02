#pragma once

#include "ExportJob.h"

class ExportRaw : public ExportJob
{
public:

	static inline constexpr std::string_view kName = "Raw";

	static std::optional<common::ChunkFlags_t> Handles(const std::filesystem::directory_entry& rDirectoryEntry);

	static constexpr int64_t kiVersion = Version(1);

	ExportRaw(common::ChunkFlags_t rChunkFlags, const std::filesystem::path& rFile)
	: ExportJob(rChunkFlags, rFile, kiVersion)
	{
	}

	virtual ~ExportRaw() = default;

protected:

	virtual void Export() override;
};
