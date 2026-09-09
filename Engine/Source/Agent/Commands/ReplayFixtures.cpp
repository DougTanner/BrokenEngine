#include "Pch.h"

#if defined(BT_SERVER)

#include "Agent/Commands/ReplayFixtures.h"

#include "File/Replay.h"

namespace engine::ReplayFixtures
{

namespace
{

struct Binding
{
	Replay* pReplay = nullptr;
	TransferCaptureSnapshot capture;
	PersistenceFailurePoint ePersistenceFailurePoint = PersistenceFailurePoint::kNone;
	GridCoord persistenceFailureCoord {};
	int64_t iPersistenceFailureActivationTick = -1;
	int64_t iPauseAfterWriterInputCount = -1;
};

Binding sBinding;

Binding* FindBinding(Replay& rReplay)
{
	return sBinding.pReplay == &rReplay ? &sBinding : nullptr;
}

void Clear(Replay& rReplay)
{
	if (sBinding.pReplay == &rReplay)
	{
		sBinding = {.pReplay = &rReplay};
	}
}

} // namespace

void Attach(Replay& rReplay)
{
	if (sBinding.pReplay != &rReplay)
	{
		sBinding = {.pReplay = &rReplay};
	}
}

void Detach(Replay& rReplay)
{
	if (sBinding.pReplay == &rReplay)
	{
		sBinding = {};
	}
}

void Reset(Replay& rReplay)
{
	Clear(rReplay);
}

void RecordingInvalidated(Replay& rReplay)
{
	Clear(rReplay);
}

void RecordingStartFailed(Replay& rReplay)
{
	Clear(rReplay);
}

void RecordingStartCancelled(Replay& rReplay)
{
	Clear(rReplay);
}

void RecordingStarted(Replay& rReplay)
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
		Clear(rReplay);
		return;
	}
	if (Binding* pBinding = FindBinding(rReplay); pBinding != nullptr)
	{
		pBinding->ePersistenceFailurePoint = PersistenceFailurePoint::kNone;
		pBinding->persistenceFailureCoord = {};
		pBinding->iPersistenceFailureActivationTick = -1;
		pBinding->iPauseAfterWriterInputCount = -1;
	}
}

void PlaybackAborted(Replay& rReplay)
{
	Clear(rReplay);
}

void PlaybackAdoptionFailed(Replay& rReplay)
{
	Clear(rReplay);
}

void PlaybackAdopted(Replay& rReplay, const TransferCaptureSnapshot& rRecordingSnapshot)
{
	if (sBinding.pReplay == &rReplay)
	{
		sBinding = {.pReplay = &rReplay, .capture = rRecordingSnapshot};
		sBinding.capture.iPlaybackEventTick = -1;
	}
}

TransferCaptureSnapshot CaptureSnapshot(Replay& rReplay)
{
	if (Binding* pBinding = FindBinding(rReplay); pBinding != nullptr)
	{
		return pBinding->capture;
	}
	return {};
}

bool DropRetainedEndFrame(Replay& rReplay, GridCoord coord)
{
	auto it = rReplay.mReplayWriters.find(coord);
	if (it == rReplay.mReplayWriters.end())
	{
		return false;
	}
	if (it->second.empty())
	{
		return false;
	}

	std::vector<Replay::ReplayWriterState>& rWriterGenerations = it->second;
	if (!rWriterGenerations.back().bTerminal && rWriterGenerations.size() < 2)
	{
		return false;
	}
	const size_t uiGenerationIndex = rWriterGenerations.back().bTerminal ? rWriterGenerations.size() - 1 : rWriterGenerations.size() - 2;
	Replay::ReplayWriterState& rWriterState = rWriterGenerations.at(uiGenerationIndex);
	if (rWriterState.pRetainedEndFrame == nullptr)
	{
		return false;
	}
	rWriterState.pRetainedEndFrame.reset();
	return true;
}

bool ArmPersistenceFailure(Replay& rReplay, PersistenceFailurePoint eFailurePoint, GridCoord coord)
{
	Binding* pBinding = FindBinding(rReplay);
	if (pBinding == nullptr)
	{
		return false;
	}
	if (eFailurePoint == PersistenceFailurePoint::kCoordinateWriter || eFailurePoint == PersistenceFailurePoint::kFullFramesRecord)
	{
		auto it = rReplay.mReplayWriters.find(coord);
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
	pBinding->persistenceFailureCoord = coord;
	return true;
}

bool ConsumePersistenceFailure(Replay& rReplay, PersistenceFailurePoint eFailurePoint, GridCoord coord, int64_t iActivationTick)
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
		if (pBinding->persistenceFailureCoord != coord)
		{
			return false;
		}
		if (pBinding->iPersistenceFailureActivationTick != iActivationTick)
		{
			return false;
		}
	}

	pBinding->ePersistenceFailurePoint = PersistenceFailurePoint::kNone;
	pBinding->persistenceFailureCoord = {};
	pBinding->iPersistenceFailureActivationTick = -1;
	return true;
}

bool IsWriterPauseArmed(Replay& rReplay)
{
	const Binding* pBinding = FindBinding(rReplay);
	return pBinding != nullptr && pBinding->iPauseAfterWriterInputCount != -1;
}

void ArmPauseAfterNextWriterInput(Replay& rReplay, bool bPendingRecordingStart)
{
	if (Binding* pBinding = FindBinding(rReplay); pBinding != nullptr)
	{
		ASSERT(pBinding->iPauseAfterWriterInputCount == -1);
		pBinding->iPauseAfterWriterInputCount = bPendingRecordingStart ? 1 : pBinding->capture.iWriterInputCount + 1;
	}
}

bool ObserveWriterInput(Replay& rReplay, int64_t iTick)
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

bool ConsumeTransferCaptureFailure(Replay& rReplay)
{
	return ConsumePersistenceFailure(rReplay, PersistenceFailurePoint::kTransferCapture);
}

void ObserveAcceptedTransfers(Replay& rReplay, int64_t iEventTick, std::span<const game::StatusChange> sortedTransfers)
{
	Binding* pBinding = FindBinding(rReplay);
	if (pBinding == nullptr)
	{
		return;
	}
	if (pBinding->capture.iRecordingEventTick != iEventTick)
	{
		pBinding->capture.iRecordingEventTick = iEventTick;
		pBinding->capture.iPlaybackEventTick = -1;
		pBinding->capture.transferCounts = {};
	}
	game::CountCapturedReplayTransfers(sortedTransfers, pBinding->capture.transferCounts);
}

void ObservePlaybackEvent(Replay& rReplay, int64_t iEventTick)
{
	if (Binding* pBinding = FindBinding(rReplay); pBinding != nullptr)
	{
		pBinding->capture.iPlaybackEventTick = iEventTick;
	}
}

} // namespace engine::ReplayFixtures

#endif // BT_SERVER
