#include "ExportModel.h"

#include "FileManager.h"
#include "SourceReadValidation.h"


constexpr const char* kpcContext = "ExportModel::Export";

static std::filesystem::path GetModelMetadataPath(const std::filesystem::path& rModelPath)
{
	std::filesystem::path metadataPath = rModelPath;
	metadataPath.replace_extension();
	metadataPath += ".model.json";
	return metadataPath;
}

// The outer contour of the model's top-down triangles: the largest path of their union, so holes and specks drop out.
// Reads whichever index vector the export decoded; the other is empty. Empty when no triangle has area.
static std::vector<XMFLOAT2> BuildModelSilhouette(std::span<const XMFLOAT2> points, std::span<const uint16_t> indices16, std::span<const uint32_t> indices32)
{
	static constexpr int64_t kiClipperPrecision = 4;

	auto IndexedPoint = [&](int64_t i) -> XMFLOAT2
	{
		return points[indices16.empty() ? indices32[i] : indices16[i]];
	};

	int64_t iIndexCount = std::ssize(indices16) + std::ssize(indices32);
	Clipper2Lib::PathsD triangles;
	triangles.reserve(static_cast<size_t>(iIndexCount / 3));
	for (int64_t i = 0; i + 2 < iIndexCount; i += 3)
	{
		XMFLOAT2 f2A = IndexedPoint(i);
		XMFLOAT2 f2B = IndexedPoint(i + 1);
		XMFLOAT2 f2C = IndexedPoint(i + 2);
		double fCross = (static_cast<double>(f2B.x) - f2A.x) * (static_cast<double>(f2C.y) - f2A.y) - (static_cast<double>(f2B.y) - f2A.y) * (static_cast<double>(f2C.x) - f2A.x);
		if (fCross == 0.0)
		{
			continue;
		}
		// Mixed directions would cancel under the nonzero fill where triangles overlap.
		if (fCross < 0.0)
		{
			std::swap(f2B, f2C);
		}
		triangles.push_back({Clipper2Lib::PointD(f2A.x, f2A.y), Clipper2Lib::PointD(f2B.x, f2B.y), Clipper2Lib::PointD(f2C.x, f2C.y)});
	}

	Clipper2Lib::PathsD unioned = Clipper2Lib::Union(triangles, Clipper2Lib::FillRule::NonZero, static_cast<int>(kiClipperPrecision));
	// Holes have negative area, so the largest area is always an outer contour.
	auto it = std::ranges::max_element(unioned, {}, [](const Clipper2Lib::PathD& rPath)
	{
		return Clipper2Lib::Area(rPath);
	});
	if (it == unioned.end())
	{
		return {};
	}

	std::vector<XMFLOAT2> silhouette;
	silhouette.reserve(it->size());
	for (const Clipper2Lib::PointD& rPoint : *it)
	{
		silhouette.emplace_back(static_cast<float>(rPoint.x), static_cast<float>(rPoint.y));
	}
	return silhouette;
}


std::optional<common::ChunkFlags_t> ExportModel::Handles(const std::filesystem::directory_entry& rDirectoryEntry)
{
	return rDirectoryEntry.path().extension() == ".MODEL" ? std::optional<common::ChunkFlags_t>(common::ChunkFlags::kModel) : std::nullopt;
}

