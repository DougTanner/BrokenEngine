#pragma once

namespace engine
{

class GameBase;

class GraphicsMenuScreen
{
public:

	void Render(GameBase& rGame);

private:

	float mfDefaultsHoverAnim = 0.0f;
	float mfBackHoverAnim = 0.0f;
};

} // namespace engine
