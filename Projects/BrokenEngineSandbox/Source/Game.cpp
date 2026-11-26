#include "Game.h"

#include "Input/Input.h"

#include "Frame/Collections/Ships/ShipHitboxes.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Graphics/Camera.h"
#include "Input/GameKeybinds.h"
#include "Ui/Localization.h"
#include "Ui/MapWrappers.h"

namespace game
{

static constexpr float kfCameraShakeAdd = 0.25f;
static constexpr float kfCameraShakeMax = 1.0f;

Game::Game()
: common::Singleton<Game>(gpGame, this)
{
	InitializeLocalization();
	InitializeShipHitboxes();

	int64_t iNextAlignment = 1;
	mFleetAlignment = engine::AlignmentIdentifier(iNextAlignment++);
	mRandomAlignment = engine::AlignmentIdentifier(iNextAlignment++);
	mAlignments.AddAlignment(mFleetAlignment, mRandomAlignment, engine::AlignmentFlags::kuiEnemies);

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

#if defined(BT_CLIENT)
const engine::OwnedEntity* Game::FindLiveFlagshipOwnedEntity() const
{
	const Fleet* pFleet = mFleetSelection.FocusedFleet();
	if (pFleet == nullptr)
	{
		return nullptr;
	}
	auto memberIt = std::ranges::find(pFleet->members, pFleet->flagshipGlobalId, &FleetMember::globalId);
	if (memberIt->flags & FleetMemberFlags::kIsDead)
	{
		return nullptr;
	}
	auto ownedIt = std::ranges::find(mClientOwnedEntities, pFleet->flagshipGlobalId, &engine::OwnedEntity::globalId);
	return ownedIt != mClientOwnedEntities.end() ? &*ownedIt : nullptr;
}

engine::GlobalId Game::ClientOwnedEntityIdentifier() const
{
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
	return {};
}

engine::GlobalId Game::ClientFlagshipIdentifier() const
{
	const engine::OwnedEntity* pFlagship = FindLiveFlagshipOwnedEntity();
	return pFlagship != nullptr ? pFlagship->globalId : engine::GlobalId {};
}

// The client owned-entity list (mClientOwnedEntities) holds only engine global IDs and grid coordinates, so it is
// game-agnostic.
void Game::AddClientOwnedEntity(engine::GlobalId identifier, engine::GridCoord coordinate)
{
	// Heap: mClientOwnedEntities push_back may grow the vector
	ScopedSuppressAllocationTracking suppress;
	mClientOwnedEntities.push_back(engine::OwnedEntity {.globalId = identifier, .coord = coordinate});
}

void Game::RemoveClientOwnedEntity(engine::GlobalId identifier)
{
	auto it = std::ranges::find(mClientOwnedEntities, identifier, &engine::OwnedEntity::globalId);
	if (it != mClientOwnedEntities.end())
	{
		LOG(kNetwork, kVerbose, "RemoveClientOwnedEntity GlobalId: {} Index: {} OldOwnedEntityCount: {}", identifier, it - mClientOwnedEntities.begin(), std::ssize(mClientOwnedEntities));
		mClientOwnedEntities.erase(it);
	}
}

std::optional<ShipLocator> Game::ClientOwnedEntity(const Frame& rFrame) const
{
	return FindShipByGlobalId(rFrame, ClientOwnedEntityIdentifier());
}

bool Game::HasFocusedClientEntity() const
{
	return ClientFlagshipIdentifier().iValue != 0;
}

void Game::UpdateCameraShake()
{
	engine::GlobalId focusedGlobalIdentifier = ClientOwnedEntityIdentifier();
	if (focusedGlobalIdentifier.iValue == 0)
	{
		return;
	}

	// Read the newest ring frame in the focused entity's recorded cell, using the client cell when the ownership record is absent.
	engine::GridCoord shakeCoordinate = mClientGridCoordinate;
	auto ownedIt = std::ranges::find(mClientOwnedEntities, focusedGlobalIdentifier, &engine::OwnedEntity::globalId);
	if (ownedIt != mClientOwnedEntities.end())
	{
		shakeCoordinate = ownedIt->coord;
	}

	auto it = mCells.find(shakeCoordinate);
	const Frame* pTailFrame = it != mCells.end() ? it->second.TailFrame() : nullptr;
	if (pTailFrame != nullptr)
	{
		const Frame& rCurrentFrame = *pTailFrame;

		std::optional<ShipLocator> optionalOwnedEntity = FindShipByGlobalId(rCurrentFrame, focusedGlobalIdentifier);
		if (optionalOwnedEntity)
		{
			float fCurrentArmor = ShipHull(rCurrentFrame, *optionalOwnedEntity);
			if (fCurrentArmor < mfPreviousClientArmor)
			{
				engine::gpCamera->mfShake = std::min(engine::gpCamera->mfShake + kfCameraShakeAdd, kfCameraShakeMax);
			}
			mfPreviousClientArmor = fCurrentArmor;
		}
	}
}

bool Game::MapOpen() const
{
	return engine::gpCamera->mbFramingOverride && !engine::gpCamera->mbFramingOverrideReleasing;
}

void Game::ToggleMap()
{
	// gMapFraming and gMapBlendSeconds are read only here, so a slider change to either takes effect at the next toggle.
	if (!MapOpen())
	{
		engine::gpCamera->SetFramingOverride(gMapFraming.mfCurrent, gMapBlendSeconds.mfCurrent);
	}
	else
	{
		engine::gpCamera->ReleaseFramingOverride(gMapBlendSeconds.mfCurrent);
	}
}

float Game::MapOpacity() const
{
	if (!engine::gpCamera->mbFramingOverride)
	{
		return 0.0f;
	}

	return std::clamp(std::log(engine::gpCamera->RenderFraming() / engine::CameraBase::kfFramingMaximum) / std::log(gMapOpaqueFraming.mfCurrent / engine::CameraBase::kfFramingMaximum), 0.0f, 1.0f);
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
}

void Game::ResetGameState()
{
#if defined(BT_CLIENT)
	mClientOwnedEntities.clear();
	mFleetSelection.Clear();
	mMapFleetMarkers.clear();
	// Close the map with no ease, so no world-hidden frame runs over the menu.
	engine::gpCamera->mbFramingOverride = false;
	static_cast<Camera&>(*engine::gpCamera).ClearSubscriptionAnchor();
	mfPreviousClientArmor = 0.0f;
#endif
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
#endif // BT_CLIENT

#if defined(BT_CLIENT)
void Game::ProcessGameMenuInput(const engine::InputPoll& rInputPoll)
{
	if (rInputPoll.Pressed(GameKeyAction::kToggleMap))
	{
		bool bToggleMap = !(mGameFlags & engine::GameFlags::kMainMenu) && meUiState == engine::UiState::kNone;
		if constexpr (kbDebugInput)
		{
			bToggleMap |= (mGameFlags & engine::GameFlags::kMainMenu) && meUiState == engine::UiState::kPause;
		}
		if (bToggleMap)
		{
			ToggleMap();
		}
	}

	if (rInputPoll.KeyboardPressed(VK_OEM_3) && !(mGameFlags & engine::GameFlags::kMainMenu) && meUiState == engine::UiState::kNone && !MapOpen())
	{
		engine::gpCamera->SetObserverMode(engine::gpCamera->meObserverState != engine::CameraBase::ObserverState::kObserving);
	}

	if constexpr (kbFreeCamera)
	{
		if (rInputPoll.KeyboardPressed(VK_BACK) && (mGameFlags & engine::GameFlags::kMainMenu))
		{
			Camera& rCamera = static_cast<Camera&>(*engine::gpCamera);
			rCamera.ResetMenuPose();
			rCamera.ApplyPose(true);
		}
	}

	if constexpr (kbDebugInput)
	{
		bool bReturnPressed = rInputPoll.KeyboardPressed(VK_RETURN);
		bool bCycleMenuIslandPressed = rInputPoll.KeyboardPressed('E');

		if (bReturnPressed && (mGameFlags & engine::GameFlags::kMainMenu) && gpClientSession->mpRuntime->mpClient == nullptr)
		{
			if (gpClientSession->mpRuntime->mStateFlags & engine::ClientSessionStateFlags::kServerDiscovered)
			{
				gpClientSession->mpRuntime->ConnectToDiscoveredServer(engine::kiDefaultPort);
			}
			else
			{
				gpClientSession->mpRuntime->Connect("127.0.0.1", engine::kiDefaultPort);
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

			// Pre-mint the texture slot now (mirrors ClientSessionRuntime::StartTerrainPreparation) so the
			// chunk loads, and the elevation upload request on a distinct transfer queue, are in-flight
			// before UpdateActiveIslands references the slot this same frame. AcquireTextureSlot is
			// idempotent (hot-path early return).
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
		// A different focused member's hull is unrelated to the stored one, so the next comparison adds no shake.
		if (mFleetSelection.mFocusedMemberGlobalId != mRememberedFocusedShipIdentifier)
		{
			mfPreviousClientArmor = 0.0f;
		}
		mRememberedFocusedShipIdentifier = mFleetSelection.mFocusedMemberGlobalId;
	}
}
#endif // BT_CLIENT

#if defined(BT_CLIENT)
common::crc_t Game::GetNextMusicTrack()
{
	if ((mGameFlags & engine::GameFlags::kMainMenu))
	{
		miMenuMusicIndex = (miMenuMusicIndex + 1) % static_cast<int64_t>(std::size(kpMenuMusicPlaylist));
		return kpMenuMusicPlaylist[miMenuMusicIndex];
	}
	else
	{
		miGameMusicIndex = (miGameMusicIndex + 1) % static_cast<int64_t>(std::size(kpGameMusicPlaylist));
		return kpGameMusicPlaylist[miGameMusicIndex];
	}
}
#endif // BT_CLIENT

void Game::InitializeFramePostRender(Frame& rFrame)
{
	rFrame.postRender.fleetAlignment = mFleetAlignment;
	rFrame.postRender.randomAlignment = mRandomAlignment;
	rFrame.postRender.alignments = mAlignments;
}

void Game::RestoreReplayMetadata(const ReplayMeta& rMetadata)
{
	SetClientGridCoordinate(rMetadata.clientGridCoordinate);
}

} // namespace game
