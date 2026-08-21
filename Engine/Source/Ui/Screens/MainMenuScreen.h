#pragma once

namespace engine
{

class GameBase;

class MainMenuScreen
{
public:

	void Render(GameBase& rGame);

private:

	// Local Server / Remote Server / Graphics / Audio / Game Settings / Quit
	static constexpr int64_t kiMenuButtonCount = 6;

	float mfButtonHoverAnims[kiMenuButtonCount] {};
};

} // namespace engine
