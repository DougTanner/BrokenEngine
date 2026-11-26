#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class PauseMenuScreen
{
public:

	void Render();

	// Resume / Graphics / Audio / Game Settings / Keybinds / Main Menu / Quit
	static constexpr int64_t kiMenuButtonCount = 7;

	float mpfButtonHoverAnimations[kiMenuButtonCount] {};
};

} // namespace engine

#endif // BT_CLIENT
