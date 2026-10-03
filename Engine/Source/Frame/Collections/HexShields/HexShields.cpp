#include "HexShields.h"

#if defined(BT_CLIENT)

namespace engine
{

template struct Collection<HexShieldsInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<HexShieldsPostRender>;

} // namespace engine

#endif // BT_CLIENT
