#include "Billboards.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<BillboardsInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<BillboardsPostRender>;

} // namespace engine

#endif // BT_CLIENT
