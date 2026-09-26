#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class MainMenuScreen
{
public:

	void Render();

private:

	// Local Server / Remote Server / Graphics / Audio / Game Settings / Quit
	static constexpr int64_t kiMenuButtonCount = 6;

	float mfButtonHoverAnims[kiMenuButtonCount] {};
};

} // namespace engine

#endif // BT_CLIENT
