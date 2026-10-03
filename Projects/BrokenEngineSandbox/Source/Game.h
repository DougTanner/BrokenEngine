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
	engine::GridCoord clientGridCoord {};
	int64_t iClientPlayerIdValue = 0;
	float fPreviousClientArmor = 0.0f;
	uint8_t uiPad[4] {};
};

// Isolated replay metadata: staged by ReadReplayMeta, applied only once the whole replay generation validates.
struct ReplayStagedMeta
{
	ReplayMeta meta {};
};

#if defined(BT_CLIENT)
// The ship ID is part of the key so a pending request clears when focus moves to another ship, even one with the same mode.
struct WeaponModeKey
{
	engine::GlobalId playerId {};
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
	std::filesystem::path QuicksaveFile()
	{
		return std::filesystem::path("ServerQuicksave.save");
	}
#if defined(BT_CLIENT)
	bool ShouldTrapCursor() override;
	bool ShouldUseCrosshair() override;
	bool ShouldShowInGameUi() override;
#endif

	void ChangeFrame(GameFlags_t gameFlags);
	void CreateNewFrame(GameFlags_t gameFlags);

#if defined(BT_CLIENT)
	void ProcessGameMenuInput(const engine::MenuInput& rMenuInput, const engine::InputPoll& rInputPoll) override;

	engine::StandardMenuModel GetStandardMenuModel() const override;
	void ApplyStandardMenuAction(engine::StandardMenuAction eAction) override;
#endif

	// Multi-frame grid
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
	engine::GlobalId ClientPlayerId() const;
	bool IsClientPlayer(engine::GlobalId id) const;
	void AddClientPlayer(engine::GlobalId id, engine::GridCoord coord);
	void RemoveClientPlayer(engine::GlobalId id);
	void RestoreReplayMeta(const ReplayMeta& rMeta);
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
	engine::GlobalId mRememberedFocusedShipId {};
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

	void SetClientGridCoord(engine::GridCoord coord)
	{
		mClientGridCoord = coord;
		miVisibleNeighborCount = 0;
	}

private:

#if defined(BT_CLIENT)
	static constexpr common::crc_t mMenuMusicPlaylist[4] {data::kAudioMusicdoodlewavCrc, data::kAudioMusicMandatoryOvertimewavCrc, data::kAudioMusicsong18wavCrc, data::kAudioMusicTyhosibzzzzwavCrc};
	static constexpr common::crc_t mGameMusicPlaylist[4] {data::kAudioMusicS31UnexpectedTroublewavCrc, data::kAudioMusicS31HighAlertwavCrc, data::kAudioMusicS31OnPatrolwavCrc, data::kAudioMusicS31TheGearsofProgresswavCrc};

	int64_t miMenuMusicIndex = 0;
	int64_t miGameMusicIndex = 0;
#endif

public:
	std::vector<engine::GlobalId> mClientPlayerIds;
	std::vector<engine::GridCoord> mClientPlayerCoords;
	float mfPreviousClientArmor = 0.0f;
	engine::AlignmentIdentifier mPlayerAlignment {};
private:
	engine::AlignmentIdentifier mEnemyAlignment {};
public:
	engine::Alignments mAlignments {};

#if defined(BT_CLIENT)
	void StartMenuMusic()
	{
		miMenuMusicIndex = 0;
		engine::gpAudioManager->PlayMusic(mMenuMusicPlaylist[0]);
	}

	void StartGameMusic()
	{
		miGameMusicIndex = 0;
		engine::gpAudioManager->PlayMusic(mGameMusicPlaylist[0]);
	}
#endif

	void InitFramePostRender(Frame& rFrame);

private:
};

inline Game* gpGame = nullptr;

} // namespace game
