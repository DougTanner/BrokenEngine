#pragma once

namespace common
{

enum class ModelHardpointKind : uint8_t
{
	kMissileTube,
	kFighterLaunchBay,
};

inline constexpr const char* kpcModelHardpointKindNames[] = {"missileTube", "fighterLaunchBay"};

// Both vectors are in pre-alignment rest model coordinates, the space of ModelMetadata::f3Offset. The direction is
// stored as read: nonzero, not necessarily unit length.
struct ModelHardpoint
{
	ModelHardpointKind eKind = ModelHardpointKind::kMissileTube;
	XMFLOAT3A f3Position {};
	XMFLOAT3A f3Direction {};
};

// An adjacent `.model.json` sidecar holding alignment, the hitbox setting, the outline inset, the navigation obstacle
// tag and hardpoints, read by DataPacker export and ModelViewer authoring. The inset is in aligned model units, the
// outline's own space.
struct ModelMetadata
{
	XMFLOAT3A f3RotationDegrees {};
	XMFLOAT3A f3Scale {1.0f, 1.0f, 1.0f};
	XMFLOAT3A f3Offset {};
	ModelHitboxLevel eHitboxLevel = ModelHitboxLevel::kSphere;
	float fOutlineInsetMeters = 0.1f;
	bool bNavigationObstacle = false;
	std::vector<ModelHardpoint> hardpoints;
};

// The sidecar "hitbox" values, indexed by ModelHitboxLevel.
inline constexpr const char* kpcModelHitboxLevelNames[] = {"sphere", "box", "outline"};

inline XMFLOAT3A ReadModelMetadataVector(const nlohmann::json& rMetadata, std::string_view name, const std::filesystem::path& rMetadataPath)
{
	const nlohmann::json& rValues = rMetadata.at(name);
	if (!rValues.is_array() || rValues.size() != 3)
	{
		throw std::runtime_error(std::format("\"{}\" {} must contain exactly three numeric components", rMetadataPath.string(), name));
	}
	float pfValues[3] {};
	for (int64_t i = 0; i < std::ssize(pfValues); ++i)
	{
		if (!rValues.at(static_cast<size_t>(i)).is_number())
		{
			throw std::runtime_error(std::format("\"{}\" {} component {} must be numeric", rMetadataPath.string(), name, i));
		}
		double fValue = rValues.at(static_cast<size_t>(i)).get<double>();
		if (!std::isfinite(fValue) || std::abs(fValue) > std::numeric_limits<float>::max())
		{
			throw std::runtime_error(std::format("\"{}\" {} component {} must fit in a finite float", rMetadataPath.string(), name, i));
		}
		pfValues[i] = static_cast<float>(fValue);
	}
	return XMFLOAT3A(pfValues[0], pfValues[1], pfValues[2]);
}

// Trust boundary: the sidecar is an authored source file. Throws std::exception on any unreadable or invalid record.
inline ModelMetadata ReadModelMetadata(const std::filesystem::path& rMetadataPath)
{
	std::vector<std::byte> metadataBytes = ReadEntireFile(rMetadataPath);
	const char* pcMetadata = reinterpret_cast<const char*>(metadataBytes.data());
	nlohmann::json metadata = nlohmann::json::parse(pcMetadata, pcMetadata + metadataBytes.size());
	if (!metadata.is_object() || !metadata.at("version").is_number_integer() || metadata.at("version") != 6)
	{
		throw std::runtime_error(std::format("\"{}\" must contain model metadata version 6", rMetadataPath.string()));
	}
	const char* const* ppcHitboxLevelName = std::find(std::begin(kpcModelHitboxLevelNames), std::end(kpcModelHitboxLevelNames), metadata.at("hitbox"));
	if (ppcHitboxLevelName == std::end(kpcModelHitboxLevelNames))
	{
		throw std::runtime_error(std::format(R"("{}" hitbox must be "sphere", "box", or "outline")", rMetadataPath.string()));
	}
	const nlohmann::json& rOutlineInset = metadata.at("outlineInset");
	if (!rOutlineInset.is_number())
	{
		throw std::runtime_error(std::format("\"{}\" outlineInset must be numeric", rMetadataPath.string()));
	}
	double fOutlineInset = rOutlineInset.get<double>();
	// An outset could leave the hull that the row radius and box are derived from.
	if (!std::isfinite(fOutlineInset) || fOutlineInset < 0.0 || fOutlineInset > std::numeric_limits<float>::max())
	{
		throw std::runtime_error(std::format("\"{}\" outlineInset must fit in a finite float and be at or above 0", rMetadataPath.string()));
	}
	const nlohmann::json& rNavigationObstacle = metadata.at("navigationObstacle");
	if (!rNavigationObstacle.is_boolean())
	{
		throw std::runtime_error(std::format("\"{}\" navigationObstacle must be true or false", rMetadataPath.string()));
	}
	ModelMetadata modelMetadata
	{
		.f3RotationDegrees = ReadModelMetadataVector(metadata, "rotationDegrees", rMetadataPath),
		.f3Scale = ReadModelMetadataVector(metadata, "scale", rMetadataPath),
		.f3Offset = ReadModelMetadataVector(metadata, "offset", rMetadataPath),
		.eHitboxLevel = static_cast<ModelHitboxLevel>(ppcHitboxLevelName - std::begin(kpcModelHitboxLevelNames)),
		.fOutlineInsetMeters = static_cast<float>(fOutlineInset),
		.bNavigationObstacle = rNavigationObstacle.get<bool>(),
	};
	if (modelMetadata.f3Scale.x <= 0.0f || modelMetadata.f3Scale.y <= 0.0f || modelMetadata.f3Scale.z <= 0.0f)
	{
		throw std::runtime_error(std::format("\"{}\" scale components must be positive", rMetadataPath.string()));
	}
	const nlohmann::json& rHardpoints = metadata.at("hardpoints");
	if (!rHardpoints.is_array())
	{
		throw std::runtime_error(std::format("\"{}\" hardpoints must be an array", rMetadataPath.string()));
	}
	modelMetadata.hardpoints.reserve(rHardpoints.size());
	for (const nlohmann::json& rHardpoint : rHardpoints)
	{
		if (!rHardpoint.is_object())
		{
			throw std::runtime_error(std::format("\"{}\" hardpoints entries must be objects", rMetadataPath.string()));
		}
		const nlohmann::json& rKind = rHardpoint.at("kind");
		const char* const* ppcKindName = std::find(std::begin(kpcModelHardpointKindNames), std::end(kpcModelHardpointKindNames), rKind);
		if (!rKind.is_string() || ppcKindName == std::end(kpcModelHardpointKindNames))
		{
			throw std::runtime_error(std::format("\"{}\" hardpoint kind must be missileTube or fighterLaunchBay", rMetadataPath.string()));
		}
		ModelHardpoint& rHardpointRecord = modelMetadata.hardpoints.emplace_back(ModelHardpoint
		{
			.eKind = static_cast<ModelHardpointKind>(ppcKindName - std::begin(kpcModelHardpointKindNames)),
			.f3Position = ReadModelMetadataVector(rHardpoint, "position", rMetadataPath),
			.f3Direction = ReadModelMetadataVector(rHardpoint, "direction", rMetadataPath),
		});
		if (rHardpointRecord.f3Direction.x == 0.0f && rHardpointRecord.f3Direction.y == 0.0f && rHardpointRecord.f3Direction.z == 0.0f)
		{
			throw std::runtime_error(std::format("\"{}\" hardpoints direction must be nonzero", rMetadataPath.string()));
		}
	}
	return modelMetadata;
}

} // namespace common
