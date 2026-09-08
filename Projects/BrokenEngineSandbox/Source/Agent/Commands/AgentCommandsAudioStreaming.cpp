#include "Agent/Commands/AgentCommandsAudioStreaming.h"

#if defined(BT_CLIENT)

#include "Agent/AgentCommandServer.h"
#include "Agent/Commands/AudioStreamingFixture.h"

#if defined(BT_DEBUG)
#include "Data/Audio.h"
#endif

namespace game
{

namespace
{

#if defined(BT_DEBUG)

constexpr uint64_t kuiAudioStreamingFixtureReadLength = 16 * 1024;

const char* AudioStreamingFixturePartitionName(engine::AudioStreamingFixturePartition ePartition)
{
	switch (ePartition)
	{
	case engine::AudioStreamingFixturePartition::kMain:
		return "main";
	case engine::AudioStreamingFixturePartition::kLoader0:
		return "loader0";
	case engine::AudioStreamingFixturePartition::kLoader1:
		return "loader1";
	}
	return "unknown";
}

const char* AudioStreamingFixturePhaseName(engine::AudioStreamingFixturePhase ePhase)
{
	switch (ePhase)
	{
	case engine::AudioStreamingFixturePhase::kStartup:
		return "startup";
	case engine::AudioStreamingFixturePhase::kRefillQueued:
		return "refill_queued";
	case engine::AudioStreamingFixturePhase::kRefillLoading:
		return "refill_loading";
	case engine::AudioStreamingFixturePhase::kRefillReady:
		return "refill_ready";
	case engine::AudioStreamingFixturePhase::kRefillCancelled:
		return "refill_cancelled";
	case engine::AudioStreamingFixturePhase::kExistingQueued:
		return "existing_queued";
	case engine::AudioStreamingFixturePhase::kExistingComplete:
		return "existing_complete";
	}
	return "unknown";
}

const char* AudioStreamingFixtureQueueStateName(engine::AudioStreamingFixtureQueueState eState)
{
	switch (eState)
	{
	case engine::AudioStreamingFixtureQueueState::kFree:
		return "free";
	case engine::AudioStreamingFixtureQueueState::kQueued:
		return "queued";
	case engine::AudioStreamingFixtureQueueState::kLoading:
		return "loading";
	case engine::AudioStreamingFixtureQueueState::kReady:
		return "ready";
	}
	return "unknown";
}

const char* AudioStreamingFixtureHoldOwnerName(engine::AudioStreamingFixtureHoldOwner eOwner)
{
	switch (eOwner)
	{
	case engine::AudioStreamingFixtureHoldOwner::kNone:
		return "none";
	case engine::AudioStreamingFixtureHoldOwner::kControlled:
		return "controlled";
	case engine::AudioStreamingFixtureHoldOwner::kInvalid:
		return "invalid";
	}
	return "unknown";
}

const char* AudioStreamingFixtureHoldStateName(engine::AudioStreamingFixtureHoldState eState)
{
	switch (eState)
	{
	case engine::AudioStreamingFixtureHoldState::kIdle:
		return "idle";
	case engine::AudioStreamingFixtureHoldState::kArmed:
		return "armed";
	case engine::AudioStreamingFixtureHoldState::kConsumed:
		return "consumed";
	}
	return "unknown";
}

enum class AudioStreamingFixtureCoexistenceFlags : uint8_t
{
	kStaging = 0x01,
	kControlled = 0x02,
	kComplete = 0x04,
};
using AudioStreamingFixtureCoexistenceFlags_t = common::Flags<AudioStreamingFixtureCoexistenceFlags>;

struct AudioStreamingFixtureCoexistenceState
{
	~AudioStreamingFixtureCoexistenceState()
	{
		if ((flags & AudioStreamingFixtureCoexistenceFlags::kStaging) && engine::gpFileManager != nullptr)
		{
			engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseCoexistence();
		}
	if ((flags & AudioStreamingFixtureCoexistenceFlags::kControlled) && !(flags & AudioStreamingFixtureCoexistenceFlags::kComplete))
		{
			if (engine::gpFileManager != nullptr)
			{
				engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseHold();
			}
			if (engine::gpAudioManager != nullptr)
			{
				engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseControls();
			}
		}
	}

	AudioStreamingFixtureCoexistenceFlags_t flags;
	uint64_t uiStartSequence = 0;
};

struct AudioStreamingFixtureStartState
{
	~AudioStreamingFixtureStartState()
	{
		if (!bComplete)
		{
			if (engine::gpFileManager != nullptr)
			{
				engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseHold();
			}
			if (engine::gpAudioManager != nullptr)
			{
				engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseControls();
			}
		}
	}

	bool bComplete = false;
};

struct AudioStreamingFixtureReleaseState
{
	~AudioStreamingFixtureReleaseState()
	{
		if (engine::gpFileManager != nullptr)
		{
			engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseHold();
		}
		if (engine::gpAudioManager != nullptr)
		{
			engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseControls();
		}
	}
};

struct AudioStreamingFixtureInvalidState
{
	~AudioStreamingFixtureInvalidState()
	{
		if (engine::gpFileManager != nullptr)
		{
			engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseInvalid();
		}
	}

