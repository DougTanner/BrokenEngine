#include "Game.h"

#include "Audio/StreamingVoices.h"
#include "File/Replay.h"
#include "Input/Input.h"
#include "Network/Server/ServerBroadcaster.h"
#include "Network/Server/ServerTransferManager.h"

#include "Agent/Commands/ClientPacketFaultFixture.h"
#include "Frame/Collections/Players/Players.h"
#include "Network/GamePacketType.h"
#include "Ui/Localization.h"

namespace game
{

static constexpr float kfCameraShakeAdd = 0.25f;
static constexpr float kfCameraShakeMax = 1.0f;

// Debug-only main-menu island browser: build the origin cell as a single island centered at (0,0),
// selected by index into the boot-fixed area-sorted list of all packed islands (largest footprint
// first). clear()+push_back reuses the vector's capacity (grown once under
// ScopedSuppressAllocationTracking in CreateNewFrame), so the cycle path never heap-allocates in the
// main loop.
static void BuildMenuIslandPlacement(int64_t iIndex, std::vector<engine::IslandPlacement>& rOutput)
{
	rOutput.clear();
	common::crc_t islandCrc = engine::gpIslandTerrain->mIslandCrcsByArea.at(static_cast<size_t>(iIndex));
	rOutput.push_back({.islandCrc = islandCrc, .f2WorldPosition = {0.0f, 0.0f}, .fRotation = 0.0f});
}

Game::Game()
{
	ASSERT(gpGame == nullptr);

	gpGame = this;

	InitializeLocalization();

	int64_t iNextAlignment = 1;
	mPlayerAlignment = engine::AlignmentIdentifier {iNextAlignment++};
	mEnemyAlignment = engine::AlignmentIdentifier {iNextAlignment++};
	mAlignments.AddAlignment(mPlayerAlignment, mEnemyAlignment, engine::AlignmentFlags::kuiEnemies);

#if defined(BT_SERVER)
	mpServerSession = std::make_unique<ServerSession>();
	if (!mGameSaveLoad.Autoload())
	{
		CreateNewFrame(GameFlags::kGame);
	}
	meUiState = engine::UiState::kNone;
#else
	CreateNewFrame(GameFlags::kMainMenu);
	mGameFlags.Set(engine::GameFlags::kMainMenu);
#endif // BT_SERVER

#if defined(BT_CLIENT)
	mpClientSession = std::make_unique<ClientSession>();
#endif

#if defined(BT_CLIENT)
	StartMenuMusic();
	engine::gpAudioManager->mpStreamingVoices->mGetNextTrack = [this]()
	{
		return GetNextMusicTrack();
	};
#endif // BT_CLIENT

}

engine::GlobalId Game::ClientPlayerIdentifier() const
{
#if defined(BT_CLIENT)
	const Fleet* pFleet = mFleetSelection.FocusedFleet();
	if (pFleet != nullptr)
	{
		engine::GlobalId focusedMemberGlobalIdentifier = mFleetSelection.mFocusedMemberGlobalId;
		auto it = std::ranges::find(pFleet->members, focusedMemberGlobalIdentifier, &FleetMember::globalPlayerId);
		if ((focusedMemberGlobalIdentifier.iValue != 0) && it != pFleet->members.end() && !(it->flags & FleetMemberFlags::kIsDead))
		{
			return it->globalPlayerId;
		}
	}
#endif
	return {};
}

void Game::AddClientPlayer(engine::GlobalId identifier, engine::GridCoord coordinate)
{
	// Heap: mClientPlayerIdentifiers / mClientPlayerCoordinates push_back may grow vectors
	ScopedSuppressAllocationTracking suppress;
	mClientPlayerIdentifiers.push_back(identifier);
	mClientPlayerCoordinates.push_back(coordinate);
}

void Game::RemoveClientPlayer(engine::GlobalId identifier)
{
	for (int64_t i = 0; i < std::ssize(mClientPlayerIdentifiers); ++i)
	{
		if (mClientPlayerIdentifiers.at(i) == identifier)
		{
			LOG(kNetwork, kVerbose, "RemoveClientPlayer GlobalPlayerId: {} Index: {} OldPlayerCount: {}", identifier, i, std::ssize(mClientPlayerIdentifiers));
			mClientPlayerIdentifiers.erase(mClientPlayerIdentifiers.begin() + i);
			mClientPlayerCoordinates.erase(mClientPlayerCoordinates.begin() + i);
			return;
		}
	}
}

std::optional<int64_t> Game::ClientPlayerIndex(const PlayersPostRender& rPlayers) const
{
	engine::GlobalId focusedIdentifier = ClientPlayerIdentifier();
	if ((focusedIdentifier.iValue != 0))
	{
		for (int64_t i = 0; i < rPlayers.iCount; ++i)
		{
			if (rPlayers.pGlobalPlayerIds[i] == focusedIdentifier)
			{
				return i;
			}
		}
	}

	return std::nullopt;
}

#if defined(BT_CLIENT)
XMVECTOR Game::GetClientPlayerPosition() const
{
	const engine::CoordFrames& rFrames = mCoordinateFrames.at(mClientGridCoordinate);
	if (rFrames.iSnapshotCount > 0)
	{
		int64_t iTailPhysical = engine::SnapshotIndex(rFrames.iSnapshotHead, rFrames.iSnapshotCount - 1);
		const std::unique_ptr<Frame>& rpTail = rFrames.snapshots[iTailPhysical];
		if (rpTail != nullptr)
		{
			std::optional<int64_t> playerIndex = ClientPlayerIndex(*rpTail->postRender.pPlayers);
			if (playerIndex)
			{
				return rpTail->interpolate.pPlayers->pVecPositions[*playerIndex];
			}
		}
	}
	XMVECTOR vecArea = engine::LocalFrameArea();
	return XMVectorSet((XMVectorGetX(vecArea) + XMVectorGetZ(vecArea)) * 0.5f, (XMVectorGetY(vecArea) + XMVectorGetW(vecArea)) * 0.5f, 0.0f, 1.0f);
}
#endif // BT_CLIENT

void Game::ComputeActiveSet()
{
#if defined(BT_SERVER)
	gpServerSession->mpRuntime->ComputeActiveSet();
#else
	// Heap: mActiveCoordinates vector clear/push_back may allocate. Persists as Game member across frame updates
	ScopedSuppressAllocationTracking suppress;

	if (!(mGameFlags & engine::GameFlags::kMainMenu))
	{
		mActiveCoordinates.clear();
		for (const auto& [rCoordinate, rFrames] : mCoordinateFrames)
		{
			if (rFrames.iSnapshotCount > 0 && (rFrames.iConfirmedTick >= 0 || rCoordinate == mClientGridCoordinate))
			{
				mActiveCoordinates.push_back(rCoordinate);
			}
		}

		{
			auto it = mCoordinateFrames.find(mClientGridCoordinate);
			if (it == mCoordinateFrames.end() || it->second.iSnapshotCount == 0)
			{
				CreateFrameAtCoordinate(mClientGridCoordinate);
			}
			if (!std::ranges::contains(mActiveCoordinates, mClientGridCoordinate))
			{
				mActiveCoordinates.push_back(mClientGridCoordinate);
			}
		}

		miVisibleNeighborCount = 0;
		if ((ClientPlayerIdentifier().iValue != 0))
		{
			// Camera-zoom-dependent VisibleArea: mf4LargeVisibleArea packs (minX, maxY, maxX, minY).
			const XMFLOAT4& rf4Visible = engine::gpCamera->mf4LargeVisibleArea;
			// The visible area is in the camera basis cell's frame, which is not always the client cell: it lags a
			// client-cell change and can be another coord entirely while render-camera selection falls back. Each
			// neighbour rectangle therefore offsets the one local area every cell has by that neighbour's own offset
			// from the camera basis, so both sides of the test are always in the same frame.
			XMVECTOR vecArea = engine::LocalFrameArea();
			float fCellMinX = XMVectorGetX(vecArea);
			float fCellMaxY = XMVectorGetY(vecArea);
			float fCellMaxX = XMVectorGetZ(vecArea);
			float fCellMinY = XMVectorGetW(vecArea);

			auto EnsureNeighbor = [&](engine::GridCoord neighbor)
			{
				auto it = mCoordinateFrames.find(neighbor);
				if (it == mCoordinateFrames.end() || it->second.iSnapshotCount == 0)
				{
					CreateFrameAtCoordinate(neighbor);
				}
				if (!std::ranges::contains(mActiveCoordinates, neighbor))
				{
					mActiveCoordinates.push_back(neighbor);
				}
			};

			// Adjacent-only clamp: at wide zoom the VisibleArea may extend past the 3x3 ring;
			// only the immediate ring is ever subscribed regardless.
			for (int64_t i = -1; i <= 1; ++i)
			{
				for (int64_t j = -1; j <= 1; ++j)
				{
					if (i == 0 && j == 0)
					{
						continue;
					}
					// A cell at the numeric edge of the grid has no neighbour in that direction; skip it rather than
					// wrap to the opposite end.
					engine::GridCoord neighbor {};
					if (!engine::TryAddGridCoordinate(mClientGridCoordinate, i, j, neighbor))
					{
						continue;
					}
					XMFLOAT2 f2Offset = engine::MakeRenderBasis(neighbor, engine::gpCamera->mBasisCoordinate).f2Offset;
					float fNeighborMinX = fCellMinX + f2Offset.x;
					float fNeighborMaxX = fCellMaxX + f2Offset.x;
					float fNeighborMinY = fCellMinY + f2Offset.y;
					float fNeighborMaxY = fCellMaxY + f2Offset.y;
					if (rf4Visible.x < fNeighborMaxX && rf4Visible.z > fNeighborMinX && rf4Visible.w < fNeighborMaxY
					 && rf4Visible.y > fNeighborMinY)
					{
						mVisibleNeighbors[miVisibleNeighborCount++] = neighbor;
						EnsureNeighbor(neighbor);
					}
				}
			}
		}

	}
	else
	{
		mActiveCoordinates.clear();
		mActiveCoordinates.push_back(mClientGridCoordinate);
	}

	// Delete local-only frames outside the active set, preserve network-subscribed frames
	std::erase_if(mCoordinateFrames, [this](const std::pair<const engine::GridCoord, engine::CoordFrames>& rPair)
	{
		return !std::ranges::contains(mActiveCoordinates, rPair.first) && rPair.second.iConfirmedTick < 0;
	});

	ASSERT(std::ranges::count_if(mCoordinateFrames, [](const std::pair<const engine::GridCoord, engine::CoordFrames>& rPair)
	{
		return rPair.second.iConfirmedTick < 0;
	}) <= 9);

#endif // BT_SERVER
}

#if defined(BT_CLIENT)
void Game::UpdateActiveIslands()
{
	// Include confirmed cells and the client cell's provisional frame.
	// The filtered coordinates use per-frame workbuffer storage; UpdateActiveIslands' nested reservation pops before this arena.
	common::ScopedWorkbufferArena subscribedArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const engine::GridCoord& rCoordinate : mActiveCoordinates)
	{
		auto it = mCoordinateFrames.find(rCoordinate);
		if (it != mCoordinateFrames.end() && (it->second.iConfirmedTick >= 0 || rCoordinate == mClientGridCoordinate))
		{
			subscribedArena.mBuffer.PushBack<engine::GridCoord>(rCoordinate);
		}
	}
	engine::gpIslands->UpdateActiveIslands(mCoordinateFrames, subscribedArena.mBuffer.Span<const engine::GridCoord>());
}
#endif // BT_CLIENT

