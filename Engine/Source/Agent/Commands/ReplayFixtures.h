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

struct TransferCaptureEvent
{
	int64_t iRecordingEventTick = -1;
	int64_t iPlaybackEventTick = -1;
	game::ReplayTransferCaptureCounts transferCounts;
};

struct TransferCaptureSnapshot
{
	int64_t iFirstWriterInputTick = -1;
	int64_t iWriterInputCount = 0;
	std::vector<TransferCaptureEvent> events;
};

void Attach(Replay& rReplay);
void Detach(const Replay& rReplay);
void Reset(Replay& rReplay);
void RecordingStarted(const Replay& rReplay);
void RecordingStopped(Replay& rReplay, bool bPersistenceSucceeded);
void PlaybackAdopted(Replay& rReplay, const TransferCaptureSnapshot& rRecordingSnapshot);

[[nodiscard]] TransferCaptureSnapshot CaptureSnapshot(const Replay& rReplay);
[[nodiscard]] bool DropRetainedEndFrame(Replay& rReplay, GridCoord coordinate);
[[nodiscard]] bool ArmPersistenceFailure(const Replay& rReplay, PersistenceFailurePoint eFailurePoint, GridCoord coordinate = {});
[[nodiscard]] bool ConsumePersistenceFailure(const Replay& rReplay, PersistenceFailurePoint eFailurePoint, GridCoord coordinate = {}, int64_t iActivationTick = -1);
[[nodiscard]] bool IsWriterPauseArmed(const Replay& rReplay);
void ArmPauseAfterNextWriterInput(const Replay& rReplay, bool bPendingRecordingStart);
[[nodiscard]] bool ObserveWriterInput(const Replay& rReplay, int64_t iTick);
void ObserveAcceptedTransfers(const Replay& rReplay, int64_t iEventTick, std::span<const game::StatusChange> sortedTransfers);
void ObservePlaybackEvent(const Replay& rReplay, int64_t iEventTick);

} // namespace ReplayFixtures

} // namespace engine

#endif // defined(BT_SERVER)