	engine::AudioStreamingFixtureInvalidResult result;
	bool bHasResult = false;
};

nlohmann::json BuildAudioStreamingFixtureVoiceSummary(const engine::AudioStreamingFixtureVoiceSummary& rSummary)
{
	return {
		{"present", rSummary.flags & engine::AudioStreamingFixtureVoiceFlags::kPresent},
		{"crc", rSummary.crc},
		{"volume", rSummary.fVolume},
		{"fadingIn", rSummary.flags & engine::AudioStreamingFixtureVoiceFlags::kFadingIn},
		{"fadingOut", rSummary.flags & engine::AudioStreamingFixtureVoiceFlags::kFadingOut},
		{"pendingSlots", rSummary.iPendingSlots},
		{"buffersQueued", rSummary.iBuffersQueued},
		{"underrunning", rSummary.flags & engine::AudioStreamingFixtureVoiceFlags::kUnderrunning},
	};
}

nlohmann::json BuildAudioStreamingFixtureSnapshot()
{
	engine::AudioStreamingFixtureSnapshot snapshot;
	engine::AudioStreamingFixtureAudioSnapshot audioSnapshot;
	bool bCombinedCoherent = false;
	for (int64_t iAttempt = 0; iAttempt < 8; ++iAttempt)
	{
		const engine::AudioStreamingFixtureSnapshot before = engine::gpAgentCommandServer->mpAudioStreamingFixture->InspectFile();
		audioSnapshot = engine::gpAgentCommandServer->mpAudioStreamingFixture->InspectAudio();
		snapshot = engine::gpAgentCommandServer->mpAudioStreamingFixture->InspectFile();
		bCombinedCoherent = before.uiScenarioGate == snapshot.uiScenarioGate && before.uiActiveWriters == snapshot.uiActiveWriters
		                 && before.uiReservationBoundary == snapshot.uiReservationBoundary && before.uiRetryCount == snapshot.uiRetryCount
		                 && before.eHoldOwner == snapshot.eHoldOwner && before.eHoldState == snapshot.eHoldState
		                 && (before.flags & engine::AudioStreamingFixtureSnapshotFlags::kLoaderStaged) == (snapshot.flags & engine::AudioStreamingFixtureSnapshotFlags::kLoaderStaged)
		                 && before.uiHeldIndex == snapshot.uiHeldIndex && before.uiHeldGeneration == snapshot.uiHeldGeneration
		                 && before.uiMainHead == snapshot.uiMainHead && before.uiLoader0Head == snapshot.uiLoader0Head
		                 && before.uiLoader1Head == snapshot.uiLoader1Head;
		for (size_t i = 0; bCombinedCoherent && i < snapshot.poolEntries.size(); ++i)
		{
			const engine::AudioStreamingFixturePoolEntry& rBefore = before.poolEntries[i];
			const engine::AudioStreamingFixturePoolEntry& rAfter = snapshot.poolEntries[i];
			bCombinedCoherent = rBefore.uiIndex == rAfter.uiIndex && rBefore.eState == rAfter.eState
			                 && rBefore.uiGeneration == rAfter.uiGeneration && rBefore.crc == rAfter.crc
			                 && rBefore.uiOffset == rAfter.uiOffset && rBefore.uiLength == rAfter.uiLength;
		}
		if (bCombinedCoherent)
		{
			break;
		}
	}
	snapshot.flags.Set(engine::AudioStreamingFixtureSnapshotFlags::kCoherent, (snapshot.flags & engine::AudioStreamingFixtureSnapshotFlags::kCoherent) && bCombinedCoherent);
	nlohmann::json result;
	result["scenarioGate"] = snapshot.uiScenarioGate;
	result["scenarioGeneration"] = snapshot.uiScenarioGeneration;
	result["activeWriters"] = snapshot.uiActiveWriters;
	result["reservationStart"] = snapshot.uiReservationStart;
	result["reservationBoundary"] = snapshot.uiReservationBoundary;
	result["retryCount"] = snapshot.uiRetryCount;
	result["holdOwner"] = AudioStreamingFixtureHoldOwnerName(snapshot.eHoldOwner);
	result["holdState"] = AudioStreamingFixtureHoldStateName(snapshot.eHoldState);
	result["heldPoolIndex"] = snapshot.uiHeldIndex == std::numeric_limits<uint32_t>::max()
			? nlohmann::json(nullptr)
			: nlohmann::json(snapshot.uiHeldIndex);
	result["heldGeneration"] = snapshot.uiHeldGeneration;
	result["heldState"] = AudioStreamingFixtureQueueStateName(snapshot.eHeldState);
	result["coherent"] = snapshot.flags & engine::AudioStreamingFixtureSnapshotFlags::kCoherent;
	result["gapFree"] = snapshot.flags & engine::AudioStreamingFixtureSnapshotFlags::kGapFree;
	result["controls"] = {
		{"holdArmed", snapshot.eHoldState != engine::AudioStreamingFixtureHoldState::kIdle},
		{"loaderStaged", snapshot.flags & engine::AudioStreamingFixtureSnapshotFlags::kLoaderStaged},
		{"controlledPublication", audioSnapshot.flags & engine::AudioStreamingFixtureAudioSnapshotFlags::kControlledPublication},
		{"saturationActive", audioSnapshot.flags & engine::AudioStreamingFixtureAudioSnapshotFlags::kSaturationActive},
		{"publicationAllowance", audioSnapshot.uiPublicationAllowance},
	};
	result["poolEntries"] = nlohmann::json::array();
	for (const engine::AudioStreamingFixturePoolEntry& rEntry : snapshot.poolEntries)
	{
		result["poolEntries"].push_back(
		{
			{"index", rEntry.uiIndex},
			{"state", AudioStreamingFixtureQueueStateName(rEntry.eState)},
			{"generation", rEntry.uiGeneration},
			{"crc", rEntry.crc},
			{"offset", rEntry.uiOffset},
			{"length", rEntry.uiLength},
		});
	}
	result["current"] = BuildAudioStreamingFixtureVoiceSummary(audioSnapshot.current);
	result["newestFade"] = BuildAudioStreamingFixtureVoiceSummary(audioSnapshot.newestFade);
	result["olderFades"] = {
		{"count", audioSnapshot.olderFades.iCount},
		{"pendingSlots", audioSnapshot.olderFades.iPendingSlots},
		{"underrunningCount", audioSnapshot.olderFades.iUnderrunningCount},
	};
	result["partitions"] = nlohmann::json::array(
	{
		{{"name", "main"}, {"head", snapshot.uiMainHead}, {"dropped", snapshot.uiMainDropped}},
		{{"name", "loader0"}, {"head", snapshot.uiLoader0Head}, {"dropped", snapshot.uiLoader0Dropped}},
		{{"name", "loader1"}, {"head", snapshot.uiLoader1Head}, {"dropped", snapshot.uiLoader1Dropped}},
	});
	const bool bOverflow = snapshot.uiMainDropped != 0 || snapshot.uiLoader0Dropped != 0 || snapshot.uiLoader1Dropped != 0;
	result["overflow"] = bOverflow;
	result["evidenceValid"] = (snapshot.flags & engine::AudioStreamingFixtureSnapshotFlags::kCoherent)
	                        && (snapshot.flags & engine::AudioStreamingFixtureSnapshotFlags::kGapFree) && !bOverflow;
	result["records"] = nlohmann::json::array();
	for (int64_t i = 0; i < snapshot.iRecordCount; ++i)
	{
		const engine::AudioStreamingFixtureRecord& rRecord = snapshot.records.at(i);
		nlohmann::json record;
		record["sequence"] = rRecord.uiSequence;
		record["scenarioGeneration"] = rRecord.uiScenarioGeneration;
		record["partition"] = AudioStreamingFixturePartitionName(rRecord.ePartition);
		record["phase"] = AudioStreamingFixturePhaseName(rRecord.ePhase);
		record["threadId"] = rRecord.uiThreadId;
		record["threadPriority"] = rRecord.iThreadPriority;
		record["poolIndex"] = rRecord.uiPoolIndex == std::numeric_limits<uint32_t>::max()
		                    ? nlohmann::json(nullptr)
		                    : nlohmann::json(rRecord.uiPoolIndex);
		record["crc"] = rRecord.crc;
		record["offset"] = rRecord.uiOffset;
		record["length"] = rRecord.uiLength;
		record["state"] = AudioStreamingFixtureQueueStateName(rRecord.eState);
		record["generation"] = rRecord.uiGeneration;
		record["cancelAcknowledged"] = rRecord.bCancelAcknowledged;
		result["records"].push_back(std::move(record));
	}
	return result;
}

nlohmann::json BuildAudioStreamingFixtureInvalidResult(const engine::AudioStreamingFixtureInvalidResult& rInvalid)
{
	return {
		{"missingCrcFailed", rInvalid.bMissingCrcFailed},
		{"wrongCrcFailed", rInvalid.bWrongCrcFailed},
		{"overflowFailed", rInvalid.bOverflowFailed},
		{"outOfRangeFailed", rInvalid.bOutOfRangeFailed},
		{"zeroLengthFailed", rInvalid.bZeroLengthFailed},
		{"oversizeFailed", rInvalid.bOversizeFailed},
		{"shortAccepted", rInvalid.bShortAccepted},
		{"tooSmallPollFailed", rInvalid.bTooSmallPollFailed},
		{"tooSmallPollNoWrite", rInvalid.bTooSmallPollNoWrite},
		{"loadingCancelled", rInvalid.bLoadingCancelled},
		{"loadingEntryNotReused", rInvalid.bLoadingEntryNotReused},
	};
}

bool AudioStreamingFixtureEvidenceValid(const engine::AudioStreamingFixtureSnapshot& rSnapshot)
{
	return (rSnapshot.flags & engine::AudioStreamingFixtureSnapshotFlags::kCoherent)
		 && (rSnapshot.flags & engine::AudioStreamingFixtureSnapshotFlags::kGapFree) && rSnapshot.uiMainDropped == 0
		 && rSnapshot.uiLoader0Dropped == 0 && rSnapshot.uiLoader1Dropped == 0;
}

int64_t CountAudioStreamingFixtureRecords(const engine::AudioStreamingFixtureSnapshot& rSnapshot, engine::AudioStreamingFixturePhase ePhase, common::crc_t crc, uint64_t uiAfterSequence = 0)
{
	int64_t iCount = 0;
	for (int64_t i = 0; i < rSnapshot.iRecordCount; ++i)
	{
		const engine::AudioStreamingFixtureRecord& rRecord = rSnapshot.records.at(i);
		iCount += rRecord.uiSequence > uiAfterSequence && rRecord.ePhase == ePhase && rRecord.crc == crc ? 1 : 0;
	}
	return iCount;
}

bool HasAudioStreamingFixtureAcknowledgement(const engine::AudioStreamingFixtureSnapshot& rSnapshot, common::crc_t crc)
{
	for (int64_t i = 0; i < rSnapshot.iRecordCount; ++i)
	{
		const engine::AudioStreamingFixtureRecord& rRecord = rSnapshot.records.at(i);
		if (rRecord.crc == crc && rRecord.ePhase == engine::AudioStreamingFixturePhase::kRefillCancelled && rRecord.bCancelAcknowledged)
		{
			return true;
		}
	}
	return false;
}

enum class AudioStreamingFixtureSaturationPhase : uint8_t
{
	kSetup,
	kWaitForPrimaryStream,
	kWaitForMandatoryStream,
	kWaitForRetry,
	kWaitForSettled,
};

struct AudioStreamingFixtureSaturationState
{
	~AudioStreamingFixtureSaturationState()
	{
		if (engine::gpFileManager != nullptr)
		{
			engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseSaturation();
			engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseHold();
		}
		if (engine::gpAudioManager != nullptr)
		{
			engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseControls();
		}
	}

