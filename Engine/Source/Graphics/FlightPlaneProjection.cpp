#if defined(BT_CLIENT)

#include "Graphics/GraphicsUtils.h"

#include "Game.h"

namespace engine
{

// Re-home a cell-local position that has stepped outside its cell onto the neighbouring cell that contains
// it, rewriting both the coord and the local XY. GlobalElevation's callers hand in a position local to the
// basis coord, which can lie past the cell edge. Each step moves to the crossed edge's neighbour with the
// nearest center, which shortens the local position, then nudges the landing so a rounding residue cannot step back
// and forth across one edge or vertex. Returns false when a neighbour would leave the signed-int32 identity range, so
// an edge cell reports sea floor instead of wrapping to the far side.
static bool ResolveLocalPosition(GridCoord& rCoord, XMFLOAT4A& rf4Local)
{
	XMVECTOR vecLocal = XMLoadFloat4A(&rf4Local);
	while (!IsInsideCell(vecLocal))
	{
		const common::HexDirection& rDirection = kCellDirections.pEntries[CellExitDirection(vecLocal)];
		if (!TryAddGridCoordinate(rCoord, rDirection.iDeltaQ, rDirection.iDeltaR, rCoord))
		{
			return false;
		}

		vecLocal = NudgeIntoCell(XMVectorSubtract(vecLocal, XMLoadFloat2(&rDirection.f2CenterOffset)));
	}

	XMStoreFloat4A(&rf4Local, vecLocal);
	return true;
}

// Render path (ProjectToFlightPlane). Resolves a position past the cell edge onto the neighbouring cell and takes that
// cell's highest island elevation.
static float XM_CALLCONV GlobalElevation(GridCoord coord, FXMVECTOR vecLocalPosition)
{
	// Frame Purity Constraint: GlobalElevation walks mCells with libm trig and must never run from frame-tick
	// code — the sim hot path uses CellElevationSampler::Sample/CellNormal.
	ASSERT(common::gpThreadLocal != nullptr && !common::gpThreadLocal->mbInFrameTick);

	XMFLOAT4A f4Local {};
	XMStoreFloat4A(&f4Local, vecLocalPosition);

	if (!ResolveLocalPosition(coord, f4Local))
	{
		return gpIslandTerrain->mfSeaFloorElevation;
	}

	// Look up per-cell placements. Cells outside the simulated set fall through to sea floor.
	auto it = game::gpGame->mCells.find(coord);
	if (it == game::gpGame->mCells.end())
	{
		return gpIslandTerrain->mfSeaFloorElevation;
	}

	return gpIslandTerrain->CellElevation(it->second.staticData, f4Local);
}

XMVECTOR ProjectToFlightPlane(XMVECTOR vecLocalPosition, const RenderBasis& rBasis, int64_t iPlane)
{
	// The elevation query is answered in the emitter's own cell, from the coordinate the basis carries; the
	// projection toward the eye is the conversion point, so the rebase happens exactly once here.
	float fElevation = GlobalElevation(rBasis.coordinate, vecLocalPosition);
	return common::ToBaseHeight(Rebase(rBasis, vecLocalPosition), engine::gpCamera->mVecEyePosition, std::max(fElevation, gpFlightPlanes->Plane(iPlane).fHeightMeters));
}

XMVECTOR ProjectToBaseHeight(XMVECTOR vecLocalPosition, const RenderBasis& rBasis)
{
	return ProjectToFlightPlane(vecLocalPosition, rBasis, 0);
}

} // namespace engine

#endif // BT_CLIENT
