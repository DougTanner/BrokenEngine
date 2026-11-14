#include "Wrecks.h"

#if defined(BT_CLIENT)

#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Ships/ShipHitboxes.h"

namespace engine
{

template struct Collection<game::WrecksInterpolate>;
template struct Collection<game::WrecksPostRender>;

} // namespace engine

namespace game
{

constexpr std::chrono::duration<float> kBattleshipWreckPowerLossTime = std::chrono::duration<float>(1.0f);
constexpr float kfBattleshipWreckGravity = 9.8f;
constexpr float kfBattleshipWreckWaterDeceleration = 30.0f;
constexpr float kfBattleshipWreckSinkSpeed = 0.5f;

constexpr std::chrono::duration<float> kWreckExplosionDuration = std::chrono::duration<float>(2.0f);
constexpr float kfBattleshipExplosionParticleCount = 8.0f;
constexpr float kfBattleshipExplosionSizeStart = kfBattleshipRadius * 0.625f;
constexpr float kfBattleshipExplosionSizeEnd = kfBattleshipRadius * 0.25f;
constexpr float kfBattleshipExplosionSmoke = 0.5f;
constexpr float kfBattleshipExplosionDirectionJitter = 0.5f;
constexpr int64_t kiBattleshipExplosionTrailCount = 5;

// Defined in Battleships.cpp, set during Register().
extern int64_t giBattleshipExplosionTypeIndex;

void WrecksInterpolate::Update(FrameInterpolate& __restrict rCurrentFrameInterpolate, const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	WrecksInterpolate& rCurrent = *rCurrentFrameInterpolate.pWrecks;
	const WrecksInterpolate& rPrevious = *rPreviousFrame.interpolate.pWrecks;
	const WrecksPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pWrecks;
	float fSeconds = rCurrentFrameInterpolate.fDeltaTime;

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Gravity ramps up as the ship loses power, the water brakes the fall to a slow sink, and the terrain stops it.
		std::chrono::duration<float> fallTime = rPrevious.pFallTimes[i] + std::chrono::duration<float>(fSeconds);
		float fGravity = kfBattleshipWreckGravity * std::min(fallTime / kBattleshipWreckPowerLossTime, 1.0f);
		float fFallSpeed = rPrevious.pfFallSpeeds[i];
		float fZ = XMVectorGetZ(rPrevious.pVecPositions[i]);
		if (fZ > 0.0f)
		{
			fFallSpeed += fGravity * fSeconds;
		}
		else
		{
			fFallSpeed = std::max(fFallSpeed - kfBattleshipWreckWaterDeceleration * fSeconds, kfBattleshipWreckSinkSpeed);
		}
		fZ -= fFallSpeed * fSeconds;
		if (!rPreviousPostRender.pbSinks[i] && fZ <= rPreviousPostRender.pfRestZ[i])
		{
			fZ = rPreviousPostRender.pfRestZ[i];
			fFallSpeed = 0.0f;
		}

		rCurrent.pVecPositions[i] = XMVectorSetZ(rPrevious.pVecPositions[i], fZ);
		rCurrent.pfFallSpeeds[i] = fFallSpeed;
		rCurrent.pFallTimes[i] = fallTime;
	}
}

void WrecksPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
}

void WrecksPostRender::Destroy(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData)
{
	const WrecksInterpolate& rInterpolate = *rFrame.interpolate.pWrecks;
	WrecksPostRender& rPostRender = *rFrame.postRender.pWrecks;

	// The client subscription's cell loaded area, or during a camera jump the previous cell loaded area too; map view
	// leaves both unchanged. Only the main thread writes them.
	XMVECTOR vecCellCenter = engine::CellCenterOffset(engine::gpCamera->mBasisCoordinate, rStaticData.coordinate);
	bool bInLoadedArea = common::RectangleOverlapsHex(engine::gpCamera->mf4CellLoadedArea, vecCellCenter, engine::kCellDirections) || (engine::gpCamera->mbPreviousCellLoadedAreaValid && common::RectangleOverlapsHex(engine::gpCamera->mf4PreviousCellLoadedArea, vecCellCenter, engine::kCellDirections));

	engine::DestroySweep(rInterpolate, rPostRender, [&](int64_t i)
	{
		return !bInLoadedArea || (rPostRender.pbSinks[i] && XMVectorGetZ(rInterpolate.pVecPositions[i]) + rPostRender.pfTopOffsets[i] < 0.0f);
	}, [&](int64_t i)
	{
		engine::MarkDestroy(rPostRender.pLifecycleFlags[i]);
	});
}

