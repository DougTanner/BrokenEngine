#include "PlanScheduler.h"

#include "CoordinationStore.h"
#include "PlanMetadata.h"
#include "ToolCliCommon.h"

#include <algorithm>
#include <iostream>
#include <span>
#include <unordered_map>

namespace toolcli
{
	constexpr uint64_t kuiClaimLifetimeTicks = 48ui64 * 60ui64 * 60ui64 * 10'000'000ui64;
	// Scheduler changes queue behind peer sessions, so the guard waits far longer than the
	// Guard default; deliberately separate from the build lock's wait.
	constexpr int64_t kiSchedulerGuardWaitSeconds = 500;

	struct Arguments
	{
		std::wstring repository;
		std::wstring worktree;
		std::wstring primaryWorktree;
		std::wstring branch;
		std::wstring owner;
		std::wstring session;
		std::wstring plan;
		bool bUserAuthorizedRejection = false;
		bool bLintOnly = false;
	};

	struct Claim
	{
		std::filesystem::path path;
		nlohmann::json json;
	};

	static void TrimLineEnding(std::string& rValue)
	{
		while (!rValue.empty() && (rValue.back() == '\r' || rValue.back() == '\n')) rValue.pop_back();
	}


	static bool IsLowerHex(std::string_view text, int64_t iLength)
	{
		return iLength >= 0 && text.size() == static_cast<size_t>(iLength) && std::all_of(text.begin(), text.end(), [](char cValue)
		{
			return (cValue >= '0' && cValue <= '9') || (cValue >= 'a' && cValue <= 'f');
		});
	}


	static bool ReadRequiredString(const nlohmann::json& rValue, const char* pcField, std::string& rResult)
	{
		if (!rValue.is_object() || !rValue.contains(pcField) || !rValue[pcField].is_string())
		{
			return false;
		}
		rResult = rValue[pcField].get<std::string>();
		return !rResult.empty();
	}

	static void PrintResult(nlohmann::json value, int64_t iSchemaVersion)
	{
		value["schemaVersion"] = iSchemaVersion;
		std::cout << value.dump() << '\n';
	}

	static int Failure(std::string_view code, int iExitCode = kiExitFailure)
	{
		PrintResult({ { "status", "error" }, { "code", code } }, 2);
		return iExitCode;
	}


	static bool ParseArguments(std::span<wchar_t* const> argumentValues, int64_t iStart, Arguments& rArguments)
	{
		for (int64_t i = iStart; i < std::ssize(argumentValues); ++i)
		{
			std::wstring_view option = argumentValues[i];
			if (option == L"--user-authorized-rejection")
			{
				rArguments.bUserAuthorizedRejection = true;
				continue;
			}
			if (option == L"--lint-only")
			{
				rArguments.bLintOnly = true;
				continue;
			}
			std::wstring* pDestination = nullptr;
			if (option == L"--repo")
			{
				pDestination = &rArguments.repository;
			}
			else if (option == L"--worktree")
			{
				pDestination = &rArguments.worktree;
			}
			else if (option == L"--primary-worktree")
			{
				pDestination = &rArguments.primaryWorktree;
			}
			else if (option == L"--branch")
			{
				pDestination = &rArguments.branch;
			}
			else if (option == L"--owner")
			{
				pDestination = &rArguments.owner;
			}
			else if (option == L"--session")
			{
				pDestination = &rArguments.session;
			}
			else if (option == L"--plan")
			{
				pDestination = &rArguments.plan;
			}
			else
			{
				return false;
			}
			if (++i >= std::ssize(argumentValues))
			{
				return false;
			}
			*pDestination = argumentValues[i];
		}
		return true;
	}

	static bool IsPathBelow(const std::filesystem::path& rChild, const std::filesystem::path& rParent)
	{
		std::error_code error;
		std::filesystem::path child = std::filesystem::weakly_canonical(ExtendedLengthPath(rChild), error);
		if (error)
		{
			return false;
		}
		std::filesystem::path parent = std::filesystem::weakly_canonical(ExtendedLengthPath(rParent), error);
		if (error)
		{
			return false;
		}
		auto childIt = child.begin();
		for (auto parentIt = parent.begin(); parentIt != parent.end(); ++parentIt, ++childIt)
		{
			if (childIt == child.end() || *childIt != *parentIt)
			{
				return false;
			}
		}
		return true;
	}


	static bool IsCanonicalPositiveDecimal(std::wstring_view rValue)
	{
		return !rValue.empty() && rValue.size() <= 10 && rValue.front() >= L'1' && rValue.front() <= L'9' && std::all_of(rValue.begin() + 1, rValue.end(), [](wchar_t cValue)
		{
			return cValue >= L'0' && cValue <= L'9';
		});
	}

