#include "LandingLockCommands.h"

#include "CoordinationStore.h"
#include "LandingLockLifecycle.h"
#include "ToolCliCommon.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <thread>

namespace toolcli
{
	using coordination::CurrentUtcTicks;
	using coordination::CurrentUtcTimestamp;
	using coordination::FormatUtcTimestamp;
	using coordination::Guard;
	using coordination::HasOwner;
	using coordination::Locator;
	using coordination::ParseUtcTimestamp;
	using coordination::PrintMetadata;
	using coordination::ReadMetadata;
	using coordination::WriteMetadataAtomic;

	constexpr int64_t kiMaximumWaitSeconds = 3'600;
	constexpr int64_t kiMinimumPollMilliseconds = 50;
	constexpr int64_t kiMaximumPollMilliseconds = 5'000;
	constexpr int64_t kiDefaultPollMilliseconds = 500;

	static std::optional<Locator> MakeLandingLocator(std::wstring_view repository)
	{
		if (repository.empty())
		{
			Fail("landing lock requires --repo");
			return std::nullopt;
		}

		std::optional<std::wstring> logicalKey = coordination::CanonicalizeDirectoryPath(repository);
		if (!logicalKey)
		{
			Fail("invalid lock logical key");
			return std::nullopt;
		}

		std::optional<Locator> locator = coordination::MakeLocator(L"landing", *logicalKey);
		if (!locator)
		{
			Fail("could not resolve lock storage");
			return std::nullopt;
		}
		return locator;
	}

	static std::optional<Locator> ParseLocator(std::span<const wchar_t* const> argumentValues, int64_t iStartIndex, std::wstring& rOwner, std::wstring& rExpectedOwner, std::wstring& rSession, std::wstring& rWorktree, int64_t& riLeaseSeconds, int64_t& riWaitSeconds, int64_t& riPollMilliseconds)
	{
		std::wstring repository;
		for (int64_t i = iStartIndex; i < std::ssize(argumentValues); ++i)
		{
			std::wstring_view argument = argumentValues[i];
			std::wstring* pDestination = nullptr;
			int64_t* piNumericDestination = nullptr;
			if (argument == L"--repo")
			{
				pDestination = &repository;
			}
			else if (argument == L"--owner")
			{
				pDestination = &rOwner;
			}
			else if (argument == L"--expect")
			{
				pDestination = &rExpectedOwner;
			}
			else if (argument == L"--session")
			{
				pDestination = &rSession;
			}
			else if (argument == L"--worktree")
			{
				pDestination = &rWorktree;
			}
			else if (argument == L"--lease-seconds")
			{
				piNumericDestination = &riLeaseSeconds;
			}
			else if (argument == L"--wait-seconds")
			{
				piNumericDestination = &riWaitSeconds;
			}
			else if (argument == L"--poll-milliseconds")
			{
				piNumericDestination = &riPollMilliseconds;
			}
			else
			{
				Fail("unknown lock argument: " + WideToUtf8(argument));
				return std::nullopt;
			}
			if (++i >= std::ssize(argumentValues))
			{
				Fail("lock option requires a value");
				return std::nullopt;
			}
			if (piNumericDestination != nullptr)
			{
				wchar_t* pEnd = nullptr;
				*piNumericDestination = std::wcstoll(argumentValues[i], &pEnd, 10);
				if (pEnd == argumentValues[i] || *pEnd != L'\0')
				{
					Fail(WideToUtf8(argument) + " must be an integer");
					return std::nullopt;
				}
				continue;
			}
			*pDestination = argumentValues[i];
		}
		return MakeLandingLocator(repository);
	}

	enum class LandingRecordState
	{
		kReadable,
		kAbsent,
		kUnverifiable,
	};

	enum class LandingReleaseOperation
	{
		kRelease,
		kSteal,
	};

