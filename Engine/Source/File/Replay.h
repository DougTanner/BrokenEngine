#pragma once

#if defined(BT_SERVER)

#include "Save/GameSaveLoad.h"

namespace engine
{

class GameBase;

class Replay
{
public:

	enum class ReplayPersistenceFailurePoint : uint8_t
	{
		kNone,
		kManifestInvalidation,
		kGrid,
		kCoordinateWriter,
		kMetadata,
		kInventory,
		kFinalManifest,
	};

	struct ReplayTransferCaptureInfo
	{
		int64_t iRecordingEventTick = -1;
		int64_t iPlaybackEventTick = -1;
		int64_t iFirstWriterInputTick = -1;
		int64_t iWriterInputCount = 0;
		// Game-owned half of the capture; carried here and cleared with the rest, never inspected.
		game::ReplayTransferCaptureCounts transferCounts;
		int64_t iPauseAfterWriterInputCount = -1;
	};

	ReplayTransferCaptureInfo mReplayTransferCaptureInfo;

	Replay(GameBase& rGameBase);
	~Replay();

	void SaveLoadReplay();
	bool SyncReplayTick(); // false when final replay reader retires and this fixed-tick iteration must stop before dispatch

	bool IsRecording() const { return !mReplayWriters.empty(); }
	void ResetStreams();
	void CaptureHarvestedTransfers(GridCoord coord, std::span<const game::StatusChange> transfers, const game::Frame& rPreTransferFrame);
	void RetainReplayEndFrame(GridCoord coord, std::unique_ptr<game::Frame>& rpFrame);
	bool DropRetainedReplayEndFrame(GridCoord coord);
	bool ArmReplayPersistenceFailure(ReplayPersistenceFailurePoint eFailurePoint, GridCoord coord = {});

private:

	// Rebuilds its replay active set from mReplayReaders.
	friend class GameBase;

	void ClearReplayTransientState();
	void ClearReplayAbortState();
	// Republishes engine::GameBase::mbReplaying, the single owner of "replay playback is running".
	// Call after every change to the live or pending reader sets.
	void PublishReplayingState();

	GameBase& mrGameBase;

	struct ReplayWriterState
	{
		std::unique_ptr<DifferenceStreamWriter<game::Frame, game::FrameInput>> pWriter;
		std::unique_ptr<game::Frame> pRetainedEndFrame;
		int64_t iActivationTick = 0;
		bool bTerminal = false;
	};

	struct PendingReplayReader
	{
		std::unique_ptr<DifferenceStreamReader<game::Frame, game::FrameInput>> pReader;
		std::unique_ptr<game::Frame> pSavedStart;
		game::FrameInput initialInput;
		int64_t iActivationTick = 0;
	};

	std::unordered_map<GridCoord, ReplayWriterState> mReplayWriters;
	std::unordered_map<GridCoord, std::unique_ptr<DifferenceStreamReader<game::Frame, game::FrameInput>>> mReplayReaders;
	std::unordered_map<GridCoord, PendingReplayReader> mPendingReplayReaders;
	int64_t miReplayInitialTick = 0;
	ReplayPersistenceFailurePoint meReplayPersistenceFailurePoint = ReplayPersistenceFailurePoint::kNone;
	GridCoord mReplayPersistenceFailureCoord {};

	bool ConsumeReplayPersistenceFailure(ReplayPersistenceFailurePoint eFailurePoint, GridCoord coord = {});
	void UpdateTerminalReplayWriter(GridCoord coord, ReplayWriterState& rWriterState, const game::Frame& rEndFrame);
	void ActivateReplayReader(GridCoord coord, PendingReplayReader&& rPendingReader);
};

inline Replay* gpReplay = nullptr;

} // namespace engine

#endif // defined(BT_SERVER)
