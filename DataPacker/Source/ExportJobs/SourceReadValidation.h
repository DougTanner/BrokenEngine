#pragma once

// Trust-boundary read helpers shared by the export jobs that parse opaque source files field by field.
// Every throw is prefixed with the caller-supplied context so an aggregate diagnostic still names the
// job that failed.

inline int64_t SourceFileSize(const std::filesystem::path& rPath, std::string_view context)
{
	std::error_code fileSizeError;
	uintmax_t uiFileSize = std::filesystem::file_size(rPath, fileSizeError);
	if (fileSizeError || !std::in_range<std::streamoff>(uiFileSize))
	{
		throw std::runtime_error(std::format("{} invalid source file size", context));
	}
	return static_cast<int64_t>(uiFileSize);
}

inline uintmax_t MultiplySourceBytes(uintmax_t uiCount, size_t uiElementSize, std::string_view context)
{
	uintmax_t uiElementSizeValue = static_cast<uintmax_t>(uiElementSize);
	if (uiElementSizeValue == 0 || uiCount > std::numeric_limits<uintmax_t>::max() / uiElementSizeValue)
	{
		throw std::runtime_error(std::format("{} source byte count overflow", context));
	}
	return uiCount * uiElementSizeValue;
}

inline uintmax_t AddSourceBytes(uintmax_t uiLeft, uintmax_t uiRight, std::string_view context)
{
	if (uiLeft > std::numeric_limits<uintmax_t>::max() - uiRight)
	{
		throw std::runtime_error(std::format("{} source byte offset overflow", context));
	}
	return uiLeft + uiRight;
}

inline void RequireSourceExtent(uintmax_t uiFileSize, uintmax_t uiOffset, uintmax_t uiByteCount, std::string_view context)
{
	if (uiOffset > uiFileSize || uiByteCount > uiFileSize - uiOffset)
	{
		throw std::runtime_error(std::format("{} source file is truncated", context));
	}
}

inline void ReadSourceBytes(std::istream& rStream, std::span<char> data, std::string_view context)
{
	if (!std::in_range<std::streamsize>(data.size()))
	{
		throw std::runtime_error(std::format("{} source read size overflow", context));
	}
	if (data.empty())
	{
		return;
	}

	int64_t iByteCount = static_cast<int64_t>(data.size());
	rStream.read(data.data(), static_cast<std::streamsize>(iByteCount));
	if (!rStream || rStream.gcount() != iByteCount)
	{
		throw std::runtime_error(std::format("{} failed to read complete source file", context));
	}
}

inline void SkipSourceBytes(std::istream& rStream, uintmax_t uiByteCount, std::string_view context)
{
	if (!std::in_range<std::streamoff>(uiByteCount))
	{
		throw std::runtime_error(std::format("{} source seek size overflow", context));
	}
	rStream.seekg(static_cast<std::streamoff>(uiByteCount), std::ios::cur);
	if (!rStream)
	{
		throw std::runtime_error(std::format("{} failed to seek source file", context));
	}
}
