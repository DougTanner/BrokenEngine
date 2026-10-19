#include "Frame.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/Collections/Ships/ShipStatusChanges.h"
#include "Frame/FrameCollections.h"
#include "Profile/ProfileRows.h"

namespace game
{

constexpr float kfShipSpawnInterval = 0.5f;

static float AdmitSpawnTimer(float fSpawnTimer)
{
	if (!std::isfinite(fSpawnTimer) || fSpawnTimer >= kfShipSpawnInterval)
	{
		throw std::ios_base::failure("FrameInterpolate fSpawnTimer");
	}
	return fSpawnTimer;
}

// Requires the Pushers id map already rebuilt from the read ids; that rebuild rejects id 0, so the map lookup also
// rejects a zero handle. A pusher leaves only with its owner and rests with no lifecycle bit, so a kTransfer or
// kDestroy bit on one would let the next compaction fold remove it while its owner still holds the handle.
static void ValidatePusherOwners(const Frame& rFrame)
{
	const FrameInterpolate& rInterpolate = rFrame.interpolate;
	const engine::PushersInterpolate& rPushers = rInterpolate.pushers;
	int64_t iOwnerCount = 0;
	ForEachShipType(rFrame, [&]([[maybe_unused]] ShipType eType, const auto& rShipInterpolate, [[maybe_unused]] const auto& rShipPostRender)
	{
		iOwnerCount += rShipInterpolate.iCount;
	});
	if (rPushers.iCount != iOwnerCount)
	{
		throw std::ios_base::failure("Frame pusher owners");
	}
	const engine::PushersPostRender& rPushersPostRender = rFrame.postRender.pushers;
	for (int64_t i = 0; i < rPushersPostRender.iCount; ++i)
	{
		if (std::to_underlying(rPushersPostRender.pLifecycleFlags[i].meFlags) != 0)
		{
			throw std::ios_base::failure("Frame pusher owners");
		}
	}

	auto seenAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<bool*>(rPushers.iCount * static_cast<int64_t>(sizeof(bool)));
	bool* pbSeen = seenAllocation.mpData;
	std::fill_n(pbSeen, rPushers.iCount, false);
	ForEachShipType(rFrame, [&]([[maybe_unused]] ShipType eType, const auto& rShipInterpolate, [[maybe_unused]] const auto& rShipPostRender)
	{
		for (engine::pusher_t pusher : std::span<const engine::pusher_t>(rShipInterpolate.pPushers, static_cast<size_t>(rShipInterpolate.iCount)))
		{
			auto it = rPushers.idToIndexMap.find(pusher);
			if (it == rPushers.idToIndexMap.end() || pbSeen[it->second])
			{
				throw std::ios_base::failure("Frame pusher owners");
			}
			pbSeen[it->second] = true;
		}
	});
}

// Bump this base on any change that shifts computed frame CRCs without bumping a collection's own kiVersion
// — notably the CRC mixing algorithm/constants in Common/Crc.h. This gate is the only thing distinguishing
// "data desynced" from "checksum algorithm changed"; skipping the bump makes straddling replays false-desync.
const int64_t Frame::kiVersion = 138 + engine::kiFrameBaseVersion + engine::SumVersions(GameInterpolateTypes {}) + engine::SumVersions(GamePostRenderTypes {});

FrameInterpolate::FrameInterpolate()
: pBlasters(std::make_unique<BlastersInterpolate>())
, pMissiles(std::make_unique<MissilesInterpolate>())
, pFighters(std::make_unique<FightersInterpolate>())
, pOmnis(std::make_unique<OmnisInterpolate>())
, pBattleships(std::make_unique<BattleshipsInterpolate>())
{
}

FrameInterpolate::~FrameInterpolate() = default;
FrameInterpolate::FrameInterpolate(FrameInterpolate&&) noexcept = default;
FrameInterpolate& FrameInterpolate::operator=(FrameInterpolate&&) noexcept = default;

FramePostRender::FramePostRender()
: pBlasters(std::make_unique<BlastersPostRender>())
, pMissiles(std::make_unique<MissilesPostRender>())
, pFighters(std::make_unique<FightersPostRender>())
, pOmnis(std::make_unique<OmnisPostRender>())
, pBattleships(std::make_unique<BattleshipsPostRender>())
{
}

FramePostRender::~FramePostRender() = default;
FramePostRender::FramePostRender(FramePostRender&&) noexcept = default;
FramePostRender& FramePostRender::operator=(FramePostRender&&) noexcept = default;

Frame::Frame() = default;
Frame::~Frame() = default;
Frame::Frame(Frame&&) noexcept = default;
Frame& Frame::operator=(Frame&&) noexcept = default;

void FrameInterpolate::Register()
{
	engine::ForEachRegister(GameInterpolateTypes {});
}

#if defined(BT_CLIENT)
void FrameInterpolate::GraphicsResources()
{
	engine::ForEachGraphicsResources(GameInterpolateTypes {});
}
#endif // BT_CLIENT

void FrameInterpolate::AllocateAndCopy(FrameInterpolate& __restrict rCurrent, const FrameInterpolate& __restrict rPrevious)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateAllocateAndCopy);

	FrameInterpolateBase::AllocateAndCopy(rCurrent, rPrevious);

	engine::AllocateAndCopyCollections(GameInterpolateCollections(rCurrent), GameInterpolateCollections(rPrevious), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(GameInterpolateCollections(rCurrent))>)> {});
}

