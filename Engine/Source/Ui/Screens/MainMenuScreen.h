#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class MainMenuScreen
{
public:

	void Render();

	// Local Server / Remote Server / Graphics / Audio / Game Settings / Quit
	static constexpr int64_t kiMenuButtonCount = 6;

	float mfButtonHoverAnimations[kiMenuButtonCount] {};
};

} // namespace engine

#endif // BT_CLIENT