	AudioStreamingFixtureSaturationPhase ePhase = AudioStreamingFixtureSaturationPhase::kSetup;
	nlohmann::json saturated;
	float fSaturatedCurrentVolume = 0.0f;
	float fSaturatedNewestVolume = 0.0f;
};

bool IsAudioStreamingFixtureSaturated(const engine::AudioStreamingFixtureSnapshot& rFile, const engine::AudioStreamingFixtureAudioSnapshot& rAudio)
{
	if (!AudioStreamingFixtureEvidenceValid(rFile))
	{
		return false;
	}
	if (rFile.uiRetryCount == 0)
	{
		return false;
	}
	if (!(rFile.flags & engine::AudioStreamingFixtureSnapshotFlags::kLoaderStaged))
	{
		return false;
	}
	int64_t iNonFree = 0;
	int64_t iPrimaryStreamHeld = 0;
	int64_t iMandatoryStreamQueued = 0;
	int64_t iSongStreamQueued = 0;
	for (const engine::AudioStreamingFixturePoolEntry& rEntry : rFile.poolEntries)
	{
		iNonFree += rEntry.eState != engine::AudioStreamingFixtureQueueState::kFree ? 1 : 0;
		iPrimaryStreamHeld += rEntry.crc == data::kAudioMusicdoodlewavCrc && rEntry.eState == engine::AudioStreamingFixtureQueueState::kLoading ? 1 : 0;
		iMandatoryStreamQueued += rEntry.crc == data::kAudioMusicMandatoryOvertimewavCrc && rEntry.eState == engine::AudioStreamingFixtureQueueState::kQueued ? 1 : 0;
		iSongStreamQueued += rEntry.crc == data::kAudioMusicsong18wavCrc && rEntry.eState == engine::AudioStreamingFixtureQueueState::kQueued ? 1 : 0;
	}
	return iNonFree == 6 && iPrimaryStreamHeld == 1 && iMandatoryStreamQueued == 3 && iSongStreamQueued == 2
		&& CountAudioStreamingFixtureRecords(rFile, engine::AudioStreamingFixturePhase::kRefillCancelled, data::kAudioMusicdoodlewavCrc) >= 1
		&& (rAudio.current.flags & engine::AudioStreamingFixtureVoiceFlags::kPresent) && rAudio.current.crc == data::kAudioMusicsong18wavCrc
		&& (rAudio.newestFade.flags & engine::AudioStreamingFixtureVoiceFlags::kPresent)
		&& rAudio.newestFade.crc == data::kAudioMusicMandatoryOvertimewavCrc && rAudio.olderFades.iCount >= 1
		&& rAudio.olderFades.iUnderrunningCount >= 1;
}

#endif

} // namespace

void CommandAudioStreamingFixture([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("audio_streaming_fixture requires kbDebugInput build");
	}
#if defined(BT_DEBUG)
	else
	{
		// Heap: hostile-input validation and the bounded JSON fixture response
		ScopedSuppressAllocationTracking suppress;
		constexpr const char* kpSchema = "audio_streaming_fixture requires exactly {\"action\":\"start|inspect|clear|suspend|resume|hold_read|release_read|coexistence|invalid|saturate\"}";
		if (!rParams.is_object())
		{
			throw std::runtime_error(kpSchema);
		}
		if (rParams.size() != 1)
		{
			throw std::runtime_error(kpSchema);
		}
		if (!rParams.contains("action"))
		{
			throw std::runtime_error(kpSchema);
		}
		if (!rParams.at("action").is_string())
		{
			throw std::runtime_error(kpSchema);
		}
		if (engine::gpFileManager == nullptr)
		{
			throw std::runtime_error("audio_streaming_fixture requires initialized File and Audio managers");
		}
		if (engine::gpAudioManager == nullptr)
		{
			throw std::runtime_error("audio_streaming_fixture requires initialized File and Audio managers");
		}

		const std::string action = rParams.at("action").get<std::string>();
		if (action != "start" && action != "inspect" && action != "clear" && action != "suspend" && action != "resume"
		 && action != "hold_read" && action != "release_read" && action != "coexistence" && action != "invalid" && action != "saturate")
		{
			throw std::runtime_error("audio_streaming_fixture 'action' must be start|inspect|clear|suspend|resume|hold_read|release_read|coexistence|invalid|saturate");
		}
		static bool sbFixtureActionIssued = false;
		const bool bFirstFixtureAction = !sbFixtureActionIssued;
		sbFixtureActionIssued = true;

		if (action == "start")
		{
			std::shared_ptr<AudioStreamingFixtureStartState> pState = std::make_shared<AudioStreamingFixtureStartState>();
			if (engine::gpAgentCommandServer->mpAudioStreamingFixture->Begin(data::kAudioMusicdoodlewavCrc, 0, kuiAudioStreamingFixtureReadLength))
			{
				if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->Play(data::kAudioMusicdoodlewavCrc))
				{
					throw std::runtime_error("audio_streaming_fixture start requires a live audio voice");
				}
				rResult = BuildAudioStreamingFixtureSnapshot();
				pState->bComplete = true;
				return;
			}
			engine::gpAgentCommandServer->DeferResponse([pState]() -> std::optional<nlohmann::json>
			{
				if (engine::gpFileManager == nullptr)
				{
					throw std::runtime_error("audio_streaming_fixture managers were destroyed while start awaited quiescence");
				}
				if (engine::gpAudioManager == nullptr)
				{
					throw std::runtime_error("audio_streaming_fixture managers were destroyed while start awaited quiescence");
				}
				if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->Begin(data::kAudioMusicdoodlewavCrc, 0, kuiAudioStreamingFixtureReadLength))
				{
					return std::nullopt;
				}
				ScopedSuppressAllocationTracking deferredSuppress;
				if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->Play(data::kAudioMusicdoodlewavCrc))
				{
					throw std::runtime_error("audio_streaming_fixture start requires a live audio voice");
				}
				nlohmann::json result = BuildAudioStreamingFixtureSnapshot();
				pState->bComplete = true;
				return result;
			});
			return;
		}
		if (action == "inspect")
		{
			rResult = BuildAudioStreamingFixtureSnapshot();
			return;
		}
		if (action == "clear")
		{
			engine::gpAgentCommandServer->mpAudioStreamingFixture->Clear();
			rResult = BuildAudioStreamingFixtureSnapshot();
			return;
		}
		if (action == "suspend")
		{
			engine::gpAudioManager->Suspend();
			rResult = BuildAudioStreamingFixtureSnapshot();
			return;
		}
		if (action == "resume")
		{
			engine::gpAudioManager->Resume();
			rResult = BuildAudioStreamingFixtureSnapshot();
			return;
		}
		if (action == "hold_read")
		{
			if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->RequestHold())
			{
				throw std::runtime_error("audio_streaming_fixture hold_read is valid only before the first start in a fresh process");
			}
			rResult = BuildAudioStreamingFixtureSnapshot();
			return;
		}
		if (action == "release_read")
		{
			std::shared_ptr<AudioStreamingFixtureReleaseState> pState = std::make_shared<AudioStreamingFixtureReleaseState>();
			engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseHold();
			engine::gpAgentCommandServer->DeferResponse([pState]() -> std::optional<nlohmann::json>
			{
				if (engine::gpFileManager == nullptr)
				{
					throw std::runtime_error("audio_streaming_fixture managers were destroyed while release_read awaited acknowledgement");
				}
				if (engine::gpAudioManager == nullptr)
				{
					throw std::runtime_error("audio_streaming_fixture managers were destroyed while release_read awaited acknowledgement");
				}
				const engine::AudioStreamingFixtureSnapshot snapshot = engine::gpAgentCommandServer->mpAudioStreamingFixture->InspectFile();
				if (snapshot.eHoldState != engine::AudioStreamingFixtureHoldState::kIdle)
				{
					return std::nullopt;
				}
				if (snapshot.uiHeldIndex != std::numeric_limits<uint32_t>::max())
				{
					return std::nullopt;
				}
				engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseControls();
				ScopedSuppressAllocationTracking deferredSuppress;
				nlohmann::json result = BuildAudioStreamingFixtureSnapshot();
				return result;
			});
			return;
		}
		if (action == "coexistence")
		{
			std::shared_ptr<AudioStreamingFixtureCoexistenceState> pState =
				std::make_shared<AudioStreamingFixtureCoexistenceState>();
			const engine::AudioStreamingFixtureSnapshot before = engine::gpAgentCommandServer->mpAudioStreamingFixture->InspectFile();
			pState->uiStartSequence = before.uiReservationBoundary;
			pState->flags.Set(AudioStreamingFixtureCoexistenceFlags::kControlled, engine::gpAgentCommandServer->mpAudioStreamingFixture->InspectAudio().flags & engine::AudioStreamingFixtureAudioSnapshotFlags::kControlledPublication);
			pState->flags.Set(AudioStreamingFixtureCoexistenceFlags::kStaging);
			engine::gpAgentCommandServer->mpAudioStreamingFixture->StartCoexistence(data::kAudioBlaster16793__pushtobreak__earth1wavCrc, data::kAudioBlaster793907__cvltiv8r__snaresbycvltiv8r301wavCrc);
			if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->Play(data::kAudioMusicdoodlewavCrc))
			{
				throw std::runtime_error("audio_streaming_fixture coexistence requires a live audio voice");
			}
			engine::gpAgentCommandServer->DeferResponse([pState]() -> std::optional<nlohmann::json>
			{
				if (engine::gpFileManager == nullptr)
				{
					throw std::runtime_error("audio_streaming_fixture managers were destroyed while coexistence staged work");
				}
				if (engine::gpAudioManager == nullptr)
				{
					throw std::runtime_error("audio_streaming_fixture managers were destroyed while coexistence staged work");
				}
				if ((pState->flags & AudioStreamingFixtureCoexistenceFlags::kStaging)
				  && !engine::gpAgentCommandServer->mpAudioStreamingFixture->FinishCoexistence())
				{
					return std::nullopt;
				}
				pState->flags.Clear(AudioStreamingFixtureCoexistenceFlags::kStaging);
				const engine::AudioStreamingFixtureSnapshot snapshot = engine::gpAgentCommandServer->mpAudioStreamingFixture->InspectFile();
				if (pState->flags & AudioStreamingFixtureCoexistenceFlags::kControlled)
				{
					if (CountAudioStreamingFixtureRecords(snapshot, engine::AudioStreamingFixturePhase::kExistingComplete, data::kAudioBlaster16793__pushtobreak__earth1wavCrc) != 1)
					{
						return std::nullopt;
					}
					if (CountAudioStreamingFixtureRecords(snapshot, engine::AudioStreamingFixturePhase::kExistingComplete, data::kAudioBlaster793907__cvltiv8r__snaresbycvltiv8r301wavCrc) != 1)
					{
						return std::nullopt;
					}
					if (CountAudioStreamingFixtureRecords(snapshot, engine::AudioStreamingFixturePhase::kRefillReady, data::kAudioMusicdoodlewavCrc, pState->uiStartSequence) < 3)
					{
						return std::nullopt;
					}
				}
				ScopedSuppressAllocationTracking deferredSuppress;
				nlohmann::json result = BuildAudioStreamingFixtureSnapshot();
				pState->flags.Set(AudioStreamingFixtureCoexistenceFlags::kComplete);
				return result;
			});
			return;
		}
		if (action == "invalid")
		{
			std::shared_ptr<AudioStreamingFixtureInvalidState> pState = std::make_shared<AudioStreamingFixtureInvalidState>();
			const engine::AudioStreamingFixtureInvalidResult invalid = engine::gpAgentCommandServer->mpAudioStreamingFixture->RunInvalid();
			if (!invalid.bPending)
			{
				pState->result = invalid;
				pState->bHasResult = true;
			}
			engine::gpAgentCommandServer->DeferResponse([pState]() -> std::optional<nlohmann::json>
			{
				if (engine::gpFileManager == nullptr)
				{
					throw std::runtime_error("audio_streaming_fixture File manager was destroyed while invalid awaited Loading");
				}
				if (!pState->bHasResult)
				{
					const engine::AudioStreamingFixtureInvalidResult deferredInvalid = engine::gpAgentCommandServer->mpAudioStreamingFixture->RunInvalid();
					if (deferredInvalid.bPending)
					{
						return std::nullopt;
					}
					pState->result = deferredInvalid;
					pState->bHasResult = true;
				}
				const engine::AudioStreamingFixtureSnapshot snapshot = engine::gpAgentCommandServer->mpAudioStreamingFixture->InspectFile();
				if (snapshot.eHoldState != engine::AudioStreamingFixtureHoldState::kIdle)
				{
					return std::nullopt;
				}
				if (snapshot.uiHeldIndex != std::numeric_limits<uint32_t>::max())
				{
					return std::nullopt;
				}
				ScopedSuppressAllocationTracking deferredSuppress;
				nlohmann::json result = BuildAudioStreamingFixtureInvalidResult(pState->result);
				result["snapshot"] = BuildAudioStreamingFixtureSnapshot();
				return result;
			});
			return;
		}
		if (!bFirstFixtureAction)
		{
			throw std::runtime_error("audio_streaming_fixture saturate is valid only as the first fixture action in a fresh process");
		}
		std::shared_ptr<AudioStreamingFixtureSaturationState> pState = std::make_shared<AudioStreamingFixtureSaturationState>();
		if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->RequestHold())
		{
			throw std::runtime_error("audio_streaming_fixture saturate could not reserve its controlled hold");
		}
		engine::gpAgentCommandServer->mpAudioStreamingFixture->mCommandFlags.Set(engine::AudioStreamingFixtureCommandFlags::kSaturationActive);
		engine::gpAgentCommandServer->DeferResponse([pState]() -> std::optional<nlohmann::json>
		{
			if (engine::gpFileManager == nullptr)
			{
				throw std::runtime_error("audio_streaming_fixture managers were destroyed while saturate owned controls");
			}
			if (engine::gpAudioManager == nullptr)
			{
				throw std::runtime_error("audio_streaming_fixture managers were destroyed while saturate owned controls");
			}
			const engine::AudioStreamingFixtureSnapshot fileSnapshot = engine::gpAgentCommandServer->mpAudioStreamingFixture->InspectFile();
			const engine::AudioStreamingFixtureAudioSnapshot audioSnapshot = engine::gpAgentCommandServer->mpAudioStreamingFixture->InspectAudio();
			switch (pState->ePhase)
			{
			case AudioStreamingFixtureSaturationPhase::kSetup:
				if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->Begin(data::kAudioMusicdoodlewavCrc, 0, kuiAudioStreamingFixtureReadLength))
				{
					return std::nullopt;
				}
				if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->Play(data::kAudioMusicdoodlewavCrc))
				{
					throw std::runtime_error("audio_streaming_fixture saturate requires stream A");
				}
				pState->ePhase = AudioStreamingFixtureSaturationPhase::kWaitForPrimaryStream;
				return std::nullopt;
			case AudioStreamingFixtureSaturationPhase::kWaitForPrimaryStream:
			{
				bool bPrimaryStreamHeld = false;
				for (const engine::AudioStreamingFixturePoolEntry& rEntry : fileSnapshot.poolEntries)
				{
					bPrimaryStreamHeld = bPrimaryStreamHeld || (rEntry.crc == data::kAudioMusicdoodlewavCrc && rEntry.uiOffset == 0 && rEntry.uiLength == kuiAudioStreamingFixtureReadLength && rEntry.eState == engine::AudioStreamingFixtureQueueState::kLoading);
				}
				bool bReadyOne = false;
				bool bReadyTwo = false;
				for (int64_t i = 0; i < fileSnapshot.iRecordCount; ++i)
				{
					const engine::AudioStreamingFixtureRecord& rRecord = fileSnapshot.records.at(i);
					if (rRecord.crc == data::kAudioMusicdoodlewavCrc && rRecord.ePartition == engine::AudioStreamingFixturePartition::kMain
					 && rRecord.ePhase == engine::AudioStreamingFixturePhase::kRefillReady)
					{
						bReadyOne = bReadyOne || rRecord.uiOffset == kuiAudioStreamingFixtureReadLength;
						bReadyTwo = bReadyTwo || rRecord.uiOffset == 2 * kuiAudioStreamingFixtureReadLength;
					}
				}
				if (!bPrimaryStreamHeld)
				{
					return std::nullopt;
				}
				if (!bReadyOne)
				{
					return std::nullopt;
				}
				if (!bReadyTwo)
				{
					return std::nullopt;
				}
				if (!(audioSnapshot.current.flags & engine::AudioStreamingFixtureVoiceFlags::kPresent))
				{
					return std::nullopt;
				}
				if (audioSnapshot.current.crc != data::kAudioMusicdoodlewavCrc)
				{
					return std::nullopt;
				}
				if (audioSnapshot.current.flags & engine::AudioStreamingFixtureVoiceFlags::kFadingIn)
				{
					return std::nullopt;
				}
				if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->BeginSaturation())
				{
					return std::nullopt;
				}
				if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->Play(data::kAudioMusicMandatoryOvertimewavCrc))
				{
					throw std::runtime_error("audio_streaming_fixture saturate requires stream B");
				}
				pState->ePhase = AudioStreamingFixtureSaturationPhase::kWaitForMandatoryStream;
				return std::nullopt;
			}
			case AudioStreamingFixtureSaturationPhase::kWaitForMandatoryStream:
			{
				int64_t iMandatoryStreamQueued = 0;
				for (const engine::AudioStreamingFixturePoolEntry& rEntry : fileSnapshot.poolEntries)
				{
					iMandatoryStreamQueued += rEntry.crc == data::kAudioMusicMandatoryOvertimewavCrc && rEntry.eState == engine::AudioStreamingFixtureQueueState::kQueued ? 1 : 0;
				}
				if (iMandatoryStreamQueued != 3)
				{
					return std::nullopt;
				}
				if (!(audioSnapshot.current.flags & engine::AudioStreamingFixtureVoiceFlags::kPresent))
				{
					return std::nullopt;
				}
				if (audioSnapshot.current.crc != data::kAudioMusicMandatoryOvertimewavCrc)
				{
					return std::nullopt;
				}
				if (audioSnapshot.current.fVolume < 0.4f)
				{
					return std::nullopt;
				}
				if (!(audioSnapshot.newestFade.flags & engine::AudioStreamingFixtureVoiceFlags::kPresent))
				{
					return std::nullopt;
				}
				if (audioSnapshot.newestFade.crc != data::kAudioMusicdoodlewavCrc)
				{
					return std::nullopt;
				}
				if (audioSnapshot.newestFade.fVolume < 0.4f)
				{
					return std::nullopt;
				}
				if (!engine::gpAgentCommandServer->mpAudioStreamingFixture->Play(data::kAudioMusicsong18wavCrc))
				{
					throw std::runtime_error("audio_streaming_fixture saturate requires stream C");
				}
				pState->ePhase = AudioStreamingFixtureSaturationPhase::kWaitForRetry;
				return std::nullopt;
			}
			case AudioStreamingFixtureSaturationPhase::kWaitForRetry:
				if (fileSnapshot.uiRetryCount == 0)
				{
					return std::nullopt;
				}
				engine::gpAgentCommandServer->mpAudioStreamingFixture->StopPublication();
				if (!IsAudioStreamingFixtureSaturated(fileSnapshot, audioSnapshot))
				{
					return std::nullopt;
				}
				pState->fSaturatedCurrentVolume = audioSnapshot.current.fVolume;
				pState->fSaturatedNewestVolume = audioSnapshot.newestFade.fVolume;
				{
					ScopedSuppressAllocationTracking deferredSuppress;
					pState->saturated = BuildAudioStreamingFixtureSnapshot();
				}
				engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseSaturation();
				engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseHold();
				pState->ePhase = AudioStreamingFixtureSaturationPhase::kWaitForSettled;
				return std::nullopt;
			case AudioStreamingFixtureSaturationPhase::kWaitForSettled:
				if (!AudioStreamingFixtureEvidenceValid(fileSnapshot))
				{
					return std::nullopt;
				}
				if (fileSnapshot.flags & engine::AudioStreamingFixtureSnapshotFlags::kLoaderStaged)
				{
					return std::nullopt;
				}
				if (fileSnapshot.eHoldState != engine::AudioStreamingFixtureHoldState::kIdle)
				{
					return std::nullopt;
				}
				if (fileSnapshot.uiHeldIndex != std::numeric_limits<uint32_t>::max())
				{
					return std::nullopt;
				}
				if (!HasAudioStreamingFixtureAcknowledgement(fileSnapshot, data::kAudioMusicdoodlewavCrc))
				{
					return std::nullopt;
				}
				if (CountAudioStreamingFixtureRecords(fileSnapshot, engine::AudioStreamingFixturePhase::kRefillReady, data::kAudioMusicMandatoryOvertimewavCrc) == 0)
				{
					return std::nullopt;
				}
				if (CountAudioStreamingFixtureRecords(fileSnapshot, engine::AudioStreamingFixturePhase::kRefillReady, data::kAudioMusicsong18wavCrc) == 0)
				{
					return std::nullopt;
				}
				if (!(audioSnapshot.current.flags & engine::AudioStreamingFixtureVoiceFlags::kPresent))
				{
					return std::nullopt;
				}
				if (audioSnapshot.current.crc != data::kAudioMusicsong18wavCrc)
				{
					return std::nullopt;
				}
				if (audioSnapshot.current.fVolume <= pState->fSaturatedCurrentVolume)
				{
					return std::nullopt;
				}
				if (!(audioSnapshot.newestFade.flags & engine::AudioStreamingFixtureVoiceFlags::kPresent))
				{
					return std::nullopt;
				}
				if (audioSnapshot.newestFade.crc != data::kAudioMusicMandatoryOvertimewavCrc)
				{
					return std::nullopt;
				}
				if (audioSnapshot.newestFade.fVolume >= pState->fSaturatedNewestVolume)
				{
					return std::nullopt;
				}
				engine::gpAgentCommandServer->mpAudioStreamingFixture->ReleaseControls();
				{
					ScopedSuppressAllocationTracking deferredSuppress;
					return nlohmann::json {{"saturated", std::move(pState->saturated)}, {"settled", BuildAudioStreamingFixtureSnapshot()}};
				}
			}
			return std::nullopt;
		});
		return;
	}
#endif
}

} // namespace game

#endif // defined(BT_CLIENT)
