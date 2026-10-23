#pragma once

#if defined(BT_CLIENT)

namespace game
{

class Camera final : public engine::CameraBase
{
public:

	Camera();
	void Update();
	void Fit(const XMFLOAT3A& rf3Center, float fRadius);
	void Orbit(float fYawDelta, float fPitchDelta);
	void Zoom(float fWheelDelta);

private:
	static constexpr float kfInitialPitch = 0.5404195f;
	float mfRadius = 1.0f;
	float mfYaw = 0.0f;
	float mfPitch = kfInitialPitch;
	float mfZoom = 1.0f;
};

} // namespace game

#endif // defined(BT_CLIENT)
