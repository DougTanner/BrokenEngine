#include "Game.h"

#include "Input/Input.h"

#include "Frame/Collections/Omnis/Omnis.h"
#include "Ui/Localization.h"

namespace game
{

static constexpr float kfCameraShakeAdd = 0.25f;
static constexpr float kfCameraShakeMax = 1.0f;

Game::Game()
: common::Singleton<Game>(gpGame, this)
{
	InitializeLocalization();

	int64_t iNextAlignment = 1;
	mOmniAlignment = engine::AlignmentIdentifier {iNextAlignment++};
	mEnemyAlignment = engine::AlignmentIdentifier {iNextAlignment++};
	mAlignments.AddAlignment(mOmniAlignment, mEnemyAlignment, engine::AlignmentFlags::kuiEnemies);

#if defined(BT_SERVER)
	mpServerSession = std::make_unique<ServerSession>();
	if (!mServerSaveLoad.Autoload())
	{
		CreateNewFrame();
	}
	meUiState = engine::UiState::kNone;
#else
	mGameFlags.Set(engine::GameFlags::kMainMenu);
	CreateNewFrame();
#endif // BT_SERVER

#if defined(BT_CLIENT)
	mpClientSession = std::make_unique<ClientSession>();
#endif

#if defined(BT_CLIENT)
	StartMenuMusic();
#endif // BT_CLIENT

}

engine::GlobalId Game::ClientOwnedEntityIdentifier() const
{
#if defined(BT_CLIENT)
	const Fleet* pFleet = mFleetSelection.FocusedFleet();
	if (pFleet != nullptr)
	{
		engine::GlobalId focusedMemberGlobalIdentifier = mFleetSelection.mFocusedMemberGlobalId;
		auto it = std::ranges::find(pFleet->members, focusedMemberGlobalIdentifier, &FleetMember::globalId);
		if ((focusedMemberGlobalIdentifier.iValue != 0) && it != pFleet->members.end() && !(it->flags & FleetMemberFlags::kIsDead))
		{
			return it->globalId;
		}
	}
#endif
	return {};
}

void Game::AddClientOwnedEntity(engine::GlobalId identifier, engine::GridCoord coordinate)
{
	// Heap: mClientOwnedEntityIdentifiers / mClientOwnedEntityCoordinates push_back may grow vectors
	ScopedSuppressAllocationTracking suppress;
	mClientOwnedEntityIdentifiers.push_back(identifier);
	mClientOwnedEntityCoordinates.push_back(coordinate);
}

void Game::RemoveClientOwnedEntity(engine::GlobalId identifier)
{
	for (int64_t i = 0; i < std::ssize(mClientOwnedEntityIdentifiers); ++i)
	{
		if (mClientOwnedEntityIdentifiers.at(i) == identifier)
		{
			LOG(kNetwork, kVerbose, "RemoveClientOwnedEntity GlobalId: {} Index: {} OldOwnedEntityCount: {}", identifier, i, std::ssize(mClientOwnedEntityIdentifiers));
			mClientOwnedEntityIdentifiers.erase(mClientOwnedEntityIdentifiers.begin() + i);
			mClientOwnedEntityCoordinates.erase(mClientOwnedEntityCoordinates.begin() + i);
			return;
		}
	}
}

std::optional<int64_t> Game::ClientOwnedEntityIndex(const OmnisPostRender& rOmnis) const
{
	engine::GlobalId focusedIdentifier = ClientOwnedEntityIdentifier();
	if ((focusedIdentifier.iValue != 0))
	{
		for (int64_t i = 0; i < rOmnis.iCount; ++i)
		{
			if (rOmnis.pGlobalIds[i] == focusedIdentifier)
			{
				return i;
			}
		}
	}

	return std::nullopt;
}

#if defined(BT_CLIENT)
bool Game::HasFocusedClientEntity() const
{
	return ClientOwnedEntityIdentifier().iValue != 0;
}

void Game::UpdateCameraShake()
{
	// Camera shake — read most recent ring frame
	auto it = mCells.find(mClientGridCoordinate);
	const Frame* pTailFrame = it != mCells.end() ? it->second.TailFrame() : nullptr;
	if ((ClientOwnedEntityIdentifier().iValue != 0) && pTailFrame != nullptr)
	{
		const Frame& rCurrentFrame = *pTailFrame;
		const OmnisPostRender& rOmnisPostRender = *rCurrentFrame.postRender.pOmnis;

		std::optional<int64_t> optionalOwnedEntityIndex = ClientOwnedEntityIndex(rOmnisPostRender);
		if (optionalOwnedEntityIndex)
		{
			float fCurrentArmor = rOmnisPostRender.pfArmors[*optionalOwnedEntityIndex];
			if (fCurrentArmor < mfPreviousClientArmor)
			{
				engine::gpCamera->mfShake = std::min(engine::gpCamera->mfShake + kfCameraShakeAdd, kfCameraShakeMax);
			}
			mfPreviousClientArmor = fCurrentArmor;
		}
	}
}
#endif // BT_CLIENT

Game::~Game()
{
	if (engine::gpAgentCommandServer != nullptr)
	{
		engine::gpAgentCommandServer->ClearDeferredResponse();
	}
#if defined(BT_CLIENT)
	mpClientSession.reset();
#endif // BT_CLIENT

#if defined(BT_SERVER)
	mpServerSession.reset();
#endif

	if (!(mMenuFlags & engine::MenuFlags::kMouseVisible))
	{
		ShowCursor(TRUE);
	}
}

void Game::ResetGameState()
{
#if defined(BT_CLIENT)
	mWeaponModeToggle.Reset();
#endif // BT_CLIENT
	mClientOwnedEntityIdentifiers.clear();
	mClientOwnedEntityCoordinates.clear();
#if defined(BT_CLIENT)
	mFleetSelection.Clear();
#endif
	mfPreviousClientArmor = 0.0f;
}

#if defined(BT_CLIENT)
engine::StandardMenuModel Game::GetStandardMenuModel() const
{
	return engine::StandardMenuModel
	{
		.pcTitle = "BROKEN ENGINE",
		.features = {engine::StandardMenuFeature::kLocalServer, engine::StandardMenuFeature::kRemoteServer},
		.state = gpClientSession->mpRuntime->GetStandardMenuState(),
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
			ChangeFrameToMainMenu();
			break;
	}
}
#endif // BT_CLIENT

#if defined(BT_CLIENT)
void Game::ProcessGameMenuInput(const engine::InputPoll& rInputPoll)
{
	if constexpr (kbDebugInput)
	{
		bool bReturnPressed = rInputPoll.KeyboardPressed(VK_RETURN);
		bool bCycleMenuIslandPressed = rInputPoll.KeyboardPressed('E');

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
			auto it = mCells.find(engine::kOriginCoordinate);
			BuildMenuIslandPlacement(miMenuIslandIndex, it->second.staticData.islands);
			// Cycling rewrites the placement list on an existing cell — drop the derived elevation grid
			// and the render-path query cache so RunFrameTick rebuilds both from the new placements
			// next tick.
			it->second.staticData.elevationGrid = {};
			it->second.staticData.islandRenderQueries = {};

			// Pre-mint the texture slot now (mirrors ClientSessionBase::ApplyReceivedStaticData) so the
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
void Game::CaptureClientState()
{
	// When no fleet is focused (boot before first sync, or post-disconnect cleared fleets), preserve the remembered fleet/ship —
	// don't overwrite the just-loaded saved state with zeros. The next valid focus (user click or post-sync auto-activate) updates it.
	const Fleet* pFleet = mFleetSelection.FocusedFleet();
	if (pFleet != nullptr)
	{
		mRememberedFleetGuid = pFleet->guid;
		mRememberedFocusedShipIdentifier = mFleetSelection.mFocusedMemberGlobalId;
	}
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
	rFrame.postRender.omniAlignment = mOmniAlignment;
	rFrame.postRender.enemyAlignment = mEnemyAlignment;
	rFrame.postRender.alignments = mAlignments;
}

void Game::RestoreReplayMetadata(const ReplayMeta& rMetadata)
{
	SetClientGridCoordinate(rMetadata.clientGridCoordinate);
	if (rMetadata.iClientOwnedEntityIdentifierValue != 0)
	{
		engine::GlobalId globalIdentifier {.iValue = rMetadata.iClientOwnedEntityIdentifierValue};
		AddClientOwnedEntity(globalIdentifier, rMetadata.clientGridCoordinate);
	}
	mfPreviousClientArmor = rMetadata.fPreviousClientArmor;
}

} // namespace game