#if defined(BT_SERVER)
void Game::EnsureNextFrames()
{
	// Heap: coordinate-frame entries and their next frames persist across ticks.
	ScopedSuppressAllocationTracking suppress;

	for (const engine::GridCoord& rCoordinate : mActiveCoordinates)
	{
		if (mCoordinateFrames.try_emplace(rCoordinate).first->second.pNext == nullptr)
		{
			mCoordinateFrames.at(rCoordinate).pNext = std::make_unique<Frame>();
		}
	}
}
#endif // BT_SERVER

void Game::BuildFrameInputs()
{
#if defined(BT_SERVER)
	gpServerSession->mpBroadcaster->BuildFrameInputs();
#else
	// Heap: unordered_map clear/insert for per-coordinate FrameInputs. Map persists as Game member
	ScopedSuppressAllocationTracking suppress;

	mFrameInputs.clear();

	for (const engine::GridCoord& rCoordinate : mActiveCoordinates)
	{
		if (!mCoordinateFrames.contains(rCoordinate))
		{
			continue;
		}

		mFrameInputs.try_emplace(rCoordinate);
	}

	// Camera shake — read most recent ring frame (head + count - 1)
	auto it = mCoordinateFrames.find(mClientGridCoordinate);
	const Frame* pTailFrame = nullptr;
	if (it != mCoordinateFrames.end() && it->second.iSnapshotCount > 0)
	{
		int64_t iTailPhysical = engine::SnapshotIndex(it->second.iSnapshotHead, it->second.iSnapshotCount - 1);
		pTailFrame = it->second.snapshots[iTailPhysical].get();
	}
	if ((ClientPlayerIdentifier().iValue != 0) && pTailFrame != nullptr)
	{
		const Frame& rCurrentFrame = *pTailFrame;
		const PlayersPostRender& rPlayersPostRender = *rCurrentFrame.postRender.pPlayers;

		std::optional<int64_t> playerIndex = ClientPlayerIndex(rPlayersPostRender);
		if (playerIndex)
		{
			float fCurrentArmor = rPlayersPostRender.pfArmors[*playerIndex];
			if (fCurrentArmor < mfPreviousClientArmor)
			{
				engine::gpCamera->mfShake = std::min(engine::gpCamera->mfShake + kfCameraShakeAdd, kfCameraShakeMax);
			}
			mfPreviousClientArmor = fCurrentArmor;
		}
	}
#endif // BT_SERVER
}