void FrameInterpolate::Update(FrameInterpolate& __restrict rCurrent, const Frame& __restrict rPreviousFrame, float fDeltaTime)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateUpdate);

	const FrameInterpolate& rPrevious = rPreviousFrame.interpolate;

	FrameInterpolateBase::Update(rCurrent, rPreviousFrame, fDeltaTime);

	rCurrent.fSpawnTimer = rPrevious.fSpawnTimer + fDeltaTime;

	engine::ForEachInterpolateUpdate(GameInterpolateTypes {}, rCurrent, rPreviousFrame);
}

void FramePostRender::AllocateAndCopy(FramePostRender& __restrict rCurrent, const FramePostRender& __restrict rPrevious)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderAllocateAndCopy);

	engine::FramePostRenderBase::AllocateAndCopy(rCurrent, rPrevious);

	engine::AllocateAndCopyCollections(GamePostRenderCollections(rCurrent), GamePostRenderCollections(rPrevious), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(GamePostRenderCollections(rCurrent))>)> {});
}

void FramePostRender::Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const FrameInput& __restrict rFrameInput, const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderUpdate);

	FramePostRenderBase::Update(rFrame, rPreviousFrame, rFrameInput, rStaticData);

	rFrame.postRender.randomAlignment = rPreviousFrame.postRender.randomAlignment;
	rFrame.postRender.fleetAlignment = rPreviousFrame.postRender.fleetAlignment;

	engine::ForEachPostRenderUpdate(GamePostRenderTypes {}, rFrame, rPreviousFrame, rStaticData);

	ApplyUpdateStatusChanges(rFrame, rFrameInput, rStaticData);
}

void FramePostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ForEachPostRenderTransfer(GamePostRenderTypes {}, rFrame, rStaticData);
}

void FramePostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderDestroy);

	engine::ForEachPostRenderDestroy(GamePostRenderTypes {}, rFrame, rStaticData);

	engine::CompactCollections(rFrame, std::tuple_cat(rFrame.interpolate.Collections(), GameInterpolateCollections(rFrame.interpolate)), std::tuple_cat(rFrame.postRender.Collections(), GamePostRenderCollections(rFrame.postRender)));
}

