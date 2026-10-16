#include "Puffs.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<PuffsInterpolate>;
template struct Collection<PuffsPostRender>;

void PuffsPostRender::Destroy(game::Frame& __restrict rFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
	DestroyExpiredControlled(rFrame.interpolate.puffs, rFrame.postRender.puffs, TickToSimulationTime(rFrame.interpolate.iTick), [](PuffsInterpolate& rInterpolate, PuffsPostRender& rPostRender, int64_t& i)
	{
		DestroyElement(rInterpolate, rPostRender, i, rInterpolate.Members(), rPostRender.Members());
		--i;
	});
}

} // namespace engine

#endif // BT_CLIENT
