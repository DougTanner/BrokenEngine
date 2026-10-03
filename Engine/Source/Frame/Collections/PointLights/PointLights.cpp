#include "PointLights.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<PointLightsInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<PointLightsPostRender>;

void PointLightsPostRender::Destroy(game::Frame& __restrict rFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
{
	DestroyExpiredControlled(rFrame.interpolate.pointLights, rFrame.postRender.pointLights, rFrame.interpolate.fCurrentTime, [](PointLightsInterpolate& rInterpolate, PointLightsPostRender& rPostRender, int64_t& i)
	{
		RemoveIndexableElement(rInterpolate, rPostRender, rPostRender.pIds[i], rInterpolate.Members(), rPostRender.Members());
		--i;
	});
}

} // namespace engine

#endif // BT_CLIENT