	static int EmitLandingConflict(const Locator& rLocator, const nlohmann::json& rMetadata, LandingRecordState eRecordState)
	{
		if (eRecordState == LandingRecordState::kReadable)
		{
			PrintMetadata(landing::LandingStatus(rMetadata, rLocator));
		}
		else if (eRecordState == LandingRecordState::kAbsent)
		{
			std::cout << "{\"held\":false}\n";
		}
		else
		{
			std::cout << "{\"held\":true,\"leaseState\":\"unverifiable\"}\n";
		}
		return kiExitStateConflict;
	}

	static int HandleClaim(const Locator& rLocator, nlohmann::json& rMetadata, bool bExists, std::wstring_view owner, std::wstring_view session, std::wstring_view worktree, int64_t iLeaseSeconds)
	{
		if (bExists)
		{
			return EmitLandingConflict(rLocator, rMetadata, LandingRecordState::kReadable);
		}
		rMetadata = landing::NewLandingMetadata(rLocator, owner, session, worktree, std::chrono::seconds(iLeaseSeconds));
		if (!WriteMetadataAtomic(rLocator.path, rMetadata))
		{
			FailWindows("write lock metadata");
			return kiExitFailure;
		}
		PrintMetadata(landing::LandingStatus(rMetadata, rLocator));
		return kiExitOk;
	}

	static int HandleRefresh(const Locator& rLocator, nlohmann::json& rMetadata, bool bExists, std::wstring_view owner)
	{
		uint64_t uiCurrentTicks = CurrentUtcTicks();
		std::optional<landing::LandingLease> lease = bExists ? landing::ValidateLandingLease(rMetadata, rLocator, uiCurrentTicks) : std::nullopt;
		if (!lease)
		{
			return EmitLandingConflict(rLocator, rMetadata, bExists ? LandingRecordState::kReadable : LandingRecordState::kAbsent);
		}
		if (lease->owner != WideToUtf8(owner))
		{
			return EmitLandingConflict(rLocator, rMetadata, bExists ? LandingRecordState::kReadable : LandingRecordState::kAbsent);
		}
		if (uiCurrentTicks >= lease->uiExpiresTicks)
		{
			return EmitLandingConflict(rLocator, rMetadata, bExists ? LandingRecordState::kReadable : LandingRecordState::kAbsent);
		}
		std::string timestamp = CurrentUtcTimestamp();
		uint64_t uiHeartbeatTicks = 0;
		ParseUtcTimestamp(timestamp, uiHeartbeatTicks);
		if (uiHeartbeatTicks < lease->uiHeartbeatTicks)
		{
			return EmitLandingConflict(rLocator, rMetadata, LandingRecordState::kReadable);
		}
		rMetadata["heartbeatAt"] = timestamp;
		rMetadata["expiresAt"] = FormatUtcTimestamp(uiHeartbeatTicks + static_cast<uint64_t>(lease->duration.count()) * 10'000'000ui64);
		if (!WriteMetadataAtomic(rLocator.path, rMetadata))
		{
			FailWindows("refresh lock metadata");
			return kiExitFailure;
		}
		PrintMetadata(landing::LandingStatus(rMetadata, rLocator));
		return kiExitOk;
	}

