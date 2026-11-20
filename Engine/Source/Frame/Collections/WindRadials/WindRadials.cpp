#include "WindRadials.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<WindRadialsInterpolate>;
template struct Collection<WindRadialsPostRender>;

void WindRadialsPostRender::Destroy(game::Frame& __restrict rFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
	DestroyExpiredControlled(rFrame.interpolate.windRadials, rFrame.postRender.windRadials, TickToSimulationTime(rFrame.interpolate.iTick));
}

} // namespace engine

#endif // BT_CLIENT
