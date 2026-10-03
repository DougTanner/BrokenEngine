#include "FleetSelection.h"

#if defined(BT_CLIENT)

#include "Game.h"

namespace game
{

void FleetSelection::AutoSelectFirstAliveMember()
{
	mFocusedMemberGlobalId = {};
	gpGame->SetClientGridCoord({});
	const Fleet* pFleet = FocusedFleet();
	if (pFleet != nullptr)
	{
		for (int64_t i = 0; i < std::ssize(pFleet->members); ++i)
		{
			if (!(pFleet->members.at(static_cast<size_t>(i)).flags & FleetMemberFlags::kIsDead))
			{
				SelectPlayerInFleet(pFleet->members.at(static_cast<size_t>(i)).globalPlayerId);
				return;
			}
		}
	}
}

void FleetSelection::FocusNextFleet()
{
	if (miFocusedFleetIndex < std::ssize(mClientFleets) - 1)
	{
		++miFocusedFleetIndex;
		AutoSelectFirstAliveMember();
		gpGame->CaptureClientStateIfChanged();
	}
}

void FleetSelection::FocusPrevFleet()
{
	if (miFocusedFleetIndex > 0)
	{
		--miFocusedFleetIndex;
		AutoSelectFirstAliveMember();
		gpGame->CaptureClientStateIfChanged();
	}
}

const Fleet* FleetSelection::FocusedFleet() const
{
	if (miFocusedFleetIndex >= 0 && miFocusedFleetIndex < std::ssize(mClientFleets))
	{
		return &mClientFleets.at(static_cast<size_t>(miFocusedFleetIndex));
	}
	return nullptr;
}

void FleetSelection::SelectPlayerInFleet(engine::GlobalId memberGlobalPlayerId)
{
	const Fleet* pFleet = FocusedFleet();
	if (pFleet == nullptr)
	{
		return;
	}

	if (!(memberGlobalPlayerId.iValue != 0))
	{
		return;
	}
	auto memberIt = std::ranges::find(pFleet->members, memberGlobalPlayerId, &FleetMember::globalPlayerId);
	if (memberIt == pFleet->members.end())
	{
		return;
	}

	mFocusedMemberGlobalId = memberGlobalPlayerId;
	gpGame->mWeaponModeToggle.Reset();

	// Update mClientGridCoord to match selected player's coord
	const FleetMember& rMember = *memberIt;
	if (!(rMember.flags & FleetMemberFlags::kIsDead))
	{
		for (int64_t i = 0; i < std::ssize(gpGame->mClientPlayerIds); ++i)
		{
			if (gpGame->mClientPlayerIds.at(i) == rMember.globalPlayerId)
			{
				gpGame->SetClientGridCoord(gpGame->mClientPlayerCoords.at(i));
				break;
			}
		}
	}

	gpGame->CaptureClientStateIfChanged();
}

void FleetSelection::SyncFleets(std::vector<Fleet>&& fleets)
{
	// Heap: mClientFleets rebuild + LOG argument formatting allocations
	ScopedSuppressAllocationTracking suppress;

	LOG(kNetwork, kVerbose, "SyncFleets Fleets: {} Members: {} FocusedFleet: {} FocusedMember: {}", std::ssize(fleets), !fleets.empty() ? std::ssize(fleets.at(0).members) : 0, miFocusedFleetIndex, mFocusedMemberGlobalId.iValue);

	int64_t iPrevFleetCount = std::ssize(mClientFleets);
	int64_t iPrevFocusedFleetMemberCount = 0;
	FleetGuid prevFocusedFleetGuid {};
	if (miFocusedFleetIndex >= 0 && miFocusedFleetIndex < iPrevFleetCount)
	{
		const Fleet& rPrevFocusedFleet = mClientFleets.at(static_cast<size_t>(miFocusedFleetIndex));
		iPrevFocusedFleetMemberCount = std::ssize(rPrevFocusedFleet.members);
		prevFocusedFleetGuid = rPrevFocusedFleet.guid;
	}
	mClientFleets = std::move(fleets);

	auto FindFleetIndexByGuid = [this](const FleetGuid& rGuid) -> int64_t
	{
		if ((rGuid.uiHigh == 0 && rGuid.uiLow == 0))
		{
			return -1;
		}
		for (int64_t i = 0; i < std::ssize(mClientFleets); ++i)
		{
			if (mClientFleets.at(static_cast<size_t>(i)).guid == rGuid)
			{
				return i;
			}
		}
		return -1;
	};

	// Restore from disk-persisted client state on the first sync after a reconnect-style clear.
	// The remembered FleetGuid identifies which fleet to focus; a missing or destroyed ship falls back to the fleet's current flagship.
	bool bRestoredRemembered = false;
	if (iPrevFleetCount == 0)
	{
		int64_t iRememberedFleetIndex = FindFleetIndexByGuid(gpGame->mRememberedFleetGuid);
		if (iRememberedFleetIndex >= 0)
		{
			const Fleet& rFleet = mClientFleets.at(static_cast<size_t>(iRememberedFleetIndex));
			miFocusedFleetIndex = iRememberedFleetIndex;
			mFocusedMemberGlobalId = {};
			if ((gpGame->mRememberedFocusedShipId.iValue != 0))
			{
				for (int64_t j = 0; j < std::ssize(rFleet.members); ++j)
				{
					const FleetMember& rMember = rFleet.members.at(static_cast<size_t>(j));
					if (rMember.globalPlayerId == gpGame->mRememberedFocusedShipId && !(rMember.flags & FleetMemberFlags::kIsDead))
					{
						mFocusedMemberGlobalId = rMember.globalPlayerId;
						break;
					}
				}
			}
			// The server's fleet invariant guarantees the flagship names a member, or is {} for an empty fleet.
			if (!(mFocusedMemberGlobalId.iValue != 0))
			{
				mFocusedMemberGlobalId = rFleet.flagshipGlobalPlayerId;
			}
			// Suppress the auto-newest-fleet / auto-newest-member branches below.
			iPrevFocusedFleetMemberCount = std::ssize(rFleet.members);
			bRestoredRemembered = true;
		}
	}

	if (!bRestoredRemembered)
	{
		// Re-anchor by identity before clamping: the server can erase a fleet from the middle of the vector,
		// so an index still in range would otherwise silently address a different fleet.
		int64_t iReanchoredFleetIndex = FindFleetIndexByGuid(prevFocusedFleetGuid);
		if (iReanchoredFleetIndex >= 0)
		{
			miFocusedFleetIndex = iReanchoredFleetIndex;
		}

		// Clamp fleet index
		if (miFocusedFleetIndex >= std::ssize(mClientFleets))
		{
			miFocusedFleetIndex = std::ssize(mClientFleets) - 1;
		}

		// Auto-activate newly created fleet
		if (iPrevFleetCount < std::ssize(mClientFleets))
		{
			miFocusedFleetIndex = std::ssize(mClientFleets) - 1;
			mFocusedMemberGlobalId = {};
			iPrevFocusedFleetMemberCount = 0;
		}
	}

	// Drop a focused member the focused fleet no longer has, or auto-select one
	const Fleet* pFleet = FocusedFleet();
	if (pFleet != nullptr)
	{
		if (!std::ranges::contains(pFleet->members, mFocusedMemberGlobalId, &FleetMember::globalPlayerId))
		{
			mFocusedMemberGlobalId = {};
		}

		// Auto-focus newly added member (fleet member count grew)
		if (std::ssize(pFleet->members) > iPrevFocusedFleetMemberCount)
		{
			mFocusedMemberGlobalId = pFleet->members.back().globalPlayerId;
		}
		else if (!(mFocusedMemberGlobalId.iValue != 0) && !pFleet->members.empty())
		{
			mFocusedMemberGlobalId = pFleet->members.back().globalPlayerId;
		}

		// If focused member is dead, auto-fallback to first alive member
		auto focusedIt = std::ranges::find(pFleet->members, mFocusedMemberGlobalId, &FleetMember::globalPlayerId);
		if ((mFocusedMemberGlobalId.iValue != 0) && focusedIt != pFleet->members.end() && (focusedIt->flags & FleetMemberFlags::kIsDead))
		{
			mFocusedMemberGlobalId = {};
			for (int64_t i = 0; i < std::ssize(pFleet->members); ++i)
			{
				if (!(pFleet->members.at(static_cast<size_t>(i)).flags & FleetMemberFlags::kIsDead))
				{
					mFocusedMemberGlobalId = pFleet->members.at(static_cast<size_t>(i)).globalPlayerId;
					break;
				}
			}
		}
	}
	else
	{
		mFocusedMemberGlobalId = {};
		mSpawnIntoFleetToggle.Reset();
	}

	// Update mClientGridCoord based on current selection
	engine::GlobalId focusedId = gpGame->ClientPlayerId();
	bool bGridCoordResolved = false;
	if ((focusedId.iValue != 0))
	{
		for (int64_t i = 0; i < std::ssize(gpGame->mClientPlayerIds); ++i)
		{
			if (gpGame->mClientPlayerIds.at(i) == focusedId)
			{
				gpGame->SetClientGridCoord(gpGame->mClientPlayerCoords.at(i));
				bGridCoordResolved = true;
				break;
			}
		}
	}

	// No valid selection — camera to origin
	if (!bGridCoordResolved)
	{
		gpGame->SetClientGridCoord({});
	}

	// Capture whatever final focus state SyncFleets settled on (covers server-driven changes the user didn't trigger directly).
	gpGame->CaptureClientStateIfChanged();
}

void FleetSelection::Clear()
{
	mClientFleets.clear();
	miFocusedFleetIndex = -1;
	mFocusedMemberGlobalId = {};
	mCreateFleetToggle.Reset();
	mSpawnIntoFleetToggle.Reset();
	mDeleteFleetToggle.Reset();
	mNavigationDelayControl.Reset();
}

} // namespace game

#endif // BT_CLIENT
