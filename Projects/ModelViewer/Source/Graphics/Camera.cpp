#include "Camera.h"

#if defined(BT_CLIENT)

namespace game
{

Camera::Camera()
: engine::CameraBase({.vecInitialPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)})
{
	mfSunAngle = 1.15f;
}

void Camera::Update()
{
	float fHalfFov = std::min(XM_PIDIV4 * 0.5f, std::atan(std::tan(XM_PIDIV4 * 0.5f) * engine::gpSwapchainManager->mfAspectRatio));
	float fDistance = mfZoom * 1.1f * mfRadius / std::sin(fHalfFov);
	glm::mat3 initialBasis(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, std::sin(kfInitialPitch), std::cos(kfInitialPitch)), glm::vec3(0.0f, -std::cos(kfInitialPitch), std::sin(kfInitialPitch)));
	glm::mat3 orientation = initialBasis * glm::mat3(mArcBall.getTransformation());
	glm::vec3 toEye = orientation[2];
	glm::vec3 up = orientation[1];
	XMVECTOR vecToEyeNormal = XMVectorSet(toEye.x, toEye.y, toEye.z, 0.0f);
	mVecModelEyePosition = XMVectorAdd(mVecModelCenter, XMVectorScale(vecToEyeNormal, fDistance));
	mMatModelView = XMMatrixLookAtRH(mVecModelEyePosition, mVecModelCenter, XMVectorSet(up.x, up.y, up.z, 0.0f));
	mMatModelPerspective = XMMatrixPerspectiveFovRH(XM_PIDIV4, engine::gpSwapchainManager->mfAspectRatio, std::max(0.001f * mfRadius, fDistance - 1.5f * mfRadius), fDistance + 2.0f * mfRadius);

	mVecPosition = XMVectorSetZ(mVecPosition, engine::gpFlightPlanes->Lowest().fHeightMeters);
	mfCameraEyeHeight = kfCameraEyeHeightDefault;
	mfShadowTexelEyeHeight = mfCameraEyeHeight;
	mfLightingTexelEyeHeight = mfCameraEyeHeight;
	mVecToEyeNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	mVecEyePosition = XMVectorAdd(mVecPosition, XMVectorSet(0.0f, 0.0f, mfCameraEyeHeight, 0.0f));
	CalculateMatricesAndVisibleArea();
}

void Camera::Fit(const XMFLOAT3A& rf3Center, float fRadius)
{
	mVecModelCenter = XMVectorSetW(XMLoadFloat3A(&rf3Center), 1.0f);
	mfRadius = fRadius;
	mArcBall = arc_ball::ArcBall(glm::vec3(0.0f), 0.75f, glm::mat4(1.0f));
	mfZoom = 1.0f;
}

void Camera::UpdateFlight(const engine::FrameInterpolateBase& rFrame, FXMVECTOR vecTarget, float fDeltaTime)
{
	engine::CameraBase::Update(rFrame, fDeltaTime, 0.0f, false, engine::CameraTarget::Tracked(vecTarget, XMVectorZero()));
	mMatModelView = mMatView;
	mMatModelPerspective = mMatPerspective;
	mVecModelEyePosition = mVecEyePosition;
}

void Camera::Zoom(float fWheelDelta)
{
	mfZoom = std::clamp(mfZoom * std::exp(-0.15f * fWheelDelta), 0.5f, 5.0f);
}

void Camera::RotateSun(const glm::vec2& rAnchor, const glm::vec2& rPosition)
{
	arc_ball::ArcBall increment(glm::vec3(0.0f), 0.75f, glm::mat4(1.0f));
	increment.beginDrag(rAnchor);
	increment.drag(rPosition);
	XMFLOAT4X4A f4x4View {};
	XMStoreFloat4x4A(&f4x4View, mMatModelView);
	glm::mat3 cameraBasis(glm::vec3(f4x4View._11, f4x4View._21, f4x4View._31), glm::vec3(f4x4View._12, f4x4View._22, f4x4View._32), glm::vec3(f4x4View._13, f4x4View._23, f4x4View._33));
	// ArcBall rotates the camera basis; a direction follows the inverse rotation in that basis.
	glm::mat3 screenRotation = glm::transpose(glm::mat3(increment.getTransformation()));
	glm::quat worldRotation = glm::quat_cast(cameraBasis * screenRotation * glm::transpose(cameraBasis));
	mModelSunRotation = glm::normalize(worldRotation * mModelSunRotation);
}

float Camera::SunAngle() const
{
	return mfSunAngle;
}

} // namespace game

#endif // defined(BT_CLIENT)
