#include "StatusChange.h"

namespace game
{

template <typename DATA>
concept HasSpawnTraits = requires { typename SpawnTraits<DATA>::Collection; };

// The three ship SpawnInfo types: each carries a controller and an owned-entity identity.
template <typename DATA>
concept IsShipSpawnInfo = requires(const DATA& rData) { rData.eController; rData.globalId; rData.clientGuid; };

template <typename CHANGE>
using SpawnPayloadPositionType = std::conditional_t<std::is_const_v<CHANGE>, const XMVECTOR, XMVECTOR>;

// Only spawn and transfer tags carry a SpawnInfo; any other tag here is a caller bug.
template <typename CHANGE>
static SpawnPayloadPositionType<CHANGE>& SpawnPayloadPosition(CHANGE& rChange)
{
	return std::visit([]([[maybe_unused]] auto& rData) -> SpawnPayloadPositionType<CHANGE>&
	{
		if constexpr (HasSpawnTraits<std::remove_cvref_t<decltype(rData)>>)
		{
			return rData.vecPosition;
		}
		else
		{
			ASSERT(false);
			std::unreachable();
		}
	}, rChange.data);
}

XMVECTOR SpawnPosition(const StatusChange& rChange)
{
	return SpawnPayloadPosition(rChange);
}

XMVECTOR& SpawnPositionRef(StatusChange& rChange)
{
	return SpawnPayloadPosition(rChange);
}

void ValidateSpawnVectors(const StatusChange& rChange)
{
	std::visit([]([[maybe_unused]] const auto& rData)
	{
		if constexpr (HasSpawnTraits<std::remove_cvref_t<decltype(rData)>>)
		{
			common::ValidateVector<true>(rData.vecPosition);
			if constexpr (requires { rData.vecDirection; })
			{
				common::ValidateVector<false>(rData.vecDirection);
			}
			if constexpr (requires { rData.vecVelocity; })
			{
				common::ValidateVector<false>(rData.vecVelocity);
			}
		}
		else
		{
			ASSERT(false);
		}
	}, rChange.data);
}

bool IsOwnedEntityTransfer(const StatusChange& rChange)
{
	return IsTransferType(rChange.eType) && std::visit([](const auto& rData)
	{
		if constexpr (IsShipSpawnInfo<std::remove_cvref_t<decltype(rData)>>)
		{
			return rData.eController == ShipController::kFleet;
		}
		else
		{
			return false;
		}
	}, rChange.data);
}

// Only ship spawn and transfer tags carry an owned-entity identity; any other tag here is a caller bug.
engine::GlobalId OwnedSpawnGlobalId(const StatusChange& rChange)
{
	return std::visit([](const auto& rData) -> engine::GlobalId
	{
		if constexpr (IsShipSpawnInfo<std::remove_cvref_t<decltype(rData)>>)
		{
			return rData.globalId;
		}
		else
		{
			ASSERT(false);
			std::unreachable();
		}
	}, rChange.data);
}

engine::ClientGuid OwnedSpawnClientGuid(const StatusChange& rChange)
{
	return std::visit([](const auto& rData) -> engine::ClientGuid
	{
		if constexpr (IsShipSpawnInfo<std::remove_cvref_t<decltype(rData)>>)
		{
			return rData.clientGuid;
		}
		else
		{
			ASSERT(false);
			std::unreachable();
		}
	}, rChange.data);
}

bool IsAdoptableStatusChange(const StatusChange& rChange)
{
	return std::visit([](const auto& rData)
	{
		using Data = std::remove_cvref_t<decltype(rData)>;
		if constexpr (std::is_same_v<Data, BlastersPostRender::SpawnInfo>)
		{
			return rData.iTypeIndex >= 0 && rData.iTypeIndex < std::ssize(BlastersInterpolate::sTypes);
		}
		else if constexpr (std::is_same_v<Data, engine::ExplosionsPostRender::SpawnInfo>)
		{
			return rData.iTypeIndex >= 0 && rData.iTypeIndex < std::ssize(engine::ExplosionsInterpolate::sTypes);
		}
		else if constexpr (IsShipSpawnInfo<Data>)
		{
			return rData.eController < ShipController::kCount;
		}
		else if constexpr (requires { rData.eType; })
		{
			return rData.eType < ShipType::kCount;
		}
		else
		{
			return true;
		}
	}, rChange.data);
}

void ApplySpawnStatusChange(Frame& __restrict rFrame, const StatusChange& rChange, engine::GridCoord coordinate)
{
	std::visit([&]([[maybe_unused]] const auto& rData)
	{
		using Data = std::remove_cvref_t<decltype(rData)>;
		if constexpr (std::is_same_v<Data, engine::ExplosionsPostRender::SpawnInfo>)
		{
			// rFrame was just ticked, so its tick is the one an in-tick explosion spawned this tick would start at.
			engine::ExplosionsPostRender::Spawn(rFrame, coordinate, engine::TickToSimulationTime(rFrame.interpolate.iTick), rData);
		}
		else if constexpr (HasSpawnTraits<Data>)
		{
			SpawnTraits<Data>::Collection::Spawn(rFrame, rData);
		}
		else
		{
			ASSERT(false);
			std::unreachable();
		}
	}, rChange.data);
}

} // namespace game