void Game::HarvestTransfers()
{
#if defined(BT_SERVER)
	gpServerSession->mpTransferManager->HarvestTransfers();
	// mTransfers still holds the batch just applied; the broadcaster clears it later this tick.
	for (const auto& [rCoordinate, rTransfers] : gpServerSession->mpTransferManager->mTransfers)
	{
		gpServerSession->miHarvestedTransferTotal += std::ssize(rTransfers);
	}
#endif
}

Game::~Game()
{
	if (engine::gpAgentCommandServer != nullptr)
	{
		engine::gpAgentCommandServer->ClearDeferredResponse();
	}
#if defined(BT_CLIENT)
	mpClientSession.reset();
	engine::gpAudioManager->mpStreamingVoices->mGetNextTrack = nullptr;
#endif // BT_CLIENT

#if defined(BT_SERVER)
	mpServerSession.reset();
#endif

	if (!(mMenuFlags & engine::MenuFlags::kMouseVisible))
	{
		ShowCursor(TRUE);
	}

	if (gpGame == this)
	{
		gpGame = nullptr;
	}
}

void Game::Reset()
{
	LOG(kDefault, kDebug, "Game::Reset()");

	miTickCounter = 0;
	mfCurrentTime = 0.0f;

#if defined(BT_SERVER)
	engine::gpReplay->ResetStreams();
	mGameFlags.Set(engine::GameFlags::kSaveReplay, false);
#endif // BT_SERVER

#if defined(BT_CLIENT)
	engine::gpCamera->ResetForSession();
	engine::gbSmokeClear = true;
	engine::gpParticleManager->mbReset = true;
	engine::WindTrailsInterpolate::sPreviousPositions.clear();
	mVecVisualErrorOffset = {};
	mWeaponModeToggle.Reset();
#endif // BT_CLIENT

	mGameFlags.Set(engine::GameFlags::kPaused, false);
	mClientPlayerIdentifiers.clear();
	mClientPlayerCoordinates.clear();
#if defined(BT_CLIENT)
	mFleetSelection.Clear();
	mbClientGridCoordinatePinned = false;
#endif
	mfPreviousClientArmor = 0.0f;
	SetClientGridCoordinate(engine::kOriginCoordinate);
	mActiveCoordinates.clear();
	mActiveCoordinates.push_back(mClientGridCoordinate);
}

