#pragma once

#if defined(BT_CLIENT)

namespace game
{

class Camera final : public engine::CameraBase
{
public:

	Camera();
	void Update();
	void UpdateFlight(const engine::FrameInterpolateBase& rFrame, FXMVECTOR vecTarget, float fDeltaTime);
	void Fit(const XMFLOAT3A& rf3Center, float fRadius);
	void Zoom(float fWheelDelta);
	void RotateSun(const glm::vec2& rAnchor, const glm::vec2& rPosition);
	float SunAngle() const;
	arc_ball::ArcBall mArcBall {glm::vec3(0.0f), 0.75f, glm::mat4(1.0f)};
	XMVECTOR mVecModelCenter {0.0f, 0.0f, 0.0f, 1.0f};
	XMMATRIX mMatModelView {};
	XMMATRIX mMatModelPerspective {};
	XMVECTOR mVecModelEyePosition {};

private:
	static constexpr float kfInitialPitch = 0.5404195f;
	float mfRadius = 1.0f;
	float mfZoom = 1.0f;
};

} // namespace game

#endif // defined(BT_CLIENT)
