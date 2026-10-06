#include "ExportModel.h"

#include "SourceReadValidation.h"


constexpr const char* kpcContext = "ExportModel::Export";


std::optional<common::ChunkFlags_t> ExportModel::Handles(const std::filesystem::directory_entry& rDirectoryEntry)
{
	return rDirectoryEntry.path().extension() == ".MODEL" ? std::optional<common::ChunkFlags_t>(common::ChunkFlags::kModel) : std::nullopt;
}

void ExportModel::Export()
{
	int64_t iStride = sizeof(common::ModelVertex);
	std::vector<uint32_t> materialIndexPositions;
	std::vector<uint16_t> indices16;
	std::vector<uint32_t> indices32;
	std::vector<std::byte> vertices;

	size_t uiMaterialCount = 0;
	size_t uiIndexCount = 0;
	size_t uiVertexCount = 0;

	int64_t iFileSize = SourceFileSize(mInputPath, kpcContext);
	std::fstream fileStream(mInputPath, std::ios::in | std::ios::binary);
	if (!fileStream)
	{
		throw std::runtime_error("ExportModel::Export failed to open source file");
	}
	RequireSourceExtent(static_cast<uintmax_t>(iFileSize), 0, sizeof(uiMaterialCount), kpcContext);
	ReadSourceBytes(fileStream, std::span<char>(reinterpret_cast<char*>(&uiMaterialCount), sizeof(uiMaterialCount)), kpcContext);

	if (!std::in_range<int64_t>(uiMaterialCount))
	{
		throw std::runtime_error("ExportModel::Export material count is not representable");
	}
	int64_t iMaterialCountValue = static_cast<int64_t>(uiMaterialCount);
	uintmax_t uiMaterialIndexBytes = MultiplySourceBytes(static_cast<uintmax_t>(iMaterialCountValue), sizeof(uint32_t), kpcContext);
	uintmax_t uiMaterialInfoBytes = MultiplySourceBytes(static_cast<uintmax_t>(iMaterialCountValue), sizeof(common::MaterialInfo), kpcContext);
	int64_t iFileOffset = sizeof(uiMaterialCount);
	RequireSourceExtent(static_cast<uintmax_t>(iFileSize), static_cast<uintmax_t>(iFileOffset), uiMaterialIndexBytes, kpcContext);
	iFileOffset = static_cast<int64_t>(AddSourceBytes(static_cast<uintmax_t>(iFileOffset), uiMaterialIndexBytes, kpcContext));
	RequireSourceExtent(static_cast<uintmax_t>(iFileSize), static_cast<uintmax_t>(iFileOffset), uiMaterialInfoBytes, kpcContext);
	materialIndexPositions.resize(uiMaterialCount);
	ReadSourceBytes(fileStream, std::span<char>(reinterpret_cast<char*>(materialIndexPositions.data()), static_cast<size_t>(uiMaterialIndexBytes)), kpcContext);
	// Skip past material info data (not needed for model export)
	SkipSourceBytes(fileStream, uiMaterialInfoBytes, kpcContext);
	iFileOffset = static_cast<int64_t>(AddSourceBytes(static_cast<uintmax_t>(iFileOffset), uiMaterialInfoBytes, kpcContext));
	int64_t iCountsBytes = static_cast<int64_t>(AddSourceBytes(sizeof(uiIndexCount), sizeof(uiVertexCount), kpcContext));
	RequireSourceExtent(static_cast<uintmax_t>(iFileSize), static_cast<uintmax_t>(iFileOffset), static_cast<uintmax_t>(iCountsBytes), kpcContext);
	ReadSourceBytes(fileStream, std::span<char>(reinterpret_cast<char*>(&uiIndexCount), sizeof(uiIndexCount)), kpcContext);
	ReadSourceBytes(fileStream, std::span<char>(reinterpret_cast<char*>(&uiVertexCount), sizeof(uiVertexCount)), kpcContext);
	iFileOffset = static_cast<int64_t>(AddSourceBytes(static_cast<uintmax_t>(iFileOffset), static_cast<uintmax_t>(iCountsBytes), kpcContext));

	if (!std::in_range<int64_t>(uiVertexCount))
	{
		throw std::runtime_error("ExportModel::Export vertex count is not representable");
	}
	bool bUsesU16Indices = common::ModelHeader::UsesU16Indices(static_cast<int64_t>(uiVertexCount));
	int64_t iIndexElementSize = bUsesU16Indices ? sizeof(uint16_t) : sizeof(uint32_t);
	if (!std::in_range<int64_t>(uiIndexCount))
	{
		throw std::runtime_error("ExportModel::Export index count is not representable");
	}
	uintmax_t uiIndexBytes = MultiplySourceBytes(static_cast<uintmax_t>(uiIndexCount), static_cast<size_t>(iIndexElementSize), kpcContext);
	RequireSourceExtent(static_cast<uintmax_t>(iFileSize), static_cast<uintmax_t>(iFileOffset), uiIndexBytes, kpcContext);
	int64_t iVertexOffset = static_cast<int64_t>(AddSourceBytes(static_cast<uintmax_t>(iFileOffset), uiIndexBytes, kpcContext));
	uintmax_t uiVertexBytes = MultiplySourceBytes(static_cast<uintmax_t>(uiVertexCount), sizeof(common::ModelVertex), kpcContext);
	RequireSourceExtent(static_cast<uintmax_t>(iFileSize), static_cast<uintmax_t>(iVertexOffset), uiVertexBytes, kpcContext);
	int64_t iExpectedFileSize = static_cast<int64_t>(AddSourceBytes(static_cast<uintmax_t>(iVertexOffset), uiVertexBytes, kpcContext));
	if (iExpectedFileSize != iFileSize)
	{
		throw std::runtime_error("ExportModel::Export source file has trailing data");
	}
	if (bUsesU16Indices)
	{
		indices16.resize(uiIndexCount);
		ReadSourceBytes(fileStream, std::span<char>(reinterpret_cast<char*>(indices16.data()), static_cast<size_t>(uiIndexBytes)), kpcContext);
	}
	else
	{
		indices32.resize(uiIndexCount);
		ReadSourceBytes(fileStream, std::span<char>(reinterpret_cast<char*>(indices32.data()), static_cast<size_t>(uiIndexBytes)), kpcContext);
	}
	// Trust boundary: a decoded index at or past the vertex count would read outside the packed vertex
	// buffer at draw time, and the runtime validates only counts and byte extents.
	for (int64_t i = 0; i < static_cast<int64_t>(uiIndexCount); ++i)
	{
		int64_t iIndex = static_cast<int64_t>(bUsesU16Indices ? indices16.at(static_cast<size_t>(i)) : indices32.at(static_cast<size_t>(i)));
		if (iIndex >= static_cast<int64_t>(uiVertexCount))
		{
			throw std::runtime_error("ExportModel::Export index is out of range");
		}
	}
	vertices.resize(static_cast<size_t>(uiVertexBytes));
	ReadSourceBytes(fileStream, std::span<char>(reinterpret_cast<char*>(vertices.data()), static_cast<size_t>(uiVertexBytes)), kpcContext);
	fileStream.close();

	if (uiIndexBytes > static_cast<uintmax_t>(std::numeric_limits<int64_t>::max()) - (common::kiAlignmentBytes - 1))
	{
		throw std::runtime_error("ExportModel::Export index data size overflow");
	}
	int64_t iIndexCount = static_cast<int64_t>(uiIndexCount);
	int64_t iIndicesSize = std::ssize(indices16) > 0
		? common::ModelHeader::VerticesOffset(iIndexCount, sizeof(uint16_t))
		: common::ModelHeader::VerticesOffset(iIndexCount, sizeof(uint32_t));
	if (iIndicesSize < 0 || uiVertexBytes > static_cast<uintmax_t>(std::numeric_limits<int64_t>::max() - iIndicesSize))
	{
		throw std::runtime_error("ExportModel::Export model data size overflow");
	}
	int64_t iDataSize = iIndicesSize + static_cast<int64_t>(uiVertexBytes);
	int64_t iMaximumDataSize = std::numeric_limits<int64_t>::max() - common::kiChunkDataOffset - (common::kiAlignmentBytes - 1);
	if (iDataSize > iMaximumDataSize)
	{
		throw std::runtime_error("ExportModel::Export chunk data size overflow");
	}
	auto [pHeader, dataSpan] = AllocateHeaderAndData(iDataSize);
	pHeader->modelHeader.iIndexCount = iIndexCount;
	pHeader->modelHeader.iVertexCount = static_cast<int64_t>(uiVertexCount);
	pHeader->modelHeader.iStride = iStride;
	if (std::ssize(indices16) > 0)
	{
		std::memcpy(dataSpan.data(), indices16.data(), common::VectorByteSize(indices16));
	}
	else
	{
		std::memcpy(dataSpan.data(), indices32.data(), common::VectorByteSize(indices32));
	}
	std::memcpy(dataSpan.data() + iIndicesSize, vertices.data(), common::VectorByteSize(vertices));
}
