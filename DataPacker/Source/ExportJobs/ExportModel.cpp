#include "ExportModel.h"

#include "SourceReadValidation.h"

namespace
{

constexpr const char* kpcContext = "ExportModel::Export";

} // namespace

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

	const uintmax_t uiFileSize = SourceFileSize(mInputPath, kpcContext);
	std::fstream fileStream(mInputPath, std::ios::in | std::ios::binary);
	if (!fileStream)
	{
		throw std::runtime_error("ExportModel::Export failed to open source file");
	}
	RequireSourceExtent(uiFileSize, 0, sizeof(uiMaterialCount), kpcContext);
	ReadSourceBytes(fileStream, reinterpret_cast<char*>(&uiMaterialCount), sizeof(uiMaterialCount), kpcContext);

	const uintmax_t uiMaterialCountValue = static_cast<uintmax_t>(uiMaterialCount);
	if (uiMaterialCount > static_cast<size_t>(std::numeric_limits<int64_t>::max()))
	{
		throw std::runtime_error("ExportModel::Export material count is not representable");
	}
	const uintmax_t uiMaterialIndexBytes = MultiplySourceBytes(uiMaterialCountValue, sizeof(uint32_t), kpcContext);
	const uintmax_t uiMaterialInfoBytes = MultiplySourceBytes(uiMaterialCountValue, sizeof(common::MaterialInfo), kpcContext);
	uintmax_t uiFileOffset = sizeof(uiMaterialCount);
	RequireSourceExtent(uiFileSize, uiFileOffset, uiMaterialIndexBytes, kpcContext);
	uiFileOffset = AddSourceBytes(uiFileOffset, uiMaterialIndexBytes, kpcContext);
	RequireSourceExtent(uiFileSize, uiFileOffset, uiMaterialInfoBytes, kpcContext);
	materialIndexPositions.resize(uiMaterialCount);
	ReadSourceBytes(fileStream, reinterpret_cast<char*>(materialIndexPositions.data()), uiMaterialIndexBytes, kpcContext);
	// Skip past material info data (not needed for model export)
	SkipSourceBytes(fileStream, uiMaterialInfoBytes, kpcContext);
	uiFileOffset = AddSourceBytes(uiFileOffset, uiMaterialInfoBytes, kpcContext);
	const uintmax_t uiCountsBytes = AddSourceBytes(sizeof(uiIndexCount), sizeof(uiVertexCount), kpcContext);
	RequireSourceExtent(uiFileSize, uiFileOffset, uiCountsBytes, kpcContext);
	ReadSourceBytes(fileStream, reinterpret_cast<char*>(&uiIndexCount), sizeof(uiIndexCount), kpcContext);
	ReadSourceBytes(fileStream, reinterpret_cast<char*>(&uiVertexCount), sizeof(uiVertexCount), kpcContext);
	uiFileOffset = AddSourceBytes(uiFileOffset, uiCountsBytes, kpcContext);

	if (uiVertexCount > static_cast<size_t>(std::numeric_limits<int64_t>::max()))
	{
		throw std::runtime_error("ExportModel::Export vertex count is not representable");
	}
	const bool bUsesU16Indices = common::ModelHeader::UsesU16Indices(static_cast<int64_t>(uiVertexCount));
	const size_t uiIndexElementSize = bUsesU16Indices ? sizeof(uint16_t) : sizeof(uint32_t);
	if (uiIndexCount > static_cast<size_t>(std::numeric_limits<int64_t>::max()))
	{
		throw std::runtime_error("ExportModel::Export index count is not representable");
	}
	const uintmax_t uiIndexBytes = MultiplySourceBytes(static_cast<uintmax_t>(uiIndexCount), uiIndexElementSize, kpcContext);
	RequireSourceExtent(uiFileSize, uiFileOffset, uiIndexBytes, kpcContext);
	const uintmax_t uiVertexOffset = AddSourceBytes(uiFileOffset, uiIndexBytes, kpcContext);
	const uintmax_t uiVertexBytes = MultiplySourceBytes(static_cast<uintmax_t>(uiVertexCount), sizeof(common::ModelVertex), kpcContext);
	RequireSourceExtent(uiFileSize, uiVertexOffset, uiVertexBytes, kpcContext);
	const uintmax_t uiExpectedFileSize = AddSourceBytes(uiVertexOffset, uiVertexBytes, kpcContext);
	if (uiExpectedFileSize != uiFileSize)
	{
		throw std::runtime_error("ExportModel::Export source file has trailing data");
	}
	if (bUsesU16Indices)
	{
		indices16.resize(uiIndexCount);
		ReadSourceBytes(fileStream, reinterpret_cast<char*>(indices16.data()), uiIndexBytes, kpcContext);
	}
	else
	{
		indices32.resize(uiIndexCount);
		ReadSourceBytes(fileStream, reinterpret_cast<char*>(indices32.data()), uiIndexBytes, kpcContext);
	}
	// Trust boundary: a decoded index at or past the vertex count would read outside the packed vertex
	// buffer at draw time, and the runtime validates only counts and byte extents.
	for (size_t i = 0; i < uiIndexCount; ++i)
	{
		const size_t uiIndex = bUsesU16Indices ? static_cast<size_t>(indices16.at(i)) : static_cast<size_t>(indices32.at(i));
		if (uiIndex >= uiVertexCount)
		{
			throw std::runtime_error("ExportModel::Export index is out of range");
		}
	}
	vertices.resize(static_cast<size_t>(uiVertexBytes));
	ReadSourceBytes(fileStream, reinterpret_cast<char*>(vertices.data()), uiVertexBytes, kpcContext);
	fileStream.close();

	if (uiIndexBytes > static_cast<uintmax_t>(std::numeric_limits<int64_t>::max()) - (common::kiAlignmentBytes - 1))
	{
		throw std::runtime_error("ExportModel::Export index data size overflow");
	}
	const int64_t iIndexCount = static_cast<int64_t>(uiIndexCount);
	const int64_t iIndicesSize = indices16.size() > 0
		? common::ModelHeader::VerticesOffset(iIndexCount, sizeof(uint16_t))
		: common::ModelHeader::VerticesOffset(iIndexCount, sizeof(uint32_t));
	if (iIndicesSize < 0 || uiVertexBytes > static_cast<uintmax_t>(std::numeric_limits<int64_t>::max() - iIndicesSize))
	{
		throw std::runtime_error("ExportModel::Export model data size overflow");
	}
	const int64_t iDataSize = iIndicesSize + static_cast<int64_t>(uiVertexBytes);
	const uintmax_t uiMaximumDataSize = static_cast<uintmax_t>(std::numeric_limits<int64_t>::max())
		- static_cast<uintmax_t>(common::kiChunkDataOffset)
		- static_cast<uintmax_t>(common::kiAlignmentBytes - 1);
	if (static_cast<uintmax_t>(iDataSize) > uiMaximumDataSize)
	{
		throw std::runtime_error("ExportModel::Export chunk data size overflow");
	}
	auto [pHeader, dataSpan] = AllocateHeaderAndData(iDataSize);
	pHeader->modelHeader.iIndexCount = iIndexCount;
	pHeader->modelHeader.iVertexCount = static_cast<int64_t>(uiVertexCount);
	pHeader->modelHeader.iStride = iStride;
	if (indices16.size() > 0)
	{
		std::memcpy(dataSpan.data(), indices16.data(), common::VectorByteSize(indices16));
	}
	else
	{
		std::memcpy(dataSpan.data(), indices32.data(), common::VectorByteSize(indices32));
	}
	std::memcpy(dataSpan.data() + iIndicesSize, vertices.data(), common::VectorByteSize(vertices));
}
