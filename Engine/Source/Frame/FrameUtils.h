#pragma once

#include "Frame/GridCoord.h"

namespace game
{

struct Frame;
struct FrameInterpolate;
struct FramePostRender;

} // namespace game

namespace engine
{

struct CellStaticData;

template <auto MEMBER, common::FixedString NAME>
struct FrameColumn
{
	static constexpr auto kpMember = MEMBER;
	static constexpr std::string_view kName = NAME.pcData;
};

// The edit_frame key is the column's own member token.
#define FRAME_COLUMN(OWNER, MEMBER) engine::FrameColumn<&OWNER::MEMBER, #MEMBER>

template <typename... COLUMNS>
struct FrameColumnList {};

// Decoupled movement: drag decays velocity, acceleration scales down near max speed
// kbBlendVelocityToDirection: when true, blends velocity direction toward vecDirection (airplane-like constraint)
template<bool kbBlendVelocityToDirection = false>
[[nodiscard]] inline XMVECTOR XM_CALLCONV ApplyMovement(FXMVECTOR vecVelocity, FXMVECTOR vecDirection, float fDeltaTime, float fAcceleration, float fDrag, float fMaxSpeed, float fVelocityToDirection = 0.0f)
{
	XMVECTOR vecResult = XMVectorMultiply(XMVectorReplicate(common::ExponentialDecay(fDrag, fDeltaTime)), vecVelocity);

	float fSpeed = XMVectorGetX(XMVector3Length(vecResult));
	float fAccelerationScale = 1.0f - std::min(fSpeed / fMaxSpeed, 1.0f);
	vecResult = XMVectorMultiplyAdd(XMVectorReplicate(fDeltaTime * fAcceleration * fAccelerationScale), vecDirection, vecResult);

	if constexpr (kbBlendVelocityToDirection)
	{
		float fDecay = common::ExponentialDecay(fVelocityToDirection, fDeltaTime);
		XMVECTOR vecVelocityComponent = XMVectorMultiply(XMVectorReplicate(fDecay), XMVector3Normalize(vecResult));
		XMVECTOR vecDirectionComponent = XMVectorMultiply(XMVectorReplicate(1.0f - fDecay), vecDirection);
		vecResult = XMVectorMultiply(XMVector3Length(vecResult), XMVector3Normalize(XMVectorAdd(vecVelocityComponent, vecDirectionComponent)));
	}

	return vecResult;
}

struct SegmentHit
{
	bool bHit = false;
	float fTime = 0.0f;
	XMVECTOR vecPosition {};
};

// Whether a cell-local position lies inside its own cell. Geometry is centered cell-local, so the grid coordinate is
// identity only and never moves an edge.
inline bool XM_CALLCONV IsInsideCell(FXMVECTOR vecPosition)
{
	return common::IsInsideHex(vecPosition, kCellDirections, 0.0f);
}

inline SegmentHit XM_CALLCONV TracePointToCellExit(FXMVECTOR vecStartPosition, FXMVECTOR vecEndPosition)
{
	std::optional<float> optionalTime = common::TraceHexExit(vecStartPosition, vecEndPosition, kCellDirections);
	if (!optionalTime.has_value())
	{
		return {};
	}

	return
	{
		.bHit = true,
		.fTime = *optionalTime,
		.vecPosition = XMVectorLerp(vecStartPosition, vecEndPosition, *optionalTime),
	};
}

// The kCellDirections entry whose neighbour a position outside IsInsideCell belongs to.
inline int64_t XM_CALLCONV CellExitDirection(FXMVECTOR vecPosition)
{
	return common::HexExitDirection(vecPosition, kCellDirections);
}

inline int64_t GridDistance(GridCoord first, GridCoord second)
{
	return common::HexGridDistance(static_cast<int64_t>(second.iQ) - static_cast<int64_t>(first.iQ), static_cast<int64_t>(second.iR) - static_cast<int64_t>(first.iR));
}

// The offset from fromCoordinate's center to toCoordinate's center, with W 0.
inline XMVECTOR XM_CALLCONV CellCenterOffset(GridCoord fromCoordinate, GridCoord toCoordinate)
{
	return common::HexCenterOffset(static_cast<int64_t>(toCoordinate.iQ) - static_cast<int64_t>(fromCoordinate.iQ), static_cast<int64_t>(toCoordinate.iR) - static_cast<int64_t>(fromCoordinate.iR), kCellDirections);
}

inline std::pair<double, double> CellCenterOffsetDouble(GridCoord fromCoordinate, GridCoord toCoordinate)
{
	return common::HexCenterOffsetDouble(static_cast<int64_t>(toCoordinate.iQ) - static_cast<int64_t>(fromCoordinate.iQ), static_cast<int64_t>(toCoordinate.iR) - static_cast<int64_t>(fromCoordinate.iR), kCellDirections);
}

// Subtracting a float center offset whose x component rounds can leave a position that was just past one cell's edge
// on the far side of the destination cell's facing edge or vertex. Eight one-ulp steps toward the cell center on
// both axes cover that rounding for any position within one tick of travel of the edge it crossed; the cap is twice
// that and moves a position at most about 1 mm per axis.
inline constexpr int64_t kiLandingNudgeSteps = 16;

// Steps x and y one ulp toward the local origin while the position is outside IsInsideCell, at most
// kiLandingNudgeSteps times; the result can still be outside.
inline XMVECTOR XM_CALLCONV NudgeIntoCell(FXMVECTOR vecPosition)
{
	XMVECTOR vecResult = vecPosition;
	for (int64_t i = 0; i < kiLandingNudgeSteps && !IsInsideCell(vecResult); ++i)
	{
		vecResult = XMVectorSetY(XMVectorSetX(vecResult, std::nextafter(XMVectorGetX(vecResult), 0.0f)), std::nextafter(XMVectorGetY(vecResult), 0.0f));
	}
	return vecResult;
}

// Neighbour and transfer-destination coordinate arithmetic. Returns false and leaves rOutputCoordinate untouched when
// the sum leaves the signed-int32 identity range, so a cell at a numeric edge omits that neighbour instead of
// wrapping to the opposite end of the grid.
[[nodiscard]] inline bool TryAddGridCoordinate(GridCoord coordinate, int64_t iDeltaQ, int64_t iDeltaR, GridCoord& rOutputCoordinate)
{
	int64_t iSumQ = static_cast<int64_t>(coordinate.iQ) + static_cast<int64_t>(iDeltaQ);
	int64_t iSumR = static_cast<int64_t>(coordinate.iR) + static_cast<int64_t>(iDeltaR);
	if (!std::in_range<int32_t>(iSumQ))
	{
		return false;
	}
	if (!std::in_range<int32_t>(iSumR))
	{
		return false;
	}

	rOutputCoordinate = {.iQ = static_cast<int32_t>(iSumQ), .iR = static_cast<int32_t>(iSumR)};
	return true;
}

inline constexpr int64_t kiInitialTransferCapacity = 256;

template<typename... TS>
struct TypeList {};

template<typename TUPLE>
struct TupleToTypeList;

template<typename... TS>
struct TupleToTypeList<std::tuple<TS...>>
{
	using type = TypeList<std::remove_reference_t<TS>...>;
};

template<typename TUPLE>
using TupleToTypeList_t = typename TupleToTypeList<TUPLE>::type;

template<typename... TS>
consteval int64_t SumVersions(TypeList<TS...>)
{
	return (0i64 + ... + TS::kiVersion);
}

template<typename... TS>
void ForEachInterpolateUpdate(TypeList<TS...>, game::FrameInterpolate& __restrict rCurrent, const game::Frame& __restrict rPreviousFrame)
{
	([&]
	{
		static_assert(requires { TS::Update(rCurrent, rPreviousFrame); });
		TS::Update(rCurrent, rPreviousFrame);
	}(), ...);
}

template<typename... TS>
void ForEachInterpolateRender(TypeList<TS...>, const game::FrameInterpolate& __restrict rCurrent, int64_t iCommandBuffer)
{
	([&]
	{
		if constexpr (!requires { TS::kbManualRender; } && requires { TS::Render(rCurrent, iCommandBuffer); })
		{
			TS::Render(rCurrent, iCommandBuffer);
		}
	}(), ...);
}

template<typename... TS>
void ForEachBeginRender(TypeList<TS...>, int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoords)
{
	([&]
	{
		if constexpr (requires { TS::BeginRender(iCommandBuffer, rRenderInterpolates, rActiveCoords); })
		{
			TS::BeginRender(iCommandBuffer, rRenderInterpolates, rActiveCoords);
		}
	}(), ...);
}

template<typename... TS>
constexpr void ForEachEndRender(TypeList<TS...>, int64_t iCommandBuffer)
{
	([&]
	{
		if constexpr (requires { TS::EndRender(iCommandBuffer); })
		{
			TS::EndRender(iCommandBuffer);
		}
	}(), ...);
}

template<typename... TS>
constexpr void ForEachRegister(TypeList<TS...>)
{
	([&]
	{
		if constexpr (requires { TS::Register(); })
		{
			TS::Register();
		}
	}(), ...);
}

template<typename... TS>
constexpr void ForEachGraphicsResources(TypeList<TS...>)
{
	([&]
	{
		if constexpr (requires { TS::GraphicsResources(); })
		{
			TS::GraphicsResources();
		}
	}(), ...);
}

template<typename... TS>
void ForEachPostRenderUpdate(TypeList<TS...>, game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const CellStaticData& rStaticData)
{
	([&]
	{
		static_assert(requires { TS::Update(rFrame, rPreviousFrame, rStaticData); });
		TS::Update(rFrame, rPreviousFrame, rStaticData);
	}(), ...);
}

template<typename... TS>
void ForEachPostRenderPreCollision(TypeList<TS...>, game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const CellStaticData& rStaticData)
{
	([&]
	{
		if constexpr (requires { TS::PreCollision(rFrame, rPreviousFrame, rStaticData); })
		{
			TS::PreCollision(rFrame, rPreviousFrame, rStaticData);
		}
	}(), ...);
}

template<typename... TS>
void ForEachPostRenderPostCollision(TypeList<TS...>, game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const CellStaticData& rStaticData)
{
	([&]
	{
		if constexpr (requires { TS::PostCollision(rFrame, rPreviousFrame, rStaticData); })
		{
			TS::PostCollision(rFrame, rPreviousFrame, rStaticData);
		}
	}(), ...);
}

template<typename... TS>
void ForEachPostRenderAreaDamage(TypeList<TS...>, game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const CellStaticData& rStaticData)
{
	([&]
	{
		if constexpr (requires { TS::AreaDamage(rFrame, rPreviousFrame, rStaticData); })
		{
			TS::AreaDamage(rFrame, rPreviousFrame, rStaticData);
		}
	}(), ...);
}

template<typename... TS>
void ForEachPostRenderTransfer(TypeList<TS...>, game::Frame& __restrict rFrame, const CellStaticData& rStaticData)
{
	([&]
	{
		if constexpr (requires { TS::Transfer(rFrame, rStaticData); })
		{
			TS::Transfer(rFrame, rStaticData);
		}
	}(), ...);
}

template<typename... TS>
void ForEachPostRenderDestroy(TypeList<TS...>, game::Frame& __restrict rFrame, const CellStaticData& rStaticData)
{
	([&]
	{
		if constexpr (requires { TS::Destroy(rFrame, rStaticData); })
		{
			TS::Destroy(rFrame, rStaticData);
		}
	}(), ...);
}

template<typename... TS>
void ForEachPostRenderSpawn(TypeList<TS...>, game::Frame& __restrict rFrame, const CellStaticData& rStaticData)
{
	([&]
	{
		if constexpr (requires { TS::Spawn(rFrame, rStaticData); })
		{
			TS::Spawn(rFrame, rStaticData);
		}
	}(), ...);
}

template<typename TUPLE_CURRENT, typename TUPLE_PREVIOUS, int64_t... INDICES>
void AllocateAndCopyCollections(TUPLE_CURRENT&& rCurrent, const TUPLE_PREVIOUS& rPrevious, std::integer_sequence<int64_t, INDICES...>)
{
	([](auto& rCurrentCollection, const auto& rPreviousCollection)
	{
		using CollectionType = std::remove_cvref_t<decltype(rCurrentCollection)>;
		if constexpr (requires { CollectionType::AllocateAndCopy(rCurrentCollection, rPreviousCollection); })
		{
			CollectionType::AllocateAndCopy(rCurrentCollection, rPreviousCollection);
		}
		else
		{
			engine::AllocateAndCopyMembers(rCurrentCollection, rPreviousCollection);
		}
	}(std::get<static_cast<size_t>(INDICES)>(rCurrent), std::get<static_cast<size_t>(INDICES)>(rPrevious)), ...);
}

// Every collection logs in tuple order; the fold never short-circuits.
template<typename TUPLE_CURRENT, typename TUPLE_OTHER, int64_t... INDICES>
bool LogDifferencesCollections(const TUPLE_CURRENT& rCurrent, const TUPLE_OTHER& rOther, std::integer_sequence<int64_t, INDICES...>)
{
	bool bEqual = true;
	((bEqual &= std::get<static_cast<size_t>(INDICES)>(rCurrent).LogDifferences(std::get<static_cast<size_t>(INDICES)>(rOther))), ...);
	return bEqual;
}

// Reverse walk over a paired collection, releasing every element the predicate selects.
// Reverse order keeps swap-and-pop removal safe: the row swapped in from the tail has already been visited,
// so Release must not disturb the loop index.
template <typename INTERPOLATE, typename POST_RENDER, typename PREDICATE, typename RELEASE>
void DestroySweep(const INTERPOLATE& rInterpolate, [[maybe_unused]] const POST_RENDER& rPostRender, PREDICATE Predicate, RELEASE Release)
{
	for (int64_t i = rInterpolate.iCount - 1; i >= 0; --i)
	{
		if (!Predicate(i)) [[likely]]
		{
			continue;
		}

		Release(i);
	}
}

// Releases every row a nonzero handle in row i's OwnedHandles() columns names; a pair half without the accessor owns
// nothing.
template <typename FRAME, typename INTERPOLATE, typename POST_RENDER>
void ReleaseOwnedHandles([[maybe_unused]] FRAME& rFrame, INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, [[maybe_unused]] int64_t i)
{
	auto OwnedHandleColumns = [](auto& rCollection)
	{
		if constexpr (requires { rCollection.OwnedHandles(); })
		{
			return rCollection.OwnedHandles();
		}
		else
		{
			return std::tie();
		}
	};

	std::apply([&](auto&... rHandleColumns)
	{
		(ForEachMemberPointer(rHandleColumns, [&](auto& rpHandles)
		{
			ReleaseIndexableElementAndClearHandle(rFrame, rpHandles[i]);
		}), ...);
	}, std::tuple_cat(OwnedHandleColumns(rInterpolate), OwnedHandleColumns(rPostRender)));
}

// The one removal of kTransfer and kDestroy rows, at the end of the Destroy phase. The cascade pass releases the rows
// each such row owns in every pair before any pair compacts, so an owned row leaves in its owner's fold whatever the
// tuple order.
template <typename FRAME, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE, int64_t... INDICES>
void CompactCollections(FRAME& rFrame, const INTERPOLATE_TUPLE& rInterpolateCollections, const POST_RENDER_TUPLE& rPostRenderCollections, std::integer_sequence<int64_t, INDICES...>)
{
	auto IsCompacted = [](const auto& rPostRender, int64_t i)
	{
		return (rPostRender.pLifecycleFlags[i] & LifecycleFlags::kTransfer) || (rPostRender.pLifecycleFlags[i] & LifecycleFlags::kDestroy);
	};

	([&](auto& rInterpolate, auto& rPostRender)
	{
		for (int64_t i = 0; i < rInterpolate.iCount; ++i)
		{
			if (IsCompacted(rPostRender, i))
			{
				ReleaseOwnedHandles(rFrame, rInterpolate, rPostRender, i);
			}
		}
	}(std::get<static_cast<size_t>(INDICES)>(rInterpolateCollections), std::get<static_cast<size_t>(INDICES)>(rPostRenderCollections)), ...);

	([&](auto& rInterpolate, auto& rPostRender)
	{
		DestroySweep(rInterpolate, rPostRender, [&](int64_t i)
		{
			return IsCompacted(rPostRender, i);
		}, [&](int64_t i)
		{
			if constexpr (HasIdToIndex<std::remove_cvref_t<decltype(rInterpolate)>>)
			{
				RemoveIndexableElement(rInterpolate, rPostRender, rPostRender.pIds[i], rInterpolate.Members(), rPostRender.Members());
			}
			else
			{
				DestroyElement(rInterpolate, rPostRender, i, rInterpolate.Members(), rPostRender.Members());
			}
		});
	}(std::get<static_cast<size_t>(INDICES)>(rInterpolateCollections), std::get<static_cast<size_t>(INDICES)>(rPostRenderCollections)), ...);
}

template <typename FRAME, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void CompactCollections(FRAME& rFrame, const INTERPOLATE_TUPLE& rInterpolateCollections, const POST_RENDER_TUPLE& rPostRenderCollections)
{
	static_assert(std::tuple_size_v<INTERPOLATE_TUPLE> == std::tuple_size_v<POST_RENDER_TUPLE>);
	CompactCollections(rFrame, rInterpolateCollections, rPostRenderCollections, std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<INTERPOLATE_TUPLE>)> {});
}

