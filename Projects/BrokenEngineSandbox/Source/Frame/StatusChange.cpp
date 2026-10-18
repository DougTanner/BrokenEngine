#include "StatusChange.h"

namespace game
{

template <typename DATA>
concept HasSpawnTraits = requires { typename SpawnTraits<DATA>::Collection; };

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

engine::GlobalId OwnedSpawnGlobalId(const StatusChange& rChange)
{
	return std::get<OmnisPostRender::SpawnInfo>(rChange.data).globalId;
}

engine::ClientGuid OwnedSpawnClientGuid(const StatusChange& rChange)
{
	return std::get<OmnisPostRender::SpawnInfo>(rChange.data).clientGuid;
}

bool IsAdoptableStatusChange(const StatusChange& rChange)
{
	if (const BlastersPostRender::SpawnInfo* pBlaster = std::get_if<BlastersPostRender::SpawnInfo>(&rChange.data); pBlaster != nullptr)
	{
		return pBlaster->iTypeIndex >= 0 && pBlaster->iTypeIndex < std::ssize(BlastersInterpolate::sTypes);
	}
	if (const engine::ExplosionsPostRender::SpawnInfo* pExplosion = std::get_if<engine::ExplosionsPostRender::SpawnInfo>(&rChange.data); pExplosion != nullptr)
	{
		return pExplosion->iTypeIndex >= 0 && pExplosion->iTypeIndex < std::ssize(engine::ExplosionsInterpolate::sTypes);
	}
	return true;
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
