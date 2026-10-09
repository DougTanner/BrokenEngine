#include "ExportRaw.h"

std::optional<common::ChunkFlags_t> ExportRaw::Handles(const std::filesystem::directory_entry& rDirectoryEntry)
{
	if (!rDirectoryEntry.is_regular_file())
	{
		return std::nullopt;
	}

	for (const std::filesystem::path& rPart : rDirectoryEntry.path())
	{
		if (rPart == "Raw")
		{
			return common::ChunkFlags::kRaw;
		}
	}

	return std::nullopt;
}

void ExportRaw::Export()
{
	std::error_code fileSizeError;
	uintmax_t uiFileSize = std::filesystem::file_size(mInputPath, fileSizeError);
	if (fileSizeError)
	{
		throw std::runtime_error("ExportRaw::Export invalid source file size");
	}

	ASSERT(uiFileSize <= static_cast<uintmax_t>(std::numeric_limits<int64_t>::max() - common::kiChunkDataOffset - (common::kiAlignmentBytes - 1)));

	std::fstream fileStream(mInputPath, std::ios::in | std::ios::binary);
	if (!fileStream)
	{
		throw std::runtime_error("ExportRaw::Export failed to open source file");
	}

	auto [pHeader, dataSpan] = AllocateHeaderAndData(static_cast<int64_t>(uiFileSize));

	if (uiFileSize > 0)
	{
		int64_t iFileSize = static_cast<int64_t>(uiFileSize);
		fileStream.read(reinterpret_cast<char*>(dataSpan.data()), static_cast<std::streamsize>(iFileSize));
		if (!fileStream || fileStream.gcount() != iFileSize)
		{
			throw std::runtime_error("ExportRaw::Export failed to read complete source file");
		}
	}
}