// Each wave is one ship type picked at random, spaced by that type's radius. Only fleet ships, never random-spawn
// rows, anchor and repel a wave.
static void SpawnRandomShipGroup(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData)
{
	static constexpr int64_t kiGridDimension = 20;
	static constexpr int64_t kiMaximumFleetSize = 16;
	static constexpr float kfMinimumFleetDistance = 120.0f;
	static constexpr float kfDesiredAnchorDistance = 150.0f;

	// Count non-exploding fleet ships, use first as spawn center
	int64_t iSpawnCount = 0;
	auto vecFleetPosition = XMVectorZero();
	bool bFoundCenter = false;
	ForEachShipType(rFrame, [&]([[maybe_unused]] ShipType eType, const auto& rShipInterpolate, const auto& rShipPostRender)
	{
		for (int64_t i = 0; i < rShipPostRender.iCount; ++i)
		{
			if (rShipPostRender.pController[i] == ShipController::kFleet && !(rShipPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying))
			{
				if (!bFoundCenter)
				{
					vecFleetPosition = rShipInterpolate.pVecPositions[i];
					bFoundCenter = true;
				}
				++iSpawnCount;
			}
		}
	});
	if (iSpawnCount == 0)
	{
		return;
	}
	int64_t iShipCount = std::min(iSpawnCount, kiMaximumFleetSize);

	// The wave's type: one equal-odds draw over the three types, whose radius sets the wave's clearance and spacing.
	ShipType eWaveType = static_cast<ShipType>(common::Random(2i64, rFrame.postRender.randomEngine));
	// The wave's flight plane: one draw over the planes its type may fly, even when that is only the lowest.
	int64_t iWaveFlightPlane = static_cast<int64_t>(common::Random(HighestFlightPlane(eWaveType), rFrame.postRender.randomEngine));
	const engine::FlightPlane& rWaveFlightPlane = engine::gpFlightPlanes->Plane(iWaveFlightPlane);
	float fWaveRadius = eWaveType == ShipType::kFighter ? kfFighterRadius : eWaveType == ShipType::kOmni ? kfOmniRadius : kfBattleshipRadius;
	float fTerrainClearance = fWaveRadius * 2.0f;
	float fChevronStagger = fWaveRadius * 2.0f;
	float fShipSideSpacing = fWaveRadius * 3.0f;

	// Reject positions outside the cell, inside terrain at the wave's plane (with full body clearance; none on a plane
	// without terrain collision), or within horizontal visible range of any alive fleet ship on any plane
	auto IsSpawnPositionValid = [&](FXMVECTOR vecPosition) -> bool
	{
		if (!common::InsideArea(vecPosition, engine::LocalCellArea()))
		{
			return false;
		}
		if ((rWaveFlightPlane.flags & engine::FlightPlaneFlags::kTerrainCollision) && engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData).Sample(vecPosition) > rWaveFlightPlane.fHeightMeters - fTerrainClearance)
		{
			return false;
		}
		bool bNearFleetShip = false;
		ForEachShipType(rFrame, [&]([[maybe_unused]] ShipType eType, const auto& rShipInterpolate, const auto& rShipPostRender)
		{
			for (int64_t j = 0; j < rShipPostRender.iCount && !bNearFleetShip; ++j)
			{
				if (rShipPostRender.pController[j] != ShipController::kFleet)
				{
					continue;
				}

				if (rShipPostRender.pLifecycleFlags[j] & engine::LifecycleFlags::kDying)
				{
					continue;
				}
				XMVECTOR vecDelta = XMVectorSubtract(vecPosition, rShipInterpolate.pVecPositions[j]);
				bNearFleetShip = XMVectorGetX(XMVector2LengthSq(vecDelta)) < kfMinimumFleetDistance * kfMinimumFleetDistance;
			}
		});
		return !bNearFleetShip;
	};

	// Cell-area extents and grid pitch (area layout: x=minX, y=maxY, z=maxX, w=minY — see common::InsideArea)
	XMFLOAT4A f4Area;
	XMStoreFloat4A(&f4Area, engine::LocalCellArea());
	float fAreaMinimumX = f4Area.x;
	float fAreaMinimumY = f4Area.w;
	float fPitchX = (f4Area.z - f4Area.x) / static_cast<float>(kiGridDimension);
	float fPitchY = (f4Area.y - f4Area.w) / static_cast<float>(kiGridDimension);

	// Step 1: rasterize cell into a validity grid sampled at cell centers
	bool pbValidGrid[kiGridDimension * kiGridDimension] {};
	for (int64_t i = 0; i < kiGridDimension; ++i)
	{
		for (int64_t j = 0; j < kiGridDimension; ++j)
		{
			auto vecGridCell = XMVectorSet(fAreaMinimumX + (static_cast<float>(j) + 0.5f) * fPitchX, fAreaMinimumY + (static_cast<float>(i) + 0.5f) * fPitchY, rWaveFlightPlane.fHeightMeters, 1.0f);
			pbValidGrid[i * kiGridDimension + j] = IsSpawnPositionValid(vecGridCell);
		}
	}

	// Step 2: chevron template — anchor at front, ships fan back-and-side in local frame (forward = +x)
	float fCenterOffset = static_cast<float>(iShipCount - 1) * 0.5f;
	XMFLOAT2 pf2LocalOffsets[kiMaximumFleetSize] {};
	for (int64_t i = 0; i < iShipCount; ++i)
	{
		float fOffset = static_cast<float>(i) - fCenterOffset;
		pf2LocalOffsets[i].x = -std::abs(fOffset) * fChevronStagger;
		pf2LocalOffsets[i].y = fOffset * fShipSideSpacing;
	}

	// Step 3: score every grid cell as a candidate anchor; pick best-fit
	int64_t iBestScore = 0;
	float fBestDistanceCost = std::numeric_limits<float>::max();
	int64_t iBestAnchorIndex = -1;
	float fBestFacingCosine = 1.0f;
	float fBestFacingSine = 0.0f;
	for (int64_t i = 0; i < kiGridDimension; ++i)
	{
		for (int64_t j = 0; j < kiGridDimension; ++j)
		{
			float fAnchorX = fAreaMinimumX + (static_cast<float>(j) + 0.5f) * fPitchX;
			float fAnchorY = fAreaMinimumY + (static_cast<float>(i) + 0.5f) * fPitchY;

			// Facing direction: anchor -> spawn-center fleet ship (XY only)
			float fToFleetX = XMVectorGetX(vecFleetPosition) - fAnchorX;
			float fToFleetY = XMVectorGetY(vecFleetPosition) - fAnchorY;
			float fDistance = std::sqrt(fToFleetX * fToFleetX + fToFleetY * fToFleetY);
			if (fDistance < kfMinimumFleetDistance)
			{
				continue;
			}
			float fFacingCosine = fToFleetX / fDistance;
			float fFacingSine = fToFleetY / fDistance;

			// Score: count chevron ships landing on valid grid cells
			int64_t iScore = 0;
			for (int64_t k = 0; k < iShipCount; ++k)
			{
				float fLocalForward = pf2LocalOffsets[k].x;
				float fLocalSide = pf2LocalOffsets[k].y;
				float fWorldX = fAnchorX + fFacingCosine * fLocalForward - fFacingSine * fLocalSide;
				float fWorldY = fAnchorY + fFacingSine * fLocalForward + fFacingCosine * fLocalSide;
				int64_t iShipGridX = static_cast<int64_t>(std::floor((fWorldX - fAreaMinimumX) / fPitchX));
				int64_t iShipGridY = static_cast<int64_t>(std::floor((fWorldY - fAreaMinimumY) / fPitchY));
				if (iShipGridX < 0 || iShipGridX >= kiGridDimension || iShipGridY < 0 || iShipGridY >= kiGridDimension)
				{
					continue;
				}
				if (pbValidGrid[iShipGridY * kiGridDimension + iShipGridX])
				{
					++iScore;
				}
			}
			if (iScore == 0)
			{
				continue;
			}

			float fDistanceCost = std::abs(fDistance - kfDesiredAnchorDistance);
			if (iScore > iBestScore || (iScore == iBestScore && fDistanceCost < fBestDistanceCost))
			{
				iBestScore = iScore;
				fBestDistanceCost = fDistanceCost;
				iBestAnchorIndex = i * kiGridDimension + j;
				fBestFacingCosine = fFacingCosine;
				fBestFacingSine = fFacingSine;
			}
		}
	}
	if (iBestAnchorIndex < 0)
	{
		return;
	}

	// Step 4: place ships at the chosen anchor (subset fallback — skip ships whose exact position fails the precise validity check)
	int64_t iBestGridX = iBestAnchorIndex % kiGridDimension;
	int64_t iBestGridY = iBestAnchorIndex / kiGridDimension;
	float fBestAnchorX = fAreaMinimumX + (static_cast<float>(iBestGridX) + 0.5f) * fPitchX;
	float fBestAnchorY = fAreaMinimumY + (static_cast<float>(iBestGridY) + 0.5f) * fPitchY;
	ShipOrders_t waveOrders {};
	SetTargetFlightPlane(waveOrders, iWaveFlightPlane);
	for (int64_t i = 0; i < iShipCount; ++i)
	{
		float fLocalForward = pf2LocalOffsets[i].x;
		float fLocalSide = pf2LocalOffsets[i].y;
		float fWorldX = fBestAnchorX + fBestFacingCosine * fLocalForward - fBestFacingSine * fLocalSide;
		float fWorldY = fBestAnchorY + fBestFacingSine * fLocalForward + fBestFacingCosine * fLocalSide;
		auto vecSpawnPosition = XMVectorSet(fWorldX, fWorldY, rWaveFlightPlane.fHeightMeters, 1.0f);
		if (!IsSpawnPositionValid(vecSpawnPosition))
		{
			continue;
		}

		// Flattened because the spawn-center ship may fly another plane; the validity check keeps its horizontal part nonzero.
		auto vecDirectionToFleet = XMVector3Normalize(XMVectorSetZ(XMVectorSubtract(vecFleetPosition, vecSpawnPosition), 0.0f));
		switch (eWaveType)
		{
			case ShipType::kFighter:
				FightersPostRender::Spawn(rFrame,
				{
					.vecPosition = vecSpawnPosition,
					.vecDirection = vecDirectionToFleet,
					.alignment = rFrame.postRender.randomAlignment,
					.eController = ShipController::kRandomSpawn,
					.orders = waveOrders,
				});
				break;
			case ShipType::kOmni:
				OmnisPostRender::Spawn(rFrame,
				{
					.vecPosition = vecSpawnPosition,
					.vecDirection = vecDirectionToFleet,
					.alignment = rFrame.postRender.randomAlignment,
					.eController = ShipController::kRandomSpawn,
					.orders = waveOrders,
				});
				break;
			case ShipType::kBattleship:
				BattleshipsPostRender::Spawn(rFrame,
				{
					.vecPosition = vecSpawnPosition,
					.vecDirection = vecDirectionToFleet,
					.alignment = rFrame.postRender.randomAlignment,
					.eController = ShipController::kRandomSpawn,
					.orders = waveOrders,
				});
				break;
			case ShipType::kCount:
				break;
		}
	}
}

