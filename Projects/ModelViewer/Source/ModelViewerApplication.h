#pragma once

#include "Frame/CellStaticData.h"

#include "Game.h"

namespace game
{

class ModelViewerApplication : public common::Singleton<ModelViewerApplication>
{
public:

	ModelViewerApplication();
	void UpdateDefaultComparison(bool bHeld);
	void SelectLightingPreset(int64_t iSlot);
	void ResetLightingPreset();
	void ToggleFlight();
	void TogglePause();
	void SelectModel(int64_t iSelection);
	void SelectAdjacentModel(int64_t iStep);
	bool WorldVisible() const;
	void ResetDisplayTime();
	bool Render();
	void RenderUi();
	void CreateGraphicsResources();
	void RenderModel(int64_t iCommandBuffer);
	bool mbWorldBackground = true;
	float mfBackgroundGray = kVkHdrClearColor.float32[0];

private:
	enum class ViewerFlags : uint16_t
	{
		kComparingDefaults = 0x01,
		kPlaying = 0x02,
		kFlying = 0x04,
		kMetadataLoaded = 0x08,
		kMetadataValid = 0x10,
		kPaused = 0x20,
		kPicking = 0x40,
		kResetHardpointTab = 0x80,
		// Not editable; Save writes back the loaded tag.
		kNavigationObstacle = 0x100,
	};
	struct FlightBody
	{
		XMVECTOR vecPosition {};
		XMVECTOR vecDirection {1.0f, 0.0f, 0.0f, 0.0f};
		XMVECTOR vecVelocity {};
		XMVECTOR vecDestination {};
		float fDeltaRotation = 0.0f;
		float fClimbTilt = 0.0f;
	};

	struct Preview
	{
		common::crc_t crc = 0;
		const char* pcPath = nullptr;
		// Cache rest output once for bounds and rest-pose publication.
		std::vector<common::MeshData> meshData;
		std::vector<common::JointMatrix> jointMatrices;
		XMFLOAT3A f3Center {};
		float fRadius = 0.0f;
	};
	void PreparePreview(Preview& rPreview);
	template <typename VISITOR>
	void ForEachRestPosition(const Preview& rPreview, VISITOR&& rVisitor) const;
	void PickHardpoint();
	void LoadModelMetadata();
	void SaveModelMetadata();
	const common::ModelOutline& GetSelectedModelOutline() const;
	void UpdateModelOutline();
	void GetAlignedPreviewBounds(FXMMATRIX matAlignment, XMFLOAT3A& rf3Center, float& rfRadius) const;
	void ValidateModelAlignment();
	void GenerateIslands();
	bool DiscClear(FXMVECTOR vecPosition, float fHeight) const;
	void ResetFlight();
	void SelectDestination();
	void UpdateFlight(float fDeltaTime);
	void ApplyLightingValues(const std::array<float, 19>& rValues);
	void SaveLightingPreset(const std::array<float, 19>& rValues);
	std::vector<Preview> mPreviews;
	int64_t miSelected = 0;
	int64_t miFittedSelection = -1;
	int64_t miSelectedAnimation = -1;
	std::chrono::duration<float> mAnimationTime {};
	common::Flags<ViewerFlags> mFlags;
	std::array<float, 19> mLightingTweaks {};
	int64_t miSelectedLightingPreset = 0;
	uint32_t muiLightingRevision = 0;
	int64_t miDragButton = -1;
	glm::vec2 mPreviousMousePosition {};

	common::Timer mTimer;
	// Shader elapsed time advances only while unpaused, so resuming does not jump.
	std::chrono::nanoseconds mElapsedTime {};
	common::Timer mDisplayTimer;
	FlightBody mFlightBody;
	XMVECTOR mVecWaterAnchor {};
	float mfFlightHeight = 0.0f;
	common::crc_t mTallestIslandCrc = 0;
	int64_t miTallestPlacement = -1;
	int64_t miDestinationPlacement = -1;
	XMFLOAT3A mf3ModelScale {1.0f, 1.0f, 1.0f};
	XMFLOAT3A mf3ModelRotation {};
	XMFLOAT3A mf3ModelOffset {};
	// Not editable; Save writes back the loaded setting.
	common::ModelHitboxLevel meModelHitboxLevel = common::ModelHitboxLevel::kSphere;
	float mfOutlineInset = 0.0f;
	// The packed outline adjusted to mfOutlineInset, an approximation of the next bake. Invalid edits retain the last
	// valid line and leave their reason in mModelOutlineError.
	std::vector<XMFLOAT2> mOutlineVertices;
	std::string mModelOutlineError;
	std::vector<common::ModelHardpoint> mHardpoints;
	common::ModelHardpointKind meSelectedHardpointKind = common::ModelHardpointKind::kMissileTube;
	// Invalid edits retain the last complete renderable authored alignment.
	XMMATRIX mMatModelAlignment = XMMatrixIdentity();
	std::filesystem::path mModelMetadataPath;
	std::string mModelMetadataStatus;
	std::string mModelAlignmentError;
	engine::FrameInterpolateBase mCameraFrame;
	engine::GridCoord mIslandSeed = engine::kOriginCoordinate;
	engine::CellStaticData mIslandCell {.coordinate = engine::kOriginCoordinate};
	common::RandomEngine mSeedRandomEngine;
	std::unordered_map<engine::GridCoord, FrameInterpolate> mRenderInterpolates;
	std::vector<engine::GridCoord> mActiveCoordinates;
	int64_t miFrame = 0;
};

inline ModelViewerApplication* gpModelViewerApplication = nullptr;

int RunModelViewer(HINSTANCE hinstance);

} // namespace game
