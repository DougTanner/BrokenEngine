#pragma once

#if defined(BT_CLIENT)

// The engine owns desync reporting and escalation: the debug-frame request and correlation and the repeated-desync
// window that escalates to disconnect. The game supplies the two
// policy operations this core calls back into — resetting per-coord client state for a resync and logging the
// captured client/server Frame differences. Naming the game Frame the captured snapshot holds keeps this header out
// of the Engine.h aggregation; its consumers include it directly.

namespace game
{

struct Frame;

} // namespace game

namespace engine
{

struct ReconcileDesyncInfo;

class ClientDesyncCore
{
public:

	bool IsStalled() const
	{
		return mDesyncDebugState.iTick >= 0 || (mpfnAdditionalStall != nullptr && mpfnAdditionalStall(*this));
	}
	int64_t GetDesyncTick() const { return mDesyncDebugState.iTick; }

	void OnDesyncDetected(ReconcileDesyncInfo&& rDesyncInfo);
	void PollDebugFrameResponse();
	bool PollDesyncTimeout();
	void RecoverFromDesync();
	void Reset();

	bool (*mpfnAdditionalStall)(const ClientDesyncCore&) = nullptr;
	void (*mpfnResetObserver)(ClientDesyncCore&) = nullptr;

private:

	struct DesyncDebugState
	{
		int64_t iTick = -1;
		GridCoord coord {};
		std::unique_ptr<game::Frame> pClientFrame;
		std::chrono::steady_clock::time_point entryTime {};
	};
	DesyncDebugState mDesyncDebugState;

	static constexpr std::chrono::seconds kDesyncDebugTimeout {5};

	// Desync frequency tracking for escalation
	int64_t miDesyncCount = 0;
	std::chrono::steady_clock::time_point mFirstDesyncTime {};
	static constexpr int64_t kiMaxDesyncsBeforeDisconnect = 3;
	static constexpr std::chrono::seconds kDesyncWindowDuration {10};
};

} // namespace engine

#endif // BT_CLIENT