void FramePostRender::Spawn([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const FrameInput& __restrict rFrameInput, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderSpawn);

	engine::ForEachPostRenderSpawn(GamePostRenderTypes {}, rFrame, rStaticData);

	FrameInterpolate& rInterpolate = rFrame.interpolate;

	// Each interval attempts a wave capped by kiMaximumFleetSize and the non-exploding fleet ship count; invalid positions are skipped.
	while (rInterpolate.fSpawnTimer >= kfShipSpawnInterval)
	{
		rInterpolate.fSpawnTimer -= kfShipSpawnInterval;
		SpawnRandomShipGroup(rFrame, rStaticData);
	}
}

void FramePostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderPreCollision);

	engine::ForEachPostRenderPreCollision(GamePostRenderTypes {}, rFrame, rPreviousFrame, rStaticData);
}

void FramePostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderPostCollision);

	engine::ForEachPostRenderPostCollision(GamePostRenderTypes {}, rFrame, rPreviousFrame, rStaticData);
}

void FramePostRender::AreaDamage([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderAreaDamage);

	engine::ForEachPostRenderAreaDamage(GamePostRenderTypes {}, rFrame, rPreviousFrame, rStaticData);
}

std::array<engine::RegistryOwnershipLayer, std::to_underlying(ShipType::kCount)> Frame::OwnershipLayers(const Frame& rFrame)
{
	std::array<engine::RegistryOwnershipLayer, std::to_underlying(ShipType::kCount)> layers {};
	ForEachShipType(rFrame, [&](ShipType eType, [[maybe_unused]] const auto& rShipInterpolate, const auto& rShipPostRender)
	{
		layers[std::to_underlying(eType)] =
		{
			.pIdBytes = engine::RegistryIdBytes(rShipPostRender.pIds),
			.pGlobalIds = rShipPostRender.pGlobalIds,
			.pClientGuids = rShipPostRender.pClientGuids,
			.iCount = rShipPostRender.iCount,
		};
	});
	return layers;
}

