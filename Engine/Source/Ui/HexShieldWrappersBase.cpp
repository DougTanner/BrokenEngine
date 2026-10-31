#if defined(BT_CLIENT)

#include "Ui/HexShieldWrappersBase.h"

namespace engine
{

engine::Wrapper gHexShieldGrow(0.1f, 0.05f, 0.2f);
engine::Wrapper gHexShieldEdgeDistance(0.94f, 0.9f, 0.955f);
engine::Wrapper gHexShieldEdgeMultiplier(10.0f, 5.0f, 20.0f);

engine::Wrapper gHexShieldWaveMultiplier(0.35f, 0.0f, 1.0f);
engine::Wrapper gHexShieldWaveDotMultiplier(5.0f, 0.5f, 10.0f);
engine::Wrapper gHexShieldWaveIntensityMultiplier(12.0f, 0.5f, 20.0f);
engine::Wrapper gHexShieldWaveIntensityPower(1.6f, 0.25f, 4.0f);
engine::Wrapper gHexShieldWaveFalloffPower(2.1f, 0.25f, 4.0f);

engine::Wrapper gHexShieldDirectionFalloffPower(4.35f, 2.0f, 10.0f);
engine::Wrapper gHexShieldDirectionMultiplier(4.5f, 0.5f, 8.0f);

} // namespace engine

#endif // defined(BT_CLIENT)
