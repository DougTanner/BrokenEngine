#pragma once

#if defined(BT_SERVER)

#include "Agent/Commands/ServerSimulationFixtures.h"

namespace engine
{

class Replay;

namespace ReplayFixtures
{

enum class PersistenceFailurePoint : uint8_t
{
	kNone,
	kManifestInvalidation,
	kGrid,
	kCoordinateWriter,
	kFullFramesRecord,
	kMetadata,
	kInventory,
	kFinalManifest,
	kTransferCapture,
};

struct TransferCaptureSnapshot
{
	int64_t iRecordingEventTick = -1;
	int64_t iPlaybackEventTick = -1;
	int64_t iFirstWriterInputTick = -1;
	int64_t iWriterInputCount = 0;
	game::ReplayTransferCaptureCounts transferCounts;
};

void Attach(Replay& rReplay);
void Detach(Replay& rReplay);
void Reset(Replay& rReplay);
void RecordingInvalidated(Replay& rReplay);
void RecordingStartFailed(Replay& rReplay);
void RecordingStartCancelled(Replay& rReplay);
void RecordingStarted(Replay& rReplay);
void RecordingStopped(Replay& rReplay, bool bPersistenceSucceeded);
void PlaybackAborted(Replay& rReplay);
void PlaybackAdoptionFailed(Replay& rReplay);
void PlaybackAdopted(Replay& rReplay, const TransferCaptureSnapshot& rRecordingSnapshot);

[[nodiscard]] TransferCaptureSnapshot CaptureSnapshot(Replay& rReplay);
[[nodiscard]] bool DropRetainedEndFrame(Replay& rReplay, GridCoord coord);
[[nodiscard]] bool ArmPersistenceFailure(Replay& rReplay, PersistenceFailurePoint eFailurePoint, GridCoord coord = {});
[[nodiscard]] bool ConsumePersistenceFailure(Replay& rReplay, PersistenceFailurePoint eFailurePoint, GridCoord coord = {}, int64_t iActivationTick = -1);
[[nodiscard]] bool IsWriterPauseArmed(Replay& rReplay);
void ArmPauseAfterNextWriterInput(Replay& rReplay, bool bPendingRecordingStart);
[[nodiscard]] bool ObserveWriterInput(Replay& rReplay, int64_t iTick);
[[nodiscard]] bool ConsumeTransferCaptureFailure(Replay& rReplay);
void ObserveAcceptedTransfers(Replay& rReplay, int64_t iEventTick, std::span<const game::StatusChange> sortedTransfers);
void ObservePlaybackEvent(Replay& rReplay, int64_t iEventTick);

} // namespace ReplayFixtures

} // namespace engine

#endif // defined(BT_SERVER)
