#pragma once

#if defined(BT_SERVER)

// The engine owns the cross-cell transfer machinery: harvest, adjacency and destination-liveness validation,
// deterministic type ordering, destination materialization, and the post-spawn destination CRC recompute. The game
// supplies the StatusChange payloads carried across the boundary, the transfer classification, and the game-policy
// queues on game::ServerSession that this machinery drains and fills. Naming game types keeps this header out of the
// Engine.h aggregation; its consumers include it directly.

namespace engine
{

struct ClientTransferInfo
{
	engine::global_id_t globalPlayerId {};
	engine::GridCoord destination {};
	engine::ClientGuid clientGuid {};
};

class ServerTransferManager
{
public:

	void HarvestTransfers();
	void PrepareReplayTransfers(engine::GridCoord coord, std::span<const game::StatusChange> recordedTransfers);
	void ApplyReplayTransfers();
	void ResetState();
	bool QueueReplayTransferFixture(engine::GridCoord destination, game::StatusChange transfer);

	bool HasPendingSubscriptionUpdate(int64_t iClientId) const;

	std::unordered_map<engine::GridCoord, std::vector<game::StatusChange>> mTransfers;

private:

	void CollectTransfers(common::ScopedWorkbufferArena& rTransfersArena);
	void SortTransfersByType();
	void SpawnTransfers(bool bFilterDestinationLiveness);
	void ApplyPreparedTransfers(std::span<const ClientTransferInfo> clientTransfers, bool bFilterDestinationLiveness);
	void TrackClientTransfers(std::span<const ClientTransferInfo> clientTransfers);
};

} // namespace engine

#endif // BT_SERVER
