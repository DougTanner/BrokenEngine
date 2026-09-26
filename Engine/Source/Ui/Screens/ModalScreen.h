#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class ModalScreen
{
public:

	void Render();

private:

	float mfOkHoverAnim = 0.0f;
};

} // namespace engine

#endif // BT_CLIENT
