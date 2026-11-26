#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class MainMenuScreen
{
public:

	void Render();

	// Local Server / Remote Server / Graphics / Audio / Game Settings / Keybinds / Quit
	static constexpr int64_t kiMenuButtonCount = 7;

	float mpfButtonHoverAnimations[kiMenuButtonCount] {};
};

} // namespace engine

#endif // BT_CLIENT