void ExportModel::Export()
{
	common::ModelHeader modelHeader {};
	common::ModelOutline outline {};
	std::vector<common::ModelHardpoint> hardpoints;
	std::filesystem::path metadataPath = GetModelMetadataPath(mInputPath);
	if (std::filesystem::exists(metadataPath))
	{
		common::ModelMetadata metadata = common::ReadModelMetadata(metadataPath);
		modelHeader.f3RotationRadians = XMFLOAT3(XMConvertToRadians(metadata.f3RotationDegrees.x), XMConvertToRadians(metadata.f3RotationDegrees.y), XMConvertToRadians(metadata.f3RotationDegrees.z));
		modelHeader.f3Scale = metadata.f3Scale;
		modelHeader.f3Offset = metadata.f3Offset;
		outline.eHitboxLevel = metadata.eHitboxLevel;
		outline.fOutlineInsetMeters = metadata.fOutlineInsetMeters;
		outline.uiNavigationObstacle = metadata.bNavigationObstacle ? 1ui32 : 0ui32;
		int64_t iMissileTubeCount = std::ranges::count(metadata.hardpoints, common::ModelHardpointKind::kMissileTube, &common::ModelHardpoint::eKind);
		int64_t iFighterLaunchBayCount = std::ssize(metadata.hardpoints) - iMissileTubeCount;
		modelHeader.uiMissileTubeCount = static_cast<uint32_t>(iMissileTubeCount);
		modelHeader.uiFighterLaunchBayCount = static_cast<uint32_t>(iFighterLaunchBayCount);
		hardpoints = std::move(metadata.hardpoints);
		// The packed sections hold missile tubes then fighter launch bays, each kind in authored order.
		std::ranges::stable_partition(hardpoints, [](common::ModelHardpointKind eKind)
		{
			return eKind == common::ModelHardpointKind::kMissileTube;
		}, &common::ModelHardpoint::eKind);
	}

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

	// Bake the outline in the same authored alignment used to render the model.
	XMMATRIX matAlignment = common::GetModelAlignment(modelHeader);
	std::vector<common::PackedModelHardpoint> packedHardpoints;
	packedHardpoints.reserve(static_cast<size_t>(std::ssize(hardpoints)));
	for (const common::ModelHardpoint& rHardpoint : hardpoints)
	{
		common::PackedModelHardpoint& rPackedHardpoint = packedHardpoints.emplace_back();
		XMStoreFloat3(&rPackedHardpoint.f3Position, XMVector3Transform(XMLoadFloat3A(&rHardpoint.f3Position), matAlignment));
		XMStoreFloat3(&rPackedHardpoint.f3Direction, XMVector3TransformNormal(XMLoadFloat3A(&rHardpoint.f3Direction), matAlignment));
		if (!std::isfinite(rPackedHardpoint.f3Position.x) || !std::isfinite(rPackedHardpoint.f3Position.y)
		 || !std::isfinite(rPackedHardpoint.f3Position.z) || !std::isfinite(rPackedHardpoint.f3Direction.x)
		 || !std::isfinite(rPackedHardpoint.f3Direction.y) || !std::isfinite(rPackedHardpoint.f3Direction.z)
		 || (rPackedHardpoint.f3Direction.x == 0.0f && rPackedHardpoint.f3Direction.y == 0.0f && rPackedHardpoint.f3Direction.z == 0.0f))
		{
			throw std::runtime_error("ExportModel::Export aligned hardpoint must be finite with nonzero direction");
		}
	}
	const common::ModelVertex* pVertices = reinterpret_cast<const common::ModelVertex*>(vertices.data());
	std::vector<XMFLOAT2> hullPoints(uiVertexCount);
	outline.fMinimumZ = std::numeric_limits<float>::max();
	outline.fMaximumZ = std::numeric_limits<float>::lowest();
	for (int64_t i = 0; i < static_cast<int64_t>(uiVertexCount); ++i)
	{
		XMFLOAT3A f3Position {};
		XMStoreFloat3A(&f3Position, XMVector3Transform(XMLoadFloat3(&pVertices[i].f3Pos), matAlignment));
		if (!std::isfinite(f3Position.x) || !std::isfinite(f3Position.y) || !std::isfinite(f3Position.z))
		{
			throw std::runtime_error("ExportModel::Export aligned vertex position is not finite");
		}
		hullPoints.at(static_cast<size_t>(i)) = {f3Position.x, f3Position.y};
		outline.fMinimumZ = std::min(outline.fMinimumZ, f3Position.z);
		outline.fMaximumZ = std::max(outline.fMaximumZ, f3Position.z);
	}
	// Runs before BuildConvexHull, whose sort leaves hullPoints out of step with the indices.
	std::vector<XMFLOAT2> silhouette;
	if (outline.eHitboxLevel == common::ModelHitboxLevel::kOutline)
	{
		silhouette = BuildModelSilhouette(hullPoints, indices16, indices32);
	}
	std::vector<XMFLOAT2> hull = common::BuildConvexHull(hullPoints);
	if (std::ssize(hull) < 3 || std::ssize(hull) > common::kiModelOutlineVertexMaximum)
	{
		throw std::runtime_error(std::format("ExportModel::Export outline has {} hull vertices; it needs 3 to {}", std::ssize(hull), common::kiModelOutlineVertexMaximum));
	}
	if (!common::IsPolygonStrictlyConvexCcw(hull))
	{
		throw std::runtime_error("ExportModel::Export outline hull is not strictly convex and counter-clockwise");
	}
	std::copy(hull.begin(), hull.end(), outline.pf2HullVertices);
	outline.iHullVertexCount = static_cast<int32_t>(std::ssize(hull));
	std::vector<XMFLOAT2> outlineVertices = outline.eHitboxLevel == common::ModelHitboxLevel::kOutline ? common::OffsetPolygon(silhouette, -outline.fOutlineInsetMeters, common::kfModelOutlineSimplifyEpsilonMeters) : hull;
	if (std::ssize(outlineVertices) < 3 || std::ssize(outlineVertices) > common::kiModelOutlineVertexMaximum)
	{
		throw std::runtime_error(std::format("ExportModel::Export outline has {} vertices; it needs 3 to {}", std::ssize(outlineVertices), common::kiModelOutlineVertexMaximum));
	}
	// ShipHullContact's center-directed ray relies on the origin lying inside.
	if (!common::PointInPolygon({0.0f, 0.0f}, outlineVertices))
	{
		throw std::runtime_error("ExportModel::Export outline does not contain the model origin");
	}
	std::copy(outlineVertices.begin(), outlineVertices.end(), outline.pf2OutlineVertices);
	outline.iOutlineVertexCount = static_cast<int32_t>(std::ssize(outlineVertices));

	int64_t iIndexCount = static_cast<int64_t>(uiIndexCount);
	int64_t iIndicesSize = std::ssize(indices16) > 0
		? common::ModelHeader::VerticesOffset(iIndexCount, sizeof(uint16_t))
		: common::ModelHeader::VerticesOffset(iIndexCount, sizeof(uint32_t));
	int64_t iDataSize = iIndicesSize + static_cast<int64_t>(uiVertexBytes);
	ASSERT(iDataSize <= std::numeric_limits<int64_t>::max() - common::kiChunkDataOffset - (common::kiAlignmentBytes - 1) - static_cast<int64_t>(sizeof(common::ModelOutline)));
	auto [pHeader, dataSpan] = AllocateHeaderAndData(iDataSize);
	modelHeader.iIndexCount = iIndexCount;
	modelHeader.iVertexCount = static_cast<int64_t>(uiVertexCount);
	modelHeader.iStride = iStride;
	pHeader->modelHeader = modelHeader;
	if (std::ssize(indices16) > 0)
	{
		std::memcpy(dataSpan.data(), indices16.data(), common::VectorByteSize(indices16));
	}
	else
	{
		std::memcpy(dataSpan.data(), indices32.data(), common::VectorByteSize(indices32));
	}
	std::memcpy(dataSpan.data() + iIndicesSize, vertices.data(), common::VectorByteSize(vertices));

	// The resize below invalidates pHeader and dataSpan.
	int64_t iOutlineOffset = common::kiChunkDataOffset + common::ModelHeader::OutlineOffset(iDataSize);
	int64_t iHardpointsOffset = common::kiChunkDataOffset + common::ModelHeader::MissileTubesOffset(iDataSize);
	int64_t iHardpointBytes = common::VectorByteSize(packedHardpoints);
	mHeaderAndData.resize(static_cast<size_t>(iHardpointsOffset + iHardpointBytes));
	std::memcpy(mHeaderAndData.data() + iOutlineOffset, &outline, sizeof(outline));
	if (!packedHardpoints.empty())
	{
		std::memcpy(mHeaderAndData.data() + iHardpointsOffset, packedHardpoints.data(), static_cast<size_t>(iHardpointBytes));
	}
}

std::string ExportModel::GetInputFingerprint() const
{
	std::filesystem::path metadataPath = GetModelMetadataPath(mInputPath);
	nlohmann::json fingerprint;
	fingerprint["model"] = gpFileManager->mpInputFingerprintCache->Get(mInputPath);
	fingerprint["metadata"] = std::filesystem::exists(metadataPath) ? nlohmann::json(gpFileManager->mpInputFingerprintCache->Get(metadataPath)) : nlohmann::json();
	return fingerprint.dump();
}