void Game::CreateNewFrame(GameFlags_t gameFlags)
{
	// Heap: make_unique<Frame> with all its SOA collections. Frame persists across the entire
	// game state lifetime, so workbuffer (lost on Pop) can't hold it.
	ScopedSuppressAllocationTracking suppress;

	mCoordinateFrames.clear();
	engine::CoordFrames& rFrames = mCoordinateFrames.try_emplace(engine::kOriginCoordinate).first->second;
#if defined(BT_CLIENT)
	rFrames.iSnapshotHead = 0;
	rFrames.iSnapshotCount = 1;
	rFrames.snapshots[0] = std::make_unique<Frame>();
	Frame& rFrame = *rFrames.snapshots[0];
#else
	rFrames.pCurrent = std::make_unique<Frame>();
	Frame& rFrame = *rFrames.pCurrent;
#endif
	rFrame.interpolate.gameFlags.Set(gameFlags.meFlags);
	InitializeFramePostRender(rFrame);

	// Populate static data for origin coord (used as the main-menu cell)
	engine::FrameStaticData& rStaticData = rFrames.staticData;
	rStaticData.coordinate = engine::kOriginCoordinate;
	// Debug builds turn the main-menu cell into a single centered island browser ('E' cycles it);
	// release builds keep the procedural island chain. Gameplay cells always use the chain.
	bool bMenuBrowse = false;
	if constexpr (kbDebugInput)
	{
		bMenuBrowse = static_cast<bool>(gameFlags & GameFlags::kMainMenu);
	}
	if (bMenuBrowse)
	{
		BuildMenuIslandPlacement(miMenuIslandIndex, rStaticData.islands);
	}
	else
	{
		engine::GenerateIslandChain(engine::kOriginCoordinate, rStaticData.islands);
	}
	// navigationData stays empty; RunFrameTick builds it lazily on the per-coord dispatch thread.

#if defined(BT_SERVER)
	rFrames.pNext = std::make_unique<Frame>();
#endif
}

