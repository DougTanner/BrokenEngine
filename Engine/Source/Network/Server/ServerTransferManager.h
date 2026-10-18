#pragma once

#if defined(BT_SERVER)

// The engine owns the cross-cell transfer machinery: harvest, adjacency and destination-liveness validation,
// deterministic type ordering, destination materialization, and the post-spawn destination CRC recompute. The game
// supplies the StatusChange payloads carried across the boundary and the transfer classification. The manager also
// holds spawns decided outside the tick until their tick's post-tick batch. Naming game types keeps this header out of
// the Engine.h aggregation; its consumers include it directly.

namespace engine
{

struct ClientTransferInfo
{
	engine::GlobalId globalId {};
	engine::GridCoord destination {};
	engine::ClientGuid clientGuid {};
};

class ServerTransferManager
{
public:

	// A spawn decided outside the tick joins the post-tick batch of the first tick at or after iTick whose coordinate is
	// active with a committed current frame. Main thread, between ticks. A spawn tag and an in-cell position are the
	// caller's contract.
	void QueueSpawn(engine::GridCoord coordinate, int64_t iTick, const game::StatusChange& rSpawn);
	int64_t PendingSpawnCount() const;
	void HarvestTransfers();
	void PrepareReplayTransfers(engine::GridCoord coord, std::span<const game::StatusChange> recordedTransfers);
	void ApplyReplayTransfers();
	void ResetState();
	bool IsDestinationLive(engine::GridCoord destination) const;


	std::unordered_map<engine::GridCoord, std::vector<game::StatusChange>> mTransfers;

	int64_t miHarvestedTransferTotal = 0; // transfers applied since the current game state began; agent status reporting only, never CRC, wire, or save state

private:

	struct PendingSpawn
	{
		int64_t iTick = 0;
		game::StatusChange spawn;
	};

	void CollectTransfers(common::ScopedWorkbufferArena& rTransfersArena);
	void DrainPendingSpawns();
	void SortTransfersByType();
	void SpawnTransfers(bool bFilterDestinationLiveness);
	void ApplyPreparedTransfers(const common::ScopedWorkbufferArena& rTransfersArena, bool bFilterDestinationLiveness);
	void TrackClientTransfers(std::span<const ClientTransferInfo> clientTransfers);

	std::unordered_map<engine::GridCoord, std::vector<PendingSpawn>> mPendingSpawns;
};

} // namespace engine

#endif // BT_SERVER
