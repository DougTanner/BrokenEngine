#include "PlanMetadata.h"

#include "CoordinationStore.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <unordered_map>

namespace toolcli
{

bool IsValidAgentCli(std::string_view value)
{
	return value == "claude" || value == "codex" || value == "agy";
}

bool Utf8PathLess(std::wstring_view left, std::wstring_view right)
{
	return WideToUtf8(left) < WideToUtf8(right);
}

bool ParseCanonicalUtcTimestamp(std::string_view value, int64_t& rTicks)
{
	return coordination::ParseUtcTimestamp(std::string(value), rTicks) && coordination::FormatUtcTimestamp(static_cast<uint64_t>(rTicks)) == value;
}

bool ReadBytes(const std::filesystem::path& rPath, std::string& rBytes)
{
	std::ifstream input(ExtendedLengthPath(rPath), std::ios::binary);
	if (!input)
	{
		return false;
	}
	input.seekg(0, std::ios::end);
	int64_t iSize = static_cast<int64_t>(input.tellg());
	if (iSize < 0 || iSize > 4 * 1'024 * 1'024)
	{
		return false;
	}
	input.seekg(0, std::ios::beg);
	rBytes.assign(static_cast<size_t>(iSize), '\0');
	input.read(rBytes.data(), static_cast<std::streamsize>(iSize));
	return input.good() || input.eof();
}

bool NormalizePlanPath(std::wstring_view value, std::wstring& rPath)
{
	std::filesystem::path path(value);
	std::wstring_view rootRelative = value.starts_with(kCoretidePrefix) ? value.substr(kCoretidePrefix.size()) : value;
	if (!rootRelative.starts_with(L"Documents/Plans/"))
	{
		return false;
	}
	if (!value.ends_with(L".md"))
	{
		return false;
	}
	if (std::ranges::any_of(path, [](const std::filesystem::path& rPart)
	{
		return rPart == L"." || rPart == L"..";
	}))
	{
		return false;
	}
	if (path.generic_wstring() != value)
	{
		return false;
	}
	rPath = value;
	return true;
}

static bool ParsePlanBytes(Plan& rPlan)
{
	if (rPlan.bytes.starts_with("\xEF\xBB\xBF"))
	{
		rPlan.diagnostic = "manual";
		return true;
	}
	if (!rPlan.bytes.starts_with(kMarkerPrefix))
	{
		// Classifying rather than failing keeps the stale-baseline erase and dependency blocking working; the
		// reporting sites, not this classification, make a marker-less plan document loud.
		rPlan.diagnostic = "manual";
		return true;
	}
	int64_t iLineEnd = static_cast<int64_t>(rPlan.bytes.find('\n'));
	int64_t iMarkerEnd = iLineEnd == -1 ? std::ssize(rPlan.bytes) : iLineEnd;
	if (iMarkerEnd > 0 && rPlan.bytes[static_cast<size_t>(iMarkerEnd - 1)] == '\r')
	{
		--iMarkerEnd;
	}
	std::string_view marker(rPlan.bytes.data(), static_cast<size_t>(iMarkerEnd));
	if (!marker.ends_with(kMarkerSuffix))
	{
		rPlan.diagnostic = "malformed plan metadata marker";
		return false;
	}
	try
	{
		int64_t iJsonBegin = std::ssize(kMarkerPrefix);
		int64_t iJsonLength = std::ssize(marker) - iJsonBegin - std::ssize(kMarkerSuffix);
		nlohmann::json metadata = nlohmann::json::parse(std::string(marker.substr(static_cast<size_t>(iJsonBegin), static_cast<size_t>(iJsonLength))));
		if (metadata.contains("dependsOn") && metadata["dependsOn"].is_array())
		{
			std::vector<std::wstring> dependencies;
			bool bComplete = true;
			for (const nlohmann::json& rDependency : metadata["dependsOn"])
			{
				std::wstring path;
				if (!rDependency.is_string() || !NormalizePlanPath(Utf8ToWide(rDependency.get<std::string>()), path))
				{
					bComplete = false;
					break;
				}
				dependencies.push_back(std::move(path));
			}
			if (bComplete)
			{
				rPlan.dependencies = std::move(dependencies);
				rPlan.bDependenciesKnown = true;
			}
		}
		bool bHasAgentCli = metadata.is_object() && metadata.contains("agentCli");
		if (bHasAgentCli && metadata["agentCli"].is_string())
		{
			std::string agentCli = metadata["agentCli"].get<std::string>();
			if (IsValidAgentCli(agentCli))
			{
				rPlan.agentCli = std::move(agentCli);
			}
		}
		if (!metadata.is_object() || std::cmp_not_equal(metadata.size(), bHasAgentCli ? 3 : 2) || !metadata.contains("createdUtc") || !metadata["createdUtc"].is_string() || !metadata.contains("dependsOn") || !metadata["dependsOn"].is_array())
		{
			rPlan.diagnostic = "metadata requires createdUtc, dependsOn, and optional agentCli";
			return false;
		}
		int64_t iTicks = 0;
		if (!ParseCanonicalUtcTimestamp(metadata["createdUtc"].get<std::string>(), iTicks))
		{
			rPlan.diagnostic = "createdUtc is invalid";
			return false;
		}
		rPlan.createdUtc = metadata["createdUtc"].get<std::string>();
		if (!rPlan.bDependenciesKnown)
		{
			rPlan.diagnostic = "dependency is not a canonical Documents/Plans or Projects/Coretide/Documents/Plans Markdown path";
			return false;
		}
		if (!std::is_sorted(rPlan.dependencies.begin(), rPlan.dependencies.end(), Utf8PathLess) || std::adjacent_find(rPlan.dependencies.begin(), rPlan.dependencies.end()) != rPlan.dependencies.end())
		{
			rPlan.diagnostic = "dependencies must be unique ordinal-sorted";
			return false;
		}
		if (bHasAgentCli && rPlan.agentCli.empty())
		{
			rPlan.diagnostic = "agentCli must be claude, codex, or agy";
			return false;
		}
		rPlan.bValid = true;
		return true;
	}
	catch (const nlohmann::json::exception&)
	{
		rPlan.diagnostic = "metadata JSON is invalid";
		return false;
	}
}

static bool ParsePlan(Plan& rPlan)
{
	if (!ReadBytes(rPlan.diskPath, rPlan.bytes))
	{
		std::error_code error;
		rPlan.diagnostic = !std::filesystem::exists(ExtendedLengthPath(rPlan.diskPath), error) && !error ? "missing" : "could not read plan bytes";
		return false;
	}
	return ParsePlanBytes(rPlan);
}

static bool IsDirectoryGuidance(std::wstring_view path)
{
	std::wstring filename = std::filesystem::path(path).filename().wstring();
	return filename == L"AGENTS.md";
}

// Guidance is never executable; keeping its empty dependency list marked known lets RunTerminal skip it without
// failing on unknown dependencies, and keeping it in the Plan map lets MarkCycles exclude any Plan that depends on it.
static void ClassifyDirectoryGuidance(Plan& rPlan)
{
	rPlan.bValid = false;
	rPlan.diagnostic = "manual";
	rPlan.bDependenciesKnown = true;
}

// Every plan document carries byte-zero metadata, so a marker-less one is a defect rather than a reference file.
static void ReportInvalidMetadata(const Plan& rPlan, nlohmann::json& rDiagnostics)
{
	std::string message = rPlan.diagnostic == "manual" ? "plan document requires byte-zero broken-engine-plan/v2 metadata" : rPlan.diagnostic;
	rDiagnostics.push_back({{"plan", WideToUtf8(rPlan.path)}, {"code", "invalid-metadata"}, {"message", std::move(message)}});
}

bool BuildPlans(const std::filesystem::path& rRepository, std::wstring_view prefix, std::unordered_map<std::wstring, Plan>& rPlans, nlohmann::json& rDiagnostics)
{
	std::optional<std::string> optionalListing = RunGit({L"-C", rRepository.wstring(), L"ls-files", L"-z", L"--", L"Documents/Plans"});
	if (!optionalListing)
	{
		return false;
	}
	int64_t iOffset = 0;
	while (iOffset < std::ssize(*optionalListing))
	{
		int64_t iEnd = static_cast<int64_t>(optionalListing->find('\0', static_cast<size_t>(iOffset)));
		if (iEnd == -1)
		{
			return false;
		}
		std::wstring repositoryPath = Utf8ToWide(std::string_view(optionalListing->data() + iOffset, static_cast<size_t>(iEnd - iOffset)));
		std::wstring path;
		if (!NormalizePlanPath(std::wstring(prefix) + repositoryPath, path))
		{
			iOffset = iEnd + 1;
			continue;
		}
		iOffset = iEnd + 1;
		Plan plan {};
		plan.path = path;
		plan.diskPath = rRepository / repositoryPath;
		if (IsDirectoryGuidance(plan.path))
		{
			std::error_code error;
			if (!std::filesystem::exists(ExtendedLengthPath(plan.diskPath), error) && !error)
			{
				plan.diagnostic = "missing";
			}
			else
			{
				ClassifyDirectoryGuidance(plan);
			}
		}
		else
		{
			ParsePlan(plan);
			if (!plan.bValid && plan.diagnostic != "missing")
			{
				ReportInvalidMetadata(plan, rDiagnostics);
			}
		}
		rPlans.emplace(std::move(path), std::move(plan));
	}
	return true;
}

bool BuildPlansAtCommit(const std::filesystem::path& rRepository, std::wstring_view commit, std::wstring_view prefix, std::unordered_map<std::wstring, Plan>& rPlans, nlohmann::json& rDiagnostics)
{
	// -z keeps paths unambiguous.  Every path still passes the scheduler's stricter canonical check.
	std::optional<std::string> optionalListing = RunGit({L"-C", rRepository.wstring(), L"ls-tree", L"-rz", L"--full-tree", std::wstring(commit), L"--", L"Documents/Plans"});
	if (!optionalListing)
	{
		return false;
	}
	int64_t iOffset = 0;
	while (iOffset < std::ssize(*optionalListing))
	{
		int64_t iEnd = static_cast<int64_t>(optionalListing->find('\0', static_cast<size_t>(iOffset)));
		if (iEnd == -1)
		{
			return false;
		}
		std::string_view entry(optionalListing->data() + iOffset, static_cast<size_t>(iEnd - iOffset));
		iOffset = iEnd + 1;
		int64_t iTab = static_cast<int64_t>(entry.find('\t'));
		if (iTab == -1)
		{
			continue;
		}

		// Both ordinary regular-file modes hold Plan documents.  Symlinks, gitlinks, and trees stay excluded.
		if (!entry.starts_with("100644 blob ") && !entry.starts_with("100755 blob "))
		{
			continue;
		}
		std::wstring repositoryPath = Utf8ToWide(entry.substr(static_cast<size_t>(iTab + 1)));
		std::wstring path;
		if (!NormalizePlanPath(std::wstring(prefix) + repositoryPath, path))
		{
			continue;
		}
		Plan plan {};
		plan.path = path;
		if (IsDirectoryGuidance(plan.path))
		{
			ClassifyDirectoryGuidance(plan);
		}
		else
		{
			std::optional<std::string> optionalBytes = RunGit({L"-C", rRepository.wstring(), L"show", std::wstring(commit) + L":" + repositoryPath});
			if (!optionalBytes)
			{
				return false;
			}
			plan.bytes = std::move(*optionalBytes);
			ParsePlanBytes(plan);
			if (!plan.bValid)
			{
				ReportInvalidMetadata(plan, rDiagnostics);
			}
		}
		rPlans.emplace(std::move(path), std::move(plan));
	}
	return true;
}

bool IsBlockedByDependencies(const Plan& rPlan, const std::unordered_map<std::wstring, Plan>& rPlans)
{
	return std::ranges::any_of(rPlan.dependencies, [&rPlans](std::wstring_view dependency)
	{
		return rPlans.contains(std::wstring(dependency));
	});
}

std::vector<std::wstring> GetSortedPlanPaths(const std::unordered_map<std::wstring, Plan>& rPlans)
{
	std::vector<std::wstring> paths;
	for (const auto& [rPath, rPlan] : rPlans)
	{
		paths.push_back(rPath);
	}
	std::sort(paths.begin(), paths.end());
	return paths;
}

void MarkCycles(std::unordered_map<std::wstring, Plan>& rPlans, nlohmann::json& rDiagnostics)
{
	std::unordered_map<std::wstring, int64_t> colors;
	std::vector<std::wstring> stack;
	std::function<void(std::wstring_view)> Visit = [&](std::wstring_view path)
	{
		std::wstring ownedPath(path);
		colors.insert_or_assign(ownedPath, 1); stack.push_back(ownedPath);
		for (const std::wstring& rDependency : rPlans.at(ownedPath).dependencies)
		{
			auto found = rPlans.find(rDependency);
			if (found == rPlans.end() || !found->second.bValid)
			{
				continue;
			}
			int64_t iDependencyColor = 0;
			{
				auto it = colors.find(rDependency);
				if (it != colors.end())
				{
					iDependencyColor = it->second;
				}
			}
			if (iDependencyColor == 0)
			{
				Visit(rDependency);
			}
			else if (iDependencyColor == 1)
			{
				for (auto it = std::find(stack.begin(), stack.end(), rDependency); it != stack.end(); ++it)
				{
					rPlans.at(*it).bValid = false;
					rPlans.at(*it).diagnostic = "dependency cycle";
					rDiagnostics.push_back({{"plan", WideToUtf8(*it)}, {"code", "dependency-cycle"}, {"message", "plan belongs to a dependency cycle"}});
				}
			}
		}
		stack.pop_back(); colors.insert_or_assign(ownedPath, 2);
	};
	std::vector<std::wstring> paths = GetSortedPlanPaths(rPlans);
	for (const std::wstring& rPath : paths)
	{
		const Plan& rPlan = rPlans.at(rPath);
		if (rPlan.bValid && !colors.contains(rPath))
		{
			Visit(rPath);
		}
	}
	bool bChanged = true;
	while (bChanged)
	{
		bChanged = false;
		for (const std::wstring& rPath : paths)
		{
			Plan& rPlan = rPlans.at(rPath);
			if (!rPlan.bValid)
			{
				continue;
			}
			for (const std::wstring& rDependency : rPlan.dependencies)
			{
				auto it = rPlans.find(rDependency);
				if (it != rPlans.end() && !it->second.bValid)
				{
					rPlan.bValid = false;
					rPlan.diagnostic = "dependency is excluded from selection";
					rDiagnostics.push_back({{"plan", WideToUtf8(rPath)}, {"code", "dependency-excluded"}, {"message", rPlan.diagnostic}});
					bChanged = true;
					break;
				}
			}
		}
	}
}

} // namespace toolcli
