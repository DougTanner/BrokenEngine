#include "AreaLights.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<AreaLightsInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<AreaLightsPostRender>;

} // namespace engine

#endif // BT_CLIENT
