#pragma once

namespace engine
{

void SetCrashReportAppDataDirectory(const wchar_t* pcDirectory);
void ResolveCrashReportPaths();
void HandleException(std::optional<const std::exception*> pException = std::nullopt);
void ReadDxDiag();

} // namespace engine
