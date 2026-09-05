#include "Frame.h"

#include "Frame/FrameCollections.h"
#include "Profile/ProfileManager.h"
#include "Frame/Collections/Players/Players.h"
#include "Ui/LightingWrappers.h"
#include "Ui/SmokeWrappers.h"
#include "Ui/WindDepositsWrappers.h"

namespace game
{

using enum GameFlags;

// Bump this base on any change that shifts computed frame CRCs without bumping a collection's own kiVersion
// — notably the CRC mixing algorithm/constants in Common/Crc.h. This gate is the only thing distinguishing
// "data desynced" from "checksum algorithm changed"; skipping the bump makes straddling replays false-desync.
const int64_t Frame::kiVersion = 127 + engine::kiNavDataVersion + BlastersInterpolate::kiVersion + BlastersPostRender::kiVersion + MissilesInterpolate::kiVersion + MissilesPostRender::kiVersion + PlayersInterpolate::kiVersion + PlayersPostRender::kiVersion + SpaceshipsInterpolate::kiVersion + SpaceshipsPostRender::kiVersion + engine::ExplosionsInterpolate::kiVersion + engine::PushersInterpolate::kiVersion + engine::PushersPostRender::kiVersion + engine::ExplosionsPostRender::kiVersion;

// FrameInterpolate
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

// FramePostRender
FramePostRender::FramePostRender()
: pPlayers(std::make_unique<PlayersPostRender>())
, pBlasters(std::make_unique<BlastersPostRender>())
, pMissiles(std::make_unique<MissilesPostRender>())
, pSpaceships(std::make_unique<SpaceshipsPostRender>())
{
	transferRequests.reserve(engine::kuiInitialTransferCapacity);
}

FramePostRender::~FramePostRender() = default;
FramePostRender::FramePostRender(FramePostRender&&) noexcept = default;
FramePostRender& FramePostRender::operator=(FramePostRender&&) noexcept = default;

// Frame
Frame::Frame() = default;
Frame::~Frame() = default;
Frame::Frame(Frame&&) noexcept = default;
Frame& Frame::operator=(Frame&&) noexcept = default;

void FrameInterpolate::Register()
{
#if defined(BT_CLIENT)
	// Explosion tuning is game content driven by the Tweaks sliders; the engine owns only the effect mechanism.
	// This must complete before the parent Register() below, which reads every field.
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

	// Parent
	FrameInterpolateBase::Register();

	// Player
	PlayersInterpolate::Register();

	// Collections
	engine::ForEachRegister(GameInterpolateTypes{});
}

#if defined(BT_CLIENT)
void FrameInterpolate::GraphicsResources()
{
	// Parent
	FrameInterpolateBase::GraphicsResources();

	// Player
	PlayersInterpolate::GraphicsResources();

	// Collections
	engine::ForEachGraphicsResources(GameInterpolateTypes{});
}
#endif // BT_CLIENT

void FrameInterpolate::AllocateAndCopy(FrameInterpolate& __restrict rCurrent, const FrameInterpolate& __restrict rPrevious)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateAllocateAndCopy);

	// Parent
	FrameInterpolateBase::AllocateAndCopy(rCurrent, rPrevious);

	// Player
	PlayersInterpolate::AllocateAndCopy(*rCurrent.pPlayers, *rPrevious.pPlayers);

	// Collections
	engine::AllocateAndCopyCollections(GameInterpolateCollections(rCurrent), GameInterpolateCollections(rPrevious), std::make_index_sequence<std::tuple_size_v<decltype(GameInterpolateCollections(rCurrent))>>{});
}

void FrameInterpolate::Update(FrameInterpolate& __restrict rCurrent, const Frame& __restrict rPreviousFrame, float fDeltaTime)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateUpdate);

	const FrameInterpolate& rPrevious = rPreviousFrame.interpolate;

	// Parent
	FrameInterpolateBase::Update(rCurrent, rPreviousFrame, fDeltaTime);

	// Load
	GameFlags_t gameFlags = rPrevious.gameFlags;
	float fSpawnTimer = rPrevious.fSpawnTimer;

	// Main-menu frames hold the spawn timer at its zero initialization: Spawn returns before draining it, so the
	// timer must not accumulate an undrained backlog.
	if (!(gameFlags & GameFlags::kMainMenu))
	{
		fSpawnTimer += fDeltaTime;
	}

	// Save
	rCurrent.gameFlags = gameFlags;
	rCurrent.fSpawnTimer = fSpawnTimer;

	// Player
	PlayersInterpolate::Update(rCurrent, rPreviousFrame);

	// Collections
	engine::ForEachInterpolateUpdate(GameInterpolateTypes{}, rCurrent, rPreviousFrame);
}

