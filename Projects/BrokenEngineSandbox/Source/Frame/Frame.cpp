#include "Frame.h"

#include "Frame/Collections/Players/Players.h"
#include "Frame/FrameCollections.h"
#include "Profile/ProfileManager.h"
#include "Ui/LightingWrappers.h"
#include "Ui/SmokeWrappers.h"
#include "Ui/WindDepositsWrappers.h"

namespace game
{

using enum GameFlags;

constexpr float kfSpaceshipSpawnInterval = 0.5f;

static float AdmitSpawnTimer(float fSpawnTimer)
{
	if (!std::isfinite(fSpawnTimer) || fSpawnTimer >= kfSpaceshipSpawnInterval)
	{
		throw std::ios_base::failure("FrameInterpolate fSpawnTimer");
	}
	return fSpawnTimer;
}

template <typename... TS>
static consteval int64_t SumVersions(engine::TypeList<TS...>)
{
	return (0i64 + ... + TS::kiVersion);
}

using EngineInterpolateTypes = engine::TupleToTypeList_t<decltype(std::declval<engine::FrameInterpolateBase&>().ServerCollections())>;
using EnginePostRenderTypes = engine::TupleToTypeList_t<decltype(std::declval<engine::FramePostRenderBase&>().ServerCollections())>;

// Bump this base on any change that shifts computed frame CRCs without bumping a collection's own kiVersion
// — notably the CRC mixing algorithm/constants in Common/Crc.h. This gate is the only thing distinguishing
// "data desynced" from "checksum algorithm changed"; skipping the bump makes straddling replays false-desync.
const int64_t Frame::kiVersion = 133 + engine::kiNavDataVersion + PlayersInterpolate::kiVersion + PlayersPostRender::kiVersion + SumVersions(GameInterpolateTypes {}) + SumVersions(GamePostRenderTypes {}) + SumVersions(EngineInterpolateTypes {}) + SumVersions(EnginePostRenderTypes {});

FrameInterpolate::FrameInterpolate()
: pPlayers(std::make_unique<PlayersInterpolate>())
, pBlasters(std::make_unique<BlastersInterpolate>())
, pMissiles(std::make_unique<MissilesInterpolate>())
, pSpaceships(std::make_unique<SpaceshipsInterpolate>())
{
}

FrameInterpolate::~FrameInterpolate() = default;
FrameInterpolate::FrameInterpolate(FrameInterpolate&&) noexcept = default;
FrameInterpolate& FrameInterpolate::operator=(FrameInterpolate&&) noexcept = default;

FramePostRender::FramePostRender()
: pPlayers(std::make_unique<PlayersPostRender>())
, pBlasters(std::make_unique<BlastersPostRender>())
, pMissiles(std::make_unique<MissilesPostRender>())
, pSpaceships(std::make_unique<SpaceshipsPostRender>())
{
	transferRequests.reserve(static_cast<size_t>(engine::kiInitialTransferCapacity));
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
#if defined(BT_CLIENT)
	// Explosion tuning is game content driven by the Tweaks sliders; the engine owns only the effect mechanism.
	// This must complete before the engine ForEachRegister() below, which reads every field.
	engine::ExplosionsInterpolate::sTuning =
	{
		.pPrimaryVisibleAreaOne = &gExplosionPrimaryVisibleAreaOne,
		.pPrimaryVisibleAreaTwo = &gExplosionPrimaryVisibleAreaTwo,
		.pPrimaryVisibleAreaThree = &gExplosionPrimaryVisibleAreaThree,
		.pPrimaryVisibleIntensityOne = &gExplosionPrimaryVisibleIntensityOne,
		.pPrimaryVisibleIntensityTwo = &gExplosionPrimaryVisibleIntensityTwo,
		.pPrimaryVisibleIntensityThree = &gExplosionPrimaryVisibleIntensityThree,
		.pPrimaryLightingAreaOne = &gExplosionPrimaryLightingAreaOne,
		.pPrimaryLightingAreaTwo = &gExplosionPrimaryLightingAreaTwo,
		.pPrimaryLightingAreaThree = &gExplosionPrimaryLightingAreaThree,
		.pPrimaryLightingIntensityOne = &gExplosionPrimaryLightingIntensityOne,
		.pPrimaryLightingIntensityTwo = &gExplosionPrimaryLightingIntensityTwo,
		.pPrimaryLightingIntensityThree = &gExplosionPrimaryLightingIntensityThree,
		.pSecondaryVisibleAreaOne = &gExplosionSecondaryVisibleAreaOne,
		.pSecondaryVisibleAreaTwo = &gExplosionSecondaryVisibleAreaTwo,
		.pSecondaryVisibleAreaThree = &gExplosionSecondaryVisibleAreaThree,
		.pSecondaryVisibleIntensityOne = &gExplosionSecondaryVisibleIntensityOne,
		.pSecondaryVisibleIntensityTwo = &gExplosionSecondaryVisibleIntensityTwo,
		.pSecondaryVisibleIntensityThree = &gExplosionSecondaryVisibleIntensityThree,
		.pSecondaryLightingAreaOne = &gExplosionSecondaryLightingAreaOne,
		.pSecondaryLightingAreaTwo = &gExplosionSecondaryLightingAreaTwo,
		.pSecondaryLightingAreaThree = &gExplosionSecondaryLightingAreaThree,
		.pSecondaryLightingIntensityOne = &gExplosionSecondaryLightingIntensityOne,
		.pSecondaryLightingIntensityTwo = &gExplosionSecondaryLightingIntensityTwo,
		.pSecondaryLightingIntensityThree = &gExplosionSecondaryLightingIntensityThree,
		.pPrimaryPuffAreaOne = &gExplosionPrimaryPuffAreaOne,
		.pPrimaryPuffAreaTwo = &gExplosionPrimaryPuffAreaTwo,
		.pPrimaryPuffIntensityOne = &gExplosionPrimaryPuffIntensityOne,
		.pPrimaryPuffIntensityTwo = &gExplosionPrimaryPuffIntensityTwo,
		.pSecondaryPuffAreaOne = &gExplosionSecondaryPuffAreaOne,
		.pSecondaryPuffAreaTwo = &gExplosionSecondaryPuffAreaTwo,
		.pSecondaryPuffIntensityOne = &gExplosionSecondaryPuffIntensityOne,
		.pSecondaryPuffIntensityTwo = &gExplosionSecondaryPuffIntensityTwo,
		.pPrimaryTrailLength = &gExplosionPrimaryTrailLength,
		.pPrimaryTrailDuration = &gExplosionPrimaryTrailDuration,
		.pPrimaryTrailIntensity = &gExplosionPrimaryTrailIntensity,
		.pSecondaryTrailLength = &gExplosionSecondaryTrailLength,
		.pSecondaryTrailDuration = &gExplosionSecondaryTrailDuration,
		.pSecondaryTrailIntensity = &gExplosionSecondaryTrailIntensity,
		.pWindIntensity = &gWindDepositExplosionsIntensity,
		.pWindWidth = &gWindDepositExplosionsWidth,
	};
#endif // BT_CLIENT

	engine::ForEachRegister(engine::InterpolateTypes {});

	PlayersInterpolate::Register();

	engine::ForEachRegister(GameInterpolateTypes {});
}

#if defined(BT_CLIENT)
void FrameInterpolate::GraphicsResources()
{
	engine::ForEachGraphicsResources(engine::InterpolateTypes {});

	PlayersInterpolate::GraphicsResources();

	engine::ForEachGraphicsResources(GameInterpolateTypes {});
}
#endif // BT_CLIENT

void FrameInterpolate::AllocateAndCopy(FrameInterpolate& __restrict rCurrent, const FrameInterpolate& __restrict rPrevious)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateAllocateAndCopy);

	FrameInterpolateBase::AllocateAndCopy(rCurrent, rPrevious);

	engine::AllocateAndCopyMembers(*rCurrent.pPlayers, *rPrevious.pPlayers);

	engine::AllocateAndCopyCollections(GameInterpolateCollections(rCurrent), GameInterpolateCollections(rPrevious), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(GameInterpolateCollections(rCurrent))>)> {});
}

