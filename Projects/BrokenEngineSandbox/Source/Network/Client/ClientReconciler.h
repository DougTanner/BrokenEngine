#pragma once

#if defined(BT_CLIENT)

#include "Network/Client/ReconcileReplay.h"

namespace game
{

struct FrameInput;

using engine::SnapshotIndex;

struct ConfirmedClientState
{
	engine::GridCoord clientGridCoord {};
	engine::global_id_t clientGlobalPlayerId {};
	float fPreviousClientArmor = 0.0f;
};

void ReconcileUpdateClientState(std::span<const engine::CoordWork> works, bool bAnyFullReplay, ConfirmedClientState& rInOutState);

class ClientReconciler
{
public:

	ClientReconciler() = default;
	~ClientReconciler() = default;

	engine::ReconcileDesyncInfo Run();
	void Reset();

private:

	ConfirmedClientState mConfirmedClientState;
	engine::ReconcileDispatcher mDispatcher;
	float mfLastLoggedVisualErrorDelta = 0.0f;
	int64_t miLastVisualErrorLogTick = -1000;
};

} // namespace game

#endif // BT_CLIENT
