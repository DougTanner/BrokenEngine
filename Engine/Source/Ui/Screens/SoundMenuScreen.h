#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class SoundMenuScreen
{
public:

	void Render();

private:

	float mfDefaultsHoverAnim = 0.0f;
	float mfBackHoverAnim = 0.0f;
};

} // namespace engine

#endif // BT_CLIENT