template <typename INTERPOLATE, typename POST_RENDER>
void ValidateCollectionPair(INTERPOLATE& rInterpolate, const POST_RENDER& rPostRender)
{
	if (rInterpolate.iCount != rPostRender.iCount || rInterpolate.iCapacity != rPostRender.iCapacity)
	{
		throw std::ios_base::failure("Frame collection pair count/capacity mismatch");
	}
	if constexpr (HasIdToIndex<INTERPOLATE>)
	{
		// The ID map is not serialized; it is rebuilt from the paired PostRender IDs, which exist only once both halves are read.
		rInterpolate.RebuildFromIds(std::span(rPostRender.pIds, static_cast<size_t>(rInterpolate.iCount)));
	}
}

template <typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE, int64_t... INDICES>
void ValidateCollectionPairs(const INTERPOLATE_TUPLE& rInterpolateCollections, const POST_RENDER_TUPLE& rPostRenderCollections, std::integer_sequence<int64_t, INDICES...>)
{
	(ValidateCollectionPair(std::get<static_cast<size_t>(INDICES)>(rInterpolateCollections), std::get<static_cast<size_t>(INDICES)>(rPostRenderCollections)), ...);
}

template <typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void ValidateCollectionPairs(const INTERPOLATE_TUPLE& rInterpolateCollections, const POST_RENDER_TUPLE& rPostRenderCollections)
{
	static_assert(std::tuple_size_v<INTERPOLATE_TUPLE> == std::tuple_size_v<POST_RENDER_TUPLE>);
	ValidateCollectionPairs(rInterpolateCollections, rPostRenderCollections, std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<INTERPOLATE_TUPLE>)> {});
}

