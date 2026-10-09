#include "FleetSelection.h"

#if defined(BT_CLIENT)

#include "Game.h"

namespace game
{

void FleetSelection::AutoSelectFirstAliveMember()
{
	mFocusedMemberGlobalId = {};
	gpGame->SetClientGridCoordinate({});
	const Fleet* pFleet = FocusedFleet();
	if (pFleet != nullptr)
	{
		for (const FleetMember& rMember : pFleet->members)
		{
			if (!(rMember.flags & FleetMemberFlags::kIsDead))
			{
				SelectPlayerInFleet(rMember.globalPlayerId);
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
		gpGame->CaptureClientState();
	}
}

void FleetSelection::FocusPreviousFleet()
{
	if (miFocusedFleetIndex > 0)
	{
		--miFocusedFleetIndex;
		AutoSelectFirstAliveMember();
		gpGame->CaptureClientState();
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

	const FleetMember& rMember = *memberIt;
	if (!(rMember.flags & FleetMemberFlags::kIsDead))
	{
		for (int64_t i = 0; i < std::ssize(gpGame->mClientPlayerIdentifiers); ++i)
		{
			if (gpGame->mClientPlayerIdentifiers.at(i) == rMember.globalPlayerId)
			{
				gpGame->SetClientGridCoordinate(gpGame->mClientPlayerCoordinates.at(i));
				break;
			}
		}
	}

	gpGame->CaptureClientState();
}

void FleetSelection::SynchronizeFleets(std::vector<Fleet>&& rFleets)
{
	// Heap: LOG argument formatting may allocate.
	ScopedSuppressAllocationTracking suppress;

	LOG(kNetwork, kVerbose, "SyncFleets Fleets: {} Members: {} FocusedFleet: {} FocusedMember: {}", std::ssize(rFleets), !rFleets.empty() ? std::ssize(rFleets.at(0).members) : 0, miFocusedFleetIndex, mFocusedMemberGlobalId.iValue);

	int64_t iPreviousFleetCount = std::ssize(mClientFleets);
	int64_t iPreviousFocusedFleetMemberCount = 0;
	FleetGuid previousFocusedFleetGuid {};
	if (miFocusedFleetIndex >= 0 && miFocusedFleetIndex < iPreviousFleetCount)
	{
		const Fleet& rPreviousFocusedFleet = mClientFleets.at(static_cast<size_t>(miFocusedFleetIndex));
		iPreviousFocusedFleetMemberCount = std::ssize(rPreviousFocusedFleet.members);
		previousFocusedFleetGuid = rPreviousFocusedFleet.guid;
	}
	mClientFleets = std::move(rFleets);

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
	if (iPreviousFleetCount == 0)
	{
		int64_t iRememberedFleetIndex = FindFleetIndexByGuid(gpGame->mRememberedFleetGuid);
		if (iRememberedFleetIndex >= 0)
		{
			const Fleet& rFleet = mClientFleets.at(static_cast<size_t>(iRememberedFleetIndex));
			miFocusedFleetIndex = iRememberedFleetIndex;
			mFocusedMemberGlobalId = {};
			if ((gpGame->mRememberedFocusedShipIdentifier.iValue != 0))
			{
				for (const FleetMember& rMember : rFleet.members)
				{
					if (rMember.globalPlayerId == gpGame->mRememberedFocusedShipIdentifier && !(rMember.flags & FleetMemberFlags::kIsDead))
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
			iPreviousFocusedFleetMemberCount = std::ssize(rFleet.members);
			bRestoredRemembered = true;
		}
	}

	if (!bRestoredRemembered)
	{
		// Re-anchor by identity before clamping: the server can erase a fleet from the middle of the vector,
		// so an index still in range would otherwise silently address a different fleet.
		int64_t iReanchoredFleetIndex = FindFleetIndexByGuid(previousFocusedFleetGuid);
		if (iReanchoredFleetIndex >= 0)
		{
			miFocusedFleetIndex = iReanchoredFleetIndex;
		}

		if (miFocusedFleetIndex >= std::ssize(mClientFleets))
		{
			miFocusedFleetIndex = std::ssize(mClientFleets) - 1;
		}

		if (iPreviousFleetCount < std::ssize(mClientFleets))
		{
			miFocusedFleetIndex = std::ssize(mClientFleets) - 1;
			mFocusedMemberGlobalId = {};
			iPreviousFocusedFleetMemberCount = 0;
		}
	}

	const Fleet* pFleet = FocusedFleet();
	if (pFleet != nullptr)
	{
		if (!std::ranges::contains(pFleet->members, mFocusedMemberGlobalId, &FleetMember::globalPlayerId))
		{
			mFocusedMemberGlobalId = {};
		}

		if (std::ssize(pFleet->members) > iPreviousFocusedFleetMemberCount)
		{
			mFocusedMemberGlobalId = pFleet->members.back().globalPlayerId;
		}
		else if (!(mFocusedMemberGlobalId.iValue != 0) && !pFleet->members.empty())
		{
			mFocusedMemberGlobalId = pFleet->members.back().globalPlayerId;
		}

		auto focusedIt = std::ranges::find(pFleet->members, mFocusedMemberGlobalId, &FleetMember::globalPlayerId);
		if ((mFocusedMemberGlobalId.iValue != 0) && (focusedIt->flags & FleetMemberFlags::kIsDead))
		{
			mFocusedMemberGlobalId = {};
			for (const FleetMember& rMember : pFleet->members)
			{
				if (!(rMember.flags & FleetMemberFlags::kIsDead))
				{
					mFocusedMemberGlobalId = rMember.globalPlayerId;
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

	engine::GlobalId focusedId = gpGame->ClientPlayerIdentifier();
	bool bGridCoordinateResolved = false;
	if ((focusedId.iValue != 0))
	{
		for (int64_t i = 0; i < std::ssize(gpGame->mClientPlayerIdentifiers); ++i)
		{
			if (gpGame->mClientPlayerIdentifiers.at(i) == focusedId)
			{
				gpGame->SetClientGridCoordinate(gpGame->mClientPlayerCoordinates.at(i));
				bGridCoordinateResolved = true;
				break;
			}
		}
	}

	// No valid selection — camera to origin
	if (!bGridCoordinateResolved)
	{
		gpGame->SetClientGridCoordinate({});
	}

	// Capture the final focus for client-state persistence, including server-driven changes.
	gpGame->CaptureClientState();
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
