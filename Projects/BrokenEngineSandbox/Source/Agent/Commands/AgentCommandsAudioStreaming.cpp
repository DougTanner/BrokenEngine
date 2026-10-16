#include "Agent/Commands/AgentCommandsAudioStreaming.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/AudioStreamingHarnessRig.h"
#include "Agent/AgentCommandServer.h"

namespace game
{

#if defined(BT_DEBUG)

constexpr const char* kpcAudioStreamingHarnessRigSchema = "audio_streaming_harness_rig requires {\"action\":\"inspect|play|clear|suspend|resume|clear_history|hold_read|release_read|stall_loaders|release_loaders|stop_publication|resume_publication|load_chunk\"} plus exactly its action's fields: play {\"crc\"}, hold_read {\"crc\",\"offset\",\"length\"}, load_chunk {\"crc\",\"priority\":\"realtime|normal\"}";

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
		                 && before.iReservationBoundary == snapshot.iReservationBoundary && before.iRetryCount == snapshot.iRetryCount
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
	result["scenarioGeneration"] = snapshot.iScenarioGeneration;
	result["retryCount"] = snapshot.iRetryCount;
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
		{"publicationStopped", audioSnapshot.flags & engine::AudioStreamingHarnessRigAudioSnapshotFlags::kPublicationStopped},
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
	result["overflow"] = snapshot.iMainDropped != 0 || snapshot.iLoader0Dropped != 0 || snapshot.iLoader1Dropped != 0;
	result["records"] = nlohmann::json::array();
	for (int64_t i = 0; i < snapshot.iRecordCount; ++i)
	{
		const engine::AudioStreamingHarnessRigRecord& rRecord = snapshot.records.at(i);
		nlohmann::json record;
		record["sequence"] = rRecord.iSequence;
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

static void RequireAudioStreamingHarnessRigFields(const nlohmann::json& rParameters, std::initializer_list<std::string_view> fields)
{
	if (std::ssize(rParameters) != std::ssize(fields) + 1)
	{
		throw std::runtime_error(kpcAudioStreamingHarnessRigSchema);
	}
	for (std::string_view field : fields)
	{
		if (!rParameters.contains(field))
		{
			throw std::runtime_error(kpcAudioStreamingHarnessRigSchema);
		}
	}
}

static common::crc_t ReadAudioStreamingHarnessRigCrc(const nlohmann::json& rValue)
{
	if (!rValue.is_number_unsigned())
	{
		throw std::runtime_error(kpcAudioStreamingHarnessRigSchema);
	}
	return rValue.get<common::crc_t>();
}

static int64_t ReadAudioStreamingHarnessRigInteger(const nlohmann::json& rValue)
{
	if (!rValue.is_number_integer())
	{
		throw std::runtime_error(kpcAudioStreamingHarnessRigSchema);
	}
	return rValue.get<int64_t>();
}

static engine::LoadPriority ReadAudioStreamingHarnessRigPriority(const nlohmann::json& rValue)
{
	if (rValue == "realtime")
	{
		return engine::LoadPriority::kRealtime;
	}
	if (rValue == "normal")
	{
		return engine::LoadPriority::kNormal;
	}
	throw std::runtime_error(kpcAudioStreamingHarnessRigSchema);
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
		if (!rParameters.contains("action"))
		{
			throw std::runtime_error(kpcAudioStreamingHarnessRigSchema);
		}
		if (!rParameters.at("action").is_string())
		{
			throw std::runtime_error(kpcAudioStreamingHarnessRigSchema);
		}

		engine::AudioStreamingHarnessRig& rHarnessRig = *engine::gpAgentCommandServer->mpAudioStreamingHarnessRig;
		std::string action = rParameters.at("action").get<std::string>();
		if (action == "play")
		{
			RequireAudioStreamingHarnessRigFields(rParameters, {"crc"});
			if (!rHarnessRig.Play(ReadAudioStreamingHarnessRigCrc(rParameters.at("crc"))))
			{
				throw std::runtime_error("audio_streaming_harness_rig play requires a live audio voice");
			}
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "hold_read")
		{
			RequireAudioStreamingHarnessRigFields(rParameters, {"crc", "offset", "length"});
			if (!rHarnessRig.ArmHold(engine::AudioStreamingHarnessRigHoldOwner::kControlled, ReadAudioStreamingHarnessRigCrc(rParameters.at("crc")), ReadAudioStreamingHarnessRigInteger(rParameters.at("offset")), ReadAudioStreamingHarnessRigInteger(rParameters.at("length"))))
			{
				throw std::runtime_error("audio_streaming_harness_rig hold_read requires an idle hold");
			}
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "load_chunk")
		{
			RequireAudioStreamingHarnessRigFields(rParameters, {"crc", "priority"});
			rHarnessRig.LoadChunk(ReadAudioStreamingHarnessRigCrc(rParameters.at("crc")), ReadAudioStreamingHarnessRigPriority(rParameters.at("priority")));
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}

		RequireAudioStreamingHarnessRigFields(rParameters, {});
		if (action == "inspect")
		{
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "clear")
		{
			rHarnessRig.Clear();
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
		if (action == "clear_history")
		{
			if (rHarnessRig.BeginHistory())
			{
				rResult = BuildAudioStreamingHarnessRigSnapshot();
				return;
			}
			engine::gpAgentCommandServer->DeferResponse([]() -> std::optional<nlohmann::json>
			{
				if (!engine::gpAgentCommandServer->mpAudioStreamingHarnessRig->BeginHistory())
				{
					return std::nullopt;
				}
				ScopedSuppressAllocationTracking deferredSuppress;
				nlohmann::json result = BuildAudioStreamingHarnessRigSnapshot();
				return result;
			});
			return;
		}
		if (action == "release_read")
		{
			rHarnessRig.ReleaseHold(engine::AudioStreamingHarnessRigHoldOwner::kControlled);
			engine::gpAgentCommandServer->DeferResponse([]() -> std::optional<nlohmann::json>
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
				ScopedSuppressAllocationTracking deferredSuppress;
				nlohmann::json result = BuildAudioStreamingHarnessRigSnapshot();
				return result;
			});
			return;
		}
		if (action == "stall_loaders")
		{
			rHarnessRig.StallLoaders();
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "release_loaders")
		{
			rHarnessRig.ReleaseLoaders();
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "stop_publication")
		{
			rHarnessRig.mFlags.Set(engine::AudioStreamingHarnessRig::Flags::kPublicationStopped);
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		if (action == "resume_publication")
		{
			rHarnessRig.mFlags.Set(engine::AudioStreamingHarnessRig::Flags::kPublicationStopped, false);
			rResult = BuildAudioStreamingHarnessRigSnapshot();
			return;
		}
		throw std::runtime_error(kpcAudioStreamingHarnessRigSchema);
	}
#endif
}

} // namespace game

#endif // defined(BT_CLIENT)