	static bool RemovePlanAtomicTemporarySiblings(const std::filesystem::path& rWorktree, std::wstring_view planRelativePath)
	{
		std::filesystem::path planPath = rWorktree / planRelativePath;
		if (!IsPathBelow(planPath, rWorktree))
		{
			return false;
		}
		std::filesystem::path parent = planPath.parent_path();
		std::wstring prefix = planPath.filename().wstring() + L".tmp.";
		std::error_code error;
		for (std::filesystem::directory_iterator it(ExtendedLengthPath(parent), error), end; !error && it != end; it.increment(error))
		{
			std::wstring filename = it->path().filename().wstring();
			if (!filename.starts_with(prefix))
			{
				continue;
			}
			std::wstring_view suffix(filename.data() + prefix.size(), filename.size() - prefix.size());
			size_t uiSeparator = suffix.find(L'.');
			if (uiSeparator == std::wstring_view::npos || suffix.find(L'.', uiSeparator + 1) != std::wstring_view::npos || !IsCanonicalPositiveDecimal(suffix.substr(0, uiSeparator)) || !IsCanonicalPositiveDecimal(suffix.substr(uiSeparator + 1)))
			{
				continue;
			}
			std::filesystem::path temporaryPath = parent / filename;
			if (!IsPathBelow(temporaryPath, rWorktree))
			{
				continue;
			}
			std::error_code entryError;
			if (!it->is_regular_file(entryError) || entryError)
			{
				continue;
			}
			DWORD uiAttributes = ::GetFileAttributesW(ExtendedLengthPath(temporaryPath).c_str());
			if (uiAttributes == INVALID_FILE_ATTRIBUTES || (uiAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0 || (uiAttributes & FILE_ATTRIBUTE_HIDDEN) == 0 || (uiAttributes & FILE_ATTRIBUTE_TEMPORARY) == 0)
			{
				continue;
			}
			if (::DeleteFileW(ExtendedLengthPath(temporaryPath).c_str()) == FALSE)
			{
				return false;
			}
		}
		return !error || error == std::errc::no_such_file_or_directory;
	}


	static std::optional<std::filesystem::path> SchedulerRoot(std::wstring_view repository)
	{
		std::filesystem::path localApplicationData = GetLocalApplicationDataPath();
		if (localApplicationData.empty())
		{
			return std::nullopt;
		}
		std::optional<std::string> hash = coordination::HashSha256(WideToUtf8(repository));
		if (!hash)
		{
			return std::nullopt;
		}
		return localApplicationData / L"BrokenEngineLocks" / L"plan-scheduler" / Utf8ToWide(*hash);
	}

	static std::optional<std::filesystem::path> ClaimPath(const std::filesystem::path& rRoot, std::wstring_view plan)
	{
		std::optional<std::string> hash = coordination::HashSha256(WideToUtf8(plan));
		if (!hash)
		{
			return std::nullopt;
		}
		return rRoot / L"claims" / Utf8ToWide(*hash + ".json");
	}

	static bool ValidateClaim(const nlohmann::json& rClaim, std::wstring_view repository, std::wstring_view plan)
	{
		try
		{
			// An exact field count rejects every schema-v1 record, which is how the pre-cutover claims are collected.
			if (!rClaim.is_object())
			{
				return false;
			}
			if (rClaim.size() != 9)
			{
				return false;
			}
			if (!rClaim.contains("schemaVersion"))
			{
				return false;
			}
			if (!coordination::JsonIntegerEquals(rClaim["schemaVersion"], 2))
			{
				return false;
			}
			std::unordered_map<std::string, std::string> fields;
			for (const char* pcField : { "repository", "plan", "owner", "session", "worktree", "branch", "claimedAt", "expiresAt" })
			{
				if (!ReadRequiredString(rClaim, pcField, fields.try_emplace(pcField).first->second))
				{
					return false;
				}
			}
			if (fields.at("repository") != WideToUtf8(repository))
			{
				return false;
			}
			if (fields.at("plan") != WideToUtf8(plan))
			{
				return false;
			}
			uint64_t uiClaimedAt = 0;
			uint64_t uiExpiresAt = 0;
			return ParseCanonicalUtcTimestamp(fields.at("claimedAt"), uiClaimedAt) && ParseCanonicalUtcTimestamp(fields.at("expiresAt"), uiExpiresAt) && uiExpiresAt > uiClaimedAt && uiExpiresAt - uiClaimedAt == kuiClaimLifetimeTicks;
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}
	}

	static bool ReadClaim(const std::filesystem::path& rPath, Claim& rClaim)
	{
		std::string bytes;
		if (!ReadBytes(rPath, bytes))
		{
			return false;
		}
		try
		{
			rClaim.json = nlohmann::json::parse(bytes);
			rClaim.path = rPath;
			return true;
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}
	}

	static bool ClaimIsLive(const Claim& rClaim)
	{
		uint64_t uiExpiry = 0;
		return coordination::ParseUtcTimestamp(rClaim.json["expiresAt"].get<std::string>(), uiExpiry) && uiExpiry > coordination::CurrentUtcTicks();
	}

	static bool HealClaims(const std::filesystem::path& rRoot, std::wstring_view repository, const std::unordered_map<std::wstring, Plan>& rPrimaryPlans, nlohmann::json& rHealed)
	{
		std::filesystem::path claims = rRoot / L"claims";
		std::error_code error;
		for (std::filesystem::directory_iterator it(ExtendedLengthPath(claims), error), end; !error && it != end; it.increment(error))
		{
			if (!it->is_regular_file(error) || error || it->path().extension() != L".json")
			{
				continue;
			}
			Claim claim;
			std::wstring path;
			bool bRemove = !ReadClaim(it->path(), claim) || !claim.json.contains("plan") || !claim.json["plan"].is_string() || !NormalizePlanPath(Utf8ToWide(claim.json["plan"].get<std::string>()), path) || !ValidateClaim(claim.json, repository, path);
			if (!bRemove)
			{
				std::optional<std::filesystem::path> claimPath = ClaimPath(rRoot, path);
				if (!claimPath)
				{
					return false;
				}
				bRemove = it->path().filename() != claimPath->filename();
			}
			if (!bRemove && !ClaimIsLive(claim))
			{
				bRemove = true;
			}
			// Absent or demoted at the primary tip means a peer landing already completed or rejected the plan.  This
			// rule is what lets the landing script delete its own claim best-effort: a missed delete heals here.
			if (!bRemove)
			{
				auto primary = rPrimaryPlans.find(path);
				if (primary == rPrimaryPlans.end() || !primary->second.bValid)
				{
					bRemove = true;
				}
			}
			if (!bRemove && !coordination::CanonicalizeDirectoryPath(Utf8ToWide(claim.json["worktree"].get<std::string>())))
			{
				bRemove = true;
			}
			if (bRemove && ::DeleteFileW(ExtendedLengthPath(it->path()).c_str()) != FALSE)
			{
				rHealed.push_back(WideToUtf8(it->path().filename().wstring()));
			}
		}
		return true;
	}


	static std::optional<std::wstring> ResolveGitCommonDirectory(const std::filesystem::path& rWorktree)
	{
		std::optional<std::string> commonDirectory = RunGit({ L"-C", rWorktree.wstring(), L"rev-parse", L"--path-format=absolute", L"--git-common-dir" });
		if (!commonDirectory)
		{
			return std::nullopt;
		}
		TrimLineEnding(*commonDirectory);
		return coordination::CanonicalizeDirectoryPath(Utf8ToWide(*commonDirectory));
	}

	static std::optional<std::string> ResolveGitBranch(const std::filesystem::path& rWorktree)
	{
		std::optional<std::string> branch = RunGit({ L"-C", rWorktree.wstring(), L"symbolic-ref", L"--quiet", L"--short", L"HEAD" });
		if (!branch)
		{
			return std::nullopt;
		}
		TrimLineEnding(*branch);
		if (branch->empty())
		{
			return std::nullopt;
		}
		return branch;
	}

	static std::optional<std::string> ResolvePrimaryBranch(std::wstring_view repository)
	{
		std::optional<std::string> branch = RunGit({ L"--git-dir", std::wstring(repository), L"symbolic-ref", L"--quiet", L"--short", L"HEAD" });
		if (!branch)
		{
			return std::nullopt;
		}
		TrimLineEnding(*branch);
		if (branch->empty())
		{
			return std::nullopt;
		}
		return branch;
	}

	static std::optional<std::string> ResolvePrimaryReference(std::wstring_view repository)
	{
		std::optional<std::string> reference = RunGit({ L"--git-dir", std::wstring(repository), L"symbolic-ref", L"--quiet", L"HEAD" });
		if (!reference)
		{
			return std::nullopt;
		}
		TrimLineEnding(*reference);
		if (!reference->starts_with("refs/heads/") || reference->size() == std::string_view("refs/heads/").size())
		{
			return std::nullopt;
		}
		return reference;
	}

	static std::optional<std::string> ResolveCommit(const std::filesystem::path& rWorktree, std::wstring_view revision)
	{
		std::optional<std::string> commit = RunGit({ L"-C", rWorktree.wstring(), L"rev-parse", L"--verify", std::wstring(revision) + L"^{commit}" });
		if (!commit)
		{
			return std::nullopt;
		}
		TrimLineEnding(*commit);
		return IsLowerHex(*commit, 40) ? commit : std::nullopt;
	}

	static std::optional<std::string> ResolveRepositoryCommit(std::wstring_view repository, std::wstring_view revision)
	{
		std::optional<std::string> commit = RunGit({ L"--git-dir", std::wstring(repository), L"rev-parse", L"--verify", std::wstring(revision) + L"^{commit}" });
		if (!commit)
		{
			return std::nullopt;
		}
		TrimLineEnding(*commit);
		return IsLowerHex(*commit, 40) ? commit : std::nullopt;
	}

	// Healing classifies against the primary tip rather than the caller's tree, so a session that has already
	// deleted its own terminal target never heals its own live claim.
	static bool BuildPrimaryTipPlans(std::wstring_view repository, const std::filesystem::path& rWorktree, std::unordered_map<std::wstring, Plan>& rPlans)
	{
		std::optional<std::string> reference = ResolvePrimaryReference(repository);
		std::optional<std::string> tip = reference ? ResolveRepositoryCommit(repository, Utf8ToWide(*reference)) : std::nullopt;
		if (!tip)
		{
			return false;
		}
		nlohmann::json ignored = nlohmann::json::array();
		return BuildPlansAtCommit(rWorktree, Utf8ToWide(*tip), rPlans, ignored);
	}

	static bool ResolveContext(const Arguments& rArguments, std::wstring& rRepository, std::filesystem::path& rWorktree)
	{
		std::optional<std::wstring> repository = coordination::CanonicalizeDirectoryPath(rArguments.repository);
		std::optional<std::wstring> worktree = coordination::CanonicalizeDirectoryPath(rArguments.worktree);
		if (!repository || !worktree || ResolveGitCommonDirectory(*worktree) != repository)
		{
			return false;
		}
		rRepository = *repository; rWorktree = *worktree;
		return true;
	}

	static bool ResolveWorktreeContext(const Arguments& rArguments, std::wstring& rRepository, std::filesystem::path& rWorktree)
	{
		std::optional<std::wstring> worktree = coordination::CanonicalizeDirectoryPath(rArguments.worktree);
		if (!worktree)
		{
			return false;
		}
		std::optional<std::wstring> commonDirectory = ResolveGitCommonDirectory(*worktree);
		if (!commonDirectory)
		{
			return false;
		}
		std::optional<std::wstring> repository = commonDirectory;
		if (!rArguments.repository.empty())
		{
			repository = coordination::CanonicalizeDirectoryPath(rArguments.repository);
		}
		if (!repository || repository != commonDirectory)
		{
			return false;
		}
		rRepository = *repository;
		rWorktree = *worktree;
		return true;
	}

	// One claim per session, discovered by owner/session identity rather than by plan membership: a session whose
	// tree no longer carries its claimed plan still owns the claim, and gating discovery on a plan map would let it
	// escape and mint a duplicate.  Any scan ambiguity fails closed for the same reason.
	static bool FindSessionClaim(const std::filesystem::path& rRoot, std::wstring_view repository, const Arguments& rArguments, std::wstring& rPlanPath, Claim& rClaim, bool& rbFound)
	{
		rbFound = false;
		std::error_code error;
		for (std::filesystem::directory_iterator it(ExtendedLengthPath(rRoot / L"claims"), error), end; !error && it != end; it.increment(error))
		{
			if (!it->is_regular_file(error) || error || it->path().extension() != L".json")
			{
				continue;
			}
			Claim claim;
			std::wstring path;
			if (!ReadClaim(it->path(), claim) || !claim.json.contains("plan") || !claim.json["plan"].is_string() || !NormalizePlanPath(Utf8ToWide(claim.json["plan"].get<std::string>()), path) || !ValidateClaim(claim.json, repository, path) || !ClaimIsLive(claim))
			{
				continue;
			}
			if (claim.json["owner"].get<std::string>() != WideToUtf8(rArguments.owner) || claim.json["session"].get<std::string>() != WideToUtf8(rArguments.session))
			{
				continue;
			}
			rPlanPath = path;
			rClaim = std::move(claim);
			rbFound = true;
			return true;
		}
		return !error || error == std::errc::no_such_file_or_directory;
	}

	static void PrintClaim(std::string_view code, std::wstring_view planPath, const nlohmann::json& rClaim)
	{
		PrintResult({ { "status", "ok" }, { "code", code }, { "plan", WideToUtf8(planPath) }, { "owner", rClaim["owner"] }, { "session", rClaim["session"] }, { "worktree", rClaim["worktree"] }, { "branch", rClaim["branch"] }, { "claimedAt", rClaim["claimedAt"] }, { "expiresAt", rClaim["expiresAt"] } }, 2);
	}

	static int RunValidate(const Arguments& rArguments)
	{
		std::wstring repository; std::filesystem::path worktree;
		if (!ResolveContext(rArguments, repository, worktree))
		{
			return Failure("invalid-context");
		}
		std::wstring requestedPlan;
		if (!rArguments.plan.empty() && !NormalizePlanPath(rArguments.plan, requestedPlan))
		{
			return Failure("invalid-plan");
		}
		std::unordered_map<std::wstring, Plan> plans; nlohmann::json diagnostics = nlohmann::json::array();
		if (!BuildPlans(worktree, plans, diagnostics))
		{
			return Failure("scan-failed");
		}
		if (!requestedPlan.empty() && plans.find(requestedPlan) == plans.end())
		{
			return Failure("plan-not-found", kiExitStateConflict);
		}
		MarkCycles(plans, diagnostics);
		// The lint result is complete above; only the heal below needs scheduler storage and the guard, so
		// --lint-only skips that block entirely and reports an empty healedClaims to keep one output shape.
		nlohmann::json healed = nlohmann::json::array();
		if (!rArguments.bLintOnly)
		{
			std::optional<std::filesystem::path> schedulerRoot = SchedulerRoot(repository);
			if (!schedulerRoot)
			{
				return Failure("local-app-data-unavailable");
			}
			std::filesystem::path guardPath = *schedulerRoot / L"scheduler.guard";
			if (!coordination::EnsureParentDirectory(guardPath))
			{
				return Failure("storage-failed");
			}
			bool bContentionObserved = false;
			std::string failureReason;
			coordination::Guard guard(guardPath, bContentionObserved, failureReason, kiSchedulerGuardWaitSeconds * 1'000, 10'000);
			if (!guard.mbValid)
			{
				// Only contention is a state conflict; anything else means the lock file itself is unusable.
				return bContentionObserved ? Failure("busy", kiExitStateConflict) : Failure("guard-unavailable");
			}
			std::unordered_map<std::wstring, Plan> primaryPlans;
			if (BuildPrimaryTipPlans(repository, worktree, primaryPlans))
			{
				// Healing must classify the primary tip exactly as claim-next does, so a cycle there invalidates the
				// same plans.  Its diagnostics go to a separate sink because the reported ones describe only the
				// working tree.
				nlohmann::json primaryDiagnostics = nlohmann::json::array();
				MarkCycles(primaryPlans, primaryDiagnostics);
				if (!HealClaims(*schedulerRoot, repository, primaryPlans, healed))
				{
					return Failure("local-app-data-unavailable");
				}
			}
		}
		nlohmann::json output = { { "operation", "validate" }, { "status", diagnostics.empty() ? "valid" : "invalid" }, { "code", diagnostics.empty() ? "ok" : "invalid-plans" }, { "message", diagnostics.empty() ? "plan metadata is valid" : "some plans are excluded from selection" }, { "diagnostics", diagnostics }, { "notices", nlohmann::json::array() }, { "healedClaims", healed }, { "plans", nlohmann::json::array() } };
		for (const std::wstring& rPath : GetSortedPlanPaths(plans))
		{
			const Plan& rPlan = plans.at(rPath);
			if (!rPlan.bValid)
			{
				continue;
			}
			if (!requestedPlan.empty() && rPath != requestedPlan)
			{
				continue;
			}
			for (const std::wstring& rDependency : rPlan.dependencies)
			{
				if (plans.find(rDependency) == plans.end())
				{
					output["notices"].push_back({ { "plan", WideToUtf8(rPath) }, { "code", "stale-dependency" }, { "dependency", WideToUtf8(rDependency) } });
				}
			}
		}
		std::vector<const Plan*> outputPlans;
		for (const auto& [rPath, rPlan] : plans)
		{
			if (rPlan.bValid && (requestedPlan.empty() || rPath == requestedPlan))
			{
				outputPlans.push_back(&rPlan);
			}
		}
		std::sort(outputPlans.begin(), outputPlans.end(), [](const Plan* pLeft, const Plan* pRight)
		{
			return pLeft->createdUtc != pRight->createdUtc ? pLeft->createdUtc > pRight->createdUtc : Utf8PathLess(pLeft->path, pRight->path);
		});
		for (const Plan* pPlan : outputPlans)
		{
			nlohmann::json dependencies = nlohmann::json::array();
			for (const std::wstring& rDependency : pPlan->dependencies)
			{
				dependencies.push_back(WideToUtf8(rDependency));
			}
			output["plans"].push_back({ { "path", WideToUtf8(pPlan->path) }, { "createdUtc", pPlan->createdUtc }, { "dependsOn", dependencies } });
		}
		PrintResult(std::move(output), 1);
		return kiExitOk;
	}

	// A read-only preview of the scheduler row states from the session tree.  It heals nothing, takes no scheduler
	// guard, and creates no scheduler storage, so a claim record it cannot use is ignored here rather than deleted.
	static int RunList(const Arguments& rArguments)
	{
		std::wstring repository; std::filesystem::path worktree;
		if (!ResolveContext(rArguments, repository, worktree))
		{
			return Failure("invalid-context");
		}
		std::optional<std::string> primaryReference = ResolvePrimaryReference(repository);
		std::optional<std::string> primaryCommit = primaryReference ? ResolveRepositoryCommit(repository, Utf8ToWide(*primaryReference)) : std::nullopt;
		if (!primaryCommit)
		{
			return Failure("primary-revision-failed");
		}
		// Listing reads only committed trees, so any session on the primary line is a legitimate view: behind the
		// primary tip, or ahead of it after landing preparation rebased the session onto the current tip.
		std::optional<std::string> sessionCommit = ResolveCommit(worktree, L"HEAD");
		if (!sessionCommit || (!RunGit({ L"--git-dir", repository, L"merge-base", L"--is-ancestor", Utf8ToWide(*sessionCommit), Utf8ToWide(*primaryCommit) }) && !RunGit({ L"--git-dir", repository, L"merge-base", L"--is-ancestor", Utf8ToWide(*primaryCommit), Utf8ToWide(*sessionCommit) })))
		{
			return Failure("git-identity-mismatch");
		}
		// Same two Plan maps claim-next evaluates: the primary tip decides eligibility, the session tree supplies the
		// rows and their marker bytes.  Metadata that never parsed cannot fill a row, so it stays in the diagnostics.
		std::unordered_map<std::wstring, Plan> primaryPlans; nlohmann::json primaryDiagnostics = nlohmann::json::array();
		std::unordered_map<std::wstring, Plan> sessionPlans; nlohmann::json diagnostics = nlohmann::json::array();
		if (!BuildPlansAtCommit(worktree, Utf8ToWide(*primaryCommit), primaryPlans, primaryDiagnostics) || !BuildPlansAtCommit(worktree, Utf8ToWide(*sessionCommit), sessionPlans, diagnostics))
		{
			return Failure("scan-failed");
		}
		MarkCycles(primaryPlans, diagnostics);
		std::vector<const Plan*> rows;
		for (const auto& [rPath, rPlan] : sessionPlans)
		{
			if (rPlan.bValid)
			{
				rows.push_back(&rPlan);
			}
		}
		std::sort(rows.begin(), rows.end(), [](const Plan* pLeft, const Plan* pRight)
		{
			return pLeft->createdUtc != pRight->createdUtc ? pLeft->createdUtc > pRight->createdUtc : Utf8PathLess(pLeft->path, pRight->path);
		});
		std::optional<std::filesystem::path> schedulerRoot = SchedulerRoot(repository);
		if (!schedulerRoot)
		{
			return Failure("local-app-data-unavailable");
		}
		const std::filesystem::path& root = *schedulerRoot;
		nlohmann::json output = { { "operation", "list" }, { "status", "ok" }, { "code", "ok" }, { "diagnostics", diagnostics }, { "plans", nlohmann::json::array() } };
		for (const Plan* pPlan : rows)
		{
			nlohmann::json dependencies = nlohmann::json::array();
			for (const std::wstring& rDependency : pPlan->dependencies)
			{
				dependencies.push_back(WideToUtf8(rDependency));
			}
			nlohmann::json row = { { "path", WideToUtf8(pPlan->path) }, { "createdUtc", pPlan->createdUtc }, { "dependsOn", dependencies } };
			Claim claim;
			std::optional<std::filesystem::path> claimPath = ClaimPath(root, pPlan->path);
			if (!claimPath)
			{
				return Failure("local-app-data-unavailable");
			}
			bool bClaimed = ReadClaim(*claimPath, claim) && ValidateClaim(claim.json, repository, pPlan->path) && ClaimIsLive(claim) && coordination::CanonicalizeDirectoryPath(Utf8ToWide(claim.json["worktree"].get<std::string>())).has_value();
			if (bClaimed)
			{
				row["claim"] = { { "session", claim.json["session"] }, { "worktree", claim.json["worktree"] }, { "expiresAt", claim.json["expiresAt"] } };
			}
			auto primary = primaryPlans.find(pPlan->path);
			if (primary == primaryPlans.end() || !primary->second.bValid)
			{
				// Excluded from selection for a reason that is not a dependency edge: a peer landing already removed
				// the plan, or the primary tip invalidates it.
				row["state"] = "excluded";
				row["diagnostic"] = primary == primaryPlans.end() ? "absent at the primary tip" : primary->second.diagnostic;
			}
			else if (IsBlockedByDependencies(*pPlan, sessionPlans) || IsBlockedByDependencies(primary->second, primaryPlans))
			{
				std::vector<std::wstring> blocking;
				for (const std::wstring& rDependency : pPlan->dependencies)
				{
					if (sessionPlans.find(rDependency) != sessionPlans.end())
					{
						blocking.push_back(rDependency);
					}
				}
				for (const std::wstring& rDependency : primary->second.dependencies)
				{
					if (primaryPlans.find(rDependency) != primaryPlans.end() && std::find(blocking.begin(), blocking.end(), rDependency) == blocking.end())
					{
						blocking.push_back(rDependency);
					}
				}
				std::sort(blocking.begin(), blocking.end(), Utf8PathLess);
				row["state"] = "blocked";
				row["blockedBy"] = nlohmann::json::array();
				for (const std::wstring& rDependency : blocking)
				{
					row["blockedBy"].push_back(WideToUtf8(rDependency));
				}
			}
			else
			{
				row["state"] = bClaimed ? "claimed" : "eligible";
			}
			output["plans"].push_back(std::move(row));
		}
		PrintResult(std::move(output), 1);
		return kiExitOk;
	}

	static int RunClaimNext(const Arguments& rArguments)
	{
		std::wstring repository; std::filesystem::path worktree;
		if (!ResolveContext(rArguments, repository, worktree) || rArguments.owner.empty() || rArguments.session.empty() || rArguments.branch.empty() || rArguments.primaryWorktree.empty())
		{
			return Failure("invalid-context");
		}
		std::optional<std::filesystem::path> schedulerRoot = SchedulerRoot(repository);
		if (!schedulerRoot)
		{
			return Failure("local-app-data-unavailable");
		}
		const std::filesystem::path& root = *schedulerRoot;
		std::filesystem::path guardPath = root / L"scheduler.guard";
		if (!coordination::EnsureParentDirectory(guardPath))
		{
			return Failure("storage-failed");
		}
		bool bContentionObserved = false;
		std::string failureReason;
		coordination::Guard guard(guardPath, bContentionObserved, failureReason, kiSchedulerGuardWaitSeconds * 1'000, 10'000);
		if (!guard.mbValid)
		{
			// Only contention is a state conflict; anything else means the lock file itself is unusable.
			return bContentionObserved ? Failure("busy", kiExitStateConflict) : Failure("guard-unavailable");
		}
		std::optional<std::wstring> primaryWorktree = coordination::CanonicalizeDirectoryPath(rArguments.primaryWorktree);
		std::optional<std::string> worktreeBranch = ResolveGitBranch(worktree);
		std::optional<std::string> primaryBranch = ResolvePrimaryBranch(repository);
		if (!primaryWorktree || ResolveGitCommonDirectory(*primaryWorktree) != std::optional<std::wstring>(repository))
		{
			return Failure("git-identity-mismatch");
		}
		if (!worktreeBranch || *worktreeBranch != WideToUtf8(rArguments.branch))
		{
			return Failure("git-identity-mismatch");
		}
		if (!primaryBranch || ResolveGitBranch(*primaryWorktree) != primaryBranch)
		{
			return Failure("git-identity-mismatch");
		}
		std::wstring requestedPlan;
		if (!rArguments.plan.empty() && !NormalizePlanPath(rArguments.plan, requestedPlan))
		{
			return Failure("invalid-plan");
		}
		std::optional<std::string> primaryCommit = ResolveCommit(*primaryWorktree, L"HEAD");
		if (!primaryCommit)
		{
			return Failure("primary-revision-failed");
		}
		std::optional<std::string> sessionCommit = ResolveCommit(worktree, L"HEAD");
		if (!sessionCommit || !RunGit({ L"--git-dir", repository, L"merge-base", L"--is-ancestor", Utf8ToWide(*sessionCommit), Utf8ToWide(*primaryCommit) }))
		{
			return Failure("git-identity-mismatch");
		}
		// Two Plan maps: the primary tip drives healing and eligibility - including dependency blocking, so a behind
		// session neither heals a peer's claim on a plan present only at primary nor claims past a dependency edge
		// landed after its baseline; the session tree drives selection bytes and its own dependency evaluation.
		std::unordered_map<std::wstring, Plan> plans; nlohmann::json diagnostics = nlohmann::json::array();
		if (!BuildPlansAtCommit(*primaryWorktree, Utf8ToWide(*primaryCommit), plans, diagnostics))
		{
			return Failure("scan-failed");
		}
		MarkCycles(plans, diagnostics);
		nlohmann::json healed = nlohmann::json::array();
		if (!HealClaims(root, repository, plans, healed))
		{
			return Failure("local-app-data-unavailable");
		}
		std::unordered_map<std::wstring, Plan> sessionPlans; nlohmann::json sessionDiagnostics = nlohmann::json::array();
		if (!BuildPlansAtCommit(worktree, Utf8ToWide(*sessionCommit), sessionPlans, sessionDiagnostics))
		{
			return Failure("scan-failed");
		}
		std::wstring existingPlanPath; Claim existing; bool bFound = false;
		if (!FindSessionClaim(root, repository, rArguments, existingPlanPath, existing, bFound))
		{
			return Failure("claim-scan-failed");
		}
		if (bFound)
		{
			if (!requestedPlan.empty() && requestedPlan != existingPlanPath)
			{
				PrintResult({ { "status", "error" }, { "code", "claim-plan-mismatch" }, { "requestedPlan", WideToUtf8(requestedPlan) }, { "heldPlan", WideToUtf8(existingPlanPath) } }, 2);
				return kiExitStateConflict;
			}
			PrintClaim("existing", existingPlanPath, existing.json);
			return kiExitOk;
		}
		std::vector<Plan*> candidates;
		for (auto& [rPath, rPlan] : sessionPlans)
		{
			if (!rPlan.bValid)
			{
				continue;
			}
			if (IsBlockedByDependencies(rPlan, sessionPlans))
			{
				continue;
			}
			if (!requestedPlan.empty() && rPath != requestedPlan)
			{
				continue;
			}
			auto primary = plans.find(rPath);
			if (primary == plans.end() || !primary->second.bValid)
			{
				continue; // absent or demoted at the primary tip: a peer landing already completed or rejected it
			}
			if (IsBlockedByDependencies(primary->second, plans))
			{
				continue; // a prerequisite landed at the primary tip after this session's baseline
			}
			candidates.push_back(&rPlan);
		}
		std::sort(candidates.begin(), candidates.end(), [](const Plan* pLeft, const Plan* pRight)
		{
			return pLeft->createdUtc != pRight->createdUtc ? pLeft->createdUtc > pRight->createdUtc : Utf8PathLess(pLeft->path, pRight->path);
		});
		for (const Plan* pPlan : candidates)
		{
			std::optional<std::filesystem::path> claimPath = ClaimPath(root, pPlan->path);
			if (!claimPath)
			{
				return Failure("local-app-data-unavailable");
			}
			Claim occupied;
			if (ReadClaim(*claimPath, occupied))
			{
				continue; // claimed by another session, or an unhealable record
			}
			uint64_t uiClaimedAt = coordination::CurrentUtcTicks();
			nlohmann::json claim = { { "schemaVersion", 2 }, { "repository", WideToUtf8(repository) }, { "plan", WideToUtf8(pPlan->path) }, { "owner", WideToUtf8(rArguments.owner) }, { "session", WideToUtf8(rArguments.session) }, { "worktree", WideToUtf8(worktree.wstring()) }, { "branch", WideToUtf8(rArguments.branch) }, { "claimedAt", coordination::FormatUtcTimestamp(uiClaimedAt) }, { "expiresAt", coordination::FormatUtcTimestamp(uiClaimedAt + kuiClaimLifetimeTicks) } };
			if (!coordination::EnsureParentDirectory(*claimPath))
			{
				return Failure("claim-write-failed");
			}
			if (!coordination::WriteMetadataAtomic(*claimPath, claim))
			{
				return Failure("claim-write-failed");
			}
			PrintClaim("claimed", pPlan->path, claim);
			return kiExitOk;
		}
		PrintResult({ { "status", "ok" }, { "code", "none" } }, 2);
		return kiExitOk;
	}

	static int RunClaimStatus(const Arguments& rArguments)
	{
		std::wstring repository; std::filesystem::path worktree;
		if (!ResolveWorktreeContext(rArguments, repository, worktree) || rArguments.owner.empty() || rArguments.session.empty())
		{
			return Failure("invalid-context");
		}
		std::optional<std::filesystem::path> schedulerRoot = SchedulerRoot(repository);
		if (!schedulerRoot)
		{
			return Failure("local-app-data-unavailable");
		}
		const std::filesystem::path& root = *schedulerRoot;
		std::filesystem::path guardPath = root / L"scheduler.guard";
		if (!coordination::EnsureParentDirectory(guardPath))
		{
			return Failure("storage-failed");
		}
		bool bContentionObserved = false;
		std::string failureReason;
		coordination::Guard guard(guardPath, bContentionObserved, failureReason, kiSchedulerGuardWaitSeconds * 1'000, 10'000);
		if (!guard.mbValid)
		{
			// Only contention is a state conflict; anything else means the lock file itself is unusable.
			return bContentionObserved ? Failure("busy", kiExitStateConflict) : Failure("guard-unavailable");
		}
		std::wstring requestedPlan;
		if (!rArguments.plan.empty() && !NormalizePlanPath(rArguments.plan, requestedPlan))
		{
			return Failure("invalid-plan");
		}
		std::wstring plan; Claim claim; bool bFound = false;
		if (!FindSessionClaim(root, repository, rArguments, plan, claim, bFound))
		{
			return Failure("claim-scan-failed");
		}
		if (!bFound)
		{
			PrintResult({ { "status", "ok" }, { "code", "none" } }, 2);
			return kiExitOk;
		}
		if (!requestedPlan.empty() && requestedPlan != plan)
		{
			PrintResult({ { "status", "error" }, { "code", "claim-plan-mismatch" }, { "requestedPlan", WideToUtf8(requestedPlan) }, { "heldPlan", WideToUtf8(plan) } }, 2);
			return kiExitStateConflict;
		}
		PrintClaim("claimed", plan, claim.json);
		return kiExitOk;
	}

	static int RunUnclaim(const Arguments& rArguments)
	{
		std::wstring repository; std::filesystem::path worktree;
		if (!ResolveWorktreeContext(rArguments, repository, worktree) || rArguments.owner.empty() || rArguments.session.empty())
		{
			return Failure("invalid-context");
		}
		std::optional<std::filesystem::path> schedulerRoot = SchedulerRoot(repository);
		if (!schedulerRoot)
		{
			return Failure("local-app-data-unavailable");
		}
		const std::filesystem::path& root = *schedulerRoot;
		std::filesystem::path guardPath = root / L"scheduler.guard";
		if (!coordination::EnsureParentDirectory(guardPath))
		{
			return Failure("storage-failed");
		}
		bool bContentionObserved = false;
		std::string failureReason;
		coordination::Guard guard(guardPath, bContentionObserved, failureReason, kiSchedulerGuardWaitSeconds * 1'000, 10'000);
		if (!guard.mbValid)
		{
			// Only contention is a state conflict; anything else means the lock file itself is unusable.
			return bContentionObserved ? Failure("busy", kiExitStateConflict) : Failure("guard-unavailable");
		}
		std::wstring plan; Claim claim; bool bFound = false;
		if (!FindSessionClaim(root, repository, rArguments, plan, claim, bFound))
		{
			return Failure("claim-scan-failed");
		}
		if (!bFound)
		{
			PrintResult({ { "status", "ok" }, { "code", "already-absent" } }, 2);
			return kiExitOk;
		}
		if (::DeleteFileW(ExtendedLengthPath(claim.path).c_str()) == FALSE)
		{
			return Failure("delete-failed");
		}
		PrintResult({ { "status", "ok" }, { "code", "released" } }, 2);
		return kiExitOk;
	}

	static bool RenderDependencies(const Plan& rPlan, std::wstring_view removed, std::string& rBytes)
	{
		std::vector<std::wstring> dependencies = rPlan.dependencies;
		auto found = std::find(dependencies.begin(), dependencies.end(), removed);
		if (found == dependencies.end())
		{
			return false;
		}
		dependencies.erase(found);
		size_t uiLineEnd = rPlan.bytes.find('\n');
		size_t uiSuffixStart = uiLineEnd == std::string::npos ? rPlan.bytes.size() : (uiLineEnd > 0 && rPlan.bytes[uiLineEnd - 1] == '\r' ? uiLineEnd - 1 : uiLineEnd);
		nlohmann::json metadata = { { "createdUtc", rPlan.createdUtc }, { "dependsOn", nlohmann::json::array() } };
		for (const std::wstring& rDependency : dependencies)
		{
			metadata["dependsOn"].push_back(WideToUtf8(rDependency));
		}
		rBytes = std::string(kMarkerPrefix) + metadata.dump() + std::string(kMarkerSuffix) + rPlan.bytes.substr(uiSuffixStart);
		return true;
	}

	// Terminal preparation removes the target from every direct dependency child's byte-zero marker and deletes the target Plan file. Both edits are idempotent: a child whose marker no longer lists the target is not selected, and an absent target is skipped.
	static int RunTerminal(std::wstring_view operation, const Arguments& rArguments)
	{
		bool bReject = operation == L"reject";
		if (bReject && !rArguments.bUserAuthorizedRejection)
		{
			return Failure("authorization-required");
		}
		std::wstring repository; std::filesystem::path worktree;
		if (!ResolveWorktreeContext(rArguments, repository, worktree) || rArguments.owner.empty() || rArguments.session.empty())
		{
			return Failure("invalid-context");
		}
		std::optional<std::filesystem::path> schedulerRoot = SchedulerRoot(repository);
		if (!schedulerRoot)
		{
			return Failure("local-app-data-unavailable");
		}
		const std::filesystem::path& root = *schedulerRoot;
		std::filesystem::path guardPath = root / L"scheduler.guard";
		if (!coordination::EnsureParentDirectory(guardPath))
		{
			return Failure("storage-failed");
		}
		bool bContentionObserved = false;
		std::string failureReason;
		coordination::Guard guard(guardPath, bContentionObserved, failureReason, kiSchedulerGuardWaitSeconds * 1'000, 10'000);
		if (!guard.mbValid)
		{
			// Only contention is a state conflict; anything else means the lock file itself is unusable.
			return bContentionObserved ? Failure("busy", kiExitStateConflict) : Failure("guard-unavailable");
		}
		std::wstring target; Claim claim; bool bFound = false;
		if (!FindSessionClaim(root, repository, rArguments, target, claim, bFound))
		{
			return Failure("claim-scan-failed");
		}
		if (!bFound)
		{
			return Failure("claim-missing", kiExitStateConflict);
		}
		// The claim is found by owner/session identity, so a session invoked from the wrong checkout would otherwise
		// rewrite child markers and delete the Plan in that unrelated tree.  Both sides are canonical final paths.
		if (WideToUtf8(worktree.wstring()) != claim.json["worktree"].get<std::string>())
		{
			return Failure("claim-worktree-mismatch", kiExitStateConflict);
		}
		std::unordered_map<std::wstring, Plan> plans; nlohmann::json diagnostics = nlohmann::json::array();
		if (!BuildPlans(worktree, plans, diagnostics))
		{
			return Failure("scan-failed");
		}
		// Every check that can reject the operation runs before the first path changes, so a later invalid child or
		// untracked target cannot leave an earlier child marker already rewritten.
		struct ChildRewrite
		{
			std::wstring path;
			std::string before;
			std::string after;
		};
		std::vector<ChildRewrite> rewrites;
		for (const std::wstring& rPath : GetSortedPlanPaths(plans))
		{
			const Plan& rPlan = plans.at(rPath);
			if (rPath == target)
			{
				continue;
			}
			if (rPlan.diagnostic == "missing")
			{
				continue;
			}
			if (!rPlan.bDependenciesKnown)
			{
				return Failure("child-invalid", kiExitStateConflict);
			}
			if (std::find(rPlan.dependencies.begin(), rPlan.dependencies.end(), target) == rPlan.dependencies.end())
			{
				continue;
			}
			if (!rPlan.bValid)
			{
				return Failure("child-invalid", kiExitStateConflict);
			}
			ChildRewrite rewrite { .path = rPath, .before = rPlan.bytes, .after = {} };
			if (!RenderDependencies(rPlan, target, rewrite.after))
			{
				return Failure("child-invalid", kiExitStateConflict);
			}
			rewrites.push_back(std::move(rewrite));
		}
		std::filesystem::path targetDiskPath = worktree / target;
		std::error_code targetError;
		bool bTargetPresent = std::filesystem::exists(ExtendedLengthPath(targetDiskPath), targetError);
		if (targetError)
		{
			return Failure("target-read-failed");
		}
		if (bTargetPresent && plans.find(target) == plans.end())
		{
			return Failure("plan-untracked", kiExitStateConflict);
		}
		// The sweep matches the same temporary filename shape the staging below creates, so it has to finish
		// before anything is staged or it would delete this operation's own pending files.
		if (!RemovePlanAtomicTemporarySiblings(worktree, target))
		{
			return Failure("orphan-cleanup-failed");
		}
		for (const ChildRewrite& rRewrite : rewrites)
		{
			if (!RemovePlanAtomicTemporarySiblings(worktree, rRewrite.path))
			{
				return Failure("orphan-cleanup-failed");
			}
		}
		std::vector<std::filesystem::path> stagedPaths(rewrites.size());
		auto DiscardStaged = [&stagedPaths](int64_t iFirst, int64_t iLast)
		{
			for (int64_t i = iFirst; i < iLast; ++i)
			{
				::DeleteFileW(stagedPaths.at(i).c_str());
			}
		};
		for (int64_t i = 0; i < std::ssize(rewrites); ++i)
		{
			if (!coordination::StageBytesAtomic(worktree / rewrites.at(i).path, rewrites.at(i).after, stagedPaths.at(i)))
			{
				DiscardStaged(0, i);
				return Failure("rewrite-failed");
			}
		}
		auto RestorePublished = [&rewrites, &worktree](int64_t iCount)
		{
			bool bRestored = true;
			for (int64_t i = 0; i < iCount; ++i)
			{
				bRestored = coordination::WriteBytesAtomic(worktree / rewrites.at(i).path, rewrites.at(i).before) && bRestored;
			}
			return bRestored;
		};
		for (int64_t i = 0; i < std::ssize(rewrites); ++i)
		{
			if (!coordination::CommitStagedBytes(stagedPaths.at(i), worktree / rewrites.at(i).path))
			{
				RestorePublished(i);
				DiscardStaged(i + 1, std::ssize(rewrites));
				return Failure("rewrite-failed");
			}
		}
		nlohmann::json changed = nlohmann::json::array();
		if (bTargetPresent)
		{
			// The target is deleted last, so no failure path ever has to bring a deleted Plan file back.
			if (::DeleteFileW(ExtendedLengthPath(targetDiskPath).c_str()) == FALSE)
			{
				return RestorePublished(std::ssize(rewrites)) ? Failure("delete-failed") : Failure("rewrite-failed");
			}
			changed.push_back(WideToUtf8(target));
		}
		for (const ChildRewrite& rRewrite : rewrites)
		{
			changed.push_back(WideToUtf8(rRewrite.path));
		}
		PrintResult({ { "status", "ok" }, { "code", bReject ? "rejected" : "completed" }, { "plan", WideToUtf8(target) }, { "changedPaths", changed } }, 2);
		return kiExitOk;
	}

	int RunPlanSchedulerCommand(std::span<wchar_t* const> argumentValues)
	{
		if (std::ssize(argumentValues) < 3)
		{
			return Failure("usage");
		}
		std::wstring operation = ToLowerInvariant(argumentValues[2]);
		Arguments arguments {};
		if (!ParseArguments(argumentValues, 3, arguments))
		{
			return Failure("usage");
		}
		if (arguments.bLintOnly && operation != L"validate")
		{
			return Failure("usage");
		}
		if (operation == L"validate")
		{
			return RunValidate(arguments);
		}
		if (operation == L"list")
		{
			return RunList(arguments);
		}
		if (operation == L"claim-next")
		{
			return RunClaimNext(arguments);
		}
		if (operation == L"claim-status")
		{
			return RunClaimStatus(arguments);
		}
		if (operation == L"unclaim")
		{
			return RunUnclaim(arguments);
		}
		if (operation == L"complete" || operation == L"reject")
		{
			return RunTerminal(operation, arguments);
		}
		return Failure("usage");
	}
} // namespace toolcli
