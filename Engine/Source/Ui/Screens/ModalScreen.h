#pragma once

#if defined(BT_CLIENT)

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

#endif // BT_CLIENT
