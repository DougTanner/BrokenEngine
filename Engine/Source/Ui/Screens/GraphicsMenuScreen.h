#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class GraphicsMenuScreen
{
public:

	void Render();

	float mfDefaultsHoverAnimation = 0.0f;
	float mfBackHoverAnimation = 0.0f;
};

} // namespace engine

#endif // BT_CLIENT
