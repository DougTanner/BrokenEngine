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

struct FrameStaticData;

template <auto MEMBER, common::FixedString NAME>
struct FrameColumn
{
	static constexpr auto kpMember = MEMBER;
	static constexpr std::string_view kName = NAME.data;
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

struct FrameBounds
{
	float fMinX = 0.0f;
	float fMinY = 0.0f;
	float fMaxX = 0.0f;
	float fMaxY = 0.0f;
};

struct SegmentHit
{
	bool bHit = false;
	float fTime = 0.0f;
	XMVECTOR vecPosition {};
};

inline FrameBounds XM_CALLCONV ComputeFrameBounds(FXMVECTOR vecArea)
{
	// vecArea: x=minX, y=maxY, z=maxX, w=minY
	return
	{
		.fMinX = XMVectorGetX(vecArea),
		.fMinY = XMVectorGetW(vecArea),
		.fMaxX = XMVectorGetZ(vecArea),
		.fMaxY = XMVectorGetY(vecArea),
	};
}

inline SegmentHit XM_CALLCONV TracePointToFrameExit(FXMVECTOR vecArea, FXMVECTOR vecStartPosition, FXMVECTOR vecEndPosition, float fStartTime, float fEndTime)
{
	FrameBounds bounds = ComputeFrameBounds(vecArea);
	XMFLOAT4A f4Start {};
	XMFLOAT4A f4End {};
	XMStoreFloat4A(&f4Start, vecStartPosition);
	XMStoreFloat4A(&f4End, vecEndPosition);
	float fDeltaX = f4End.x - f4Start.x;
	float fDeltaY = f4End.y - f4Start.y;
	float fPercent = std::numeric_limits<float>::max();

	bool bStartsOutside = f4Start.x < bounds.fMinX || f4Start.x > bounds.fMaxX || f4Start.y < bounds.fMinY || f4Start.y > bounds.fMaxY;
	bool bStartsOnNonInwardBoundary = (f4Start.x == bounds.fMinX && fDeltaX <= 0.0f) || (f4Start.x == bounds.fMaxX && fDeltaX >= 0.0f)
	                               || (f4Start.y == bounds.fMinY && fDeltaY <= 0.0f) || (f4Start.y == bounds.fMaxY && fDeltaY >= 0.0f);
	if (bStartsOutside || bStartsOnNonInwardBoundary)
	{
		fPercent = 0.0f;
	}
	else
	{
		if (fDeltaX > 0.0f && f4End.x >= bounds.fMaxX)
		{
			fPercent = std::min(fPercent, (bounds.fMaxX - f4Start.x) / fDeltaX);
		}
		else if (fDeltaX < 0.0f && f4End.x <= bounds.fMinX)
		{
			fPercent = std::min(fPercent, (bounds.fMinX - f4Start.x) / fDeltaX);
		}
		if (fDeltaY > 0.0f && f4End.y >= bounds.fMaxY)
		{
			fPercent = std::min(fPercent, (bounds.fMaxY - f4Start.y) / fDeltaY);
		}
		else if (fDeltaY < 0.0f && f4End.y <= bounds.fMinY)
		{
			fPercent = std::min(fPercent, (bounds.fMinY - f4Start.y) / fDeltaY);
		}
	}

	if (fPercent == std::numeric_limits<float>::max())
	{
		return {};
	}

	return
	{
		.bHit = true,
		.fTime = fStartTime + fPercent * (fEndTime - fStartTime),
		.vecPosition = XMVectorLerp(vecStartPosition, vecEndPosition, fPercent),
	};
}

// The one frame area, identical in every cell: geometry is centered cell-local, so the grid coordinate is
// identity only and never scales an edge. Lanes follow the convention ComputeFrameBounds reads.
inline XMVECTOR XM_CALLCONV LocalFrameArea()
{
	return XMVectorSet(kfBaseAreaMinimumX, kfBaseAreaMaximumY, kfBaseAreaMaximumX, kfBaseAreaMinimumY);
}

// Neighbour and transfer-destination coordinate arithmetic. Returns false and leaves rOutputCoordinate untouched when
// the sum leaves the signed-int32 identity range, so a cell at a numeric edge omits that neighbour instead of
// wrapping to the opposite end of the grid.
[[nodiscard]] inline bool TryAddGridCoordinate(GridCoord coordinate, int64_t iDeltaX, int64_t iDeltaY, GridCoord& rOutputCoordinate)
{
	int64_t iSumX = static_cast<int64_t>(coordinate.iX) + static_cast<int64_t>(iDeltaX);
	int64_t iSumY = static_cast<int64_t>(coordinate.iY) + static_cast<int64_t>(iDeltaY);
	if (!std::in_range<int32_t>(iSumX))
	{
		return false;
	}
	if (!std::in_range<int32_t>(iSumY))
	{
		return false;
	}

	rOutputCoordinate = {.iX = static_cast<int32_t>(iSumX), .iY = static_cast<int32_t>(iSumY)};
	return true;
}

inline constexpr int64_t kiInitialTransferCapacity = 32;

inline bool XM_CALLCONV IsOutOfBounds(const FrameBounds& rBounds, FXMVECTOR vecPosition)
{
	float fPositionX = XMVectorGetX(vecPosition);
	float fPositionY = XMVectorGetY(vecPosition);

	return !(fPositionX > rBounds.fMinX && fPositionX < rBounds.fMaxX && fPositionY > rBounds.fMinY && fPositionY < rBounds.fMaxY);
}

inline void XM_CALLCONV ComputeTransferDelta(const FrameBounds& rBounds, FXMVECTOR vecPosition, int64_t& riDeltaX, int64_t& riDeltaY)
{
	float fPositionX = XMVectorGetX(vecPosition);
	float fPositionY = XMVectorGetY(vecPosition);
	riDeltaX = (fPositionX >= rBounds.fMaxX) ? 1 : (fPositionX <= rBounds.fMinX) ? -1 : 0;
	riDeltaY = (fPositionY >= rBounds.fMaxY) ? 1 : (fPositionY <= rBounds.fMinY) ? -1 : 0;
}

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
void ForEachPostRenderUpdate(TypeList<TS...>, game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const FrameStaticData& rStaticData)
{
	([&]
	{
		static_assert(requires { TS::Update(rFrame, rPreviousFrame, rStaticData); });
		TS::Update(rFrame, rPreviousFrame, rStaticData);
	}(), ...);
}

template<typename... TS>
void ForEachPostRenderPreCollision(TypeList<TS...>, game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const FrameStaticData& rStaticData)
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
void ForEachPostRenderPostCollision(TypeList<TS...>, game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const FrameStaticData& rStaticData)
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
void ForEachPostRenderAreaDamage(TypeList<TS...>, game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const FrameStaticData& rStaticData)
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
void ForEachPostRenderTransfer(TypeList<TS...>, game::Frame& __restrict rFrame, const FrameStaticData& rStaticData)
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
void ForEachPostRenderDestroy(TypeList<TS...>, game::Frame& __restrict rFrame, const FrameStaticData& rStaticData)
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
void ForEachPostRenderSpawn(TypeList<TS...>, game::Frame& __restrict rFrame, const FrameStaticData& rStaticData)
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
