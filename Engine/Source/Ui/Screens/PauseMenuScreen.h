#pragma once

namespace engine
{

class GameBase;

class PauseMenuScreen
{
public:

	void Render(GameBase& rGame);

private:

	// Resume / Graphics / Audio / Game Settings / Main Menu / Quit
	static constexpr int64_t kiMenuButtonCount = 6;

	float mfButtonHoverAnims[kiMenuButtonCount] {};
};

} // namespace engine
