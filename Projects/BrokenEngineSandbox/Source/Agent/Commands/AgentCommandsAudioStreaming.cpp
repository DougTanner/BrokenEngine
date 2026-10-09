#include "Agent/Commands/AgentCommandsAudioStreaming.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/AudioStreamingHarnessRig.h"
#include "Agent/AgentCommandServer.h"

#if defined(BT_DEBUG)
#include "Data/Audio.h"
#endif

namespace game
{

#if defined(BT_DEBUG)

constexpr int64_t kiAudioStreamingHarnessRigReadLength = 16 * 1'024;

static const char* AudioStreamingHarnessRigPartitionName(engine::AudioStreamingHarnessRigPartition ePartition)
{
	switch (ePartition)
	{
		case engine::AudioStreamingHarnessRigPartition::kMain:
			return "main";
		case engine::AudioStreamingHarnessRigPartition::kLoader0:
			return "loader0";
		case engine::AudioStreamingHarnessRigPartition::kLoader1:
			return "loader1";
	}
	return "unknown";
}

static const char* AudioStreamingHarnessRigPhaseName(engine::AudioStreamingHarnessRigPhase ePhase)
{
	switch (ePhase)
	{
		case engine::AudioStreamingHarnessRigPhase::kStartup:
			return "startup";
		case engine::AudioStreamingHarnessRigPhase::kRefillQueued:
			return "refill_queued";
		case engine::AudioStreamingHarnessRigPhase::kRefillLoading:
			return "refill_loading";
		case engine::AudioStreamingHarnessRigPhase::kRefillReady:
			return "refill_ready";
		case engine::AudioStreamingHarnessRigPhase::kRefillCancelled:
			return "refill_cancelled";
		case engine::AudioStreamingHarnessRigPhase::kExistingQueued:
			return "existing_queued";
		case engine::AudioStreamingHarnessRigPhase::kExistingComplete:
			return "existing_complete";
	}
	return "unknown";
}

static const char* AudioStreamingHarnessRigQueueStateName(engine::AudioStreamingHarnessRigQueueState eState)
{
	switch (eState)
	{
		case engine::AudioStreamingHarnessRigQueueState::kFree:
			return "free";
		case engine::AudioStreamingHarnessRigQueueState::kQueued:
			return "queued";
		case engine::AudioStreamingHarnessRigQueueState::kLoading:
			return "loading";
		case engine::AudioStreamingHarnessRigQueueState::kReady:
			return "ready";
	}
	return "unknown";
}

static const char* AudioStreamingHarnessRigHoldOwnerName(engine::AudioStreamingHarnessRigHoldOwner eOwner)
{
	switch (eOwner)
	{
		case engine::AudioStreamingHarnessRigHoldOwner::kNone:
			return "none";
		case engine::AudioStreamingHarnessRigHoldOwner::kControlled:
			return "controlled";
		case engine::AudioStreamingHarnessRigHoldOwner::kInvalid:
			return "invalid";
	}
	return "unknown";
}

static const char* AudioStreamingHarnessRigHoldStateName(engine::AudioStreamingHarnessRigHoldState eState)
{
	switch (eState)
	{
		case engine::AudioStreamingHarnessRigHoldState::kIdle:
			return "idle";
		case engine::AudioStreamingHarnessRigHoldState::kArmed:
			return "armed";
		case engine::AudioStreamingHarnessRigHoldState::kConsumed:
			return "consumed";
	}
	return "unknown";
}

enum class AudioStreamingHarnessRigCoexistenceFlags : uint8_t
{
	kStaging = 0x01,
	kControlled = 0x02,
	kComplete = 0x04,
};
using AudioStreamingHarnessRigCoexistenceFlags_t = common::Flags<AudioStreamingHarnessRigCoexistenceFlags>;

struct AudioStreamingHarnessRigCoexistenceState
{
	~AudioStreamingHarnessRigCoexistenceState()
	{
		if (flags & AudioStreamingHarnessRigCoexistenceFlags::kStaging)
		{
			engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseCoexistence();
		}
		if ((flags & AudioStreamingHarnessRigCoexistenceFlags::kControlled) && !(flags & AudioStreamingHarnessRigCoexistenceFlags::kComplete))
		{
			engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseHold(engine::AudioStreamingHarnessRigHoldOwner::kControlled);
			engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseControls();
		}
	}

