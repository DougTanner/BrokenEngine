#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class PauseMenuScreen
{
public:

	void Render();

	// Resume / Graphics / Audio / Game Settings / Main Menu / Quit
	static constexpr int64_t kiMenuButtonCount = 6;

	float mfButtonHoverAnimations[kiMenuButtonCount] {};
};

} // namespace engine

#endif // BT_CLIENT
