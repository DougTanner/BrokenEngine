#pragma once

#if defined(BT_CLIENT)

#include "Ui/LocalizationBase.h"

namespace engine
{

class GameSettingsScreen
{
public:

	void Render();

	float mfLanguageHoverAnimations[kiLanguageCount] {};
	float mfDefaultsHoverAnimation = 0.0f;
	float mfBackHoverAnimation = 0.0f;
};

} // namespace engine

#endif // BT_CLIENT