int64_t Frame::OwnedEntityCount(const Frame& rFrame)
{
	int64_t iCount = 0;
	ForEachShipType(rFrame, [&]([[maybe_unused]] ShipType eType, [[maybe_unused]] const auto& rShipInterpolate, const auto& rShipPostRender)
	{
		iCount += std::ranges::count(std::span<const ShipController>(rShipPostRender.pController, static_cast<size_t>(rShipPostRender.iCount)), ShipController::kFleet);
	});
	return iCount;
}

#if defined(BT_CLIENT)
void FrameInterpolate::BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	engine::ForEachBeginRender(GameInterpolateTypes {}, iCommandBuffer, rRenderInterpolates, rActiveCoordinates);

	// Population counters: one sum over the cells this transaction renders, published once so a render with no
	// renderable cell reads zero.
	int64_t iTotalOmnis = 0;
	int64_t iTotalBlasters = 0;
	int64_t iTotalMissiles = 0;
	int64_t iTotalFighters = 0;
	int64_t iTotalBattleships = 0;
	for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		if (it != rRenderInterpolates.end())
		{
			iTotalOmnis += it->second.pOmnis->iCount;
			iTotalBlasters += it->second.pBlasters->iCount;
			iTotalMissiles += it->second.pMissiles->iCount;
			iTotalFighters += it->second.pFighters->iCount;
			iTotalBattleships += it->second.pBattleships->iCount;
		}
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterOmnis).iCount = iTotalOmnis;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterBlasters).iCount = iTotalBlasters;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterBlastersRendered).iCount = iTotalBlasters;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterMissiles).iCount = iTotalMissiles;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterFighters).iCount = iTotalFighters;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterBattleships).iCount = iTotalBattleships;
	}
}

void FrameInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(kCpuTimerRender);

	engine::ForEachInterpolateRender(GameInterpolateTypes {}, rFrameInterpolate, iCommandBuffer);
}

void FrameInterpolate::EndRender(int64_t iCommandBuffer)
{
	engine::ForEachEndRender(GameInterpolateTypes {}, iCommandBuffer);
}

void FrameInterpolate::DebugRender(const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		if (it != rRenderInterpolates.end())
		{
			FightersInterpolate::DebugRender(it->second, rCoordinate);
			OmnisInterpolate::DebugRender(it->second, rCoordinate);
			BattleshipsInterpolate::DebugRender(it->second, rCoordinate);
		}
	}
}

#endif // BT_CLIENT

common::crc_t FrameInterpolate::Crcs(const FrameInterpolate& rCurrent)
{
	common::crc_t uiSharedCrc = static_cast<const engine::FrameInterpolateBase&>(rCurrent).Crcs();

	uiSharedCrc = (uiSharedCrc ^ common::Crc(rCurrent.fSpawnTimer)) * common::kCrcMultiplier;

	uiSharedCrc = engine::CollectionsCrc(uiSharedCrc, GameInterpolateCollections(rCurrent));

	return uiSharedCrc;
}

bool FrameInterpolate::LogDifferences(const FrameInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("FrameInterpolate");
	bool bEqual = true;
	bEqual &= static_cast<const engine::FrameInterpolateBase&>(*this).LogDifferences(static_cast<const engine::FrameInterpolateBase&>(rOther));
	bEqual &= common::LogDifference<"fSpawnTimer">(fSpawnTimer, rOther.fSpawnTimer);
	bEqual &= engine::LogDifferencesCollections(GameInterpolateCollections(*this), GameInterpolateCollections(rOther), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(GameInterpolateCollections(*this))>)> {});
	return bEqual;
}

void FrameInterpolate::Write(std::ostream& rStream) const
{
	static_cast<const engine::FrameInterpolateBase&>(*this).Write(rStream);

	engine::FrameValuesWrite(rStream, *this, Values());

	engine::CollectionsWrite(rStream, GameInterpolateCollections(*this));
}

void FrameInterpolate::Read(std::istream& rStream)
{
	static_cast<engine::FrameInterpolateBase&>(*this).Read(rStream);

	engine::FrameValuesRead<false>(rStream, *this, Values());
	fSpawnTimer = AdmitSpawnTimer(fSpawnTimer);

	engine::CollectionsRead(rStream, GameInterpolateCollections(*this));
}