	static int HandleRecover(const Locator& rLocator, nlohmann::json& rMetadata, bool bExists, std::wstring_view owner, std::wstring_view expectedOwner, std::wstring_view session, std::wstring_view worktree, int64_t iLeaseSeconds)
	{
		uint64_t uiNow = CurrentUtcTicks();
		std::optional<landing::LandingLease> lease = bExists ? landing::ValidateLandingLease(rMetadata, rLocator, uiNow) : std::nullopt;
		if (!lease)
		{
			return EmitLandingConflict(rLocator, rMetadata, bExists ? LandingRecordState::kReadable : LandingRecordState::kAbsent);
		}
		if (lease->owner != WideToUtf8(expectedOwner))
		{
			return EmitLandingConflict(rLocator, rMetadata, bExists ? LandingRecordState::kReadable : LandingRecordState::kAbsent);
		}
		if (uiNow < lease->uiExpiresTicks)
		{
			return EmitLandingConflict(rLocator, rMetadata, bExists ? LandingRecordState::kReadable : LandingRecordState::kAbsent);
		}
		if (!landing::AllRegisteredWorktreesClear(rLocator))
		{
			return EmitLandingConflict(rLocator, rMetadata, bExists ? LandingRecordState::kReadable : LandingRecordState::kAbsent);
		}
		nlohmann::json revalidatedMetadata;
		if (!ReadMetadata(rLocator.path, revalidatedMetadata))
		{
			std::error_code error;
			bool bRevalidatedExists = std::filesystem::exists(ExtendedLengthPath(rLocator.path), error);
			return EmitLandingConflict(rLocator, rMetadata, !error && !bRevalidatedExists ? LandingRecordState::kAbsent : LandingRecordState::kUnverifiable);
		}
		if (revalidatedMetadata != rMetadata)
		{
			return EmitLandingConflict(rLocator, revalidatedMetadata, LandingRecordState::kReadable);
		}
		rMetadata = landing::NewLandingMetadata(rLocator, owner, session, worktree, std::chrono::seconds(iLeaseSeconds));
		if (!WriteMetadataAtomic(rLocator.path, rMetadata))
		{
			FailWindows("recover lock metadata");
			return kiExitFailure;
		}
		PrintMetadata(landing::LandingStatus(rMetadata, rLocator));
		return kiExitOk;
	}

	static int HandleReleaseOrSteal(const Locator& rLocator, const nlohmann::json& rMetadata, bool bExists, LandingReleaseOperation eOperation, std::wstring_view owner, std::wstring_view expectedOwner)
	{
		if (!bExists)
		{
			return EmitLandingConflict(rLocator, rMetadata, bExists ? LandingRecordState::kReadable : LandingRecordState::kAbsent);
		}
		if (eOperation == LandingReleaseOperation::kRelease && !HasOwner(rMetadata, owner))
		{
			return EmitLandingConflict(rLocator, rMetadata, bExists ? LandingRecordState::kReadable : LandingRecordState::kAbsent);
		}
		if (eOperation == LandingReleaseOperation::kSteal && !HasOwner(rMetadata, expectedOwner))
		{
			return EmitLandingConflict(rLocator, rMetadata, bExists ? LandingRecordState::kReadable : LandingRecordState::kAbsent);
		}

		if (eOperation == LandingReleaseOperation::kRelease)
		{
			if (::DeleteFileW(ExtendedLengthPath(rLocator.path).c_str()) == FALSE)
			{
				FailWindows("release lock");
				return kiExitFailure;
			}
			return kiExitOk;
		}

		// steal: a lease-based landing lock is never stolen; recover is the expired-lease takeover.
		return EmitLandingConflict(rLocator, rMetadata, LandingRecordState::kReadable);
	}

