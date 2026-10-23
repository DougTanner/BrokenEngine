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
	XMVECTOR vecToEyeNormal = XMVectorSet(std::sin(mfYaw) * std::cos(mfPitch), -std::cos(mfYaw) * std::cos(mfPitch), std::sin(mfPitch), 0.0f);
	mVecEyePosition = XMVectorAdd(mVecPosition, XMVectorScale(vecToEyeNormal, fDistance));
	mVecToEyeNormal = XMVector3Normalize(XMVectorSubtract(mVecEyePosition, mVecPosition));
	mMatView = XMMatrixLookAtRH(mVecEyePosition, mVecPosition, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
	mMatPerspective = XMMatrixPerspectiveFovRH(XM_PIDIV4, engine::gpSwapchainManager->mfAspectRatio, std::max(0.001f * mfRadius, fDistance - 1.5f * mfRadius), fDistance + 2.0f * mfRadius);
	mfCameraEyeHeight = fDistance;
	mfShadowTexelEyeHeight = mfCameraEyeHeight;
	mfLightingTexelEyeHeight = mfCameraEyeHeight;
	XMFLOAT3A f3Center {};
	XMStoreFloat3A(&f3Center, mVecPosition);
	mf4RenderVisibleArea = {f3Center.x - fDistance, f3Center.y + fDistance, f3Center.x + fDistance, f3Center.y - fDistance};
	mf4LargeVisibleArea = mf4RenderVisibleArea;
	mf2VisibleAreaQuadSize = {1.0f, 1.0f};
}

void Camera::Fit(const XMFLOAT3A& rf3Center, float fRadius)
{
	mVecPosition = XMVectorSetW(XMLoadFloat3A(&rf3Center), 1.0f);
	mfRadius = fRadius;
	mfYaw = 0.0f;
	mfPitch = kfInitialPitch;
	mfZoom = 1.0f;
}

void Camera::Orbit(float fYawDelta, float fPitchDelta)
{
	mfYaw = std::remainder(mfYaw + fYawDelta, XM_2PI);
	mfPitch = std::clamp(mfPitch + fPitchDelta, -0.49f * XM_PI, 0.49f * XM_PI);
}

void Camera::Zoom(float fWheelDelta)
{
	mfZoom = std::clamp(mfZoom * std::exp(-0.15f * fWheelDelta), 0.5f, 5.0f);
}

} // namespace game

#endif // defined(BT_CLIENT)