void FramePostRender::AllocateAndCopy(FramePostRender& __restrict rCurrent, const FramePostRender& __restrict rPrevious)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderAllocateAndCopy);

	// Parent
	engine::FramePostRenderBase::AllocateAndCopy(rCurrent, rPrevious);

	// Player
	PlayersPostRender::AllocateAndCopy(*rCurrent.pPlayers, *rPrevious.pPlayers);

	// Collections
	engine::AllocateAndCopyCollections(GamePostRenderCollections(rCurrent), GamePostRenderCollections(rPrevious), std::make_index_sequence<std::tuple_size_v<decltype(GamePostRenderCollections(rCurrent))>>{});
}

void FramePostRender::Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const FrameInput& __restrict rFrameInput, const engine::FrameStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderUpdate);

	rFrame.postRender.transferRequests.clear();

	// Parent
	FramePostRenderBase::Update(rFrame, rPreviousFrame, rFrameInput, rStaticData);

	// Propagate game-specific fields
	rFrame.postRender.enemyAlignment = rPreviousFrame.postRender.enemyAlignment;
	rFrame.postRender.playerAlignment = rPreviousFrame.postRender.playerAlignment;

	// Player
	PlayersPostRender::Update(rFrame, rPreviousFrame, rStaticData);
	PlayersPostRender::ProcessUpdateStatusChanges(rFrame, rFrameInput, rStaticData);

	// Collections
	engine::ForEachPostRenderUpdate(GamePostRenderTypes{}, rFrame, rPreviousFrame, rStaticData);
}

void FramePostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	// Parent
	FramePostRenderBase::Transfer(rFrame, rStaticData);

	// Player
	PlayersPostRender::Transfer(rFrame, rStaticData);

	// Collections
	engine::ForEachPostRenderTransfer(GamePostRenderTypes{}, rFrame, rStaticData);
}

void FramePostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderDestroy);

	// Parent
	FramePostRenderBase::Destroy(rFrame, rStaticData);

	// Player
	PlayersPostRender::Destroy(rFrame, rStaticData);

	// Collections
	engine::ForEachPostRenderDestroy(GamePostRenderTypes{}, rFrame, rStaticData);
}

