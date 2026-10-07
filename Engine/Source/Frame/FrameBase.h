#pragma once

#include "Frame/Alignments.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/AreaLights/AreaLights.h"
#include "Frame/Collections/Billboards/Billboards.h"
#endif
#include "Frame/Collections/Explosions/Explosions.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/HexShields/HexShields.h"
#include "Frame/Collections/PointLights/PointLights.h"
#include "Frame/Collections/Puffs/Puffs.h"
#endif
#include "Frame/Collections/Pushers/Pushers.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/Sounds/Sounds.h"
#include "Frame/Collections/SmokeTrails/SmokeTrails.h"
#include "Frame/Collections/WindRadials/WindRadials.h"
#include "Frame/Collections/WindTrails/WindTrails.h"
#endif
#include "Frame/FrameUtils.h"

namespace game
{

struct FrameInput;

} // namespace game

namespace engine
{

enum class FrameFlags : uint64_t
{
	kInterpolate  = 0x00000001,
	kPostRender   = 0x00000002,
	kRecalculated = 0x00000004,
};
using FrameFlags_t = common::Flags<FrameFlags>;

// A client-only value is absent from the cross-build stream that ServerRead consumes.
template <auto MEMBER>
inline constexpr bool kbClientOnlyValue = false;

template <typename OWNER, typename... COLUMNS>
void FrameValuesWrite(std::ostream& rStream, const OWNER& rOwner, FrameColumnList<COLUMNS...>)
{
	auto WriteValue = [&]<typename COLUMN>(COLUMN)
	{
		using Value = std::remove_cvref_t<decltype(rOwner.*COLUMN::kpMember)>;
		const Value& rValue = rOwner.*COLUMN::kpMember;
		if constexpr (requires { rValue.Write(rStream); })
		{
			rValue.Write(rStream);
		}
		else
		{
			common::Write(rStream, rValue);
		}
	};
	(WriteValue(COLUMNS {}), ...);
}

template <bool SHARED, typename OWNER, typename... COLUMNS>
void FrameValuesRead(std::istream& rStream, OWNER& rOwner, FrameColumnList<COLUMNS...>)
{
	auto ReadValue = [&]<typename COLUMN>(COLUMN)
	{
		if constexpr (!SHARED || !kbClientOnlyValue<COLUMN::kpMember>)
		{
			using Value = std::remove_cvref_t<decltype(rOwner.*COLUMN::kpMember)>;
			Value& rValue = rOwner.*COLUMN::kpMember;
			if constexpr (std::is_same_v<Value, common::RandomEngine>)
			{
				uint64_t uiRandomState = 0;
				common::Read(rStream, uiRandomState);
				rValue.SetSerializedState(uiRandomState);
			}
			else if constexpr (requires { rValue.Read(rStream); })
			{
				rValue.Read(rStream);
			}
			else
			{
				common::Read(rStream, rValue);
			}
		}
	};
	(ReadValue(COLUMNS {}), ...);
}

struct FrameInterpolateBase
{
	FrameInterpolateBase() = default;
	~FrameInterpolateBase() = default;
	FrameInterpolateBase(FrameInterpolateBase&&) noexcept = default;
	FrameInterpolateBase& operator=(FrameInterpolateBase&&) noexcept = default;


	static void AllocateAndCopy(game::FrameInterpolate& __restrict rCurrent, const game::FrameInterpolate& __restrict rPrevious);
	static void Update(game::FrameInterpolate& __restrict rCurrent, const game::Frame& __restrict rPreviousFrame, float fDeltaTime);

#if defined(BT_CLIENT)
	// Client-only: phase markers (kInterpolate/kPostRender) and the replay marker (kRecalculated).
	// Excluded from CRC, serialization, and LogDifferences — server never reads or sets them.
	FrameFlags_t frameFlags {FrameFlags::kPostRender};

	// Client-only: the cell this copy's positions are local to, plus that cell's offset from the camera cell
	// the render dispatch resolved when it stamped this copy. Every renderer converts with it at its single
	// position-to-GPU point; the position columns themselves are never rewritten. Excluded from CRC,
	// serialization, and LogDifferences — the server never reads or sets it.
	RenderBasis renderBasis {};
#endif
	int64_t iTick = 0;
	float fCurrentTime = 0.0f;
	float fDeltaTime = 0.0f;

#if defined(BT_CLIENT)
	AreaLightsInterpolate areaLights {};
	BillboardsInterpolate billboards {};
#endif
	ExplosionsInterpolate explosions {};
#if defined(BT_CLIENT)
	HexShieldsInterpolate hexShields {};
	PointLightsInterpolate pointLights {};
	PuffsInterpolate puffs {};
#endif
	PushersInterpolate pushers {};
#if defined(BT_CLIENT)
	SoundsInterpolate sounds {};
	SmokeTrailsInterpolate smokeTrails {};
	WindRadialsInterpolate windRadials {};
	WindTrailsInterpolate windTrails {};
#endif

