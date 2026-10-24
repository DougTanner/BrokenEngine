#pragma once

#include "Data/Audio.h"

#include "ClientSettings.h"
#include "Fleet.h"
#include "FleetSelection.h"

#if defined(BT_SERVER)
#include "Network/Server/ServerFleetSerialization.h"
#include "Network/Server/ServerSession.h"
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
	static constexpr int64_t kiVersion = 3;
	engine::GridCoord clientGridCoordinate {};
	int64_t iClientOwnedEntityIdentifierValue = 0;
	float fPreviousClientArmor = 0.0f;
	uint8_t puiPadding[4] {};
};

#if defined(BT_SERVER)
// Isolated replay metadata: staged by ReadReplayMetadata, applied only once the whole replay generation validates.
struct ReplayStagedMeta
{
	ReplayMeta metadata {};
	std::vector<ReplayFleetRecord> fleetRecords;
	int64_t iFirstFleetChecksumTick = 0;
	std::vector<common::crc_t> fleetChecksums;
};
#endif

#if defined(BT_CLIENT)
// The ship ID is part of the key so a pending request clears when focus moves to another ship, even one with the same mode.
struct WeaponModeKey
{
	engine::GlobalId ownedEntityIdentifier {};
	bool bUseMissiles = false;

	bool operator==(const WeaponModeKey&) const = default;
};
#endif

class Game : public engine::GameBase, public common::Singleton<Game>
{
public:

	Game();
	~Game();

	void ResetGameState();

#if defined(BT_CLIENT)
	void ProcessGameMenuInput(const engine::InputPoll& rInputPoll);

	// The standard menu screens read this every render pass, and again after any action that can
	// change it, because an action's effect must be visible to later decisions in the same pass.
	engine::StandardMenuModel GetStandardMenuModel() const;
	void ApplyStandardMenuAction(engine::StandardMenuAction eAction);

	bool HasFocusedClientEntity() const;
	void UpdateCameraShake();
#endif

#if defined(BT_SERVER)
	std::unique_ptr<ServerSession> mpServerSession;
#endif
#if defined(BT_CLIENT)
	std::unique_ptr<ClientSession> mpClientSession;
#endif

	// Client owned-entity tracking (several per client)
	engine::GlobalId ClientOwnedEntityIdentifier() const;
	void AddClientOwnedEntity(engine::GlobalId identifier, engine::GridCoord coordinate);
	void RemoveClientOwnedEntity(engine::GlobalId identifier);
	void RestoreReplayMetadata(const ReplayMeta& rMetadata);
	std::optional<ShipLocator> ClientOwnedEntity(const Frame& rFrame) const;

#if defined(BT_CLIENT)
	void CaptureClientState();
#endif

#if defined(BT_CLIENT)
	common::crc_t GetNextMusicTrack();
#endif

#if defined(BT_CLIENT)
	engine::NetworkUiControl<WeaponModeKey> mWeaponModeToggle {};

	FleetSelection mFleetSelection;

	// In-memory mirror of the focused fleet and ship in ClientState.bin; loaded at startup, refreshed on every focus change, and written on orderly exit.
	game::FleetGuid mRememberedFleetGuid {};
	engine::GlobalId mRememberedFocusedShipIdentifier {};
#endif

private:

#if defined(BT_CLIENT)
	static constexpr common::crc_t kpMenuMusicPlaylist[4] {data::kAudioMusicdoodlewavCrc, data::kAudioMusicMandatoryOvertimewavCrc, data::kAudioMusicsong18wavCrc, data::kAudioMusicTyhosibzzzzwavCrc};
	static constexpr common::crc_t kpGameMusicPlaylist[4] {data::kAudioMusicS31UnexpectedTroublewavCrc, data::kAudioMusicS31HighAlertwavCrc, data::kAudioMusicS31OnPatrolwavCrc, data::kAudioMusicS31TheGearsofProgresswavCrc};

	int64_t miMenuMusicIndex = 0;
	int64_t miGameMusicIndex = 0;
#endif

public:
	std::vector<engine::GlobalId> mClientOwnedEntityIdentifiers;
	std::vector<engine::GridCoord> mClientOwnedEntityCoordinates;
	float mfPreviousClientArmor = 0.0f;
	engine::AlignmentIdentifier mFleetAlignment {};
	engine::AlignmentIdentifier mRandomAlignment {};
	engine::Alignments mAlignments {};

#if defined(BT_CLIENT)
	void StartMenuMusic()
	{
		miMenuMusicIndex = 0;
		engine::gpAudioManager->PlayMusic(kpMenuMusicPlaylist[0]);
	}

	void StartGameMusic()
	{
		miGameMusicIndex = 0;
		engine::gpAudioManager->PlayMusic(kpGameMusicPlaylist[0]);
	}
#endif

	void InitializeFramePostRender(Frame& rFrame);

private:
};

inline Game* gpGame = nullptr;

} // namespace game