void FrameInterpolate::Update(FrameInterpolate& __restrict rCurrent, const Frame& __restrict rPreviousFrame, float fDeltaTime)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateUpdate);

	const FrameInterpolate& rPrevious = rPreviousFrame.interpolate;

	FrameInterpolateBase::Update(rCurrent, rPreviousFrame, fDeltaTime);

	GameFlags_t gameFlags = rPrevious.gameFlags;
	float fSpawnTimer = rPrevious.fSpawnTimer;

	// Main-menu frames hold the spawn timer at its zero initialization: Spawn returns before draining it, so the
	// timer must not accumulate an undrained backlog.
	if (!(gameFlags & GameFlags::kMainMenu))
	{
		fSpawnTimer += fDeltaTime;
	}

	rCurrent.gameFlags = gameFlags;
	rCurrent.fSpawnTimer = fSpawnTimer;

	PlayersInterpolate::Update(rCurrent, rPreviousFrame);

	engine::ForEachInterpolateUpdate(GameInterpolateTypes {}, rCurrent, rPreviousFrame);
}

void FramePostRender::AllocateAndCopy(FramePostRender& __restrict rCurrent, const FramePostRender& __restrict rPrevious)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderAllocateAndCopy);

	engine::FramePostRenderBase::AllocateAndCopy(rCurrent, rPrevious);

	engine::AllocateAndCopyMembers(*rCurrent.pPlayers, *rPrevious.pPlayers);

	engine::AllocateAndCopyCollections(GamePostRenderCollections(rCurrent), GamePostRenderCollections(rPrevious), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(GamePostRenderCollections(rCurrent))>)> {});
}

