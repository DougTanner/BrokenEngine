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
				SelectOwnedEntityInFleet(rMember.globalId);
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

void FleetSelection::SelectOwnedEntityInFleet(engine::GlobalId memberGlobalId)
{
	const Fleet* pFleet = FocusedFleet();
	if (pFleet == nullptr)
	{
		return;
	}

	ASSERT(memberGlobalId.iValue != 0);
	auto memberIt = std::ranges::find(pFleet->members, memberGlobalId, &FleetMember::globalId);
	if (memberIt == pFleet->members.end())
	{
		return;
	}

	mFocusedMemberGlobalId = memberGlobalId;
	mPendingFocusMemberGlobalId = {};

	const FleetMember& rMember = *memberIt;
	if (!(rMember.flags & FleetMemberFlags::kIsDead))
	{
		auto ownedIt = std::ranges::find(gpGame->mClientOwnedEntities, rMember.globalId, &engine::OwnedEntity::globalId);
		if (ownedIt != gpGame->mClientOwnedEntities.end())
		{
			gpGame->SetClientGridCoordinate(ownedIt->coord);
		}
	}

	gpGame->CaptureClientState();
}

void FleetSelection::SynchronizeFleets(std::vector<Fleet>&& rFleets)
{
	++miAppliedFleetSyncCount;

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

	// Restore from disk-persisted client state on the first sync after a reconnect-style clear.
	// The remembered FleetGuid identifies which fleet to focus; a missing or destroyed ship falls back to the fleet's current flagship.
	bool bRestoredRemembered = false;
	if (iPreviousFleetCount == 0)
	{
		auto rememberedFleetIt = std::ranges::find(mClientFleets, gpGame->mRememberedFleetGuid, &Fleet::guid);
		if (rememberedFleetIt != mClientFleets.end())
		{
			const Fleet& rFleet = *rememberedFleetIt;
			miFocusedFleetIndex = rememberedFleetIt - mClientFleets.begin();
			mFocusedMemberGlobalId = {};
			if ((gpGame->mRememberedFocusedShipIdentifier.iValue != 0))
			{
				for (const FleetMember& rMember : rFleet.members)
				{
					if (rMember.globalId == gpGame->mRememberedFocusedShipIdentifier && !(rMember.flags & FleetMemberFlags::kIsDead))
					{
						mFocusedMemberGlobalId = rMember.globalId;
						break;
					}
				}
			}
			// The server's fleet invariant guarantees the flagship names a member.
			if (!(mFocusedMemberGlobalId.iValue != 0))
			{
				mFocusedMemberGlobalId = rFleet.flagshipGlobalId;
			}
			// Suppress the auto-newest-fleet / auto-newest-member branches below.
			iPreviousFocusedFleetMemberCount = std::ssize(rFleet.members);
			bRestoredRemembered = true;
		}
	}

	if (!bRestoredRemembered)
	{
		auto reanchoredFleetIt = std::ranges::find(mClientFleets, previousFocusedFleetGuid, &Fleet::guid);
		if (reanchoredFleetIt != mClientFleets.end())
		{
			miFocusedFleetIndex = reanchoredFleetIt - mClientFleets.begin();
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
		if (!std::ranges::contains(pFleet->members, mFocusedMemberGlobalId, &FleetMember::globalId))
		{
			mFocusedMemberGlobalId = {};
		}

		// A new member takes focus only when no member holds it, so spawning into a fleet keeps the current focus.
		if (std::ssize(pFleet->members) > iPreviousFocusedFleetMemberCount && mFocusedMemberGlobalId.iValue == 0)
		{
			engine::GlobalId newMemberGlobalId = pFleet->members.back().globalId;
			// The spawn event and this sync share one reliable flush, and the client applies owned-entity events
			// before game packets, so the spawn may already have been applied.
			if (std::ranges::contains(gpGame->mClientOwnedEntities, newMemberGlobalId, &engine::OwnedEntity::globalId))
			{
				mFocusedMemberGlobalId = newMemberGlobalId;
				mPendingFocusMemberGlobalId = {};
			}
			else
			{
				mPendingFocusMemberGlobalId = newMemberGlobalId;
			}
		}
		else if (!(mFocusedMemberGlobalId.iValue != 0) && !pFleet->members.empty() && pFleet->members.back().globalId != mPendingFocusMemberGlobalId)
		{
			mFocusedMemberGlobalId = pFleet->members.back().globalId;
		}

		if (!std::ranges::contains(pFleet->members, mPendingFocusMemberGlobalId, &FleetMember::globalId))
		{
			mPendingFocusMemberGlobalId = {};
		}

		// The pending member has no owned-entity coordinate until its spawn event, even once a sync clears its kIsDead.
		auto focusedIt = std::ranges::find(pFleet->members, mFocusedMemberGlobalId, &FleetMember::globalId);
		if (!(mFocusedMemberGlobalId.iValue != 0) || (focusedIt->flags & FleetMemberFlags::kIsDead))
		{
			mFocusedMemberGlobalId = {};
			for (const FleetMember& rMember : pFleet->members)
			{
				if (!(rMember.flags & FleetMemberFlags::kIsDead) && rMember.globalId != mPendingFocusMemberGlobalId)
				{
					mFocusedMemberGlobalId = rMember.globalId;
					break;
				}
			}
		}
	}
	else
	{
		mFocusedMemberGlobalId = {};
		mSpawnIntoFleetToggle.Reset();
		mDestroyMemberToggle.Reset();
		mSquadronOrdersToggle.Reset();
	}

	engine::GlobalId focusedId = gpGame->ClientOwnedEntityIdentifier();
	auto focusedOwnedIt = std::ranges::find(gpGame->mClientOwnedEntities, focusedId, &engine::OwnedEntity::globalId);
	if ((focusedId.iValue != 0) && focusedOwnedIt != gpGame->mClientOwnedEntities.end())
	{
		gpGame->SetClientGridCoordinate(focusedOwnedIt->coord);
	}
	else
	{
		// No valid selection — camera to origin
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
	mPendingFocusMemberGlobalId = {};
	mCreateFleetToggle.Reset();
	mSpawnIntoFleetToggle.Reset();
	mDestroyMemberToggle.Reset();
	mSquadronOrdersToggle.Reset();
	mFleetNavigationControl.Reset();
	mHoldPickFleetGuid = {};
}

} // namespace game

#endif // BT_CLIENT