	// Bounded blocking claim. Every attempt reads, classifies, and writes under its own guard scope; the guard is
	// released before each sleep so a holder releasing its lease can always make progress. Only the final outcome
	// prints, so the invocation still emits exactly one JSON object.
	static int WaitForLandingClaim(const Locator& rLocator, std::wstring_view owner, std::wstring_view session, std::wstring_view worktree, int64_t iLeaseSeconds, int64_t iWaitSeconds, int64_t iPollMilliseconds)
	{
		std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(iWaitSeconds);
		for (;;)
		{
			nlohmann::json metadata;
			int64_t iSleepMilliseconds = iPollMilliseconds;
			{
				std::chrono::steady_clock::duration remaining = deadline - std::chrono::steady_clock::now();
				if (remaining <= std::chrono::steady_clock::duration::zero())
				{
					return EmitLandingConflict(rLocator, metadata, LandingRecordState::kUnverifiable);
				}
				bool bContentionObserved = false;
				std::string failureReason;
				// Round the guard budget up: a floored one could expire just before the deadline and report a bounded
				// conflict for a wait that still had time left.
				Guard guard(rLocator.path.wstring() + L".guard", bContentionObserved, failureReason, std::chrono::ceil<std::chrono::milliseconds>(remaining).count());
				if (!guard.mbValid)
				{
					if (std::chrono::steady_clock::now() >= deadline)
					{
						return EmitLandingConflict(rLocator, metadata, LandingRecordState::kUnverifiable);
					}
					Fail("could not acquire lock transition guard (" + failureReason + ")");
					return kiExitFailure;
				}

				std::error_code error;
				bool bExists = std::filesystem::exists(ExtendedLengthPath(rLocator.path), error);
				if (error)
				{
					Fail("could not inspect lock");
					return kiExitFailure;
				}
				if (!bExists)
				{
					if (std::chrono::steady_clock::now() >= deadline)
					{
						return EmitLandingConflict(rLocator, metadata, LandingRecordState::kAbsent);
					}
					return HandleClaim(rLocator, metadata, false, owner, session, worktree, iLeaseSeconds);
				}
				if (!ReadMetadata(rLocator.path, metadata))
				{
					return EmitLandingConflict(rLocator, metadata, LandingRecordState::kUnverifiable);
				}

				uint64_t uiNow = CurrentUtcTicks();
				std::optional<landing::LandingLease> lease = landing::ValidateLandingLease(metadata, rLocator, uiNow);
				if (!lease)
				{
					// Unverifiable metadata is never repaired by a waiter; taking it over needs user authority.
					return EmitLandingConflict(rLocator, metadata, LandingRecordState::kReadable);
				}
				if (uiNow >= lease->uiExpiresTicks)
				{
					// A refused recovery (a registered worktree is mid Git operation) is not final: keep waiting.
					if (landing::AllRegisteredWorktreesClear(rLocator))
					{
						if (std::chrono::steady_clock::now() >= deadline)
						{
							return EmitLandingConflict(rLocator, metadata, LandingRecordState::kReadable);
						}
						// The standalone recover verb re-reads to revalidate metadata it was handed; here the metadata was
						// read inside this same guard scope, which serializes every lock transition, so it cannot have moved.
						metadata = landing::NewLandingMetadata(rLocator, owner, session, worktree, std::chrono::seconds(iLeaseSeconds));
						if (!WriteMetadataAtomic(rLocator.path, metadata))
						{
							FailWindows("recover lock metadata");
							return kiExitFailure;
						}
						PrintMetadata(landing::LandingStatus(metadata, rLocator));
						return kiExitOk;
					}
				}
				else if (lease->owner == WideToUtf8(owner) && metadata["session"].get<std::string>() == WideToUtf8(session) && metadata["worktree"].get<std::string>() == WideToUtf8(worktree))
				{
					// A live lease this requester already holds is reported at once, never waited on or refreshed.
					return EmitLandingConflict(rLocator, metadata, LandingRecordState::kReadable);
				}
				else
				{
					// Wake just after the foreign lease expires when that comes first.
					iSleepMilliseconds = std::min<int64_t>(iSleepMilliseconds, static_cast<int64_t>((lease->uiExpiresTicks - uiNow) / 10'000ui64) + 1);
				}
			}

			std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			int64_t iRemainingMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
			// A sleep that reaches the deadline leaves no attempt after it, so report the lease this attempt read
			// rather than waking past the deadline with nothing left to classify.
			if (now >= deadline || iSleepMilliseconds >= iRemainingMilliseconds)
			{
				return EmitLandingConflict(rLocator, metadata, LandingRecordState::kReadable);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(std::max<int64_t>(1, std::min<int64_t>(iSleepMilliseconds, iRemainingMilliseconds))));
		}
	}

	int RunLandingLockCommand(std::span<const wchar_t* const> argumentValues)
	{
		if (std::ssize(argumentValues) < 3)
		{
			Fail("lock requires claim, status, refresh, recover, release, or steal");
			return kiExitFailure;
		}
		std::wstring verb = ToLowerInvariant(argumentValues[2]);
		if (verb != L"claim" && verb != L"status" && verb != L"refresh" && verb != L"recover" && verb != L"release" && verb != L"steal")
		{
			Fail("unknown lock verb");
			return kiExitFailure;
		}

		std::wstring owner;
		std::wstring expectedOwner;
		std::wstring session;
		std::wstring worktree;
		int64_t iLeaseSeconds = 0;
		int64_t iWaitSeconds = 0;
		int64_t iPollMilliseconds = kiDefaultPollMilliseconds;
		std::optional<Locator> locator = ParseLocator(argumentValues, 3, owner, expectedOwner, session, worktree, iLeaseSeconds, iWaitSeconds, iPollMilliseconds);
		if (!locator)
		{
			return kiExitFailure;
		}
		if ((verb == L"claim" || verb == L"steal" || verb == L"recover") && (owner.empty() || session.empty() || worktree.empty()))
		{
			Fail("claim, steal, and recover require --owner, --session, and --worktree");
			return kiExitFailure;
		}
		if ((verb == L"release" || verb == L"refresh") && owner.empty())
		{
			Fail("release and refresh require --owner");
			return kiExitFailure;
		}
		if ((verb == L"steal" || verb == L"recover") && expectedOwner.empty())
		{
			Fail("steal and recover require --expect");
			return kiExitFailure;
		}
		if ((verb == L"claim" || verb == L"recover") && !landing::IsValidLeaseDuration(std::chrono::seconds(iLeaseSeconds)))
		{
			Fail("landing claim and recover require --lease-seconds in the range 60..86400");
			return kiExitFailure;
		}
		if (iWaitSeconds < 0 || iWaitSeconds > kiMaximumWaitSeconds)
		{
			Fail("--wait-seconds must be in the range 0..3600");
			return kiExitFailure;
		}
		if (iPollMilliseconds < kiMinimumPollMilliseconds || iPollMilliseconds > kiMaximumPollMilliseconds)
		{
			Fail("--poll-milliseconds must be in the range 50..5000");
			return kiExitFailure;
		}

		std::error_code error;
		if (!coordination::EnsureParentDirectory(locator->path))
		{
			Fail("could not create lock directory");
			return kiExitFailure;
		}
		// A positive wait owns its own per-attempt guard scopes; every other invocation keeps the single-guard shape.
		if (verb == L"claim" && iWaitSeconds > 0)
		{
			return WaitForLandingClaim(*locator, owner, session, worktree, iLeaseSeconds, iWaitSeconds, iPollMilliseconds);
		}
		bool bContentionObserved = false;
		std::string failureReason;
		Guard guard(locator->path.wstring() + L".guard", bContentionObserved, failureReason);
		if (!guard.mbValid)
		{
			Fail("could not acquire lock transition guard (" + failureReason + ")");
			return kiExitFailure;
		}

		nlohmann::json metadata;
		bool bExists = std::filesystem::exists(ExtendedLengthPath(locator->path), error);
		if (error)
		{
			Fail("could not inspect lock");
			return kiExitFailure;
		}
		bool bReadable = !bExists || ReadMetadata(locator->path, metadata);
		if (bExists && !bReadable && verb != L"status")
		{
			Fail("lock metadata is unreadable");
			return kiExitFailure;
		}

		if (verb == L"status")
		{
			if (!bExists)
			{
				return EmitLandingConflict(*locator, metadata, LandingRecordState::kAbsent);
			}
			PrintMetadata(landing::LandingStatus(metadata, *locator));
			return kiExitOk;
		}

		if (verb == L"claim")
		{
			return HandleClaim(*locator, metadata, bExists, owner, session, worktree, iLeaseSeconds);
		}

		if (verb == L"refresh")
		{
			return HandleRefresh(*locator, metadata, bExists, owner);
		}

		if (verb == L"recover")
		{
			return HandleRecover(*locator, metadata, bExists, owner, expectedOwner, session, worktree, iLeaseSeconds);
		}

		return HandleReleaseOrSteal(*locator, metadata, bExists, verb == L"release" ? LandingReleaseOperation::kRelease : LandingReleaseOperation::kSteal, owner, expectedOwner);
	}

} // namespace toolcli