void FramePostRender::Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const FrameInput& __restrict rFrameInput, const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderUpdate);

	rFrame.postRender.transferRequests.clear();

	FramePostRenderBase::Update(rFrame, rPreviousFrame, rFrameInput, rStaticData);

	rFrame.postRender.enemyAlignment = rPreviousFrame.postRender.enemyAlignment;
	rFrame.postRender.playerAlignment = rPreviousFrame.postRender.playerAlignment;

	PlayersPostRender::Update(rFrame, rPreviousFrame, rStaticData);
	PlayersPostRender::ProcessUpdateStatusChanges(rFrame, rFrameInput, rStaticData);

	engine::ForEachPostRenderUpdate(GamePostRenderTypes {}, rFrame, rPreviousFrame, rStaticData);
}

void FramePostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ForEachPostRenderTransfer(engine::PostRenderBaseTypes {}, rFrame, rStaticData);

	PlayersPostRender::Transfer(rFrame, rStaticData);

	engine::ForEachPostRenderTransfer(GamePostRenderTypes {}, rFrame, rStaticData);
}

void FramePostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderDestroy);

	engine::ForEachPostRenderDestroy(engine::PostRenderBaseTypes {}, rFrame, rStaticData);

	PlayersPostRender::Destroy(rFrame, rStaticData);

	engine::ForEachPostRenderDestroy(GamePostRenderTypes {}, rFrame, rStaticData);
}

