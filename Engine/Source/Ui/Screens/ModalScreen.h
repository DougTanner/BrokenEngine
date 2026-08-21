#pragma once

namespace engine
{

class GameBase;

class ModalScreen
{
public:

	void Render(GameBase& rGame);

private:

	float mfOkHoverAnim = 0.0f;
};

} // namespace engine
