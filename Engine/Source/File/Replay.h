#pragma once

#if defined(BT_SERVER)

#include "Save/GameSaveLoad.h"

namespace engine
{

class Replay
{
public:
	enum class ReplayTickDecision : uint8_t
	{
		kDispatch,
		kStopBeforeDispatch,
	};

	struct ReplayWriterState
	{
		std::unique_ptr<DifferenceStreamWriter<game::Frame, game::FrameInput>> pWriter;
		std::unique_ptr<game::Frame> pRetainedEndFrame;
		int64_t iActivationTick = 0;
		bool bTerminal = false;
	};

	std::unordered_map<GridCoord, std::vector<ReplayWriterState>> mReplayWriters;

	Replay();
	~Replay();

	void SaveLoadReplay();
	[[nodiscard]] ReplayTickDecision SyncReplayTick();

	void ResetStreams();
	[[nodiscard]] bool CaptureAcceptedTransfers(GridCoord destination, std::span<const game::StatusChange> sortedTransfers, const game::Frame& rPreTransferFrame);
	void RetireCoordinate(GridCoord coordinate, std::unique_ptr<game::Frame> pLastCompleteFrame);

private:

	// Rebuilds its replay active set from mReplayReaders.
	friend class GameBase;

	void ClearReplayTransientState();
	void ClearReplayAbortState();

	struct PendingReplayReader
	{
		GridCoord coordinate {};
		std::unique_ptr<DifferenceStreamReader<game::Frame, game::FrameInput>> pReader;
		std::unique_ptr<game::Frame> pSavedStart;
		game::FrameInput initialInput;
		int64_t iActivationTick = 0;
	};

	std::unordered_map<GridCoord, std::unique_ptr<DifferenceStreamReader<game::Frame, game::FrameInput>>> mReplayReaders;
	std::vector<PendingReplayReader> mPendingReplayReaders;
	int64_t miReplayInitialTick = 0;

	void InvalidateReplayRecording();
	void UpdateTerminalReplayWriter(GridCoord coordinate, const ReplayWriterState& rWriterState, const game::Frame& rEndFrame);
	void ActivateReplayReader(GridCoord coordinate, PendingReplayReader&& rPendingReader);
};

inline Replay* gpReplay = nullptr;

} // namespace engine

#endif // defined(BT_SERVER)
