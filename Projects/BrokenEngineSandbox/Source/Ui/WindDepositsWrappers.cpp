#include "WindDepositsWrappers.h"

namespace game
{

engine::Wrapper gWindDepositOmniWidth(3.0f, 0.1f, 5.0f);
engine::Wrapper gWindDepositOmniIntensity(0.03f, 0.0f, 0.1f);
engine::Wrapper gWindDepositOmniLengthMultiplier(5.0f, 0.5f, 10.0f);

engine::Wrapper gWindDepositFightersWidth(2.0f, 0.1f, 5.0f);
engine::Wrapper gWindDepositFightersIntensity(0.03f, 0.0f, 0.1f);
engine::Wrapper gWindDepositFightersLengthMultiplier(3.0f, 0.5f, 5.0f);

engine::Wrapper gWindDepositOmniBlastersWidth(1.0f, 0.1f, 2.0f);
engine::Wrapper gWindDepositOmniBlastersIntensity(0.05f, 0.0f, 0.2f);
engine::Wrapper gWindDepositOmniBlastersLengthMultiplier(3.0f, 0.5f, 6.0f);

engine::Wrapper gWindDepositExplosionsWidth(10.0f, 4.0f, 50.0f);
engine::Wrapper gWindDepositExplosionsIntensity(0.003f, 0.0f, 0.005f);

} // namespace game