	AudioStreamingHarnessRigCoexistenceFlags_t flags;
	int64_t iStartSequence = 0;
};

struct AudioStreamingHarnessRigStartState
{
	~AudioStreamingHarnessRigStartState()
	{
		if (!bComplete)
		{
			engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseHold(engine::AudioStreamingHarnessRigHoldOwner::kControlled);
			engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseControls();
		}
	}

	bool bComplete = false;
};

struct AudioStreamingHarnessRigReleaseState
{
	~AudioStreamingHarnessRigReleaseState()
	{
		engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseHold(engine::AudioStreamingHarnessRigHoldOwner::kControlled);
		engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseControls();
	}
};

struct AudioStreamingHarnessRigInvalidState
{
	~AudioStreamingHarnessRigInvalidState()
	{
		engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseInvalid();
	}

	engine::AudioStreamingHarnessRigInvalidResult result;
	bool bHasResult = false;
};

static nlohmann::json BuildAudioStreamingHarnessRigVoiceSummary(const engine::AudioStreamingHarnessRigVoiceSummary& rSummary)
{
	return
	{
		{"present", rSummary.flags & engine::AudioStreamingHarnessRigVoiceFlags::kPresent},
		{"crc", rSummary.crc},
		{"volume", rSummary.fVolume},
		{"fadingIn", rSummary.flags & engine::AudioStreamingHarnessRigVoiceFlags::kFadingIn},
		{"fadingOut", rSummary.flags & engine::AudioStreamingHarnessRigVoiceFlags::kFadingOut},
		{"pendingSlots", rSummary.iPendingSlots},
		{"buffersQueued", rSummary.iBuffersQueued},
		{"underrunning", rSummary.flags & engine::AudioStreamingHarnessRigVoiceFlags::kUnderrunning},
	};
}

static nlohmann::json BuildAudioStreamingHarnessRigSnapshot()
{
	engine::AudioStreamingHarnessRigSnapshot snapshot;
	engine::AudioStreamingHarnessRigAudioSnapshot audioSnapshot;
	bool bCombinedCoherent = false;
	for (int64_t j = 0; j < 8; ++j)
	{
		engine::AudioStreamingHarnessRigSnapshot before = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->InspectFile();
		audioSnapshot = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->InspectAudio();
		snapshot = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->InspectFile();
		bCombinedCoherent = before.uiScenarioGate == snapshot.uiScenarioGate && before.iActiveWriters == snapshot.iActiveWriters
		                 && before.uiReservationBoundary == snapshot.uiReservationBoundary && before.uiRetryCount == snapshot.uiRetryCount
		                 && before.eHoldOwner == snapshot.eHoldOwner && before.eHoldState == snapshot.eHoldState
		                 && (before.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kLoaderStaged) == (snapshot.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kLoaderStaged)
		                 && before.iHeldIndex == snapshot.iHeldIndex && before.iHeldGeneration == snapshot.iHeldGeneration
		                 && before.iMainHead == snapshot.iMainHead && before.iLoader0Head == snapshot.iLoader0Head
		                 && before.iLoader1Head == snapshot.iLoader1Head;
		bCombinedCoherent = bCombinedCoherent && std::ranges::equal(before.poolEntries, snapshot.poolEntries, [](const engine::AudioStreamingHarnessRigPoolEntry& rBefore, const engine::AudioStreamingHarnessRigPoolEntry& rAfter)
		{
			return rBefore.iIndex == rAfter.iIndex && rBefore.eState == rAfter.eState && rBefore.iGeneration == rAfter.iGeneration && rBefore.crc == rAfter.crc && rBefore.iOffset == rAfter.iOffset && rBefore.iLength == rAfter.iLength;
		});
		if (bCombinedCoherent)
		{
			break;
		}
	}
	snapshot.flags.Set(engine::AudioStreamingHarnessRigSnapshotFlags::kCoherent, (snapshot.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kCoherent) && bCombinedCoherent);
	nlohmann::json result;
	result["scenarioGate"] = snapshot.uiScenarioGate;
	result["scenarioGeneration"] = snapshot.iScenarioGeneration;
	result["activeWriters"] = snapshot.iActiveWriters;
	result["reservationStart"] = snapshot.uiReservationStart;
	result["reservationBoundary"] = snapshot.uiReservationBoundary;
	result["retryCount"] = snapshot.uiRetryCount;
	result["holdOwner"] = AudioStreamingHarnessRigHoldOwnerName(snapshot.eHoldOwner);
	result["holdState"] = AudioStreamingHarnessRigHoldStateName(snapshot.eHoldState);
	result["heldPoolIndex"] = snapshot.iHeldIndex == 4'294'967'295i64
			? nlohmann::json(nullptr)
			: nlohmann::json(snapshot.iHeldIndex);
	result["heldGeneration"] = snapshot.iHeldGeneration;
	result["heldState"] = AudioStreamingHarnessRigQueueStateName(snapshot.eHeldState);
	result["coherent"] = snapshot.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kCoherent;
	result["gapFree"] = snapshot.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kGapFree;
	result["controls"] =
	{
		{"holdArmed", snapshot.eHoldState != engine::AudioStreamingHarnessRigHoldState::kIdle},
		{"loaderStaged", snapshot.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kLoaderStaged},
		{"controlledPublication", audioSnapshot.flags & engine::AudioStreamingHarnessRigAudioSnapshotFlags::kControlledPublication},
		{"saturationActive", audioSnapshot.flags & engine::AudioStreamingHarnessRigAudioSnapshotFlags::kSaturationActive},
		{"publicationAllowance", audioSnapshot.iPublicationAllowance},
	};
	result["poolEntries"] = nlohmann::json::array();
	for (const engine::AudioStreamingHarnessRigPoolEntry& rEntry : snapshot.poolEntries)
	{
		result["poolEntries"].push_back(
		{
			{"index", rEntry.iIndex},
			{"state", AudioStreamingHarnessRigQueueStateName(rEntry.eState)},
			{"generation", rEntry.iGeneration},
			{"crc", rEntry.crc},
			{"offset", rEntry.iOffset},
			{"length", rEntry.iLength},
		});
	}
	result["current"] = BuildAudioStreamingHarnessRigVoiceSummary(audioSnapshot.current);
	result["newestFade"] = BuildAudioStreamingHarnessRigVoiceSummary(audioSnapshot.newestFade);
	result["olderFades"] =
	{
		{"count", audioSnapshot.olderFades.iCount},
		{"pendingSlots", audioSnapshot.olderFades.iPendingSlots},
		{"underrunningCount", audioSnapshot.olderFades.iUnderrunningCount},
	};
	result["partitions"] = nlohmann::json::array(
	{
		{{"name", "main"}, {"head", snapshot.iMainHead}, {"dropped", snapshot.iMainDropped}},
		{{"name", "loader0"}, {"head", snapshot.iLoader0Head}, {"dropped", snapshot.iLoader0Dropped}},
		{{"name", "loader1"}, {"head", snapshot.iLoader1Head}, {"dropped", snapshot.iLoader1Dropped}},
	});
	bool bOverflow = snapshot.iMainDropped != 0 || snapshot.iLoader0Dropped != 0 || snapshot.iLoader1Dropped != 0;
	result["overflow"] = bOverflow;
	result["evidenceValid"] = (snapshot.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kCoherent)
	                       && (snapshot.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kGapFree) && !bOverflow;
	result["records"] = nlohmann::json::array();
	for (int64_t i = 0; i < snapshot.iRecordCount; ++i)
	{
		const engine::AudioStreamingHarnessRigRecord& rRecord = snapshot.records.at(i);
		nlohmann::json record;
		record["sequence"] = rRecord.uiSequence;
		record["scenarioGeneration"] = rRecord.iScenarioGeneration;
		record["partition"] = AudioStreamingHarnessRigPartitionName(rRecord.ePartition);
		record["phase"] = AudioStreamingHarnessRigPhaseName(rRecord.ePhase);
		record["threadId"] = rRecord.iThreadId;
		record["threadPriority"] = rRecord.iThreadPriority;
		record["poolIndex"] = rRecord.iPoolIndex == 4'294'967'295i64
		                    ? nlohmann::json(nullptr)
		                    : nlohmann::json(rRecord.iPoolIndex);
		record["crc"] = rRecord.crc;
		record["offset"] = rRecord.iOffset;
		record["length"] = rRecord.iLength;
		record["state"] = AudioStreamingHarnessRigQueueStateName(rRecord.eState);
		record["generation"] = rRecord.iGeneration;
		record["cancelAcknowledged"] = rRecord.bCancelAcknowledged;
		result["records"].push_back(std::move(record));
	}
	return result;
}

static nlohmann::json BuildAudioStreamingHarnessRigInvalidResult(const engine::AudioStreamingHarnessRigInvalidResult& rInvalid)
{
	return
	{
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

static bool AudioStreamingHarnessRigEvidenceValid(const engine::AudioStreamingHarnessRigSnapshot& rSnapshot)
{
	return (rSnapshot.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kCoherent)
	    && (rSnapshot.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kGapFree) && rSnapshot.iMainDropped == 0
	    && rSnapshot.iLoader0Dropped == 0 && rSnapshot.iLoader1Dropped == 0;
}

static int64_t CountAudioStreamingHarnessRigRecords(const engine::AudioStreamingHarnessRigSnapshot& rSnapshot, engine::AudioStreamingHarnessRigPhase ePhase, common::crc_t crc, int64_t iAfterSequence = 0)
{
	int64_t iCount = 0;
	for (int64_t i = 0; i < rSnapshot.iRecordCount; ++i)
	{
		const engine::AudioStreamingHarnessRigRecord& rRecord = rSnapshot.records.at(i);
		iCount += rRecord.uiSequence > static_cast<uint64_t>(iAfterSequence) && rRecord.ePhase == ePhase && rRecord.crc == crc ? 1 : 0;
	}
	return iCount;
}

static bool HasAudioStreamingHarnessRigAcknowledgement(const engine::AudioStreamingHarnessRigSnapshot& rSnapshot, common::crc_t crc)
{
	for (int64_t i = 0; i < rSnapshot.iRecordCount; ++i)
	{
		const engine::AudioStreamingHarnessRigRecord& rRecord = rSnapshot.records.at(i);
		if (rRecord.crc == crc && rRecord.ePhase == engine::AudioStreamingHarnessRigPhase::kRefillCancelled && rRecord.bCancelAcknowledged)
		{
			return true;
		}
	}
	return false;
}

enum class AudioStreamingHarnessRigSaturationPhase : uint8_t
{
	kSetup,
	kWaitForPrimaryStream,
	kWaitForMandatoryStream,
	kWaitForRetry,
	kWaitForSettled,
};

struct AudioStreamingHarnessRigSaturationState
{
	~AudioStreamingHarnessRigSaturationState()
	{
		engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseSaturation();
		engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseHold(engine::AudioStreamingHarnessRigHoldOwner::kControlled);
		engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseControls();
	}

	AudioStreamingHarnessRigSaturationPhase ePhase = AudioStreamingHarnessRigSaturationPhase::kSetup;
	nlohmann::json saturated;
	float fSaturatedCurrentVolume = 0.0f;
	float fSaturatedNewestVolume = 0.0f;
};

static bool IsAudioStreamingHarnessRigSaturated(const engine::AudioStreamingHarnessRigSnapshot& rFile, const engine::AudioStreamingHarnessRigAudioSnapshot& rAudio)
{
	if (!AudioStreamingHarnessRigEvidenceValid(rFile))
	{
		return false;
	}
	if (rFile.uiRetryCount == 0)
	{
		return false;
	}
	if (!(rFile.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kLoaderStaged))
	{
		return false;
	}
	int64_t iNonFree = 0;
	int64_t iPrimaryStreamHeld = 0;
	int64_t iMandatoryStreamQueued = 0;
	int64_t iSongStreamQueued = 0;
	for (const engine::AudioStreamingHarnessRigPoolEntry& rEntry : rFile.poolEntries)
	{
		iNonFree += rEntry.eState != engine::AudioStreamingHarnessRigQueueState::kFree ? 1 : 0;
		iPrimaryStreamHeld += rEntry.crc == data::kAudioMusicdoodlewavCrc && rEntry.eState == engine::AudioStreamingHarnessRigQueueState::kLoading ? 1 : 0;
		iMandatoryStreamQueued += rEntry.crc == data::kAudioMusicMandatoryOvertimewavCrc && rEntry.eState == engine::AudioStreamingHarnessRigQueueState::kQueued ? 1 : 0;
		iSongStreamQueued += rEntry.crc == data::kAudioMusicsong18wavCrc && rEntry.eState == engine::AudioStreamingHarnessRigQueueState::kQueued ? 1 : 0;
	}
	return iNonFree == 6 && iPrimaryStreamHeld == 1 && iMandatoryStreamQueued == 3 && iSongStreamQueued == 2
	    && CountAudioStreamingHarnessRigRecords(rFile, engine::AudioStreamingHarnessRigPhase::kRefillCancelled, data::kAudioMusicdoodlewavCrc) >= 1
	    && (rAudio.current.flags & engine::AudioStreamingHarnessRigVoiceFlags::kPresent)
	    && rAudio.current.crc == data::kAudioMusicsong18wavCrc
	    && (rAudio.newestFade.flags & engine::AudioStreamingHarnessRigVoiceFlags::kPresent)
	    && rAudio.newestFade.crc == data::kAudioMusicMandatoryOvertimewavCrc && rAudio.olderFades.iCount >= 1
	    && rAudio.olderFades.iUnderrunningCount >= 1;
}

#endif

void CommandAudioStreamingHarnessRig([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("audio_streaming_harness_rig requires kbDebugInput build");
	}
#if defined(BT_DEBUG)
	else
	{
		// Heap: hostile-input validation and the bounded JSON harness rig response
		ScopedSuppressAllocationTracking suppress;
		static constexpr const char* kpcSchema = "audio_streaming_harness_rig requires exactly {\"action\":\"start|inspect|clear|suspend|resume|hold_read|release_read|coexistence|invalid|saturate\"}";
		if (!rParameters.is_object())
		{
			throw std::runtime_error(kpcSchema);
		}
		if (rParameters.size() != 1)
		{
			throw std::runtime_error(kpcSchema);
		}
		if (!rParameters.contains("action"))
		{
			throw std::runtime_error(kpcSchema);
		}
		if (!rParameters.at("action").is_string())
		{
			throw std::runtime_error(kpcSchema);
		}

		std::string action = rParameters.at("action").get<std::string>();
		if (action != "start" && action != "inspect" && action != "clear" && action != "suspend" && action != "resume"
		 && action != "hold_read" && action != "release_read" && action != "coexistence" && action != "invalid" && action != "saturate")
		{
			throw std::runtime_error("audio_streaming_harness_rig 'action' must be start|inspect|clear|suspend|resume|hold_read|release_read|coexistence|invalid|saturate");
		}
		static bool sbHarnessRigActionIssued = false;
		bool bFirstHarnessRigAction = !sbHarnessRigActionIssued;
		sbHarnessRigActionIssued = true;

		if (action == "start")
		{
			std::shared_ptr<AudioStreamingHarnessRigStartState> pState = std::make_shared<AudioStreamingHarnessRigStartState>();
			if (engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->Begin(data::kAudioMusicdoodlewavCrc, 0, kiAudioStreamingHarnessRigReadLength))
			{
				if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->Play(data::kAudioMusicdoodlewavCrc))
				{
					throw std::runtime_error("audio_streaming_harness_rig start requires a live audio voice");
				}
				rResult = BuildAudioStreamingHarnessRigSnapshot();
				pState->bComplete = true;
				return;
			}
			engine::gpAgentCommandServer->DeferResponse([pState]() -> std::optional<nlohmann::json>
			{
				if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->Begin(data::kAudioMusicdoodlewavCrc, 0, kiAudioStreamingHarnessRigReadLength))
				{
					return std::nullopt;
				}
				ScopedSuppressAllocationTracking deferredSuppress;
				if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->Play(data::kAudioMusicdoodlewavCrc))
				{
					throw std::runtime_error("audio_streaming_harness_rig start requires a live audio voice");
				}
				nlohmann::json result = BuildAudioStreamingHarnessRigSnapshot();
				pState->bComplete = true;
				return result;
			});
			return;
		}
		if (action == "inspect")
		{
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "clear")
		{
			engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->Clear();
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "suspend")
		{
			engine::gpAudioManager->Suspend();
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "resume")
		{
			engine::gpAudioManager->Resume();
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "hold_read")
		{
			if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->RequestHold())
			{
				throw std::runtime_error("audio_streaming_harness_rig hold_read is valid only before the first start in a fresh process");
			}
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "release_read")
		{
			std::shared_ptr<AudioStreamingHarnessRigReleaseState> pState = std::make_shared<AudioStreamingHarnessRigReleaseState>();
			engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseHold(engine::AudioStreamingHarnessRigHoldOwner::kControlled);
			engine::gpAgentCommandServer->DeferResponse([pState]() -> std::optional<nlohmann::json>
			{
				engine::AudioStreamingHarnessRigSnapshot snapshot = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->InspectFile();
				if (snapshot.eHoldState != engine::AudioStreamingHarnessRigHoldState::kIdle)
				{
					return std::nullopt;
				}
				if (snapshot.iHeldIndex != 4'294'967'295i64)
				{
					return std::nullopt;
				}
				engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseControls();
				ScopedSuppressAllocationTracking deferredSuppress;
				nlohmann::json result = BuildAudioStreamingHarnessRigSnapshot();
				return result;
			});
			return;
		}
		if (action == "coexistence")
		{
			std::shared_ptr<AudioStreamingHarnessRigCoexistenceState> pState = std::make_shared<AudioStreamingHarnessRigCoexistenceState>();
			engine::AudioStreamingHarnessRigSnapshot before = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->InspectFile();
			pState->iStartSequence = static_cast<int64_t>(before.uiReservationBoundary);
			pState->flags.Set(AudioStreamingHarnessRigCoexistenceFlags::kControlled, engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->InspectAudio().flags & engine::AudioStreamingHarnessRigAudioSnapshotFlags::kControlledPublication);
			pState->flags.Set(AudioStreamingHarnessRigCoexistenceFlags::kStaging);
			engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->StartCoexistence(data::kAudioBlaster16793__pushtobreak__earth1wavCrc, data::kAudioBlaster793907__cvltiv8r__snaresbycvltiv8r301wavCrc);
			if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->Play(data::kAudioMusicdoodlewavCrc))
			{
				throw std::runtime_error("audio_streaming_harness_rig coexistence requires a live audio voice");
			}
			engine::gpAgentCommandServer->DeferResponse([pState]() -> std::optional<nlohmann::json>
			{
				if ((pState->flags & AudioStreamingHarnessRigCoexistenceFlags::kStaging)
				 && !engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->FinishCoexistence())
				{
					return std::nullopt;
				}
				pState->flags.Set(AudioStreamingHarnessRigCoexistenceFlags::kStaging, false);
				engine::AudioStreamingHarnessRigSnapshot snapshot = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->InspectFile();
				if (pState->flags & AudioStreamingHarnessRigCoexistenceFlags::kControlled)
				{
					if (CountAudioStreamingHarnessRigRecords(snapshot, engine::AudioStreamingHarnessRigPhase::kExistingComplete, data::kAudioBlaster16793__pushtobreak__earth1wavCrc) != 1)
					{
						return std::nullopt;
					}
					if (CountAudioStreamingHarnessRigRecords(snapshot, engine::AudioStreamingHarnessRigPhase::kExistingComplete, data::kAudioBlaster793907__cvltiv8r__snaresbycvltiv8r301wavCrc) != 1)
					{
						return std::nullopt;
					}
					if (CountAudioStreamingHarnessRigRecords(snapshot, engine::AudioStreamingHarnessRigPhase::kRefillReady, data::kAudioMusicdoodlewavCrc, pState->iStartSequence) < 3)
					{
						return std::nullopt;
					}
				}
				ScopedSuppressAllocationTracking deferredSuppress;
				nlohmann::json result = BuildAudioStreamingHarnessRigSnapshot();
				pState->flags.Set(AudioStreamingHarnessRigCoexistenceFlags::kComplete);
				return result;
			});
			return;
		}
		if (action == "invalid")
		{
			std::shared_ptr<AudioStreamingHarnessRigInvalidState> pState = std::make_shared<AudioStreamingHarnessRigInvalidState>();
			engine::AudioStreamingHarnessRigInvalidResult invalid = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->RunInvalid();
			if (!invalid.bPending)
			{
				pState->result = invalid;
				pState->bHasResult = true;
			}
			engine::gpAgentCommandServer->DeferResponse([pState]() -> std::optional<nlohmann::json>
			{
				if (!pState->bHasResult)
				{
					engine::AudioStreamingHarnessRigInvalidResult deferredInvalid = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->RunInvalid();
					if (deferredInvalid.bPending)
					{
						return std::nullopt;
					}
					pState->result = deferredInvalid;
					pState->bHasResult = true;
				}
				engine::AudioStreamingHarnessRigSnapshot snapshot = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->InspectFile();
				if (snapshot.eHoldState != engine::AudioStreamingHarnessRigHoldState::kIdle)
				{
					return std::nullopt;
				}
				if (snapshot.iHeldIndex != 4'294'967'295i64)
				{
					return std::nullopt;
				}
				ScopedSuppressAllocationTracking deferredSuppress;
				nlohmann::json result = BuildAudioStreamingHarnessRigInvalidResult(pState->result);
				result["snapshot"] = BuildAudioStreamingHarnessRigSnapshot();
				return result;
			});
			return;
		}
		if (!bFirstHarnessRigAction)
		{
			throw std::runtime_error("audio_streaming_harness_rig saturate is valid only as the first harness rig action in a fresh process");
		}
		std::shared_ptr<AudioStreamingHarnessRigSaturationState> pState = std::make_shared<AudioStreamingHarnessRigSaturationState>();
		if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->RequestHold())
		{
			throw std::runtime_error("audio_streaming_harness_rig saturate could not reserve its controlled hold");
		}
		engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->mCommandFlags.Set(engine::AudioStreamingHarnessRigCommandFlags::kSaturationActive);
		engine::gpAgentCommandServer->DeferResponse([pState]() -> std::optional<nlohmann::json>
		{
			engine::AudioStreamingHarnessRigSnapshot fileSnapshot = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->InspectFile();
			engine::AudioStreamingHarnessRigAudioSnapshot audioSnapshot = engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->InspectAudio();
			switch (pState->ePhase)
			{
				case AudioStreamingHarnessRigSaturationPhase::kSetup:
					if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->Begin(data::kAudioMusicdoodlewavCrc, 0, kiAudioStreamingHarnessRigReadLength))
					{
						return std::nullopt;
					}
					if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->Play(data::kAudioMusicdoodlewavCrc))
					{
						throw std::runtime_error("audio_streaming_harness_rig saturate requires stream A");
					}
					pState->ePhase = AudioStreamingHarnessRigSaturationPhase::kWaitForPrimaryStream;
					return std::nullopt;
				case AudioStreamingHarnessRigSaturationPhase::kWaitForPrimaryStream:
				{
					bool bPrimaryStreamHeld = std::ranges::any_of(fileSnapshot.poolEntries, [](const engine::AudioStreamingHarnessRigPoolEntry& rEntry)
					{
						return rEntry.crc == data::kAudioMusicdoodlewavCrc && rEntry.iOffset == 0 && rEntry.iLength == kiAudioStreamingHarnessRigReadLength && rEntry.eState == engine::AudioStreamingHarnessRigQueueState::kLoading;
					});
					bool bReadyOne = false;
					bool bReadyTwo = false;
					for (int64_t i = 0; i < fileSnapshot.iRecordCount; ++i)
					{
						const engine::AudioStreamingHarnessRigRecord& rRecord = fileSnapshot.records.at(i);
						if (rRecord.crc == data::kAudioMusicdoodlewavCrc
						 && rRecord.ePartition == engine::AudioStreamingHarnessRigPartition::kMain
						 && rRecord.ePhase == engine::AudioStreamingHarnessRigPhase::kRefillReady)
						{
							bReadyOne = bReadyOne || rRecord.iOffset == kiAudioStreamingHarnessRigReadLength;
							bReadyTwo = bReadyTwo || rRecord.iOffset == 2 * kiAudioStreamingHarnessRigReadLength;
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
					if (!(audioSnapshot.current.flags & engine::AudioStreamingHarnessRigVoiceFlags::kPresent))
					{
						return std::nullopt;
					}
					if (audioSnapshot.current.crc != data::kAudioMusicdoodlewavCrc)
					{
						return std::nullopt;
					}
					if (audioSnapshot.current.flags & engine::AudioStreamingHarnessRigVoiceFlags::kFadingIn)
					{
						return std::nullopt;
					}
					if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->BeginSaturation())
					{
						return std::nullopt;
					}
					if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->Play(data::kAudioMusicMandatoryOvertimewavCrc))
					{
						throw std::runtime_error("audio_streaming_harness_rig saturate requires stream B");
					}
					pState->ePhase = AudioStreamingHarnessRigSaturationPhase::kWaitForMandatoryStream;
					return std::nullopt;
				}
				case AudioStreamingHarnessRigSaturationPhase::kWaitForMandatoryStream:
				{
					int64_t iMandatoryStreamQueued = 0;
					for (const engine::AudioStreamingHarnessRigPoolEntry& rEntry : fileSnapshot.poolEntries)
					{
						iMandatoryStreamQueued += rEntry.crc == data::kAudioMusicMandatoryOvertimewavCrc && rEntry.eState == engine::AudioStreamingHarnessRigQueueState::kQueued ? 1 : 0;
					}
					if (iMandatoryStreamQueued != 3)
					{
						return std::nullopt;
					}
					if (!(audioSnapshot.current.flags & engine::AudioStreamingHarnessRigVoiceFlags::kPresent))
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
					if (!(audioSnapshot.newestFade.flags & engine::AudioStreamingHarnessRigVoiceFlags::kPresent))
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
					if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->Play(data::kAudioMusicsong18wavCrc))
					{
						throw std::runtime_error("audio_streaming_harness_rig saturate requires stream C");
					}
					pState->ePhase = AudioStreamingHarnessRigSaturationPhase::kWaitForRetry;
					return std::nullopt;
				}
				case AudioStreamingHarnessRigSaturationPhase::kWaitForRetry:
					if (fileSnapshot.uiRetryCount == 0)
					{
						return std::nullopt;
					}
					engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->mFlags.Set(engine::AudioStreamingHarnessRig::Flags::kPublicationStopped);
					if (!IsAudioStreamingHarnessRigSaturated(fileSnapshot, audioSnapshot))
					{
						return std::nullopt;
					}
					pState->fSaturatedCurrentVolume = audioSnapshot.current.fVolume;
					pState->fSaturatedNewestVolume = audioSnapshot.newestFade.fVolume;
					{
						ScopedSuppressAllocationTracking deferredSuppress;
						pState->saturated = BuildAudioStreamingHarnessRigSnapshot();
					}
					engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseSaturation();
					engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseHold(engine::AudioStreamingHarnessRigHoldOwner::kControlled);
					pState->ePhase = AudioStreamingHarnessRigSaturationPhase::kWaitForSettled;
					return std::nullopt;
				case AudioStreamingHarnessRigSaturationPhase::kWaitForSettled:
					if (!AudioStreamingHarnessRigEvidenceValid(fileSnapshot))
					{
						return std::nullopt;
					}
					if (fileSnapshot.flags & engine::AudioStreamingHarnessRigSnapshotFlags::kLoaderStaged)
					{
						return std::nullopt;
					}
					if (fileSnapshot.eHoldState != engine::AudioStreamingHarnessRigHoldState::kIdle)
					{
						return std::nullopt;
					}
					if (fileSnapshot.iHeldIndex != 4'294'967'295i64)
					{
						return std::nullopt;
					}
					if (!HasAudioStreamingHarnessRigAcknowledgement(fileSnapshot, data::kAudioMusicdoodlewavCrc))
					{
						return std::nullopt;
					}
					if (CountAudioStreamingHarnessRigRecords(fileSnapshot, engine::AudioStreamingHarnessRigPhase::kRefillReady, data::kAudioMusicMandatoryOvertimewavCrc) == 0)
					{
						return std::nullopt;
					}
					if (CountAudioStreamingHarnessRigRecords(fileSnapshot, engine::AudioStreamingHarnessRigPhase::kRefillReady, data::kAudioMusicsong18wavCrc) == 0)
					{
						return std::nullopt;
					}
					if (!(audioSnapshot.current.flags & engine::AudioStreamingHarnessRigVoiceFlags::kPresent))
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
					if (!(audioSnapshot.newestFade.flags & engine::AudioStreamingHarnessRigVoiceFlags::kPresent))
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
					engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->ReleaseControls();
					{
						ScopedSuppressAllocationTracking deferredSuppress;
						return nlohmann::json {{"saturated", std::move(pState->saturated)}, {"settled", BuildAudioStreamingHarnessRigSnapshot()}};
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
