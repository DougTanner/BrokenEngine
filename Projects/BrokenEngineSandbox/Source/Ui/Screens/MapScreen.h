#pragma once

#if defined(BT_CLIENT)

namespace game
{

// Full-screen map overlay: an opaque background and the world's cell hexagons projected through the live camera, both
// at the map opacity.
class MapScreen
{
public:

	void Render();
};

} // namespace game

#endif // defined(BT_CLIENT)