void FrameInterpolate::ServerRead(std::istream& rStream)
{
	static_cast<engine::FrameInterpolateBase&>(*this).ServerRead(rStream);

	engine::FrameValuesRead<true>(rStream, *this, Values());

	engine::SharedCollectionsRead(rStream, GameInterpolateCollections(*this));
}

common::crc_t FramePostRender::Crcs(const FramePostRender& rCurrent)
{
	common::crc_t uiSharedCrc = static_cast<const engine::FramePostRenderBase&>(rCurrent).Crcs();

	uiSharedCrc = (uiSharedCrc ^ common::Crc(rCurrent.randomAlignment)) * common::kCrcMultiplier;
	uiSharedCrc = (uiSharedCrc ^ common::Crc(rCurrent.fleetAlignment)) * common::kCrcMultiplier;

	uiSharedCrc = engine::CollectionsCrc(uiSharedCrc, GamePostRenderCollections(rCurrent));

	return uiSharedCrc;
}

bool FramePostRender::LogDifferences(const FramePostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("FramePostRender");
	bool bEqual = true;
	bEqual &= static_cast<const engine::FramePostRenderBase&>(*this).LogDifferences(static_cast<const engine::FramePostRenderBase&>(rOther));
	bEqual &= common::LogDifference<"randomAlignment">(randomAlignment, rOther.randomAlignment);
	bEqual &= common::LogDifference<"fleetAlignment">(fleetAlignment, rOther.fleetAlignment);
	bEqual &= engine::LogDifferencesCollections(GamePostRenderCollections(*this), GamePostRenderCollections(rOther), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(GamePostRenderCollections(*this))>)> {});
	return bEqual;
}

void FramePostRender::Write(std::ostream& rStream) const
{
	static_cast<const engine::FramePostRenderBase&>(*this).Write(rStream);

	engine::FrameValuesWrite(rStream, *this, Values());

	engine::CollectionsWrite(rStream, GamePostRenderCollections(*this));
}

void FramePostRender::Read(std::istream& rStream)
{
	static_cast<engine::FramePostRenderBase&>(*this).Read(rStream);

	engine::FrameValuesRead<false>(rStream, *this, Values());

	engine::CollectionsRead(rStream, GamePostRenderCollections(*this));
}

void FramePostRender::ServerRead(std::istream& rStream)
{
	static_cast<engine::FramePostRenderBase&>(*this).ServerRead(rStream);

	engine::FrameValuesRead<true>(rStream, *this, Values());

	engine::SharedCollectionsRead(rStream, GamePostRenderCollections(*this));
}

common::crc_t Frame::Crc() const
{
	common::crc_t uiCrc = FrameInterpolate::Crcs(interpolate);
	uiCrc = (uiCrc ^ FramePostRender::Crcs(postRender)) * common::kCrcMultiplier;
	return uiCrc;
}

bool Frame::LogDifferences(const Frame& rOther) const
{
	bool bEqual = true;
	bEqual &= interpolate.LogDifferences(rOther.interpolate);
	bEqual &= postRender.LogDifferences(rOther.postRender);
	return bEqual;
}

void Frame::ServerRead(std::istream& rStream)
{
	interpolate.ServerRead(rStream);
	postRender.ServerRead(rStream);

	engine::ValidateBaseServerCollectionPairs(interpolate, postRender);
	engine::ValidateCollectionPairs(GameInterpolateCollections(interpolate), GamePostRenderCollections(postRender));
}

std::ostream& operator<<(std::ostream& rStream, const Frame& rCurrent)
{
	rCurrent.interpolate.Write(rStream);
	rCurrent.postRender.Write(rStream);
	return rStream;
}

std::istream& operator>>(std::istream& rStream, Frame& rCurrent)
{
	Frame loadedFrame;
	loadedFrame.interpolate.Read(rStream);
	loadedFrame.postRender.Read(rStream);

	engine::ValidateBaseCollectionPairs(loadedFrame.interpolate, loadedFrame.postRender);
	engine::ValidateCollectionPairs(GameInterpolateCollections(loadedFrame.interpolate), GamePostRenderCollections(loadedFrame.postRender));
	ValidatePusherOwners(loadedFrame);
	rCurrent = std::move(loadedFrame);
	return rStream;
}

} // namespace game
