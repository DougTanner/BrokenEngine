#pragma once

namespace engine
{

void SetCrashReportAppDataDirectory(const wchar_t* pcDirectory);
void ResolveCrashReportPaths();
void HandleException(std::optional<const std::exception*> pException = std::nullopt);
void PromptCrashReportDesktopCopy();
#if defined(BT_CLIENT)
void TryUploadCrashReport();
#endif
void ReadDxDiag();

} // namespace engine