static void SpawnSpaceshipGroup(Frame& __restrict rFrame, const engine::FrameStaticData& rStaticData)
{
	FrameInterpolate& rInterpolate = rFrame.interpolate;

	constexpr int64_t kiGridDim = 20;
	constexpr int64_t kiMaxFleetSize = 16;
	constexpr float kfTerrainClearance = kfSpaceshipRadius * 2.0f;
	constexpr float kfMinPlayerDistance = 120.0f;
	constexpr float kfDesiredAnchorDistance = 150.0f;
	constexpr float kfChevronStagger = kfSpaceshipRadius * 2.0f;
	constexpr float kfShipSideSpacing = kfSpaceshipRadius * 3.0f;

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
	int64_t iShipCount = std::min(iSpawnCount, kiMaxFleetSize);

	// Reject positions outside the cell, inside terrain (with full body clearance), or within visible range of any alive player
	auto IsSpawnPositionValid = [&](FXMVECTOR vecPosition) -> bool
	{
		if (!common::InsideArea(vecPosition, rStaticData.vecArea))
		{
			return false;
		}
		if (engine::gpIslandTerrain->FrameElevation(rStaticData, vecPosition) > engine::gBaseHeight.Get() - kfTerrainClearance)
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
			if (XMVectorGetX(XMVector3LengthSq(vecDelta)) < kfMinPlayerDistance * kfMinPlayerDistance)
			{
				return false;
			}
		}
		return true;
	};

	// Cell-area extents and grid pitch (vecArea layout: x=minX, y=maxY, z=maxX, w=minY — see common::InsideArea)
	XMFLOAT4A f4Area;
	XMStoreFloat4A(&f4Area, rStaticData.vecArea);
	float fAreaMinX = f4Area.x;
	float fAreaMinY = f4Area.w;
	float fPitchX = (f4Area.z - f4Area.x) / static_cast<float>(kiGridDim);
	float fPitchY = (f4Area.y - f4Area.w) / static_cast<float>(kiGridDim);

	// Step 1: rasterize cell into a validity grid sampled at cell centers
	bool aValidGrid[kiGridDim * kiGridDim] {};
	for (int64_t iGridY = 0; iGridY < kiGridDim; ++iGridY)
	{
		for (int64_t iGridX = 0; iGridX < kiGridDim; ++iGridX)
		{
			auto vecGridCell = XMVectorSet(fAreaMinX + (static_cast<float>(iGridX) + 0.5f) * fPitchX, fAreaMinY + (static_cast<float>(iGridY) + 0.5f) * fPitchY, engine::gBaseHeight.Get(), 1.0f);
			aValidGrid[iGridY * kiGridDim + iGridX] = IsSpawnPositionValid(vecGridCell);
		}
	}

	// Step 2: chevron template — anchor at front, ships fan back-and-side in local frame (forward = +x)
	float fCenterOffset = static_cast<float>(iShipCount - 1) * 0.5f;
	XMFLOAT2 aLocalOffsets[kiMaxFleetSize] {};
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
	float fBestFacingCos = 1.0f;
	float fBestFacingSin = 0.0f;
	for (int64_t iGridY = 0; iGridY < kiGridDim; ++iGridY)
	{
		for (int64_t iGridX = 0; iGridX < kiGridDim; ++iGridX)
		{
			float fAnchorX = fAreaMinX + (static_cast<float>(iGridX) + 0.5f) * fPitchX;
			float fAnchorY = fAreaMinY + (static_cast<float>(iGridY) + 0.5f) * fPitchY;

			// Facing direction: anchor -> spawn-center player (XY only)
			float fToPlayerX = XMVectorGetX(vecPlayerPosition) - fAnchorX;
			float fToPlayerY = XMVectorGetY(vecPlayerPosition) - fAnchorY;
			float fDistance = std::sqrt(fToPlayerX * fToPlayerX + fToPlayerY * fToPlayerY);
			if (fDistance < kfMinPlayerDistance)
			{
				continue;
			}
			float fFacingCos = fToPlayerX / fDistance;
			float fFacingSin = fToPlayerY / fDistance;

			// Score: count chevron ships landing on valid grid cells
			int64_t iScore = 0;
			for (int64_t i = 0; i < iShipCount; ++i)
			{
				float fLocalForward = aLocalOffsets[i].x;
				float fLocalSide = aLocalOffsets[i].y;
				float fWorldX = fAnchorX + fFacingCos * fLocalForward - fFacingSin * fLocalSide;
				float fWorldY = fAnchorY + fFacingSin * fLocalForward + fFacingCos * fLocalSide;
				int64_t iShipGridX = static_cast<int64_t>(std::floor((fWorldX - fAreaMinX) / fPitchX));
				int64_t iShipGridY = static_cast<int64_t>(std::floor((fWorldY - fAreaMinY) / fPitchY));
				if (iShipGridX < 0 || iShipGridX >= kiGridDim || iShipGridY < 0 || iShipGridY >= kiGridDim)
				{
					continue;
				}
				if (aValidGrid[iShipGridY * kiGridDim + iShipGridX])
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
				iBestAnchorIndex = iGridY * kiGridDim + iGridX;
				fBestFacingCos = fFacingCos;
				fBestFacingSin = fFacingSin;
			}
		}
	}
	if (iBestAnchorIndex < 0)
	{
		return;
	}

	// Step 4: place ships at the chosen anchor (subset fallback — skip ships whose exact position fails the precise validity check)
	int64_t iBestGridX = iBestAnchorIndex % kiGridDim;
	int64_t iBestGridY = iBestAnchorIndex / kiGridDim;
	float fBestAnchorX = fAreaMinX + (static_cast<float>(iBestGridX) + 0.5f) * fPitchX;
	float fBestAnchorY = fAreaMinY + (static_cast<float>(iBestGridY) + 0.5f) * fPitchY;
	for (int64_t i = 0; i < iShipCount; ++i)
	{
		float fLocalForward = aLocalOffsets[i].x;
		float fLocalSide = aLocalOffsets[i].y;
		float fWorldX = fBestAnchorX + fBestFacingCos * fLocalForward - fBestFacingSin * fLocalSide;
		float fWorldY = fBestAnchorY + fBestFacingSin * fLocalForward + fBestFacingCos * fLocalSide;
		auto vecSpawnPosition = XMVectorSet(fWorldX, fWorldY, engine::gBaseHeight.Get(), 1.0f);
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

void FramePostRender::Spawn([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const FrameInput& __restrict rFrameInput, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderSpawn);

	// Parent
	FramePostRenderBase::Spawn(rFrame, rStaticData);

	// Player
	PlayersPostRender::Spawn(rFrame, rFrameInput, rStaticData);

	// Collections
	engine::ForEachPostRenderSpawn(GamePostRenderTypes{}, rFrame, rStaticData);

	FrameInterpolate& rInterpolate = rFrame.interpolate;
	if (rFrame.interpolate.gameFlags & GameFlags::kMainMenu)
	{
		return;
	}

	// Spawn one spaceship per player every half second
	constexpr float kfSpawnInterval = 0.5f;
	while (rInterpolate.fSpawnTimer >= kfSpawnInterval)
	{
		rInterpolate.fSpawnTimer -= kfSpawnInterval;
		SpawnSpaceshipGroup(rFrame, rStaticData);
	}
}

void FramePostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderPreCollision);

	// Parent
	FramePostRenderBase::PreCollision(rFrame, rPreviousFrame, rStaticData);

	// Player
	PlayersPostRender::PreCollision(rFrame, rPreviousFrame, rStaticData);

	// Collections
	engine::ForEachPostRenderPreCollision(GamePostRenderTypes{}, rFrame, rPreviousFrame, rStaticData);
}

void FramePostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderPostCollision);

	// Parent
	FramePostRenderBase::PostCollision(rFrame, rPreviousFrame, rStaticData);

	// Player
	PlayersPostRender::PostCollision(rFrame, rPreviousFrame, rStaticData);

	// Collections
	engine::ForEachPostRenderPostCollision(GamePostRenderTypes{}, rFrame, rPreviousFrame, rStaticData);

	engine::Collision::Clear();
}

void FramePostRender::AreaDamage([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderAreaDamage);

	// Parent
	FramePostRenderBase::AreaDamage(rFrame, rPreviousFrame, rStaticData);

	// Collections
	engine::ForEachPostRenderAreaDamage(GamePostRenderTypes{}, rFrame, rPreviousFrame, rStaticData);

	engine::AreaDamage::Clear();
}

// Both spatial windows bind the same single spaceship source layer and the same missile subscription layer; they
// differ only in which arrival-grace column decides eligibility and whether previous positions are available.
static RegistryWindow BuildSpaceshipRegistryWindow(const Frame& rFrame, const XMVECTOR* pVecPreviousPositions, const float* pfArrivalGracePeriods, const MissilesPostRender& rSubscribers)
{
	const SpaceshipsInterpolate& rSpaceships = *rFrame.interpolate.pSpaceships;
	const SpaceshipsPostRender& rSpaceshipsPostRender = *rFrame.postRender.pSpaceships;
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	int64_t iSpaceshipCount = rSpaceships.iCount;
	int64_t iSubscriberCount = rSubscribers.iCount;

	// A spaceship is targetable after its arrival grace expires while it publishes a registry id.
	auto IsEligible = [&](int64_t i) { return rSpaceships.puiRegistryIds[i].IsValid() && pfArrivalGracePeriods[i] <= 0.0f; };

	int64_t iEligibleCount = 0;
	for (int64_t i = 0; i < iSpaceshipCount; ++i)
	{
		iEligibleCount += IsEligible(i) ? 1 : 0;
	}

	// Reserve all window storage together. PushBuffer may grow the workbuffer, so no pointer is published until
	// the combined reservation has returned. The source rows, registry scratch, padding, and layer descriptor
	// then remain in one allocation for the context's lifetime.
	const int64_t iAscendingCount = std::max(iSpaceshipCount, iSubscriberCount);
	const int64_t iAscendingBytes = iAscendingCount * static_cast<int64_t>(sizeof(int64_t));
	const int64_t iScratchBytes = engine::RegistryScratchBytes(iEligibleCount);
	const int64_t iLayerOffset = common::RoundUp(iAscendingBytes + iScratchBytes, static_cast<int64_t>(16));
	const int64_t iTotalBytes = iLayerOffset + static_cast<int64_t>(sizeof(engine::RegistrySourceLayer));
	auto pBuffer = rWorkbuffer.PushBuffer<std::byte*>(iTotalBytes);
	std::byte* pBufferBytes = static_cast<std::byte*>(pBuffer);
	int64_t* pAscendingRows = reinterpret_cast<int64_t*>(pBufferBytes);
	std::byte* pScratch = pBufferBytes + iAscendingBytes;
	engine::RegistrySourceLayer* pLayers = reinterpret_cast<engine::RegistrySourceLayer*>(pBufferBytes + iLayerOffset);

	for (int64_t i = 0; i < iAscendingCount; ++i)
	{
		pAscendingRows[i] = i;
	}

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
		.puiIds = rSpaceships.puiRegistryIds,
		.pVecCurrentPositions = rSpaceships.pVecPositions,
		.pVecPreviousPositions = pVecPreviousPositions,
		.pAlignments = rSpaceshipsPostRender.pAlignments,
		.rows = std::span<const int64_t>(pEligibleRows, static_cast<size_t>(iEligibleCount)),
		.iSourceCount = iSpaceshipCount,
	};
	std::span<const engine::RegistrySourceLayer> sourceLayers(pLayers, 1);

	engine::RegistrySubscriptionLayer subscriptionLayer
	{
		.puiTargets = rSubscribers.puiRegistryTargets,
		.rows = std::span<const int64_t>(pAscendingRows, static_cast<size_t>(iSubscriberCount)),
		.iSourceCount = iSubscriberCount,
	};

	return
	{
		.buffer = std::move(pBuffer),
		.context = engine::BuildRegistryQueryContext(rFrame.postRender.alignments, sourceLayers, std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1), std::span<std::byte>(pScratch, static_cast<size_t>(iScratchBytes))),
	};
}

