#pragma once

#if defined(BT_CLIENT)

#include "Fleet.h"
#include "Ui/NetworkUiControl.h"

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
// via the gpGame global; Game holds one instance and forwards its public fleet-navigation API.
class FleetSelection
{
public:

	int64_t FleetCount() const;
	int64_t FocusedFleetIndex() const;
	void FocusNextFleet();
	void FocusPrevFleet();
	bool CanFocusNextFleet() const;
	bool CanFocusPrevFleet() const;
	const Fleet* FocusedFleet() const;
	void SelectPlayerInFleet(int64_t iPlayerIndex);
	int64_t FocusedPlayerInFleetIndex() const;
	void SyncFleets(std::vector<Fleet>&& fleets);
	void AutoSelectFirstAliveMember();
	void Clear();

	engine::NetworkUiControl<int64_t> mCreateFleetToggle {};
	engine::NetworkUiControl<int64_t> mSpawnIntoFleetToggle {};
	engine::NetworkUiControl<int64_t> mDeleteFleetToggle {};
	engine::NetworkUiControl<NavigationDelayKey> mNavigationDelayControl {};

private:

	std::vector<Fleet> mClientFleets;
	int64_t miFocusedFleetIndex = -1;
	int64_t miFocusedPlayerInFleetIndex = -1;
};

} // namespace game

#endif // BT_CLIENT
