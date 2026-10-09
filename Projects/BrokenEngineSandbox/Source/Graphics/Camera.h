#pragma once

#if defined(BT_CLIENT)

namespace game
{

class Camera final : public engine::CameraBase
{
public:

	// Origin-coord anchor used by main-menu camera math. Origin is sampled like any other cell,
	// so this is just the (0, 0) cell center; the camera converges on the focused player anyway.
	static constexpr XMVECTOR kVecMenuIslandCenter {0.0f, 0.0f, 0.0f, 1.0f};

	// Main-menu XY offset from the menu island center is local to the origin cell.
	static constexpr XMVECTOR kVecMenuCameraOffset {20.4f, -76.3f, 0.0f, 0.0f};

	Camera();

	float SunAngle() const override;

	engine::GlobalId mLastTrackedPlayerIdentifier {};

protected:

	bool IsMainMenuFrame(const engine::FrameInterpolateBase& rFrameInterpolate) const override;
	engine::CameraTarget PullTarget(const engine::FrameInterpolateBase& rFrameInterpolate) override;

};

} // namespace game

#endif // BT_CLIENT
