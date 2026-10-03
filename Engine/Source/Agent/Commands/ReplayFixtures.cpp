#include "Pch.h"

#if defined(BT_SERVER)

#include "Agent/Commands/ReplayFixtures.h"

#include "File/Replay.h"

namespace engine::ReplayFixtures
{

struct Binding
{
	Replay* pReplay = nullptr;
	TransferCaptureSnapshot capture;
	PersistenceFailurePoint ePersistenceFailurePoint = PersistenceFailurePoint::kNone;
	GridCoord persistenceFailureCoordinate {};
	int64_t iPersistenceFailureActivationTick = -1;
	int64_t iPauseAfterWriterInputCount = -1;
};

static Binding sBinding;

static Binding* FindBinding(const Replay& rReplay)
{
	return sBinding.pReplay == &rReplay ? &sBinding : nullptr;
}

void Attach(Replay& rReplay)
{
	if (sBinding.pReplay != &rReplay)
	{
		sBinding = {.pReplay = &rReplay};
	}
}

void Detach(const Replay& rReplay)
{
	if (sBinding.pReplay == &rReplay)
	{
		sBinding = {};
	}
}

void Reset(Replay& rReplay)
{
	if (sBinding.pReplay == &rReplay)
	{
		sBinding = {.pReplay = &rReplay};
	}
}

void RecordingStarted(const Replay& rReplay)
{
	if (Binding* pBinding = FindBinding(rReplay); pBinding != nullptr)
	{
		pBinding->capture = {};
	}
}

void RecordingStopped(Replay& rReplay, bool bPersistenceSucceeded)
{
	if (!bPersistenceSucceeded)
	{
		Reset(rReplay);
		return;
	}
	if (Binding* pBinding = FindBinding(rReplay); pBinding != nullptr)
	{
		pBinding->ePersistenceFailurePoint = PersistenceFailurePoint::kNone;
		pBinding->persistenceFailureCoordinate = {};
		pBinding->iPersistenceFailureActivationTick = -1;
		pBinding->iPauseAfterWriterInputCount = -1;
	}
}

void PlaybackAdopted(Replay& rReplay, const TransferCaptureSnapshot& rRecordingSnapshot)
{
	if (sBinding.pReplay == &rReplay)
	{
		sBinding = {.pReplay = &rReplay, .capture = rRecordingSnapshot};
		for (TransferCaptureEvent& rEvent : sBinding.capture.events)
		{
			rEvent.iPlaybackEventTick = -1;
		}
	}
}

TransferCaptureSnapshot CaptureSnapshot(const Replay& rReplay)
{
	if (Binding* pBinding = FindBinding(rReplay); pBinding != nullptr)
	{
		return pBinding->capture;
	}
	return {};
}

bool DropRetainedEndFrame(Replay& rReplay, GridCoord coordinate)
{
	auto it = rReplay.mReplayWriters.find(coordinate);
	if (it == rReplay.mReplayWriters.end())
	{
		return false;
	}
	if (it->second.empty())
	{
		return false;
	}

	std::vector<Replay::ReplayWriterState>& rWriterGenerations = it->second;
	if (!rWriterGenerations.back().bTerminal && std::ssize(rWriterGenerations) < 2)
	{
		return false;
	}
	size_t uiGenerationIndex = rWriterGenerations.back().bTerminal ? rWriterGenerations.size() - 1 : rWriterGenerations.size() - 2;
	Replay::ReplayWriterState& rWriterState = rWriterGenerations.at(uiGenerationIndex);
	if (rWriterState.pRetainedEndFrame == nullptr)
	{
		return false;
	}
	rWriterState.pRetainedEndFrame.reset();
	return true;
}

bool ArmPersistenceFailure(const Replay& rReplay, PersistenceFailurePoint eFailurePoint, GridCoord coordinate)
{
	Binding* pBinding = FindBinding(rReplay);
	if (pBinding == nullptr)
	{
		return false;
	}
	if (eFailurePoint == PersistenceFailurePoint::kCoordinateWriter || eFailurePoint == PersistenceFailurePoint::kFullFramesRecord)
	{
		auto it = rReplay.mReplayWriters.find(coordinate);
		if (it == rReplay.mReplayWriters.end())
		{
			return false;
		}
		if (it->second.empty())
		{
			return false;
		}
		const std::vector<Replay::ReplayWriterState>& rWriterGenerations = it->second;
		const Replay::ReplayWriterState* pSelectedGeneration = &rWriterGenerations.back();
		if (pSelectedGeneration->bTerminal)
		{
			pSelectedGeneration = nullptr;
			for (auto generationIt = rWriterGenerations.rbegin(); generationIt != rWriterGenerations.rend(); ++generationIt)
			{
				if (generationIt->bTerminal && generationIt->pRetainedEndFrame != nullptr)
				{
					pSelectedGeneration = &*generationIt;
					break;
				}
			}
			if (pSelectedGeneration == nullptr)
			{
				return false;
			}
		}
		pBinding->iPersistenceFailureActivationTick = pSelectedGeneration->iActivationTick;
	}
	else
	{
		pBinding->iPersistenceFailureActivationTick = -1;
	}

	pBinding->ePersistenceFailurePoint = eFailurePoint;
	pBinding->persistenceFailureCoordinate = coordinate;
	return true;
}

bool ConsumePersistenceFailure(const Replay& rReplay, PersistenceFailurePoint eFailurePoint, GridCoord coordinate, int64_t iActivationTick)
{
	Binding* pBinding = FindBinding(rReplay);
	if (pBinding == nullptr)
	{
		return false;
	}
	if (pBinding->ePersistenceFailurePoint != eFailurePoint)
	{
		return false;
	}
	if (eFailurePoint == PersistenceFailurePoint::kCoordinateWriter || eFailurePoint == PersistenceFailurePoint::kFullFramesRecord)
	{
		if (pBinding->persistenceFailureCoordinate != coordinate)
		{
			return false;
		}
		if (pBinding->iPersistenceFailureActivationTick != iActivationTick)
		{
			return false;
		}
	}

	pBinding->ePersistenceFailurePoint = PersistenceFailurePoint::kNone;
	pBinding->persistenceFailureCoordinate = {};
	pBinding->iPersistenceFailureActivationTick = -1;
	return true;
}

bool IsWriterPauseArmed(const Replay& rReplay)
{
	const Binding* pBinding = FindBinding(rReplay);
	return pBinding != nullptr && pBinding->iPauseAfterWriterInputCount != -1;
}

void ArmPauseAfterNextWriterInput(const Replay& rReplay, bool bPendingRecordingStart)
{
	if (Binding* pBinding = FindBinding(rReplay); pBinding != nullptr)
	{
		ASSERT(pBinding->iPauseAfterWriterInputCount == -1);
		pBinding->iPauseAfterWriterInputCount = bPendingRecordingStart ? 1 : pBinding->capture.iWriterInputCount + 1;
	}
}

bool ObserveWriterInput(const Replay& rReplay, int64_t iTick)
{
	Binding* pBinding = FindBinding(rReplay);
	if (pBinding == nullptr)
	{
		return false;
	}
	if (pBinding->capture.iFirstWriterInputTick == -1)
	{
		pBinding->capture.iFirstWriterInputTick = iTick;
	}
	++pBinding->capture.iWriterInputCount;
	if (pBinding->iPauseAfterWriterInputCount != pBinding->capture.iWriterInputCount)
	{
		return false;
	}
	pBinding->iPauseAfterWriterInputCount = -1;
	return true;
}

void ObserveAcceptedTransfers(const Replay& rReplay, int64_t iEventTick, std::span<const game::StatusChange> sortedTransfers)
{
	Binding* pBinding = FindBinding(rReplay);
	if (pBinding == nullptr)
	{
		return;
	}
	std::vector<TransferCaptureEvent>& rEvents = pBinding->capture.events;
	// Event ticks arrive monotonically, so only the last entry can still be accumulating
	if (rEvents.empty() || rEvents.back().iRecordingEventTick != iEventTick)
	{
		// Heap: a new event tick may reallocate the list, inside ServerTransferManager::HarvestTransfers' tracking suppression
		rEvents.push_back({.iRecordingEventTick = iEventTick});
	}
	game::CountCapturedReplayTransfers(sortedTransfers, rEvents.back().transferCounts);
}

void ObservePlaybackEvent(const Replay& rReplay, int64_t iEventTick)
{
	Binding* pBinding = FindBinding(rReplay);
	if (pBinding == nullptr)
	{
		return;
	}
	for (TransferCaptureEvent& rEvent : pBinding->capture.events)
	{
		if (rEvent.iRecordingEventTick == iEventTick)
		{
			rEvent.iPlaybackEventTick = iEventTick;
			return;
		}
	}
}

} // namespace engine::ReplayFixtures

#endif // BT_SERVER