void WrecksPostRender::Spawn(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData)
{
	const BattleshipsInterpolate& rBattleships = *rFrame.interpolate.pBattleships;
	const BattleshipsPostRender& rBattleshipsPostRender = *rFrame.postRender.pBattleships;
	WrecksInterpolate& rInterpolate = *rFrame.interpolate.pWrecks;
	WrecksPostRender& rPostRender = *rFrame.postRender.pWrecks;

	for (int64_t i = 0; i < rBattleships.iCount; ++i)
	{
		// BeginExplosion sets exactly kBattleshipDestroyTime on the death tick, and the next tick's Update counts it down.
		if (!(rBattleshipsPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) || rBattleships.pDestroyedTimes[i] != kBattleshipDestroyTime)
		{
			continue;
		}

		XMVECTOR vecPosition = rBattleships.pVecPositions[i];
		engine::CollisionHitbox hitbox {};
		BuildShipHitbox(ShipType::kBattleship, rBattleshipsPostRender.pController[i], vecPosition, rBattleships.pVecDirections[i], hitbox);
		ASSERT(hitbox.eHitboxLevel >= common::ModelHitboxLevel::kBox);

		// The box rests on the highest terrain or seafloor under its center and four corners.
		engine::CellElevationSampler sampler = engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData);
		XMVECTOR vecCenter = XMVectorSetZ(vecPosition, 0.0f);
		float fHighestElevation = sampler.Sample(vecCenter);
		for (const XMFLOAT2& rf2Corner : hitbox.pf2BoxVertices)
		{
			fHighestElevation = std::max(fHighestElevation, sampler.Sample(XMVectorSet(rf2Corner.x, rf2Corner.y, 0.0f, 1.0f)));
		}

		float fStartZ = XMVectorGetZ(vecPosition);
		bool bSinks = fHighestElevation <= sampler.fSeaFloor;

		engine::GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
		int64_t iSpawnIndex = engine::AddElement(rInterpolate, rPostRender);

		rInterpolate.pVecPositions[iSpawnIndex] = vecPosition;
		rInterpolate.pVecDirections[iSpawnIndex] = rBattleships.pVecDirections[i];
		rInterpolate.pfFallSpeeds[iSpawnIndex] = 0.0f;
		rInterpolate.pFallTimes[iSpawnIndex] = std::chrono::duration<float>::zero();
		rInterpolate.pAnimationTimes[iSpawnIndex] = rBattleships.pAnimationTimes[i];

		rPostRender.pController[iSpawnIndex] = rBattleshipsPostRender.pController[i];
		rPostRender.pVecTilts[iSpawnIndex] = bSinks ? XMQuaternionIdentity() : common::QuaternionFromDirection(engine::gpIslandTerrain->CellNormal(rStaticData, vecCenter), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
		rPostRender.pfStartZ[iSpawnIndex] = fStartZ;
		rPostRender.pfRestZ[iSpawnIndex] = bSinks ? 0.0f : std::min(fStartZ, fHighestElevation + (fStartZ - hitbox.fMinimumZ));
		rPostRender.pfTopOffsets[iSpawnIndex] = hitbox.fMaximumZ - fStartZ;
		rPostRender.pbSinks[iSpawnIndex] = bSinks;
	}

	// Fall time is zero at capture and steps by exactly one tick, so a wreck emits once on each tick of its first
	// kWreckExplosionDuration.
	for (int64_t i = 0; i < rInterpolate.iCount; ++i)
	{
		std::chrono::duration<float> age = rInterpolate.pFallTimes[i];
		if (age >= kWreckExplosionDuration)
		{
			continue;
		}

		common::RandomEngine randomEngine(static_cast<uint64_t>(engine::Uuid::GenerateVisual(rFrame.postRender).iValue));

		// A uniform direction on the upper hemisphere, so the ray from outside meets the hull's side or top.
		float fZ = common::RandomUnitFloat(randomEngine);
		float fSin = 0.0f;
		float fCos = 0.0f;
		XMScalarSinCos(&fSin, &fCos, common::Random(XM_2PI, randomEngine));
		float fHorizontal = std::sqrt(1.0f - fZ * fZ);
		XMVECTOR vecDirection = XMVectorSet(fHorizontal * fCos, fHorizontal * fSin, fZ, 0.0f);

		// The start lies outside the radius enclosing the hull, and the contact is on the upright outline prism.
		ShipController eController = rPostRender.pController[i];
		XMVECTOR vecPosition = rInterpolate.pVecPositions[i];
		float fRadius = gShipHitboxShapes[std::to_underlying(ShipType::kBattleship)][std::to_underlying(eController)].fRadius;
		XMVECTOR vecStart = XMVectorMultiplyAdd(vecDirection, XMVectorReplicate(2.0f * fRadius), vecPosition);
		XMVECTOR vecContact = ShipHullContact(ShipType::kBattleship, eController, vecPosition, rInterpolate.pVecDirections[i], vecStart);

		// The tilt applies after the yaw, as the wreck draw applies it.
		XMVECTOR vecTilt = WreckTilt(rInterpolate, rPostRender, i);
		XMVECTOR vecAnchor = XMVectorSetW(XMVectorAdd(vecPosition, XMVector3RotateSafe(XMVectorSubtract(vecContact, vecPosition), vecTilt)), 1.0f);
		XMVECTOR vecTiltedDirection = XMVector3RotateSafe(vecDirection, vecTilt);

		float fPercent = 1.0f - age / kWreckExplosionDuration;
		engine::ExplosionsPostRender::Spawn(rFrame, rStaticData.coordinate, engine::TickToSimulationTime(rFrame.interpolate.iTick),
		{
			.iTypeIndex = giBattleshipExplosionTypeIndex,
			.vecPosition = vecAnchor,
			.vecDirection = common::RandomDirectionJitter<kfBattleshipExplosionDirectionJitter>(vecTiltedDirection, randomEngine),
			.flags = {engine::ExplosionFlags::kRed},
			.lifecycleFlags = {engine::LifecycleFlags::kDying},
			.iTrailCount = kiBattleshipExplosionTrailCount,
			.fTrailAngle = fPercent * XM_PI,
			.iParticleCount = static_cast<int64_t>(fPercent * kfBattleshipExplosionParticleCount),
			.fParticleAngle = fPercent * XM_PIDIV2,
			.fSizePercent = fPercent * kfBattleshipExplosionSizeStart + (1.0f - fPercent) * kfBattleshipExplosionSizeEnd,
			.fSmokePercent = fPercent * kfBattleshipExplosionSmoke,
			.fTimePercent = fPercent,
		}, randomEngine);
	}
}

} // namespace game

#endif // BT_CLIENT
