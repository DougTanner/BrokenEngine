#pragma once

#include "Ui/WrapperBase.h"

namespace game
{

inline engine::Wrapper gWindDepositOmniWidth(3.0f, 0.1f, 5.0f);
inline engine::Wrapper gWindDepositOmniIntensity(0.03f, 0.0f, 0.1f);
inline engine::Wrapper gWindDepositOmniLengthMultiplier(5.0f, 0.5f, 10.0f);
inline engine::Wrapper gWindDepositFightersWidth(2.0f, 0.1f, 5.0f);
inline engine::Wrapper gWindDepositFightersIntensity(0.03f, 0.0f, 0.1f);
inline engine::Wrapper gWindDepositFightersLengthMultiplier(3.0f, 0.5f, 5.0f);
inline engine::Wrapper gWindDepositOmniBlastersWidth(1.0f, 0.1f, 2.0f);
inline engine::Wrapper gWindDepositOmniBlastersIntensity(0.01f, 0.0f, 0.05f);
inline engine::Wrapper gWindDepositOmniBlastersLengthMultiplier(3.0f, 0.5f, 6.0f);
inline engine::Wrapper gWindDepositExplosionsWidth(10.0f, 4.0f, 50.0f);
inline engine::Wrapper gWindDepositExplosionsIntensity(0.003f, 0.0f, 0.005f);

} // namespace game
