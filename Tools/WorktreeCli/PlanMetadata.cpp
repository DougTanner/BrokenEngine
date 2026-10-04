#include "PlanMetadata.h"

#include "CoordinationStore.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <unordered_map>

namespace toolcli
{
	bool Utf8PathLess(std::wstring_view left, std::wstring_view right)
	{
		return WideToUtf8(left) < WideToUtf8(right);
	}

	bool ParseCanonicalUtcTimestamp(std::string_view value, uint64_t& rTicks)
	{
		return coordination::ParseUtcTimestamp(std::string(value), rTicks) && coordination::FormatUtcTimestamp(rTicks) == value;
	}

	bool ReadBytes(const std::filesystem::path& rPath, std::string& rBytes)
	{
		std::ifstream input(ExtendedLengthPath(rPath), std::ios::binary);
		if (!input)
		{
			return false;
		}
		input.seekg(0, std::ios::end);
		std::streamoff iSize = input.tellg();
		if (iSize < 0 || iSize > 4 * 1'024 * 1'024)
		{
			return false;
		}
		input.seekg(0, std::ios::beg);
		rBytes.assign(static_cast<size_t>(iSize), '\0');
		input.read(rBytes.data(), iSize);
		return input.good() || input.eof();
	}

	bool NormalizePlanPath(std::wstring_view value, std::wstring& rPath)
	{
		std::filesystem::path path(value);
		if (value.empty())
		{
			return false;
		}
		if (value.find(L'\\') != std::wstring::npos)
		{
			return false;
		}
		if (path.has_root_name())
		{
			return false;
		}
		if (path.has_root_directory())
		{
			return false;
		}
		if (value.rfind(L"Documents/Plans/", 0) != 0)
		{
			return false;
		}
		if (value.size() <= std::wstring_view(L"Documents/Plans/").size())
		{
			return false;
		}
		if (!value.ends_with(L".md"))
		{
			return false;
		}
		for (const std::filesystem::path& rPart : path)
		{
			if (rPart == L"." || rPart == L"..")
			{
				return false;
			}
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
		rPlan.digest = coordination::HashSha256(rPlan.bytes).value_or("");
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
		size_t uiLineEnd = rPlan.bytes.find('\n');
		size_t uiMarkerEnd = uiLineEnd == std::string::npos ? rPlan.bytes.size() : uiLineEnd;
		if (uiMarkerEnd > 0 && rPlan.bytes[uiMarkerEnd - 1] == '\r')
		{
			--uiMarkerEnd;
		}
		std::string_view marker(rPlan.bytes.data(), uiMarkerEnd);
		if (!marker.ends_with(kMarkerSuffix))
		{
			rPlan.diagnostic = "malformed plan metadata marker";
			return false;
		}
		try
		{
			size_t uiJsonBegin = kMarkerPrefix.size();
			size_t uiJsonLength = marker.size() - uiJsonBegin - kMarkerSuffix.size();
			nlohmann::json metadata = nlohmann::json::parse(std::string(marker.substr(uiJsonBegin, uiJsonLength)));
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
			if (metadata.size() != 2 || !metadata.contains("createdUtc") || !metadata["createdUtc"].is_string() || !metadata.contains("dependsOn") || !metadata["dependsOn"].is_array())
			{
				rPlan.diagnostic = "metadata requires exactly createdUtc and dependsOn";
				return false;
			}
			uint64_t uiTicks = 0;
			if (!ParseCanonicalUtcTimestamp(metadata["createdUtc"].get<std::string>(), uiTicks))
			{
				rPlan.diagnostic = "createdUtc is invalid";
				return false;
			}
			rPlan.createdUtc = metadata["createdUtc"].get<std::string>();
			if (!rPlan.bDependenciesKnown)
			{
				rPlan.diagnostic = "dependency is not a canonical Documents/Plans Markdown path";
				return false;
			}
			if (!std::is_sorted(rPlan.dependencies.begin(), rPlan.dependencies.end(), Utf8PathLess) || std::adjacent_find(rPlan.dependencies.begin(), rPlan.dependencies.end()) != rPlan.dependencies.end())
			{
				rPlan.diagnostic = "dependencies must be unique ordinal-sorted";
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

	// Guidance metadata is inert in both directions: never executable, and never another Plan's dependency child, so
	// dropping its outgoing edges keeps it out of terminal preparation's child scans.  The entry itself stays in the Plan
	// map because inbound edges block on membership alone, which is what stops a dependent plan going stale.  A tracked
	// file absent from the worktree keeps its "missing" classification, which the baseline comparison needs to erase it.
	static void ClassifyDirectoryGuidance(Plan& rPlan)
	{
		if (!IsDirectoryGuidance(rPlan.path))
		{
			return;
		}
		if (rPlan.diagnostic == "missing")
		{
			return;
		}
		rPlan.bValid = false;
		rPlan.diagnostic = "manual";
		rPlan.dependencies.clear();
		rPlan.bDependenciesKnown = true;
	}

	// Every plan document carries byte-zero metadata, so a marker-less one is a defect rather than a reference file.
	// Guidance is the one document that is never a plan, at any depth, so it stays silent instead of being reported.
	static void ReportInvalidMetadata(const Plan& rPlan, nlohmann::json& rDiagnostics)
	{
		if (IsDirectoryGuidance(rPlan.path))
		{
			return;
		}
		std::string message = rPlan.diagnostic == "manual" ? "plan document requires byte-zero broken-engine-plan/v1 metadata" : rPlan.diagnostic;
		rDiagnostics.push_back({ { "plan", WideToUtf8(rPlan.path) }, { "code", "invalid-metadata" }, { "message", message } });
	}

	bool BuildPlans(const std::filesystem::path& rWorktree, std::unordered_map<std::wstring, Plan>& rPlans, nlohmann::json& rDiagnostics)
	{
		std::optional<std::string> listing = RunGit({ L"-C", rWorktree.wstring(), L"ls-files", L"-z", L"--", L"Documents/Plans" });
		if (!listing)
		{
			return false;
		}
		size_t uiOffset = 0;
		while (uiOffset < listing->size())
		{
			size_t uiEnd = listing->find('\0', uiOffset);
			if (uiEnd == std::string::npos)
			{
				return false;
			}
			std::wstring path;
			if (!NormalizePlanPath(Utf8ToWide(std::string_view(listing->data() + uiOffset, uiEnd - uiOffset)), path))
			{
				uiOffset = uiEnd + 1;
				continue;
			}
			uiOffset = uiEnd + 1;
			Plan plan {};
			plan.path = path;
			plan.diskPath = rWorktree / path;
			ParsePlan(plan);
			ClassifyDirectoryGuidance(plan);
			if (!plan.bValid && plan.diagnostic != "missing")
			{
				ReportInvalidMetadata(plan, rDiagnostics);
			}
			rPlans.emplace(path, std::move(plan));
		}
		return true;
	}

	bool BuildPlansAtCommit(const std::filesystem::path& rWorktree, std::wstring_view commit, std::unordered_map<std::wstring, Plan>& rPlans, nlohmann::json& rDiagnostics)
	{
		// -z keeps paths unambiguous.  Every path still passes the scheduler's stricter canonical check.
		std::optional<std::string> listing = RunGit({ L"-C", rWorktree.wstring(), L"ls-tree", L"-rz", L"--full-tree", std::wstring(commit), L"--", L"Documents/Plans" });
		if (!listing)
		{
			return false;
		}
		size_t uiOffset = 0;
		while (uiOffset < listing->size())
		{
			size_t uiEnd = listing->find('\0', uiOffset);
			if (uiEnd == std::string::npos)
			{
				return false;
			}
			std::string_view entry(listing->data() + uiOffset, uiEnd - uiOffset);
			uiOffset = uiEnd + 1;
			size_t uiTab = entry.find('\t');
			if (uiTab == std::string::npos)
			{
				continue;
			}

			// Both ordinary regular-file modes hold Plan documents.  Symlinks, gitlinks, and trees stay excluded.
			if (!entry.starts_with("100644 blob ") && !entry.starts_with("100755 blob "))
			{
				continue;
			}
			std::wstring path;
			if (!NormalizePlanPath(Utf8ToWide(entry.substr(uiTab + 1)), path))
			{
				continue;
			}
			std::optional<std::string> bytes = RunGit({ L"-C", rWorktree.wstring(), L"show", std::wstring(commit) + L":" + path });
			if (!bytes)
			{
				return false;
			}
			Plan plan {};
			plan.path = path;
			plan.bytes = *bytes;
			ParsePlanBytes(plan);
			ClassifyDirectoryGuidance(plan);
			if (!plan.bValid)
			{
				ReportInvalidMetadata(plan, rDiagnostics);
			}
			rPlans.emplace(path, std::move(plan));
		}
		return true;
	}

	bool IsBlockedByDependencies(const Plan& rPlan, const std::unordered_map<std::wstring, Plan>& rPlans)
	{
		return std::ranges::any_of(rPlan.dependencies, [&rPlans](const std::wstring& rDependency)
		{
			return rPlans.find(rDependency) != rPlans.end();
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
						rDiagnostics.push_back({ { "plan", WideToUtf8(*it) }, { "code", "dependency-cycle" }, { "message", "plan belongs to a dependency cycle" } });
					}
				}
			}
			stack.pop_back(); colors.insert_or_assign(ownedPath, 2);
		};
		std::vector<std::wstring> paths = GetSortedPlanPaths(rPlans);
		for (const std::wstring& rPath : paths)
		{
			const Plan& rPlan = rPlans.at(rPath);
			auto it = colors.find(rPath);
			if (rPlan.bValid && (it == colors.end() || it->second == 0))
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
						rDiagnostics.push_back({ { "plan", WideToUtf8(rPath) }, { "code", "dependency-excluded" }, { "message", rPlan.diagnostic } });
						bChanged = true;
						break;
					}
				}
			}
		}
	}
} // namespace toolcli
