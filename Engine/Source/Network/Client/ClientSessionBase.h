#pragma once

#if defined(BT_CLIENT)

#include "Network/Client/ClientDesyncCore.h"

namespace engine
{

class ClientSessionRuntime;

// The reusable client session state and mechanics the game client session inherits: static-data application, owned-entity
// event application, reconcile and desync sequencing, and the resync, disconnect, and server-load resets. Game functions
// and members are reached by name through game::gpClientSession.
class ClientSessionBase
{
public:

	ClientSessionBase();
	~ClientSessionBase();

	void Reconcile();
	void ApplyReceivedStaticData();
	void ResetCoordinateStatesForResynchronization();

	std::unique_ptr<ClientDesyncCore> mpDesynchronizationCore;
	std::unique_ptr<ClientSessionRuntime> mpRuntime;

private:
	friend class ClientSessionRuntime;

	void PollDesynchronizationState();
	void OnRuntimeDisconnected();
	void ResetSessionForServerLoad();
	void ApplyOwnedEntityEvents();
};

} // namespace engine

#endif // BT_CLIENT
