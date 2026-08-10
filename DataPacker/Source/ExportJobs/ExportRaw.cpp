#include "ExportRaw.h"

std::optional<common::ChunkFlags_t> ExportRaw::Handles(const std::filesystem::directory_entry& rDirectoryEntry)
{
	if (!rDirectoryEntry.is_regular_file())
	{
		return std::nullopt;
	}

	// Check if any parent directory is named "Raw"
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
	// Read file size
	std::error_code fileSizeError;
	const uintmax_t uiFileSize = std::filesystem::file_size(mInputPath, fileSizeError);
	if (fileSizeError)
	{
		throw std::runtime_error("ExportRaw::Export invalid source file size");
	}

	const uintmax_t uiMaximumDataSize = static_cast<uintmax_t>(std::numeric_limits<int64_t>::max())
		- static_cast<uintmax_t>(common::kiChunkDataOffset)
		- static_cast<uintmax_t>(common::kiAlignmentBytes - 1);
	if (uiFileSize > uiMaximumDataSize)
	{
		throw std::runtime_error("ExportRaw::Export source file size overflow");
	}

	std::fstream fileStream(mInputPath, std::ios::in | std::ios::binary);
	if (!fileStream)
	{
		throw std::runtime_error("ExportRaw::Export failed to open source file");
	}

	// Allocate header and data
	auto [pHeader, dataSpan] = AllocateHeaderAndData(static_cast<int64_t>(uiFileSize));

	// Copy file contents verbatim
	if (uiFileSize > 0)
	{
		const std::streamsize iFileSize = static_cast<std::streamsize>(uiFileSize);
		fileStream.read(reinterpret_cast<char*>(dataSpan.data()), iFileSize);
		if (!fileStream || fileStream.gcount() != iFileSize)
		{
			throw std::runtime_error("ExportRaw::Export failed to read complete source file");
		}
	}
}
