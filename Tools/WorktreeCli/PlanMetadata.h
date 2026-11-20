#pragma once

#include "ToolCliCommon.h"

#include <unordered_map>

namespace toolcli
{

inline constexpr std::string_view kMarkerPrefix = "<!-- broken-engine-plan/v1 ";
inline constexpr std::string_view kMarkerSuffix = " -->";
// A Coretide repository's Plans are keyed by their session path, under this prefix.
inline constexpr std::wstring_view kCoretidePrefix = L"Projects/Coretide/";

struct Plan
{
	std::wstring path;
	std::filesystem::path diskPath;
	std::string bytes;
	std::string createdUtc;
	std::vector<std::wstring> dependencies;
	bool bDependenciesKnown = false;
	bool bValid = false;
	std::string diagnostic;
};

bool Utf8PathLess(std::wstring_view left, std::wstring_view right);
bool ParseCanonicalUtcTimestamp(std::string_view value, int64_t& rTicks);
bool ReadBytes(const std::filesystem::path& rPath, std::string& rBytes);
bool NormalizePlanPath(std::wstring_view value, std::wstring& rPath);
bool BuildPlans(const std::filesystem::path& rRepository, std::wstring_view prefix, std::unordered_map<std::wstring, Plan>& rPlans, nlohmann::json& rDiagnostics);
bool BuildPlansAtCommit(const std::filesystem::path& rRepository, std::wstring_view commit, std::wstring_view prefix, std::unordered_map<std::wstring, Plan>& rPlans, nlohmann::json& rDiagnostics);
bool IsBlockedByDependencies(const Plan& rPlan, const std::unordered_map<std::wstring, Plan>& rPlans);
std::vector<std::wstring> GetSortedPlanPaths(const std::unordered_map<std::wstring, Plan>& rPlans);
void MarkCycles(std::unordered_map<std::wstring, Plan>& rPlans, nlohmann::json& rDiagnostics);

} // namespace toolcli
