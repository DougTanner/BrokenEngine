#pragma once

#include "Ui/LocalizationBase.h"

namespace engine
{

class GameBase;

class GameSettingsScreen
{
public:

	void Render(GameBase& rGame);

private:

	float mfLanguageHoverAnims[kLanguageCount] {};
	float mfDefaultsHoverAnim = 0.0f;
	float mfBackHoverAnim = 0.0f;
};

} // namespace engine
