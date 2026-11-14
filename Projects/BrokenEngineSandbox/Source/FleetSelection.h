#pragma once

#if defined(BT_CLIENT)

#include "Ui/NetworkUiControl.h"

#include "Fleet.h"

namespace game
{

// The fleet GUID is part of the key so a pending request clears when focus moves to another fleet, even one with the same
// navigation settings.
struct FleetNavigationKey
{
	FleetGuid fleetGuid {};
	float fNavigationDelay = 0.0f;
	FleetBehaviour eBehaviour = FleetBehaviour::kRandomWalk;
	engine::GridCoord holdCoordinate {};

	bool operator==(const FleetNavigationKey&) const = default;
};

// Fleet focus controls the camera cell; gpGame provides owned-entity lists and client-state persistence.
class FleetSelection
{
public:

	void FocusNextFleet();
	void FocusPreviousFleet();
	const Fleet* FocusedFleet() const;
	void SelectOwnedEntityInFleet(engine::GlobalId memberGlobalId);
	void SynchronizeFleets(std::vector<Fleet>&& rFleets);
	void AutoSelectFirstAliveMember();
	void Clear();

	engine::NetworkUiControl<int64_t> mCreateFleetToggle;
	engine::NetworkUiControl<int64_t> mSpawnIntoFleetToggle;
	// Shared by the HUD's nav-delay slider, behaviour selector, and hold button and by the map hold pick.
	engine::NetworkUiControl<FleetNavigationKey> mFleetNavigationControl;
	engine::NetworkUiControl<int64_t> mDestroyMemberToggle;
	engine::NetworkUiControl<int64_t> mSquadronOrdersToggle;
	// The fleet whose hold hex the open map picks; zero when no pick is active.
	FleetGuid mHoldPickFleetGuid {};

	std::vector<Fleet> mClientFleets;
	// Advances on every SynchronizeFleets call and never resets; the spawn-into-fleet, destroy-member, and squadron-orders
	// toggles key on it.
	int64_t miAppliedFleetSyncCount = 0;
	int64_t miFocusedFleetIndex = -1;
	engine::GlobalId mFocusedMemberGlobalId {};
	engine::GlobalId mPendingFocusMemberGlobalId {};
	// Set by the agent reveal_ui command; Clear leaves it so a reveal outlasts game reset and server load.
	bool mbRevealFleetPanel = false;
};

} // namespace game

#endif // BT_CLIENT
