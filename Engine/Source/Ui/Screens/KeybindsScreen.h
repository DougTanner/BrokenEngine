#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class KeybindsScreen
{
public:

	void Render();

	static constexpr int64_t kiMaxRowCount = 32;

	float mpfBindButtonHoverAnimations[kiMaxRowCount] {};
	float mfDefaultsHoverAnimation = 0.0f;
	float mfBackHoverAnimation = 0.0f;
};

} // namespace engine

#endif // BT_CLIENT