#if defined(BT_CLIENT)
bool Game::ShouldUseCrosshair()
{
	auto it = mCoordinateFrames.find(mClientGridCoordinate);
	if (it == mCoordinateFrames.end())
	{
		return false;
	}
	if (it->second.iSnapshotCount == 0)
	{
		return false;
	}
	int64_t iTailPhysical = engine::SnapshotIndex(it->second.iSnapshotHead, it->second.iSnapshotCount - 1);
	const std::unique_ptr<Frame>& rpTail = it->second.snapshots[iTailPhysical];
	if (rpTail == nullptr)
	{
		return false;
	}
	return rpTail->interpolate.gameFlags & GameFlags::kGame && meUiState == engine::UiState::kNone;
}

engine::StandardMenuModel Game::GetStandardMenuModel() const
{
	const engine::ClientSessionRuntime& rRuntime = *gpClientSession->mpRuntime;

	engine::StandardMenuState_t state;
	state.Set(engine::StandardMenuState::kClientPresent, rRuntime.mpClient != nullptr);
	state.Set(engine::StandardMenuState::kDiscoveryScannerPresent, rRuntime.mpDiscoveryScanner != nullptr);
	state.Set(engine::StandardMenuState::kServerDiscovered, rRuntime.mStateFlags & engine::ClientSessionStateFlags::kServerDiscovered);
	// Set only while a client exists, so the menu can test this bit on its own.
	state.Set(engine::StandardMenuState::kConnectionAccepted, rRuntime.mpClient != nullptr && (rRuntime.mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnectionAccepted));
	state.Set(engine::StandardMenuState::kAutoConnect, kbAutoConnect);

	return engine::StandardMenuModel
	{
		.pcTitle = "BROKEN ENGINE",
		.features = {engine::StandardMenuFeature::kLocalServer, engine::StandardMenuFeature::kRemoteServer},
		.state = state,
	};
}

void Game::ApplyStandardMenuAction(engine::StandardMenuAction eAction)
{
	switch (eAction)
	{
		case engine::StandardMenuAction::kStartDiscovery:
			gpClientSession->mpRuntime->StartDiscovery();
			break;
		case engine::StandardMenuAction::kConnectToDiscoveredServer:
			gpClientSession->mpRuntime->ConnectToDiscoveredServer(engine::kiDefaultPort, NetworkSessionContract::kiCoordinateSlots);
			break;
		case engine::StandardMenuAction::kChangeFrameToMainMenu:
			ChangeFrame(GameFlags::kMainMenu);
			break;
	}
}
#endif // BT_CLIENT

