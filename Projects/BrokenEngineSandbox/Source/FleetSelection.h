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

// Fleet focus controls the camera cell; gpGame provides owned-player lists and client-state persistence.
class FleetSelection
{
public:

	void FocusNextFleet();
	void FocusPreviousFleet();
	const Fleet* FocusedFleet() const;
	void SelectPlayerInFleet(engine::GlobalId memberGlobalPlayerId);
	void SynchronizeFleets(std::vector<Fleet>&& rFleets);
	void AutoSelectFirstAliveMember();
	void Clear();

	engine::NetworkUiControl<int64_t> mCreateFleetToggle;
	engine::NetworkUiControl<int64_t> mSpawnIntoFleetToggle;
	engine::NetworkUiControl<int64_t> mDeleteFleetToggle;
	engine::NetworkUiControl<NavigationDelayKey> mNavigationDelayControl;

	std::vector<Fleet> mClientFleets;
	int64_t miFocusedFleetIndex = -1;
	engine::GlobalId mFocusedMemberGlobalId {};
};

} // namespace game

#endif // BT_CLIENT
