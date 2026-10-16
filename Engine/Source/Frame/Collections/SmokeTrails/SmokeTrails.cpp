#include "SmokeTrails.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<SmokeTrailsInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<SmokeTrailsPostRender>;

} // namespace engine

#endif // BT_CLIENT