void Game::ChangeFrame(GameFlags_t gameFlags)
{
#if defined(BT_CLIENT)
	gpClientSession->mpRuntime->Disconnect();
#endif

	if ((gameFlags & GameFlags::kMainMenu && (mGameFlags & engine::GameFlags::kMainMenu)) || (gameFlags & GameFlags::kGame && !(mGameFlags & engine::GameFlags::kMainMenu)))
	{
		DEBUG_BREAK();
		return;
	}

#if defined(BT_CLIENT)
	if (gameFlags & GameFlags::kMainMenu)
	{
		StartMenuMusic();
	}
	else
	{
		StartGameMusic();
	}
#endif // BT_CLIENT

	mGameFlags.Set(engine::GameFlags::kMainMenu, gameFlags & GameFlags::kMainMenu);
	CreateNewFrame(gameFlags);
	Reset();
}

#if defined(BT_CLIENT)
void Game::ProcessGameMenuInput(const engine::MenuInput& rMenuInput, const engine::InputPoll& rInputPoll)
{
	if constexpr (kbDebugInput)
	{
		// Deliver an armed client_packet_fault_fixture packet here: this is the first client main-loop point after
		// the agent drain that sits outside AgentCommandServer::Drain's catch, so the corrupt-stream ASSERT reaches
		// ProcessMain's handler instead of being answered as a command failure.
		InjectArmedClientPacketFault();

		// Return drives two independent game actions; poll the edge once and let both branches read it.
		bool bReturnPressed = rInputPoll.KeyboardPressed(VK_RETURN);
		bool bCycleMenuIslandPressed = rInputPoll.KeyboardPressed('E');

		// A server built without kbDebugInput strikes every debug-control request as corrupt data, so send
		// them only to a server whose accepted connection response said it takes them.
		bool bServerTakesDebugControl = engine::gpClient != nullptr && (engine::gpClient->mStateFlags & engine::Client::ClientStateFlags::kServerDebugInput);

		if (bServerTakesDebugControl)
		{
			if (rMenuInput.flags & engine::MenuInputFlags::kQuicksave)
			{
				engine::gpClient->SendSimplePacket(GamePacketType::kClientSaveRequest, engine::NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE);
			}
			if (rMenuInput.flags & engine::MenuInputFlags::kQuickload)
			{
				engine::gpClient->SendSimplePacket(GamePacketType::kClientLoadRequest, engine::NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE);
			}
			if (rMenuInput.flags & engine::MenuInputFlags::kSaveReplay)
			{
				engine::gpClient->SendSimplePacket(GamePacketType::kClientReplayRecordRequest, engine::NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE);
			}
			if (rMenuInput.flags & engine::MenuInputFlags::kLoadReplay)
			{
				engine::gpClient->SendSimplePacket(GamePacketType::kClientReplayPlaybackRequest, engine::NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE);
			}
			if (bReturnPressed)
			{
				engine::gpClient->SendSimplePacket(GamePacketType::kClientResetRequest, engine::NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE);
			}
		}

		if (rMenuInput.flags & engine::MenuInputFlags::kSlowTime)
		{
			if (bServerTakesDebugControl)
			{
				engine::gpClient->SendSimplePacket(GamePacketType::kClientTimespeedRequest, engine::NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE, 0ui8);
			}
		}
		else if (rMenuInput.flags & engine::MenuInputFlags::kSpeedUpTime)
		{
			if (bServerTakesDebugControl)
			{
				engine::gpClient->SendSimplePacket(GamePacketType::kClientTimespeedRequest, engine::NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE, 1ui8);
			}
		}

		// GameBase already flipped the local pause state, so this reports the state the client just entered.
		if (rMenuInput.flags & engine::MenuInputFlags::kTogglePauseFrame)
		{
			if (bServerTakesDebugControl)
			{
				engine::gpClient->SendSimplePacket(GamePacketType::kClientPauseRequest, engine::NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE, static_cast<uint8_t>((mGameFlags & engine::GameFlags::kPaused) ? 1 : 0));
			}
		}

		if (bReturnPressed && (mGameFlags & engine::GameFlags::kMainMenu) && gpClientSession->mpRuntime->mpClient == nullptr)
		{
			if (gpClientSession->mpRuntime->mStateFlags & engine::ClientSessionStateFlags::kServerDiscovered)
			{
				gpClientSession->mpRuntime->ConnectToDiscoveredServer(engine::kiDefaultPort, NetworkSessionContract::kiCoordinateSlots);
			}
			else
			{
				gpClientSession->ConnectToServer("127.0.0.1");
			}
		}

		if (bCycleMenuIslandPressed && (mGameFlags & engine::GameFlags::kMainMenu))
		{
			miMenuIslandIndex = (miMenuIslandIndex + 1) % std::ssize(engine::gpIslandTerrain->mIslandCrcsByArea);
			auto it = mCoordinateFrames.find(engine::kOriginCoordinate);
			BuildMenuIslandPlacement(miMenuIslandIndex, it->second.staticData.islands);
			// Cycling rewrites the placement list on an existing cell — drop the derived elevation grid
			// and the render-path query cache so RunFrameTick rebuilds both from the new placements
			// next tick.
			it->second.staticData.elevationGrid = {};
			it->second.staticData.islandRenderQueries = {};

			// Pre-mint the texture slot now (mirrors ClientSession::ApplyReceivedStaticData) so the
			// elevation upload and chunk loads are in-flight before UpdateActiveIslands references the
			// slot this same frame. AcquireTextureSlot is idempotent (hot-path early return).
			for (const engine::IslandPlacement& rPlacement : it->second.staticData.islands)
			{
				engine::gpIslandTerrainResidency->AcquireTextureSlot(rPlacement.islandCrc);
			}
		}
	}
}
#endif // BT_CLIENT