	// Collection Sync dependencies determine tuple order: consumers may read an owner's newly synced
	// current-frame data during the same phase walk. ExplosionsInterpolate::Update calls SyncExplosionTrail
	// to sync smokeTrails positions before SmokeTrailsInterpolate::Update smooths them, so explosions
	// (client index 2) must precede smokeTrails (client index 8) to avoid a one-frame trail lag.
	// The collection-count and ServerCollections() static_asserts do not verify order.
	auto Collections(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tie(rSelf.areaLights, rSelf.billboards, rSelf.explosions, rSelf.hexShields, rSelf.pointLights, rSelf.puffs, rSelf.pushers, rSelf.sounds, rSelf.smokeTrails, rSelf.windRadials, rSelf.windTrails);
#else
		return std::tie(rSelf.explosions, rSelf.pushers);
#endif
	}

#if defined(BT_CLIENT)
	static constexpr int64_t kiCollectionCount = 11;
#else
	static constexpr int64_t kiCollectionCount = 2;
#endif

	auto ServerCollections(this auto&& rSelf)
	{
		return std::tie(rSelf.explosions, rSelf.pushers);
	}

	// Visibility bounds (X = East/West, Y = North/South)
	static constexpr float kfVisibleEastWest = 65.0f;
	static constexpr float kfVisibleNorthSouth = 45.0f;

	[[nodiscard]] static bool XM_CALLCONV IsVisible(FXMVECTOR vecSource, FXMVECTOR vecTarget)
	{
		float fDeltaX = std::abs(XMVectorGetX(vecTarget) - XMVectorGetX(vecSource));
		float fDeltaY = std::abs(XMVectorGetY(vecTarget) - XMVectorGetY(vecSource));
		return fDeltaX <= kfVisibleEastWest && fDeltaY <= kfVisibleNorthSouth;
	}

	// The frame-wide values Write, Read, and ServerRead serialize, in stream order.
	static auto Values()
	{
		return FrameColumnList<FRAME_COLUMN(FrameInterpolateBase, iTick), FRAME_COLUMN(FrameInterpolateBase, fCurrentTime), FRAME_COLUMN(FrameInterpolateBase, fDeltaTime)> {};
	}

	common::crc_t Crcs() const;
	bool LogDifferences(const FrameInterpolateBase& rOther) const;
	void Write(std::ostream& rStream) const;
	void Read(std::istream& rStream);
	void ServerRead(std::istream& rStream);
};

static_assert(static_cast<int64_t>(std::tuple_size_v<decltype(std::declval<FrameInterpolateBase>().Collections())>) == FrameInterpolateBase::kiCollectionCount, "FrameInterpolateBase: Collections() tuple size does not match kiCollectionCount. Did you add a new collection member without updating Collections()?");

#if defined(BT_SERVER)
static_assert(std::is_same_v<decltype(std::declval<FrameInterpolateBase>().Collections()), decltype(std::declval<FrameInterpolateBase>().ServerCollections())>, "Server build: FrameInterpolateBase::Collections() and ServerCollections() must be the same tuple — Write() walks Collections() while ServerRead()/Crcs() walk ServerCollections(); a divergence shears the wire format.");
#endif

struct FramePostRenderBase
{
	FramePostRenderBase() = default;
	~FramePostRenderBase() = default;
	FramePostRenderBase(FramePostRenderBase&&) noexcept = default;
	FramePostRenderBase& operator=(FramePostRenderBase&&) noexcept = default;

	static void AllocateAndCopy(game::FramePostRender& __restrict rCurrent, const game::FramePostRender& __restrict rPrevious);
	static void Update(game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const game::FrameInput& __restrict rFrameInput, const CellStaticData& rStaticData);

	common::RandomEngine randomEngine {};
	uint64_t uiNextUuid = 1;
#if defined(BT_CLIENT)
	uint64_t uiNextSoundUuid = 1;
	uint64_t uiNextVisualUuid = 1;
#endif
	uint16_t uiFrameIdentifier = 0;

	common::crc_t uiSharedCrc = 0;        // Shared CRC excluding client-only and server-only fields

	Alignments alignments {};

