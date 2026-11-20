#include "Puffs.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<PuffsInterpolate>;
template struct Collection<PuffsPostRender>;

void PuffsPostRender::Destroy(game::Frame& __restrict rFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
	DestroyExpiredControlled(rFrame.interpolate.puffs, rFrame.postRender.puffs, TickToSimulationTime(rFrame.interpolate.iTick));
}

} // namespace engine

#endif // BT_CLIENT
