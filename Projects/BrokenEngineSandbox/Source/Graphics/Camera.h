#pragma once

#if defined(BT_CLIENT)

namespace game
{

class Camera final : public engine::CameraBase
{
public:

	// Origin-coord anchor used by main-menu camera math. Origin is sampled like any other cell,
	// so this is just the (0, 0) cell center; the camera converges on the focused owned entity anyway.
	static constexpr XMVECTOR kVecMenuIslandCenter {0.0f, 0.0f, 0.0f, 1.0f};

	// Main-menu XY offset from the menu island center is local to the origin cell.
	static constexpr XMVECTOR kVecMenuCameraOffset {-34.6f, -101.0f, 0.0f, 0.0f};
	static constexpr float kfMenuCameraFraming = 344.2f;

	Camera();

	float SunAngle() const;

	engine::CameraTarget PullTarget(const engine::FrameInterpolateBase& rFrameInterpolate);

	// The main menu and gameplay each keep their own pose. Free-camera builds remember the menu pose, saved in
	// ClientState.bin; other builds never store it, so the menu always opens at the default pose.
	void StorePose(bool bMainMenu);
	void ApplyPose(bool bMainMenu);
	void ResetMenuPose();

	engine::GlobalId mLastTrackedOwnedEntityIdentifier {};

	XMFLOAT2 mf2MenuPosition {};
	float mfMenuFraming = kfMenuCameraFraming;
	float mfGameFraming = kfCameraFramingInitial;

};

} // namespace game

#endif // BT_CLIENT
