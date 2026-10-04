#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class ModalScreen
{
public:

	void Render();

	float mfOkHoverAnimation = 0.0f;
};

} // namespace engine

#endif // BT_CLIENT