static void SpawnSpaceshipGroup(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData)
{
	FrameInterpolate& rInterpolate = rFrame.interpolate;

	static constexpr int64_t kiGridDimension = 20;
	static constexpr int64_t kiMaximumFleetSize = 16;
	static constexpr float kfTerrainClearance = kfSpaceshipRadius * 2.0f;
	static constexpr float kfMinimumPlayerDistance = 120.0f;
	static constexpr float kfDesiredAnchorDistance = 150.0f;
	static constexpr float kfChevronStagger = kfSpaceshipRadius * 2.0f;
	static constexpr float kfShipSideSpacing = kfSpaceshipRadius * 3.0f;

	// Count non-exploding players, use first as spawn center
	int64_t iSpawnCount = 0;
	auto vecPlayerPosition = XMVectorZero();
	bool bFoundCenter = false;
	for (int64_t i = 0; i < rInterpolate.pPlayers->iCount; ++i)
	{
		if (!(rFrame.postRender.pPlayers->pFlags[i] & PlayerFlags::kExploding))
		{
			if (!bFoundCenter)
			{
				vecPlayerPosition = rInterpolate.pPlayers->pVecPositions[i];
				bFoundCenter = true;
			}
			++iSpawnCount;
		}
	}
	if (iSpawnCount == 0)
	{
		return;
	}
	int64_t iShipCount = std::min(iSpawnCount, kiMaximumFleetSize);

	// Reject positions outside the cell, inside terrain (with full body clearance), or within visible range of any alive player
	auto IsSpawnPositionValid = [&](FXMVECTOR vecPosition) -> bool
	{
		if (!common::InsideArea(vecPosition, engine::LocalCellArea()))
		{
			return false;
		}
		if (engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData).Sample(vecPosition) > engine::gBaseHeight.mfCurrent - kfTerrainClearance)
		{
			return false;
		}
		for (int64_t j = 0; j < rInterpolate.pPlayers->iCount; ++j)
		{
			if (rFrame.postRender.pPlayers->pFlags[j] & PlayerFlags::kExploding)
			{
				continue;
			}
			XMVECTOR vecDelta = XMVectorSubtract(vecPosition, rInterpolate.pPlayers->pVecPositions[j]);
			if (XMVectorGetX(XMVector3LengthSq(vecDelta)) < kfMinimumPlayerDistance * kfMinimumPlayerDistance)
			{
				return false;
			}
		}
		return true;
	};

	// Cell-area extents and grid pitch (area layout: x=minX, y=maxY, z=maxX, w=minY — see common::InsideArea)
	XMFLOAT4A f4Area;
	XMStoreFloat4A(&f4Area, engine::LocalCellArea());
	float fAreaMinimumX = f4Area.x;
	float fAreaMinimumY = f4Area.w;
	float fPitchX = (f4Area.z - f4Area.x) / static_cast<float>(kiGridDimension);
	float fPitchY = (f4Area.y - f4Area.w) / static_cast<float>(kiGridDimension);

	// Step 1: rasterize cell into a validity grid sampled at cell centers
	bool aValidGrid[kiGridDimension * kiGridDimension] {};
	for (int64_t i = 0; i < kiGridDimension; ++i)
	{
		for (int64_t j = 0; j < kiGridDimension; ++j)
		{
			auto vecGridCell = XMVectorSet(fAreaMinimumX + (static_cast<float>(j) + 0.5f) * fPitchX, fAreaMinimumY + (static_cast<float>(i) + 0.5f) * fPitchY, engine::gBaseHeight.mfCurrent, 1.0f);
			aValidGrid[i * kiGridDimension + j] = IsSpawnPositionValid(vecGridCell);
		}
	}

	// Step 2: chevron template — anchor at front, ships fan back-and-side in local frame (forward = +x)
	float fCenterOffset = static_cast<float>(iShipCount - 1) * 0.5f;
	XMFLOAT2 aLocalOffsets[kiMaximumFleetSize] {};
	for (int64_t i = 0; i < iShipCount; ++i)
	{
		float fOffset = static_cast<float>(i) - fCenterOffset;
		aLocalOffsets[i].x = -std::abs(fOffset) * kfChevronStagger;
		aLocalOffsets[i].y = fOffset * kfShipSideSpacing;
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

			// Facing direction: anchor -> spawn-center player (XY only)
			float fToPlayerX = XMVectorGetX(vecPlayerPosition) - fAnchorX;
			float fToPlayerY = XMVectorGetY(vecPlayerPosition) - fAnchorY;
			float fDistance = std::sqrt(fToPlayerX * fToPlayerX + fToPlayerY * fToPlayerY);
			if (fDistance < kfMinimumPlayerDistance)
			{
				continue;
			}
			float fFacingCosine = fToPlayerX / fDistance;
			float fFacingSine = fToPlayerY / fDistance;

			// Score: count chevron ships landing on valid grid cells
			int64_t iScore = 0;
			for (int64_t k = 0; k < iShipCount; ++k)
			{
				float fLocalForward = aLocalOffsets[k].x;
				float fLocalSide = aLocalOffsets[k].y;
				float fWorldX = fAnchorX + fFacingCosine * fLocalForward - fFacingSine * fLocalSide;
				float fWorldY = fAnchorY + fFacingSine * fLocalForward + fFacingCosine * fLocalSide;
				int64_t iShipGridX = static_cast<int64_t>(std::floor((fWorldX - fAreaMinimumX) / fPitchX));
				int64_t iShipGridY = static_cast<int64_t>(std::floor((fWorldY - fAreaMinimumY) / fPitchY));
				if (iShipGridX < 0 || iShipGridX >= kiGridDimension || iShipGridY < 0 || iShipGridY >= kiGridDimension)
				{
					continue;
				}
				if (aValidGrid[iShipGridY * kiGridDimension + iShipGridX])
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
	for (int64_t i = 0; i < iShipCount; ++i)
	{
		float fLocalForward = aLocalOffsets[i].x;
		float fLocalSide = aLocalOffsets[i].y;
		float fWorldX = fBestAnchorX + fBestFacingCosine * fLocalForward - fBestFacingSine * fLocalSide;
		float fWorldY = fBestAnchorY + fBestFacingSine * fLocalForward + fBestFacingCosine * fLocalSide;
		auto vecSpawnPosition = XMVectorSet(fWorldX, fWorldY, engine::gBaseHeight.mfCurrent, 1.0f);
		if (!IsSpawnPositionValid(vecSpawnPosition))
		{
			continue;
		}

		auto vecDirectionToPlayer = XMVector3Normalize(XMVectorSubtract(vecPlayerPosition, vecSpawnPosition));
		SpaceshipsPostRender::Spawn(rFrame,
		{
			.vecPosition = vecSpawnPosition,
			.vecDirection = vecDirectionToPlayer,
			.alignment = rFrame.postRender.enemyAlignment,
		});
	}
}

void FramePostRender::Spawn([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const FrameInput& __restrict rFrameInput, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderSpawn);

	engine::ForEachPostRenderSpawn(engine::PostRenderBaseTypes {}, rFrame, rStaticData);

	PlayersPostRender::Spawn(rFrame, rFrameInput, rStaticData);

	engine::ForEachPostRenderSpawn(GamePostRenderTypes {}, rFrame, rStaticData);

	FrameInterpolate& rInterpolate = rFrame.interpolate;
	if (rFrame.interpolate.gameFlags & GameFlags::kMainMenu)
	{
		return;
	}

	// Each interval attempts a fleet capped by kiMaximumFleetSize and the non-exploding player count; invalid positions are skipped.
	while (rInterpolate.fSpawnTimer >= kfSpaceshipSpawnInterval)
	{
		rInterpolate.fSpawnTimer -= kfSpaceshipSpawnInterval;
		SpawnSpaceshipGroup(rFrame, rStaticData);
	}
}

void FramePostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderPreCollision);

	engine::ForEachPostRenderPreCollision(engine::PostRenderBaseTypes {}, rFrame, rPreviousFrame, rStaticData);

	PlayersPostRender::PreCollision(rFrame, rPreviousFrame, rStaticData);

	engine::ForEachPostRenderPreCollision(GamePostRenderTypes {}, rFrame, rPreviousFrame, rStaticData);
}

void FramePostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderPostCollision);

	engine::ForEachPostRenderPostCollision(engine::PostRenderBaseTypes {}, rFrame, rPreviousFrame, rStaticData);

	PlayersPostRender::PostCollision(rFrame, rPreviousFrame, rStaticData);

	engine::ForEachPostRenderPostCollision(GamePostRenderTypes {}, rFrame, rPreviousFrame, rStaticData);

	engine::Collision::siLayerCount = 0;
}

void FramePostRender::AreaDamage([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderAreaDamage);

	engine::ForEachPostRenderAreaDamage(engine::PostRenderBaseTypes {}, rFrame, rPreviousFrame, rStaticData);

	engine::ForEachPostRenderAreaDamage(GamePostRenderTypes {}, rFrame, rPreviousFrame, rStaticData);

	engine::AreaDamage::siAreaDamageSourceCount = 0;
}

// Both spatial windows bind the same single spaceship source layer and the same missile subscription layer; they
// differ only in which arrival-grace column decides eligibility and whether previous positions are available.
RegistryWindow BuildSpaceshipRegistryWindow(const Frame& rFrame, const XMVECTOR* pVecPreviousPositions, const float* pfArrivalGracePeriods, const MissilesPostRender& rSubscribers)
{
	const SpaceshipsInterpolate& rSpaceships = *rFrame.interpolate.pSpaceships;
	const SpaceshipsPostRender& rSpaceshipsPostRender = *rFrame.postRender.pSpaceships;
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	int64_t iSpaceshipCount = rSpaceships.iCount;
	int64_t iSubscriberCount = rSubscribers.iCount;

	// A spaceship is targetable after its arrival grace expires while it publishes a registry id.
	auto IsEligible = [&](int64_t i)
	{
		return (rSpaceships.puiRegistryIds[i].uuid.iValue != 0) && pfArrivalGracePeriods[i] <= 0.0f;
	};

	int64_t iEligibleCount = 0;
	for (int64_t i = 0; i < iSpaceshipCount; ++i)
	{
		iEligibleCount += IsEligible(i) ? 1 : 0;
	}

	// Reserve all window storage together. PushBuffer may grow the workbuffer, so no pointer is published until
	// the combined reservation has returned. The source rows, registry scratch, padding, and layer descriptor
	// then remain in one allocation for the context's lifetime.
	int64_t iAscendingCount = std::max(iSpaceshipCount, iSubscriberCount);
	int64_t iAscendingBytes = iAscendingCount * static_cast<int64_t>(sizeof(int64_t));
	int64_t iScratchBytes = engine::RegistryScratchBytes(iEligibleCount);
	int64_t iLayerOffset = common::RoundUp(iAscendingBytes + iScratchBytes, 16i64);
	int64_t iTotalBytes = iLayerOffset + static_cast<int64_t>(sizeof(engine::RegistrySourceLayer));
	auto pBuffer = rWorkbuffer.PushBuffer<std::byte*>(iTotalBytes);
	std::byte* pBufferBytes = static_cast<std::byte*>(pBuffer.mpData);
	int64_t* pAscendingRows = reinterpret_cast<int64_t*>(pBufferBytes);
	std::byte* pScratch = pBufferBytes + iAscendingBytes;
	engine::RegistrySourceLayer* pLayers = reinterpret_cast<engine::RegistrySourceLayer*>(pBufferBytes + iLayerOffset);

	std::iota(pAscendingRows, pAscendingRows + iAscendingCount, 0i64);

	// The registry block stores caller-filled eligible rows before derived subscriber counts; ascending row order
	// resolves exact ranking ties deterministically.
	int64_t* pEligibleRows = reinterpret_cast<int64_t*>(pScratch);
	int64_t iEligibleRow = 0;
	for (int64_t i = 0; i < iSpaceshipCount; ++i)
	{
		if (IsEligible(i))
		{
			pEligibleRows[iEligibleRow++] = i;
		}
	}

	pLayers[0] =
	{
		.pIds = rSpaceships.puiRegistryIds,
		.pVecCurrentPositions = rSpaceships.pVecPositions,
		.pVecPreviousPositions = pVecPreviousPositions,
		.pAlignments = rSpaceshipsPostRender.pAlignments,
		.rows = std::span<const int64_t>(pEligibleRows, static_cast<size_t>(iEligibleCount)),
		.iSourceCount = iSpaceshipCount,
	};
	std::span<const engine::RegistrySourceLayer> sourceLayers(pLayers, 1);

	engine::RegistrySubscriptionLayer subscriptionLayer
	{
		.pTargets = rSubscribers.puiRegistryTargets,
		.rows = std::span<const int64_t>(pAscendingRows, static_cast<size_t>(iSubscriberCount)),
		.iSourceCount = iSubscriberCount,
	};

	return
	{
		.buffer = std::move(pBuffer),
		.context = engine::BuildRegistryQueryContext(rFrame.postRender.alignments, sourceLayers, std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1), std::span<std::byte>(pScratch, static_cast<size_t>(iScratchBytes))),
	};
}

