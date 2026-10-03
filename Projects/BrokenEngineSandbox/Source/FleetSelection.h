#pragma once

#if defined(BT_CLIENT)

#include "Ui/NetworkUiControl.h"

#include "Fleet.h"

namespace game
{

// The fleet GUID is part of the key so a pending request clears when focus moves to another fleet, even one with the same delay.
struct NavigationDelayKey
{
	FleetGuid fleetGuid {};
	float fNavigationDelay = 0.0f;

	bool operator==(const NavigationDelayKey&) const = default;
};

// Owns the client's fleet list and focus state (focused fleet + member). Drives which grid cell the
// camera follows. Reaches back into Game (grid coord, client-state persistence, owned-player lists)
// via the gpGame global.
class FleetSelection
{
public:

	void FocusNextFleet();
	void FocusPrevFleet();
	const Fleet* FocusedFleet() const;
	void SelectPlayerInFleet(engine::GlobalId memberGlobalPlayerId);
	void SyncFleets(std::vector<Fleet>&& fleets);
	void AutoSelectFirstAliveMember();
	void Clear();

	engine::NetworkUiControl<int64_t> mCreateFleetToggle {};
	engine::NetworkUiControl<int64_t> mSpawnIntoFleetToggle {};
	engine::NetworkUiControl<int64_t> mDeleteFleetToggle {};
	engine::NetworkUiControl<NavigationDelayKey> mNavigationDelayControl {};

	std::vector<Fleet> mClientFleets;
	int64_t miFocusedFleetIndex = -1;
	engine::GlobalId mFocusedMemberGlobalId {};
};

} // namespace game

#endif // BT_CLIENT
