#include "Pch.h"

#include "Network/Server/ServerBroadcaster.h"

#if defined(BT_SERVER)

#include "Network/Server/ServerTransferManager.h"

#include "Agent/Commands/ServerSimulationHarnessRigs.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace engine
{

void ServerBroadcaster::BuildFrameInputs()
{
	// Heap: unordered_map clear/insert, vector resize for statusChanges
	ScopedSuppressAllocationTracking suppress;

	game::gpGame->mFrameInputs.clear();
	mBroadcastStatusChanges.clear();

	for (const engine::GridCoord& rCoord : game::gpGame->mActiveCoordinates)
	{
		game::gpGame->mFrameInputs.try_emplace(rCoord);
	}

	bool bAdvancing = game::gpGame->mfLastDeltaTime > 0.0f;
	if (bAdvancing)
	{
		game::gpServerSession->BuildAdvancingFrameInputs();
	}
}

void ServerBroadcaster::PrepareTickStatusChanges()
{
	// Heap: unordered_map insert, vector growth for statusChanges
	ScopedSuppressAllocationTracking suppress;

	// Runs once per normal-play tick, so an agent-injected StatusChange can enter any tick of a multi-tick update and
	// rides the same broadcast / CRC / replay channel as natural status changes. An entry stays queued until its
	// scheduled tick and until its coord is one the tick will simulate (active, with a committed pCurrent frame).
	game::DrainPendingAgentStatusChanges(*game::gpServerSession);

	// Save this tick's StatusChanges for broadcasting (transfers handled separately in HarvestTransfers)
	for (const auto& [rCoord, rFrameInput] : game::gpGame->mFrameInputs)
	{
		if (!rFrameInput.statusChanges.empty())
		{
			mBroadcastStatusChanges.insert_or_assign(rCoord, rFrameInput.statusChanges);
		}
	}
}

void ServerBroadcaster::BuildTickPublication(int64_t iTick, engine::ServerSessionRuntime& rRuntime, [[maybe_unused]] const common::ScopedWorkbufferArena& rPublicationArena)
{
	const std::unordered_map<engine::GridCoord, std::vector<game::StatusChange>>& rTransfers = game::gpServerSession->mpTransferManager->mTransfers;
	const std::vector<engine::GridCoord>& rActiveCoordinates = game::gpGame->mActiveCoordinates;

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	bool bReplaying = game::gpGame->mbReplaying;
	auto ForEachPublicationCoord = [&](const auto& rCallback)
	{
		if (!bReplaying)
		{
			for (const engine::GridCoord& rCoord : game::gpGame->mActiveCoordinates)
			{
				rCallback(rCoord);
			}
			return;
		}

		for (auto it = rActiveCoordinates.begin(); it != rActiveCoordinates.end(); ++it)
		{
			if (std::find(rActiveCoordinates.begin(), it, *it) == it)
			{
				rCallback(*it);
			}
		}
		for ([[maybe_unused]] const auto& [rCoord, rStatusChanges] : mBroadcastStatusChanges)
		{
			if (!std::ranges::contains(game::gpGame->mActiveCoordinates, rCoord))
			{
				rCallback(rCoord);
			}
		}
		for ([[maybe_unused]] const auto& [rCoord, rStatusChanges] : rTransfers)
		{
			if (!std::ranges::contains(game::gpGame->mActiveCoordinates, rCoord) && !mBroadcastStatusChanges.contains(rCoord))
			{
				rCallback(rCoord);
			}
		}
	};

	int64_t iPublicationCoordCount = 0;
	int64_t iStatusChangeCount = 0;
	ForEachPublicationCoord([&](const engine::GridCoord& rCoord)
	{
		++iPublicationCoordCount;
		if (auto it = mBroadcastStatusChanges.find(rCoord); it != mBroadcastStatusChanges.end())
		{
			iStatusChangeCount += std::ssize(it->second);
		}
		if (auto it = rTransfers.find(rCoord); it != rTransfers.end())
		{
			iStatusChangeCount += std::ssize(it->second);
		}
	});

	int64_t iPublicationCoordsBytes = iPublicationCoordCount * static_cast<int64_t>(sizeof(engine::GridCoord));
	int64_t iGridUpdatesOffset = common::RoundUp(iPublicationCoordsBytes, 16i64);
	int64_t iGridUpdatesBytes = iPublicationCoordCount * static_cast<int64_t>(sizeof(std::pair<engine::GridCoord, engine::GridUpdateData>));
	int64_t iStatusChangesOffset = common::RoundUp(iGridUpdatesOffset + iGridUpdatesBytes, 16i64);
	int64_t iStatusChangesBytes = iStatusChangeCount * static_cast<int64_t>(sizeof(game::StatusChange));
	int64_t iFullFramesOffset = common::RoundUp(iStatusChangesOffset + iStatusChangesBytes, 16i64);
	int64_t iFullFramesBytes = kbDesynchronizationDebugFrames ? iPublicationCoordCount * static_cast<int64_t>(sizeof(std::pair<engine::GridCoord, const game::Frame*>)) : 0;
	int64_t iPublicationBytes = kbDesynchronizationDebugFrames ? iFullFramesOffset + iFullFramesBytes : iStatusChangesOffset + iStatusChangesBytes;
	int64_t iPublicationHighWaterBytes = common::RoundUp(iPublicationBytes, 16i64) + engine::kiMaxCompressStatusChangeWorkbufferBytes;
	// Grow before any publication pointer exists, then retain that capacity for nested status compression.
	{
		[[maybe_unused]] auto highWaterAllocation = rWorkbuffer.PushBuffer<std::byte*>(iPublicationHighWaterBytes);
	}

	// One allocation keeps every publication view stable until PublishTick finishes consuming it.
	auto publicationAllocation = rWorkbuffer.PushBuffer<std::byte*>(iPublicationBytes);
	std::byte* pPublicationBytes = publicationAllocation.mpData;
	engine::GridCoord* pPublicationCoords = reinterpret_cast<engine::GridCoord*>(pPublicationBytes);
	std::pair<engine::GridCoord, engine::GridUpdateData>* pGridUpdates = reinterpret_cast<std::pair<engine::GridCoord, engine::GridUpdateData>*>(pPublicationBytes + iGridUpdatesOffset);
	game::StatusChange* pStatusChanges = reinterpret_cast<game::StatusChange*>(pPublicationBytes + iStatusChangesOffset);

	int64_t iPublicationCoordIndex = 0;
	ForEachPublicationCoord([&](const engine::GridCoord& rCoord)
	{
		pPublicationCoords[iPublicationCoordIndex++] = rCoord;
	});
	std::span<const engine::GridCoord> publicationCoords(pPublicationCoords, static_cast<size_t>(iPublicationCoordCount));

	int64_t iStatusChangeIndex = 0;
	int64_t iGridUpdateIndex = 0;
	for (const engine::GridCoord& rCoord : publicationCoords)
	{
		engine::GridUpdateData updateData {};
		updateData.uiSharedCrc = (*game::gpGame->mCells.at(rCoord).pCurrent).postRender.uiSharedCrc;
		int64_t iRunStart = iStatusChangeIndex;
		if (auto it = mBroadcastStatusChanges.find(rCoord); it != mBroadcastStatusChanges.end())
		{
			for (const game::StatusChange& rChange : it->second)
			{
				std::memcpy(pStatusChanges + iStatusChangeIndex++, &rChange, sizeof(game::StatusChange));
			}
		}
		if (auto it = rTransfers.find(rCoord); it != rTransfers.end())
		{
			for (const game::StatusChange& rChange : it->second)
			{
				std::memcpy(pStatusChanges + iStatusChangeIndex++, &rChange, sizeof(game::StatusChange));
			}
		}
		int64_t iRunCount = iStatusChangeIndex - iRunStart;
		if (iRunCount > 0)
		{
			updateData.statusChanges = {pStatusChanges + iRunStart, static_cast<size_t>(iRunCount)};
			LOG(kNetwork, kVerbose, "ServerBroadcaster::BuildTickPublication Coord: ({},{}) Tick: {} StatusChanges: {}", rCoord.iQ, rCoord.iR, iTick, iRunCount);
		}
		pGridUpdates[iGridUpdateIndex++] = {rCoord, updateData};
	}

	if constexpr (kbDesynchronizationDebugFrames)
	{
		std::pair<engine::GridCoord, const game::Frame*>* pFullFrames = reinterpret_cast<std::pair<engine::GridCoord, const game::Frame*>*>(pPublicationBytes + iFullFramesOffset);
		int64_t iFullFrameCount = 0;
		for (const engine::GridCoord& rCoord : publicationCoords)
		{
			pFullFrames[iFullFrameCount++] = {rCoord, &(*game::gpGame->mCells.at(rCoord).pCurrent)};
		}
		rRuntime.PublishTick(iTick, {pGridUpdates, static_cast<size_t>(iGridUpdateIndex)}, {pFullFrames, static_cast<size_t>(iFullFrameCount)});
	}
	else
	{
		rRuntime.PublishTick(iTick, {pGridUpdates, static_cast<size_t>(iGridUpdateIndex)}, {});
	}

	// Keep replay/live transfer state alive through PublishTick so the publication can consume it, then retire
	// this tick's transfer batch before the next replay peek or live harvest builds a new one.
	game::gpServerSession->mpTransferManager->mTransfers.clear();
}

void ServerBroadcaster::ResetState()
{
	mBroadcastStatusChanges.clear();
	game::ResetPendingAgentStatusChanges(*game::gpServerSession);
}

} // namespace engine

#endif // BT_SERVER
