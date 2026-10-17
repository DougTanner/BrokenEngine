#include "HarnessLockCommands.h"

#include "CoordinationStore.h"
#include "ToolCliCommon.h"

#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>

namespace toolcli
{
	using coordination::CurrentUtcTimestamp;
	using coordination::Guard;
	using coordination::HasOwner;
	using coordination::Locator;
	using coordination::NewMetadata;
	using coordination::PrintMetadata;
	using coordination::ReadMetadata;
	using coordination::ValidateMetadataEnvelope;
	using coordination::WriteMetadataAtomic;

	static std::optional<Locator> MakeHarnessLocator(std::wstring_view key)
	{
		std::optional<std::wstring> optionalLogicalKey = coordination::NormalizeRelativeKey(key);
		if (!optionalLogicalKey)
		{
			Fail("invalid harness lock key");
			return std::nullopt;
		}

		std::optional<Locator> optionalLocator = coordination::MakeLocator(L"harness", *optionalLogicalKey);
		if (!optionalLocator)
		{
			Fail("could not resolve harness lock storage");
		}
		return optionalLocator;
	}

	static std::optional<Locator> ParseLocator(std::span<const wchar_t* const> argumentValues, int64_t iStartIndex, std::wstring& rOwner, std::wstring& rExpectedOwner, std::wstring& rSession, std::wstring& rWorktree)
	{
		std::wstring key;
		for (int64_t i = iStartIndex; i < std::ssize(argumentValues); ++i)
		{
			std::wstring_view argument = argumentValues[static_cast<size_t>(i)];
			std::wstring* pDestination = nullptr;
			if (argument == L"--key")
			{
				pDestination = &key;
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
			*pDestination = argumentValues[static_cast<size_t>(i)];
		}
		if (key.empty())
		{
			Fail("harness lock requires --key");
			return std::nullopt;
		}
		// WideToUtf8 rejects malformed UTF-16 instead of substituting, so an empty result for a nonempty value means the
		// conversion failed. These values reach the lock path and metadata, so reject them before anything is written.
		// --expect is only compared against stored metadata and never persisted, so a failed conversion there just fails the match.
		for (const std::wstring* pValue : {&key, &rOwner, &rSession, &rWorktree})
		{
			if (!pValue->empty() && WideToUtf8(*pValue).empty())
			{
				Fail("lock option value is not valid text");
				return std::nullopt;
			}
		}
		return MakeHarnessLocator(key);
	}

	static bool StampHarnessHeartbeat(const std::filesystem::path& rPath, nlohmann::json& rMetadata)
	{
		rMetadata["heartbeatAt"] = CurrentUtcTimestamp();
		rMetadata["heartbeatPid"] = ::GetCurrentProcessId();
		return WriteMetadataAtomic(rPath, rMetadata);
	}


	int64_t RunHarnessLockCommand(std::span<const wchar_t* const> argumentValues)
	{
		if (std::ssize(argumentValues) < 3)
		{
			Fail("lock requires token, claim, status, release, steal, or heartbeat");
			return kiExitFailure;
		}
		std::wstring verb = ToLowerInvariant(argumentValues[2]);
		if (verb == L"token")
		{
			if (std::ssize(argumentValues) != 3)
			{
				Fail("lock token accepts no arguments");
				return kiExitFailure;
			}
			return PrintOwnerToken();
		}
		if (verb != L"claim" && verb != L"status" && verb != L"release" && verb != L"steal" && verb != L"heartbeat")
		{
			Fail("unknown lock verb");
			return kiExitFailure;
		}

		std::wstring owner;
		std::wstring expectedOwner;
		std::wstring session;
		std::wstring worktree;
		std::optional<Locator> optionalLocator = ParseLocator(argumentValues, 3, owner, expectedOwner, session, worktree);
		if (!optionalLocator)
		{
			return kiExitFailure;
		}
		if ((verb == L"claim" || verb == L"steal") && (owner.empty() || session.empty() || worktree.empty()))
		{
			Fail("claim and steal require --owner, --session, and --worktree");
			return kiExitFailure;
		}
		if ((verb == L"release" || verb == L"heartbeat") && owner.empty())
		{
			Fail("release and heartbeat require --owner");
			return kiExitFailure;
		}
		if (verb == L"steal" && expectedOwner.empty())
		{
			Fail("steal requires --expect");
			return kiExitFailure;
		}

		if (!coordination::EnsureParentDirectory(optionalLocator->path))
		{
			Fail("could not create lock directory");
			return kiExitFailure;
		}
		bool bContentionObserved = false;
		std::string failureReason;
		Guard guard(optionalLocator->path.wstring() + L".guard", bContentionObserved, failureReason);
		if (!guard.mbValid)
		{
			Fail("could not acquire lock transition guard (" + failureReason + ")");
			return kiExitFailure;
		}

		std::error_code error;
		nlohmann::json metadata;
		bool bExists = std::filesystem::exists(ExtendedLengthPath(optionalLocator->path), error);
		if (error)
		{
			Fail("could not inspect lock");
			return kiExitFailure;
		}
		if (bExists && !ReadMetadata(optionalLocator->path, metadata))
		{
			Fail("lock metadata is unreadable");
			return kiExitFailure;
		}
		if (bExists && !ValidateMetadataEnvelope(metadata, *optionalLocator, coordination::kiSchemaVersion))
		{
			Fail("lock metadata envelope is invalid");
			return kiExitFailure;
		}

		if (verb == L"status")
		{
			if (!bExists)
			{
				std::cout << "{\"held\":false}\n";
				return kiExitStateConflict;
			}
			PrintMetadata(metadata);
			return kiExitOk;
		}
		if (verb == L"claim")
		{
			if (bExists)
			{
				PrintMetadata(metadata);
				return kiExitStateConflict;
			}
			metadata = NewMetadata(*optionalLocator, owner, session, worktree);
			if (!WriteMetadataAtomic(optionalLocator->path, metadata))
			{
				FailWindows("write lock metadata");
				return kiExitFailure;
			}
			PrintMetadata(metadata);
			return kiExitOk;
		}
		if (!bExists)
		{
			std::cout << "{\"held\":false}\n";
			return kiExitStateConflict;
		}
		if ((verb == L"release" || verb == L"heartbeat") && !HasOwner(metadata, owner))
		{
			PrintMetadata(metadata);
			return kiExitStateConflict;
		}
		if (verb == L"steal" && !HasOwner(metadata, expectedOwner))
		{
			PrintMetadata(metadata);
			return kiExitStateConflict;
		}
		if (verb == L"release")
		{
			if (::DeleteFileW(ExtendedLengthPath(optionalLocator->path).c_str()) == FALSE)
			{
				FailWindows("release lock");
				return kiExitFailure;
			}
			return kiExitOk;
		}
		if (verb == L"heartbeat")
		{
			if (!StampHarnessHeartbeat(optionalLocator->path, metadata))
			{
				FailWindows("write lock metadata");
				return kiExitFailure;
			}
			PrintMetadata(metadata);
			return kiExitOk;
		}

		metadata = NewMetadata(*optionalLocator, owner, session, worktree);
		if (!WriteMetadataAtomic(optionalLocator->path, metadata))
		{
			FailWindows("replace lock metadata");
			return kiExitFailure;
		}
		PrintMetadata(metadata);
		return kiExitOk;
	}

	bool RefreshHarnessHeartbeat(std::wstring_view owner, int64_t iMaximumWaitMilliseconds)
	{
		std::optional<Locator> optionalLocator = MakeHarnessLocator(L"default");
		if (!optionalLocator)
		{
			return false;
		}
		bool bContentionObserved = false;
		std::string failureReason;
		Guard guard(optionalLocator->path.wstring() + L".guard", bContentionObserved, failureReason, iMaximumWaitMilliseconds);
		if (!guard.mbValid)
		{
			return false;
		}
		nlohmann::json metadata;
		if (!ReadMetadata(optionalLocator->path, metadata) || !ValidateMetadataEnvelope(metadata, *optionalLocator, coordination::kiSchemaVersion) || !HasOwner(metadata, owner))
		{
			return false;
		}
		return StampHarnessHeartbeat(optionalLocator->path, metadata);
	}
}