engine::RegistryOwnershipLayer Frame::OwnershipLayer(const Frame& rFrame)
{
	PlayersPostRender& rPlayers = *rFrame.postRender.pPlayers;

	return
	{
		.pIdBytes = engine::RegistryIdBytes(rPlayers.pIds),
		.pGlobalIds = rPlayers.pGlobalPlayerIds,
		.pClientGuids = rPlayers.pClientGuids,
		.iCount = rPlayers.iCount,
	};
}

#if defined(BT_CLIENT)
void FrameInterpolate::BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	engine::ExplosionsInterpolate::siTotalCount = 0;
	engine::ForEachBeginRender(engine::InterpolateTypes {}, iCommandBuffer, rRenderInterpolates, rActiveCoordinates);

	PlayersInterpolate::BeginRender(iCommandBuffer, rRenderInterpolates, rActiveCoordinates);

	engine::ForEachBeginRender(GameInterpolateTypes {}, iCommandBuffer, rRenderInterpolates, rActiveCoordinates);

	// Population counters: one sum over the cells this transaction renders, published once so a render with no
	// renderable cell reads zero.
	int64_t iTotalPlayers = 0;
	int64_t iTotalBlasters = 0;
	int64_t iTotalMissiles = 0;
	int64_t iTotalSpaceships = 0;
	for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		if (it != rRenderInterpolates.end())
		{
			iTotalPlayers += it->second.pPlayers->iCount;
			iTotalBlasters += it->second.pBlasters->iCount;
			iTotalMissiles += it->second.pMissiles->iCount;
			iTotalSpaceships += it->second.pSpaceships->iCount;
		}
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterPlayers).iCount = iTotalPlayers;
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
		gpProfileManager->GetCpuCounter(kCpuCounterSpaceships).iCount = iTotalSpaceships;
	}
}

void FrameInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(kCpuTimerRender);

	// Parent (excludes manually rendered collections)
	engine::ExplosionsInterpolate::siTotalCount += rFrameInterpolate.explosions.iCount;
	engine::ForEachInterpolateRender(engine::InterpolateTypes {}, rFrameInterpolate, iCommandBuffer);

	PlayersInterpolate::Render(rFrameInterpolate, iCommandBuffer);

	engine::ForEachInterpolateRender(GameInterpolateTypes {}, rFrameInterpolate, iCommandBuffer);
}

void FrameInterpolate::EndRender(int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(engine::kCpuCounterExplosions).iCount = engine::ExplosionsInterpolate::siTotalCount;
	}
	engine::ForEachEndRender(engine::InterpolateTypes {}, iCommandBuffer);

	PlayersInterpolate::EndRender(iCommandBuffer);

	engine::ForEachEndRender(GameInterpolateTypes {}, iCommandBuffer);
}

