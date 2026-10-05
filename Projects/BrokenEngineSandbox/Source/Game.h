#pragma once

#include "Data/Audio.h"

#include "ClientSettings.h"
#include "Fleet.h"
#include "FleetSelection.h"

#if defined(BT_SERVER)
#include "Network/Server/ServerSession.h"
#include "Save/GameSaveLoad.h"
#endif
#if defined(BT_CLIENT)
#include "Ui/NetworkUiControl.h"

#include "Network/Client/ClientSession.h"
#endif

namespace game
{

#include "Version.h"

#if defined(BT_CLIENT)
inline constexpr std::string_view kGameName = "Broken Engine Sandbox";
#else
inline constexpr std::string_view kGameName = "Broken Engine Sandbox Server";
#endif

struct ReplayMeta
{
	static constexpr int64_t kiVersion = 2;
	engine::GridCoord clientGridCoordinate {};
	int64_t iClientPlayerIdentifierValue = 0;
	float fPreviousClientArmor = 0.0f;
	uint8_t uiPadding[4] {};
};

// Isolated replay metadata: staged by ReadReplayMetadata, applied only once the whole replay generation validates.
struct ReplayStagedMeta
{
	ReplayMeta metadata {};
};

#if defined(BT_CLIENT)
// The ship ID is part of the key so a pending request clears when focus moves to another ship, even one with the same mode.
struct WeaponModeKey
{
	engine::GlobalId playerIdentifier {};
	bool bUseMissiles = false;

	bool operator==(const WeaponModeKey&) const = default;
};
#endif

class Game : public engine::GameBase
{
public:

	Game();
	~Game() override;

	void Reset();
#if defined(BT_CLIENT)
	bool ShouldUseCrosshair() override;
#endif

	void ChangeFrame(GameFlags_t gameFlags);
	void CreateNewFrame(GameFlags_t gameFlags);

#if defined(BT_CLIENT)
	void ProcessGameMenuInput(const engine::MenuInput& rMenuInput, const engine::InputPoll& rInputPoll) override;

	engine::StandardMenuModel GetStandardMenuModel() const override;
	void ApplyStandardMenuAction(engine::StandardMenuAction eAction) override;
#endif

	void ComputeActiveSet();
#if defined(BT_CLIENT)
	void UpdateActiveIslands();
#endif
#if defined(BT_SERVER)
	void EnsureNextFrames();
#endif
	void BuildFrameInputs();
	void HarvestTransfers();

#if defined(BT_SERVER)
	std::unique_ptr<ServerSession> mpServerSession;
	GameSaveLoad mGameSaveLoad;
#endif
#if defined(BT_CLIENT)
	std::unique_ptr<ClientSession> mpClientSession;
#endif

	// Client player tracking (multi-player per client)
	engine::GlobalId ClientPlayerIdentifier() const;
	void AddClientPlayer(engine::GlobalId identifier, engine::GridCoord coordinate);
	void RemoveClientPlayer(engine::GlobalId identifier);
	void RestoreReplayMetadata(const ReplayMeta& rMetadata);
	std::optional<int64_t> ClientPlayerIndex(const PlayersPostRender& rPlayers) const;

#if defined(BT_CLIENT)
	XMVECTOR GetClientPlayerPosition() const;
#endif

#if defined(BT_CLIENT)
	void CaptureClientStateIfChanged();
#endif

#if defined(BT_CLIENT)
	common::crc_t GetNextMusicTrack();
#endif

#if defined(BT_CLIENT)
	XMVECTOR mVecVisualErrorOffset {};
	engine::NetworkUiControl<WeaponModeKey> mWeaponModeToggle {};

	FleetSelection mFleetSelection;

	// In-memory mirror of ClientState.bin; loaded at startup, refreshed whenever any tracked field changes, and written on orderly exit.
	game::FleetGuid mRememberedFleetGuid {};
	engine::GlobalId mRememberedFocusedShipIdentifier {};
	float mfRememberedCameraEyeHeightTarget = engine::Camera::kfCameraEyeHeightInitial;

	static constexpr float kfVisualErrorDecayRate = 15.0f;
	static constexpr float kfVisualErrorMaxDistance = 5.0f;
	static constexpr float kfVisualErrorMinDistance = 0.001f;
#endif

	// Debug-only main-menu island browser index into engine::gpIslandTerrain->mIslandCrcsByArea
	// (largest footprint first); advanced by 'E'.
	int64_t miMenuIslandIndex = 0;

	engine::GridCoord mVisibleNeighbors[8] {};
	int64_t miVisibleNeighborCount = 0;

#if defined(BT_CLIENT)
	// Harness-only viewer pin: set by set_client_grid_coord, cleared by release_client_grid_coord or Reset.
	// Survives server load and replay restart so a recipe can hold one cell subscribed across playback loops.
	bool mbClientGridCoordinatePinned = false;
#endif

	void SetClientGridCoordinate(engine::GridCoord coordinate)
	{
#if defined(BT_CLIENT)
		if (mbClientGridCoordinatePinned)
		{
			return;
		}
#endif
		mClientGridCoordinate = coordinate;
		miVisibleNeighborCount = 0;
	}

#if defined(BT_CLIENT)
	void PinClientGridCoordinate(engine::GridCoord coordinate)
	{
		mbClientGridCoordinatePinned = false;
		SetClientGridCoordinate(coordinate);
		mbClientGridCoordinatePinned = true;
	}
#endif

private:

#if defined(BT_CLIENT)
	static constexpr common::crc_t kMenuMusicPlaylist[4] {data::kAudioMusicdoodlewavCrc, data::kAudioMusicMandatoryOvertimewavCrc, data::kAudioMusicsong18wavCrc, data::kAudioMusicTyhosibzzzzwavCrc};
	static constexpr common::crc_t kGameMusicPlaylist[4] {data::kAudioMusicS31UnexpectedTroublewavCrc, data::kAudioMusicS31HighAlertwavCrc, data::kAudioMusicS31OnPatrolwavCrc, data::kAudioMusicS31TheGearsofProgresswavCrc};

	int64_t miMenuMusicIndex = 0;
	int64_t miGameMusicIndex = 0;
#endif

public:
	std::vector<engine::GlobalId> mClientPlayerIdentifiers;
	std::vector<engine::GridCoord> mClientPlayerCoordinates;
	float mfPreviousClientArmor = 0.0f;
	engine::AlignmentIdentifier mPlayerAlignment {};
	engine::AlignmentIdentifier mEnemyAlignment {};
	engine::Alignments mAlignments {};

#if defined(BT_CLIENT)
	void StartMenuMusic()
	{
		miMenuMusicIndex = 0;
		engine::gpAudioManager->PlayMusic(kMenuMusicPlaylist[0]);
	}

	void StartGameMusic()
	{
		miGameMusicIndex = 0;
		engine::gpAudioManager->PlayMusic(kGameMusicPlaylist[0]);
	}
#endif

	void InitializeFramePostRender(Frame& rFrame);

private:
};

inline Game* gpGame = nullptr;

} // namespace game