#if defined(BT_CLIENT)
void Game::CaptureClientStateIfChanged()
{
	// When no fleet is focused (boot before first sync, or post-disconnect cleared fleets), preserve the remembered fleet/ship —
	// don't overwrite the just-loaded saved state with zeros. The next valid focus (user click or post-sync auto-activate) updates it.
	game::FleetGuid newFleetGuid = mRememberedFleetGuid;
	engine::GlobalId newShipIdentifier = mRememberedFocusedShipIdentifier;
	const Fleet* pFleet = mFleetSelection.FocusedFleet();
	if (pFleet != nullptr)
	{
		newFleetGuid = pFleet->guid;
		newShipIdentifier = mFleetSelection.mFocusedMemberGlobalId;
	}

	float fNewCameraEyeHeightTarget = engine::gpCamera->mfCameraEyeHeightTarget;

	if (newFleetGuid == mRememberedFleetGuid && newShipIdentifier == mRememberedFocusedShipIdentifier
	 && fNewCameraEyeHeightTarget == mfRememberedCameraEyeHeightTarget)
	{
		return;
	}

	mRememberedFleetGuid              = newFleetGuid;
	mRememberedFocusedShipIdentifier          = newShipIdentifier;
	mfRememberedCameraEyeHeightTarget = fNewCameraEyeHeightTarget;
}
#endif // BT_CLIENT

#if defined(BT_CLIENT)
common::crc_t Game::GetNextMusicTrack()
{
	if ((mGameFlags & engine::GameFlags::kMainMenu))
	{
		miMenuMusicIndex = (miMenuMusicIndex + 1) % static_cast<int64_t>(std::size(kMenuMusicPlaylist));
		return kMenuMusicPlaylist[miMenuMusicIndex];
	}
	else
	{
		miGameMusicIndex = (miGameMusicIndex + 1) % static_cast<int64_t>(std::size(kGameMusicPlaylist));
		return kGameMusicPlaylist[miGameMusicIndex];
	}
}
#endif // BT_CLIENT

void Game::InitializeFramePostRender(Frame& rFrame)
{
	rFrame.postRender.uiFrameIdentifier = muiNextFrameId++;
	rFrame.postRender.randomEngine.TimeSeed();
	rFrame.postRender.playerAlignment = mPlayerAlignment;
	rFrame.postRender.enemyAlignment = mEnemyAlignment;
	rFrame.postRender.alignments = mAlignments;
}

void Game::RestoreReplayMetadata(const ReplayMeta& rMetadata)
{
	SetClientGridCoordinate(rMetadata.clientGridCoordinate);
	if (rMetadata.iClientPlayerIdentifierValue != 0)
	{
		engine::GlobalId globalIdentifier {.iValue = rMetadata.iClientPlayerIdentifierValue};
		AddClientPlayer(globalIdentifier, rMetadata.clientGridCoordinate);
	}
	mfPreviousClientArmor = rMetadata.fPreviousClientArmor;
}

} // namespace game
