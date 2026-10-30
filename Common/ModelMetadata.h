#pragma once

namespace common
{

// An adjacent `.model.json` alignment sidecar, read by DataPacker export and ModelViewer authoring.
struct ModelMetadata
{
	XMFLOAT3A f3RotationDegrees {};
	XMFLOAT3A f3Scale {1.0f, 1.0f, 1.0f};
	XMFLOAT3A f3Offset {};
};

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
	if (!metadata.is_object() || !metadata.at("version").is_number_integer() || metadata.at("version") != 1)
	{
		throw std::runtime_error(std::format("\"{}\" must contain model metadata version 1", rMetadataPath.string()));
	}
	ModelMetadata modelMetadata
	{
		.f3RotationDegrees = ReadModelMetadataVector(metadata, "rotationDegrees", rMetadataPath),
		.f3Scale = ReadModelMetadataVector(metadata, "scale", rMetadataPath),
		.f3Offset = ReadModelMetadataVector(metadata, "offset", rMetadataPath),
	};
	if (modelMetadata.f3Scale.x <= 0.0f || modelMetadata.f3Scale.y <= 0.0f || modelMetadata.f3Scale.z <= 0.0f)
	{
		throw std::runtime_error(std::format("\"{}\" scale components must be positive", rMetadataPath.string()));
	}
	return modelMetadata;
}

} // namespace common
