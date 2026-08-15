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

// Decoupled movement: drag decays velocity, acceleration scales down near max speed
// kbBlendVelocityToDirection: when true, blends velocity direction toward vecDirection (airplane-like constraint)
template<bool kbBlendVelocityToDirection = false>
[[nodiscard]] inline XMVECTOR XM_CALLCONV ApplyMovement(FXMVECTOR vecVelocity, FXMVECTOR vecDirection, float fDeltaTime, float fAcceleration, float fDrag, float fMaxSpeed, float fVelocityToDirection = 0.0f)
{
	XMVECTOR vecResult = XMVectorMultiply(XMVectorReplicate(common::ExponentialDecay(fDrag, fDeltaTime)), vecVelocity);

	float fSpeed = XMVectorGetX(XMVector3Length(vecResult));
	float fAccelScale = 1.0f - std::min(fSpeed / fMaxSpeed, 1.0f);
	vecResult = XMVectorMultiplyAdd(XMVectorReplicate(fDeltaTime * fAcceleration * fAccelScale), vecDirection, vecResult);

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
	float fMinX {};
	float fMinY {};
	float fMaxX {};
	float fMaxY {};
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
	return {
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
	bool bStartsOnNonInwardBoundary =
		(f4Start.x == bounds.fMinX && fDeltaX <= 0.0f) ||
		(f4Start.x == bounds.fMaxX && fDeltaX >= 0.0f) ||
		(f4Start.y == bounds.fMinY && fDeltaY <= 0.0f) ||
		(f4Start.y == bounds.fMaxY && fDeltaY >= 0.0f);
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

// Compute world-space frame bounds for a given grid coordinate
inline XMVECTOR XM_CALLCONV ComputeFrameArea(FXMVECTOR vecBaseArea, GridCoord coord)
{
	float fWidth = XMVectorGetZ(vecBaseArea) - XMVectorGetX(vecBaseArea);
	float fHeight = XMVectorGetY(vecBaseArea) - XMVectorGetW(vecBaseArea);
	XMVECTOR vecOffset = XMVectorSet(static_cast<float>(coord.x) * fWidth, static_cast<float>(coord.y) * fHeight, static_cast<float>(coord.x) * fWidth, static_cast<float>(coord.y) * fHeight);
	return XMVectorAdd(vecBaseArea, vecOffset);
}

inline constexpr size_t kuiInitialTransferCapacity = 32;

inline bool XM_CALLCONV IsOutOfBounds(const FrameBounds& rBounds, FXMVECTOR vecPosition)
{
	float fPositionX = XMVectorGetX(vecPosition);
	float fPositionY = XMVectorGetY(vecPosition);

	return !(fPositionX > rBounds.fMinX && fPositionX < rBounds.fMaxX &&
	         fPositionY > rBounds.fMinY && fPositionY < rBounds.fMaxY);
}

inline void XM_CALLCONV ComputeTransferDelta(const FrameBounds& rBounds, FXMVECTOR vecPosition, int8_t& rDeltaX, int8_t& rDeltaY)
{
	float fPositionX = XMVectorGetX(vecPosition);
	float fPositionY = XMVectorGetY(vecPosition);
	rDeltaX = static_cast<int8_t>((fPositionX >= rBounds.fMaxX) ? 1 : (fPositionX <= rBounds.fMinX) ? -1 : 0);
	rDeltaY = static_cast<int8_t>((fPositionY >= rBounds.fMaxY) ? 1 : (fPositionY <= rBounds.fMinY) ? -1 : 0);
}

// Type list for fold expression iteration
template<typename... TS>
struct TypeList {};

// Convert std::tuple<T1&, T2&, ...> to TypeList<T1, T2, ...>
// Strips references from tuple element types
template<typename TUPLE>
struct TupleToTypeList;

template<typename... TS>
struct TupleToTypeList<std::tuple<TS...>>
{
	using type = TypeList<std::remove_reference_t<TS>...>;
};

template<typename TUPLE>
using TupleToTypeList_t = typename TupleToTypeList<TUPLE>::type;

// ForEach helpers for collection iteration via fold expressions
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

// AllocateAndCopy helper using tuple and index sequence
template<typename TUPLE_CURRENT, typename TUPLE_PREVIOUS, size_t... INDICES>
void AllocateAndCopyCollections(TUPLE_CURRENT&& current, TUPLE_PREVIOUS&& previous, std::index_sequence<INDICES...>)
{
	(std::remove_reference_t<std::tuple_element_t<INDICES, std::remove_cvref_t<TUPLE_CURRENT>>>::AllocateAndCopy(
		std::get<INDICES>(current), std::get<INDICES>(previous)), ...);
}

// LogDifferences helper using tuple and index sequence; left-to-right, never short-circuits (every collection logs)
template<typename TUPLE_CURRENT, typename TUPLE_OTHER, size_t... INDICES>
bool LogDifferencesCollections(TUPLE_CURRENT&& current, TUPLE_OTHER&& other, std::index_sequence<INDICES...>)
{
	bool bEqual = true;
	((bEqual &= std::get<INDICES>(current).LogDifferences(std::get<INDICES>(other))), ...);
	return bEqual;
}

// Reverse walk over a paired collection, releasing every element the predicate selects.
// Reverse order keeps swap-and-pop removal safe: the row swapped in from the tail has already been visited,
// so release must not disturb the loop index.
template <typename TInterpolate, typename TPostRender, typename TPredicate, typename TRelease>
void DestroySweep(TInterpolate& rInterpolate, [[maybe_unused]] TPostRender& rPostRender, TPredicate predicate, TRelease release)
{
	for (int64_t i = rInterpolate.iCount - 1; i >= 0; --i)
	{
		if (!predicate(i)) [[likely]]
		{
			continue;
		}

		release(i);
	}
}

template <typename INTERPOLATE, typename POST_RENDER>
void ValidateCollectionPair(const INTERPOLATE& rInterpolate, const POST_RENDER& rPostRender)
{
	if (rInterpolate.iCount != rPostRender.iCount || rInterpolate.iCapacity != rPostRender.iCapacity)
	{
		throw common::CorruptStreamException("Frame collection pair count/capacity mismatch");
	}
}

template <typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE, size_t... INDICES>
void ValidateCollectionPairs(const INTERPOLATE_TUPLE& rInterpolateCollections, const POST_RENDER_TUPLE& rPostRenderCollections, std::index_sequence<INDICES...>)
{
	(ValidateCollectionPair(std::get<INDICES>(rInterpolateCollections), std::get<INDICES>(rPostRenderCollections)), ...);
}

template <typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void ValidateCollectionPairs(const INTERPOLATE_TUPLE& rInterpolateCollections, const POST_RENDER_TUPLE& rPostRenderCollections)
{
	static_assert(std::tuple_size_v<INTERPOLATE_TUPLE> == std::tuple_size_v<POST_RENDER_TUPLE>);
	ValidateCollectionPairs(rInterpolateCollections, rPostRenderCollections, std::make_index_sequence<std::tuple_size_v<INTERPOLATE_TUPLE>> {});
}

// Collection tuple folds: left-to-right over the tuple, so CRC mixing and byte order follow tuple order
template <typename TUPLE>
common::crc_t CollectionsCrc(common::crc_t sharedCrc, TUPLE&& collections)
{
	std::apply([&](const auto&... cols)
	{
		((sharedCrc = (sharedCrc ^ SharedCollectionCrc(cols)) * common::kCrcMultiplier), ...);
	}, collections);

	return sharedCrc;
}

template <typename TUPLE>
void CollectionsWrite(std::ostream& rStream, TUPLE&& collections)
{
	std::apply([&](const auto&... cols)
	{
		(CollectionWrite(rStream, cols, cols.Members()), ...);
	}, collections);
}

template <typename TUPLE>
void CollectionsRead(std::istream& rStream, TUPLE&& collections)
{
	std::apply([&](auto&... cols)
	{
		(CollectionRead(rStream, cols, cols.Members()), ...);
	}, collections);
}

template <typename TUPLE>
void SharedCollectionsRead(std::istream& rStream, TUPLE&& collections)
{
	std::apply([&](auto&... cols)
	{
		(SharedCollectionRead(rStream, cols), ...);
	}, collections);
}

} // namespace engine
