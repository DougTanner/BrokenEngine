#include "WindRadials.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<WindRadialsInterpolate>;
template struct Collection<WindRadialsPostRender>;

void WindRadialsPostRender::Destroy(game::Frame& __restrict rFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
{
	DestroyExpiredControlled(rFrame.interpolate.windRadials, rFrame.postRender.windRadials, rFrame.interpolate.fCurrentTime, [](WindRadialsInterpolate& rInterpolate, WindRadialsPostRender& rPostRender, int64_t& riIndex)
	{
		DestroyElement(rInterpolate, rPostRender, riIndex, rInterpolate.Members(), rPostRender.Members());
		--riIndex;
	});
}

} // namespace engine

#endif // BT_CLIENT