RegistryWindow Frame::MissileUpdateWindow(const Frame& rFrame, const Frame& rPreviousFrame)
{
	// Missiles update before Spaceships, so the current rows still line up with the previous frame's: previous
	// positions are what a retained missile homes on, and previous arrival grace is the eligibility this phase saw.
	return BuildSpaceshipRegistryWindow(rFrame, rPreviousFrame.interpolate.pSpaceships->pVecPositions, rPreviousFrame.postRender.pSpaceships->pfArrivalGracePeriods, *rPreviousFrame.postRender.pMissiles);
}

RegistryWindow Frame::PlayerSpawnWindow(const Frame& rFrame)
{
	// Transfer and Destroy have already moved rows this tick, so previous-frame rows no longer line up and no
	// previous positions are bound. A missile acquiring here does not home until the next tick, which needs none.
	return BuildSpaceshipRegistryWindow(rFrame, nullptr, rFrame.postRender.pSpaceships->pfArrivalGracePeriods, *rFrame.postRender.pMissiles);
}

engine::RegistryOwnershipLayer Frame::OwnershipLayer(const Frame& rFrame)
{
	PlayersPostRender& rPlayers = *rFrame.postRender.pPlayers;

	return
	{
		.pIdBytes = engine::RegistryIdBytes(rPlayers.puiIds),
		.pGlobalIds = rPlayers.pGlobalPlayerIds,
		.pClientGuids = rPlayers.pClientGuids,
		.iCount = rPlayers.iCount,
	};
}

#if defined(BT_CLIENT)
void FrameInterpolate::BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoords)
{
	// Parent
	engine::FrameInterpolateBase::BeginRender(iCommandBuffer, rRenderInterpolates, rActiveCoords);

	// Player
	PlayersInterpolate::BeginRender(iCommandBuffer, rRenderInterpolates, rActiveCoords);

	// Collections
	engine::ForEachBeginRender(GameInterpolateTypes{}, iCommandBuffer, rRenderInterpolates, rActiveCoords);
}

void FrameInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(kCpuTimerRender);

	// Parent (excludes manually rendered collections)
	engine::FrameInterpolateBase::Render(rFrameInterpolate, iCommandBuffer);

	// Player
	PlayersInterpolate::Render(rFrameInterpolate, iCommandBuffer);

	// Collections
	engine::ForEachInterpolateRender(GameInterpolateTypes{}, rFrameInterpolate, iCommandBuffer);
}

void FrameInterpolate::EndRender(int64_t iCommandBuffer)
{
	// Parent
	engine::FrameInterpolateBase::EndRender(iCommandBuffer);

	// Player
	PlayersInterpolate::EndRender(iCommandBuffer);

	// Collections
	engine::ForEachEndRender(GameInterpolateTypes{}, iCommandBuffer);
}

void FrameInterpolate::DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coord)
{
	PlayersInterpolate::DebugRender(rFrameInterpolate, coord);
}
#endif // BT_CLIENT

common::crc_t FrameInterpolate::Crcs(const FrameInterpolate& rCurrent)
{
	common::crc_t sharedCrc = static_cast<const engine::FrameInterpolateBase&>(rCurrent).Crcs();

	sharedCrc = (sharedCrc ^ common::Crc(rCurrent.gameFlags)) * common::kCrcMultiplier;
	sharedCrc = (sharedCrc ^ common::Crc(rCurrent.fSpawnTimer)) * common::kCrcMultiplier;

	// A SharedCrcMembers entry absent from SharedMembers would CRC client-local state — permanent false desync
	ASSERT(engine::IsMemberTupleSubset(rCurrent.pPlayers->SharedCrcMembers(), rCurrent.pPlayers->SharedMembers()));
	sharedCrc = (sharedCrc ^ engine::CollectionCrc(*rCurrent.pPlayers, rCurrent.pPlayers->SharedCrcMembers())) * common::kCrcMultiplier;

	sharedCrc = engine::CollectionsCrc(sharedCrc, GameInterpolateCollections(rCurrent));

	return sharedCrc;
}

bool FrameInterpolate::LogDifferences(const FrameInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("FrameInterpolate");
	bool bEqual = true;
	bEqual &= static_cast<const engine::FrameInterpolateBase&>(*this).LogDifferences(
		static_cast<const engine::FrameInterpolateBase&>(rOther));
	bEqual &= common::LogDifference<"fSpawnTimer">(fSpawnTimer, rOther.fSpawnTimer);
	bEqual &= common::LogDifference<"gameFlags">(gameFlags, rOther.gameFlags);
	bEqual &= pPlayers->LogDifferences(*rOther.pPlayers);
	bEqual &= engine::LogDifferencesCollections(GameInterpolateCollections(*this), GameInterpolateCollections(rOther), std::make_index_sequence<std::tuple_size_v<decltype(GameInterpolateCollections(*this))>>{});
	return bEqual;
}

void FrameInterpolate::Write(std::ostream& rStream) const
{
	static_cast<const engine::FrameInterpolateBase&>(*this).Write(rStream);

	common::Write(rStream, fSpawnTimer);
	common::Write(rStream, gameFlags);

	engine::CollectionWrite(rStream, *pPlayers, pPlayers->Members());

	engine::CollectionsWrite(rStream, GameInterpolateCollections(*this));
}

void FrameInterpolate::Read(std::istream& rStream)
{
	static_cast<engine::FrameInterpolateBase&>(*this).Read(rStream);

	common::Read(rStream, fSpawnTimer);
	fSpawnTimer = std::isfinite(fSpawnTimer) ? fSpawnTimer : 0.0f;
	common::Read(rStream, gameFlags);

	engine::CollectionRead(rStream, *pPlayers, pPlayers->Members());

	engine::CollectionsRead(rStream, GameInterpolateCollections(*this));
}

void FrameInterpolate::ServerRead(std::istream& rStream)
{
	static_cast<engine::FrameInterpolateBase&>(*this).ServerRead(rStream);

	common::Read(rStream, fSpawnTimer);
	fSpawnTimer = std::isfinite(fSpawnTimer) ? fSpawnTimer : 0.0f;
	common::Read(rStream, gameFlags);

	engine::SharedCollectionRead(rStream, *pPlayers);

	engine::SharedCollectionsRead(rStream, GameInterpolateCollections(*this));
}