#endif // BT_CLIENT

common::crc_t FrameInterpolate::Crcs(const FrameInterpolate& rCurrent)
{
	common::crc_t uiSharedCrc = static_cast<const engine::FrameInterpolateBase&>(rCurrent).Crcs();

	uiSharedCrc = (uiSharedCrc ^ common::Crc(rCurrent.gameFlags)) * common::kCrcMultiplier;
	uiSharedCrc = (uiSharedCrc ^ common::Crc(rCurrent.fSpawnTimer)) * common::kCrcMultiplier;

	// A SharedCrcMembers entry absent from SharedMembers would CRC client-local state — permanent false desync
	ASSERT(engine::IsMemberTupleSubset(rCurrent.pPlayers->SharedCrcMembers(), rCurrent.pPlayers->SharedMembers()));
	uiSharedCrc = (uiSharedCrc ^ engine::CollectionCrc(*rCurrent.pPlayers, rCurrent.pPlayers->SharedCrcMembers())) * common::kCrcMultiplier;

	uiSharedCrc = engine::CollectionsCrc(uiSharedCrc, GameInterpolateCollections(rCurrent));

	return uiSharedCrc;
}

bool FrameInterpolate::LogDifferences(const FrameInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("FrameInterpolate");
	bool bEqual = true;
	bEqual &= static_cast<const engine::FrameInterpolateBase&>(*this).LogDifferences(static_cast<const engine::FrameInterpolateBase&>(rOther));
	bEqual &= common::LogDifference<"fSpawnTimer">(fSpawnTimer, rOther.fSpawnTimer);
	bEqual &= common::LogDifference<"gameFlags">(gameFlags, rOther.gameFlags);
	bEqual &= pPlayers->LogDifferences(*rOther.pPlayers);
	bEqual &= engine::LogDifferencesCollections(GameInterpolateCollections(*this), GameInterpolateCollections(rOther), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(GameInterpolateCollections(*this))>)> {});
	return bEqual;
}

void FrameInterpolate::Write(std::ostream& rStream) const
{
	static_cast<const engine::FrameInterpolateBase&>(*this).Write(rStream);

	engine::FrameValuesWrite(rStream, *this, Values());

	engine::CollectionWrite(rStream, *pPlayers, pPlayers->Members());

	engine::CollectionsWrite(rStream, GameInterpolateCollections(*this));
}

void FrameInterpolate::Read(std::istream& rStream)
{
	static_cast<engine::FrameInterpolateBase&>(*this).Read(rStream);

	engine::FrameValuesRead<false>(rStream, *this, Values());
	fSpawnTimer = AdmitSpawnTimer(fSpawnTimer);

	engine::CollectionRead(rStream, *pPlayers, pPlayers->Members());

	engine::CollectionsRead(rStream, GameInterpolateCollections(*this));
}

void FrameInterpolate::ServerRead(std::istream& rStream)
{
	static_cast<engine::FrameInterpolateBase&>(*this).ServerRead(rStream);

	engine::FrameValuesRead<true>(rStream, *this, Values());

	engine::SharedCollectionRead(rStream, *pPlayers);

	engine::SharedCollectionsRead(rStream, GameInterpolateCollections(*this));
}

common::crc_t FramePostRender::Crcs(const FramePostRender& rCurrent)
{
	common::crc_t uiSharedCrc = static_cast<const engine::FramePostRenderBase&>(rCurrent).Crcs();

	uiSharedCrc = (uiSharedCrc ^ common::Crc(rCurrent.enemyAlignment)) * common::kCrcMultiplier;
	uiSharedCrc = (uiSharedCrc ^ common::Crc(rCurrent.playerAlignment)) * common::kCrcMultiplier;

	// A SharedCrcMembers entry absent from SharedMembers would CRC client-local state — permanent false desync
	ASSERT(engine::IsMemberTupleSubset(rCurrent.pPlayers->SharedCrcMembers(), rCurrent.pPlayers->SharedMembers()));
	uiSharedCrc = (uiSharedCrc ^ engine::CollectionCrc(*rCurrent.pPlayers, rCurrent.pPlayers->SharedCrcMembers())) * common::kCrcMultiplier;

	uiSharedCrc = engine::CollectionsCrc(uiSharedCrc, GamePostRenderCollections(rCurrent));

	return uiSharedCrc;
}

