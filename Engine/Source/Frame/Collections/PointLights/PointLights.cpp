#include "PointLights.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<PointLightsInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<PointLightsPostRender>;

void PointLightsPostRender::Destroy(game::Frame& __restrict rFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
	DestroyExpiredControlled(rFrame.interpolate.pointLights, rFrame.postRender.pointLights, TickToSimulationTime(rFrame.interpolate.iTick));
}

} // namespace engine

#endif // BT_CLIENT