common::crc_t FramePostRender::Crcs(const FramePostRender& rCurrent)
{
	common::crc_t sharedCrc = static_cast<const engine::FramePostRenderBase&>(rCurrent).Crcs();

	sharedCrc = (sharedCrc ^ common::Crc(rCurrent.enemyAlignment)) * common::kCrcMultiplier;
	sharedCrc = (sharedCrc ^ common::Crc(rCurrent.playerAlignment)) * common::kCrcMultiplier;

	// A SharedCrcMembers entry absent from SharedMembers would CRC client-local state — permanent false desync
	ASSERT(engine::IsMemberTupleSubset(rCurrent.pPlayers->SharedCrcMembers(), rCurrent.pPlayers->SharedMembers()));
	sharedCrc = (sharedCrc ^ engine::CollectionCrc(*rCurrent.pPlayers, rCurrent.pPlayers->SharedCrcMembers())) * common::kCrcMultiplier;

	sharedCrc = engine::CollectionsCrc(sharedCrc, GamePostRenderCollections(rCurrent));

	return sharedCrc;
}

bool FramePostRender::LogDifferences(const FramePostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("FramePostRender");
	bool bEqual = true;
	bEqual &= static_cast<const engine::FramePostRenderBase&>(*this).LogDifferences(
		static_cast<const engine::FramePostRenderBase&>(rOther));
	bEqual &= common::LogDifference<"enemyAlignment">(enemyAlignment, rOther.enemyAlignment);
	bEqual &= common::LogDifference<"playerAlignment">(playerAlignment, rOther.playerAlignment);
	bEqual &= pPlayers->LogDifferences(*rOther.pPlayers);
	bEqual &= engine::LogDifferencesCollections(GamePostRenderCollections(*this), GamePostRenderCollections(rOther), std::make_index_sequence<std::tuple_size_v<decltype(GamePostRenderCollections(*this))>>{});
	return bEqual;
}

void FramePostRender::Write(std::ostream& rStream) const
{
	static_cast<const engine::FramePostRenderBase&>(*this).Write(rStream);

	enemyAlignment.Write(rStream);
	playerAlignment.Write(rStream);

	engine::CollectionWrite(rStream, *pPlayers, pPlayers->Members());

	engine::CollectionsWrite(rStream, GamePostRenderCollections(*this));
}

void FramePostRender::Read(std::istream& rStream)
{
	static_cast<engine::FramePostRenderBase&>(*this).Read(rStream);

	enemyAlignment.Read(rStream);
	playerAlignment.Read(rStream);

	engine::CollectionRead(rStream, *pPlayers, pPlayers->Members());

	engine::CollectionsRead(rStream, GamePostRenderCollections(*this));
}

void FramePostRender::ServerRead(std::istream& rStream)
{
	static_cast<engine::FramePostRenderBase&>(*this).ServerRead(rStream);

	enemyAlignment.Read(rStream);
	playerAlignment.Read(rStream);

	engine::SharedCollectionRead(rStream, *pPlayers);

	engine::SharedCollectionsRead(rStream, GamePostRenderCollections(*this));
}

bool PrepareTransferRequest(FramePostRender& rPostRender, const engine::FrameBounds& rBounds, TransferRequest& rRequest)
{
	engine::ComputeTransferDelta(rBounds, rRequest.data.vecPosition, rRequest.iDeltaX, rRequest.iDeltaY);

	// Heap realloc warning: capacity exceeded during a shared per-tick burst. Producers are
	// unbounded, so investigate entities re-flagging kTransfer across iterations or an
	// unexpected push path.
	return rPostRender.transferRequests.size() == rPostRender.transferRequests.capacity();
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

common::crc_t Frame::Crcs() const
{
	common::crc_t crc = FrameInterpolate::Crcs(interpolate);
	crc = (crc ^ FramePostRender::Crcs(postRender)) * common::kCrcMultiplier;
	return crc;
}

common::crc_t Frame::Crc() const
{
	return Crcs();
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
