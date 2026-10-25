#include "ShipStatusChanges.h"

#include "Frame/CellStaticData.h"

#include "Frame/Collections/Ships/ShipQueries.h"

namespace game
{

// Calls rVisitor(Interpolate, PostRender, iRow) for the eType row whose uuid is iUuid; returns false when none has it.
template <typename VISITOR>
static bool VisitShipByUuid(Frame& rFrame, ShipType eType, int64_t iUuid, VISITOR&& rVisitor)
{
	bool bFound = false;
	ForEachShipType(rFrame, [&](ShipType eShipType, auto& rInterpolate, auto& rPostRender)
	{
		if (eShipType != eType)
		{
			return;
		}
		using Id = typename std::remove_cvref_t<decltype(rInterpolate)>::id_t;
		auto it = rInterpolate.idToIndexMap.find(Id(engine::Uuid(iUuid)));
		if (it != rInterpolate.idToIndexMap.end())
		{
			rVisitor(rInterpolate, rPostRender, it->second);
			bFound = true;
		}
	});
	return bFound;
}

void ApplyUpdateStatusChanges(Frame& rFrame, const FrameInput& rFrameInput, const engine::CellStaticData& rStaticData)
{
	int64_t iUpdateFleetCount = 0;
	engine::GridCoord updateFleetNewCoordinate {};

	for (const StatusChange& rStatusChange : rFrameInput.statusChanges)
	{
		if (rStatusChange.eType == StatusChangeType::kUpdateFleet)
		{
			const UpdateFleetData& rUpdate = std::get<UpdateFleetData>(rStatusChange.data);
			bool bFound = VisitShipByUuid(rFrame, rUpdate.eType, rUpdate.iUuid, [&]([[maybe_unused]] const auto& rInterpolate, auto& rPostRender, int64_t iIndex)
			{
				rPostRender.pFleetWantedCoordinates[iIndex] = rUpdate.fleetWantedCoordinate;
				rPostRender.puiPendingFleetWantedCoordinateTicks[iIndex] = rUpdate.uiPendingFleetWantedCoordinateTicks;
			});
			if (bFound)
			{
				++iUpdateFleetCount;
				updateFleetNewCoordinate = rUpdate.fleetWantedCoordinate;
			}
			continue;
		}

		if (rStatusChange.eType == StatusChangeType::kUpdateShip)
		{
			const UpdateShipData& rUpdate = std::get<UpdateShipData>(rStatusChange.data);
			bool bFound = VisitShipByUuid(rFrame, rUpdate.eType, rUpdate.iUuid, [&]([[maybe_unused]] const auto& rInterpolate, auto& rPostRender, int64_t iIndex)
			{
				// Weapon mode change is deferred via countdown ticks
				rPostRender.pOrders[iIndex].Set(ShipOrders::kPendingUseMissiles, rUpdate.bUseMissiles);
				if constexpr (requires { rPostRender.puiPendingWeaponModeTicks; })
				{
					rPostRender.puiPendingWeaponModeTicks[iIndex] = rUpdate.uiPendingWeaponModeTicks;
				}
				rPostRender.pNavigationDelays[iIndex] = rUpdate.navigationDelaySeconds;
				rPostRender.pCellChangeTimers[iIndex] = rUpdate.navigationDelaySeconds;
				if (rUpdate.uiTargetFlightPlane != kuiKeepTargetFlightPlane)
				{
					SetTargetFlightPlane(rPostRender.pOrders[iIndex], rUpdate.uiTargetFlightPlane);
				}
			});
			if (!bFound)
			{
				LOG(kNetwork, kWarning, "ApplyUpdateStatusChanges::kUpdateShip Type: {} Uuid: {} NOT FOUND in idToIndexMap", ShipTypeName(rUpdate.eType), rUpdate.iUuid);
			}
			continue;
		}

		if (rStatusChange.eType == StatusChangeType::kDestroyShip)
		{
			const DestroyShipData& rDestroy = std::get<DestroyShipData>(rStatusChange.data);
			VisitShipByUuid(rFrame, rDestroy.eType, rDestroy.iUuid, [&]([[maybe_unused]] const auto& rInterpolate, auto& rPostRender, int64_t iIndex)
			{
				rPostRender.pLifecycleFlags[iIndex].Set(engine::LifecycleFlags::kDestroy);
			});
			continue;
		}
	}

	if (iUpdateFleetCount > 0)
	{
		LOG(kNetwork, kVerbose, "ApplyUpdateStatusChanges::kUpdateFleet Coord: ({},{}) NewWantedCoord: ({},{}) Ships: {}", rStaticData.coordinate.iQ, rStaticData.coordinate.iR, updateFleetNewCoordinate.iQ, updateFleetNewCoordinate.iR, iUpdateFleetCount);
	}
}

} // namespace game