bool FramePostRender::LogDifferences(const FramePostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("FramePostRender");
	bool bEqual = true;
	bEqual &= static_cast<const engine::FramePostRenderBase&>(*this).LogDifferences(static_cast<const engine::FramePostRenderBase&>(rOther));
	bEqual &= common::LogDifference<"enemyAlignment">(enemyAlignment, rOther.enemyAlignment);
	bEqual &= common::LogDifference<"playerAlignment">(playerAlignment, rOther.playerAlignment);
	bEqual &= pPlayers->LogDifferences(*rOther.pPlayers);
	bEqual &= engine::LogDifferencesCollections(GamePostRenderCollections(*this), GamePostRenderCollections(rOther), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(GamePostRenderCollections(*this))>)> {});
	return bEqual;
}

void FramePostRender::Write(std::ostream& rStream) const
{
	static_cast<const engine::FramePostRenderBase&>(*this).Write(rStream);

	engine::FrameValuesWrite(rStream, *this, Values());

	engine::CollectionWrite(rStream, *pPlayers, pPlayers->Members());

	engine::CollectionsWrite(rStream, GamePostRenderCollections(*this));
}

void FramePostRender::Read(std::istream& rStream)
{
	static_cast<engine::FramePostRenderBase&>(*this).Read(rStream);

	engine::FrameValuesRead<false>(rStream, *this, Values());

	engine::CollectionRead(rStream, *pPlayers, pPlayers->Members());

	engine::CollectionsRead(rStream, GamePostRenderCollections(*this));
}

void FramePostRender::ServerRead(std::istream& rStream)
{
	static_cast<engine::FramePostRenderBase&>(*this).ServerRead(rStream);

	engine::FrameValuesRead<true>(rStream, *this, Values());

	engine::SharedCollectionRead(rStream, *pPlayers);

	engine::SharedCollectionsRead(rStream, GamePostRenderCollections(*this));
}

bool PrepareTransferRequest(const FramePostRender& rPostRender, const engine::CellBounds& rBounds, TransferRequest& rRequest)
{
	int64_t iDeltaX = 0;
	int64_t iDeltaY = 0;
	engine::ComputeTransferDelta(rBounds, rRequest.data.vecPosition, iDeltaX, iDeltaY);
	rRequest.iDeltaX = static_cast<int8_t>(iDeltaX);
	rRequest.iDeltaY = static_cast<int8_t>(iDeltaY);

	// The payload leaves here already expressed in the destination cell's local frame: one cell width per
	// transferred axis. Every downstream consumer — network transfer, SpawnTransfer, replay reconcile —
	// forwards the position unchanged, so this is the single conversion point.
	rRequest.data.vecPosition = XMVectorSubtract(rRequest.data.vecPosition, XMVectorSet(static_cast<float>(rRequest.iDeltaX) * engine::kfCellWidth, static_cast<float>(rRequest.iDeltaY) * engine::kfCellHeight, 0.0f, 0.0f));

	// Transfer producers are unbounded, so a full buffer can reallocate during a shared per-tick burst.
	return std::ssize(rPostRender.transferRequests) == static_cast<int64_t>(rPostRender.transferRequests.capacity());
}

void PushTransferRequest(FramePostRender& rPostRender, const TransferRequest& rRequest)
{
	common::ValidateVector<true >(rRequest.data.vecPosition);
	common::ValidateVector<false>(rRequest.data.vecDirection);
	common::ValidateVector<false>(rRequest.data.vecVelocity);
	{
		// Heap: no finite reserve can be proven sufficient because engine::GrowPairedCollections
		// grows collections unbounded
		ScopedSuppressAllocationTracking suppress;
		rPostRender.transferRequests.push_back(rRequest);
	}
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

	engine::FrameInterpolateBase& rInterpolateBase = interpolate;
	engine::FramePostRenderBase& rPostRenderBase = postRender;
	engine::ValidateCollectionPairs(rInterpolateBase.ServerCollections(), rPostRenderBase.ServerCollections());
	engine::ValidateCollectionPair(*interpolate.pPlayers, *postRender.pPlayers);
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

	engine::FrameInterpolateBase& rInterpolateBase = loadedFrame.interpolate;
	engine::FramePostRenderBase& rPostRenderBase = loadedFrame.postRender;
	engine::ValidateCollectionPairs(rInterpolateBase.Collections(), rPostRenderBase.Collections());
	engine::ValidateCollectionPair(*loadedFrame.interpolate.pPlayers, *loadedFrame.postRender.pPlayers);
	engine::ValidateCollectionPairs(GameInterpolateCollections(loadedFrame.interpolate), GamePostRenderCollections(loadedFrame.postRender));
	rCurrent = std::move(loadedFrame);
	return rStream;
}

} // namespace game
