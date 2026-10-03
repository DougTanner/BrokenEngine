#include "Sounds.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<SoundsInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<SoundsPostRender>;

} // namespace engine

#endif // BT_CLIENT