// Collection tuple folds: left-to-right over the tuple, so CRC mixing and byte order follow tuple order
template <typename TUPLE>
common::crc_t CollectionsCrc(common::crc_t uiSharedCrc, const TUPLE& rCollections)
{
	std::apply([&](const auto&... rCollection)
	{
		((uiSharedCrc = (uiSharedCrc ^ SharedCollectionCrc(rCollection)) * common::kCrcMultiplier), ...);
	}, rCollections);

	return uiSharedCrc;
}

template <typename TUPLE>
void CollectionsWrite(std::ostream& rStream, const TUPLE& rCollections)
{
	std::apply([&](const auto&... rCollection)
	{
		(CollectionWrite(rStream, rCollection, rCollection.Members()), ...);
	}, rCollections);
}

template <typename TUPLE>
void CollectionsRead(std::istream& rStream, TUPLE&& rCollections)
{
	std::apply([&](auto&... rCollection)
	{
		(CollectionRead(rStream, rCollection, rCollection.Members()), ...);
	}, rCollections);
}

template <typename TUPLE>
void SharedCollectionsRead(std::istream& rStream, TUPLE&& rCollections)
{
	std::apply([&](auto&... rCollection)
	{
		(SharedCollectionRead(rStream, rCollection), ...);
	}, rCollections);
}

} // namespace engine
