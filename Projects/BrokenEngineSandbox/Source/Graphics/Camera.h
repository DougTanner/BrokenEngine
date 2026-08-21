#pragma once

#if defined(BT_CLIENT)

namespace game
{

class Camera final : public engine::Camera
{
public:

	// Origin-coord anchor used by main-menu camera math. Origin is sampled like any other cell,
	// so this is just the (0, 0) cell center; the camera converges on the focused player anyway.
	static constexpr XMVECTOR kVecMenuIslandCenter {0.0f, 0.0f, 0.0f, 1.0f};

	// Main-menu camera target, as world-space XY offset from the menu island center.
	static constexpr XMVECTOR kVecMenuCameraOffset {20.4f, -76.3f, 0.0f, 0.0f};

	Camera();

	float SunAngle() const override;

	engine::global_id_t mLastTrackedPlayerId {};

protected:

	bool IsMainMenuFrame(const engine::FrameInterpolateBase& rFrameInterpolate) const override;
	engine::CameraTarget PullTarget(const engine::FrameInterpolateBase& rFrameInterpolate) override;
	void OnUpdateComplete() override;

};

} // namespace game

#endif // BT_CLIENT
