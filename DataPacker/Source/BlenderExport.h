#pragma once

inline constexpr int64_t kiBlenderExportVersion = 1;

bool BlenderExportDirty(const std::filesystem::path& rSource, const std::filesystem::path& rOutput, const std::optional<std::string>& rOptionalMarker) noexcept;
std::string ExportBlenderScene(const std::filesystem::path& rSource, const std::filesystem::path& rOutput, const std::function<void(const std::filesystem::path&)>& rValidate, const std::function<void()>& rInvalidate);