	int64_t MakeUuid(uint64_t& ruiCounter)
	{
		int64_t iCounter = ruiCounter++;
		return (static_cast<int64_t>(uiFrameIdentifier) << 48) | (iCounter & 0x0000'FFFF'FFFF'FFFF);
	}

#if defined(BT_CLIENT)
	AreaLightsPostRender areaLights {};
	BillboardsPostRender billboards {};
#endif
	ExplosionsPostRender explosions {};
#if defined(BT_CLIENT)
	HexShieldsPostRender hexShields {};
	PointLightsPostRender pointLights {};
	PuffsPostRender puffs {};
#endif
	PushersPostRender pushers {};
#if defined(BT_CLIENT)
	SoundsPostRender sounds {};
	SmokeTrailsPostRender smokeTrails {};
	WindRadialsPostRender windRadials {};
	WindTrailsPostRender windTrails {};
#endif

	auto Collections(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tie(rSelf.areaLights, rSelf.billboards, rSelf.explosions, rSelf.hexShields, rSelf.pointLights, rSelf.puffs, rSelf.pushers, rSelf.sounds, rSelf.smokeTrails, rSelf.windRadials, rSelf.windTrails);
#else
		return std::tie(rSelf.explosions, rSelf.pushers);
#endif
	}

#if defined(BT_CLIENT)
	static constexpr int64_t kiCollectionCount = 11;
#else
	static constexpr int64_t kiCollectionCount = 2;
#endif

	auto ServerCollections(this auto&& rSelf)
	{
		return std::tie(rSelf.explosions, rSelf.pushers);
	}

	// The frame-wide values Write and Read serialize, in stream order; ServerRead skips the kbClientOnlyValue ones.
	static auto Values()
	{
		return FrameColumnList<
			FRAME_COLUMN(FramePostRenderBase, randomEngine),
			FRAME_COLUMN(FramePostRenderBase, uiNextUuid),
#if defined(BT_CLIENT)
			FRAME_COLUMN(FramePostRenderBase, uiNextSoundUuid),
			FRAME_COLUMN(FramePostRenderBase, uiNextVisualUuid),
#endif
			FRAME_COLUMN(FramePostRenderBase, uiFrameIdentifier),
			FRAME_COLUMN(FramePostRenderBase, alignments)> {};
	}

	common::crc_t Crcs() const;
	bool LogDifferences(const FramePostRenderBase& rOther) const;
	void Write(std::ostream& rStream) const;
	void Read(std::istream& rStream);
	void ServerRead(std::istream& rStream);
};

#if defined(BT_CLIENT)
template <>
inline constexpr bool kbClientOnlyValue<&FramePostRenderBase::uiNextSoundUuid> = true;
template <>
inline constexpr bool kbClientOnlyValue<&FramePostRenderBase::uiNextVisualUuid> = true;
#endif

static_assert(static_cast<int64_t>(std::tuple_size_v<decltype(std::declval<FramePostRenderBase>().Collections())>) == FramePostRenderBase::kiCollectionCount, "FramePostRenderBase: Collections() tuple size does not match kiCollectionCount. Did you add a new collection member without updating Collections()?");

static_assert(std::tuple_size_v<decltype(std::declval<FrameInterpolateBase>().Collections())> == std::tuple_size_v<decltype(std::declval<FramePostRenderBase>().Collections())>, "FrameInterpolateBase and FramePostRenderBase must have the same number of collections");

#if defined(BT_SERVER)
static_assert(std::is_same_v<decltype(std::declval<FramePostRenderBase>().Collections()), decltype(std::declval<FramePostRenderBase>().ServerCollections())>, "Server build: FramePostRenderBase::Collections() and ServerCollections() must be the same tuple — Write() walks Collections() while ServerRead()/Crcs() walk ServerCollections(); a divergence shears the wire format.");
#endif

// Type aliases derived from Collections() - must be after class definitions are complete
using InterpolateTypes = TupleToTypeList_t<decltype(std::declval<FrameInterpolateBase>().Collections())>;
using PostRenderBaseTypes = TupleToTypeList_t<decltype(std::declval<FramePostRenderBase>().Collections())>;

// Inline definition - must be after FramePostRenderBase is complete
inline Uuid Uuid::Generate(FramePostRenderBase& rFramePostRender)
{
	return Uuid {rFramePostRender.MakeUuid(rFramePostRender.uiNextUuid)};
}

#if defined(BT_CLIENT)
inline Uuid Uuid::GenerateVisual(FramePostRenderBase& rFramePostRender)
{
	return Uuid {rFramePostRender.MakeUuid(rFramePostRender.uiNextVisualUuid)};
}
#endif

struct ActiveFrameReference
{
	game::Frame* pNext = nullptr;
	game::Frame* pCurrent = nullptr;
	game::FrameInput* pFrameInput = nullptr;
	const CellStaticData* pStaticData = nullptr;
};

void RunFrameTick(const ActiveFrameReference& rReference, int64_t iTickCounter, float fCurrentTime);

} // namespace engine
