#pragma once

#include "Frame/CellStaticData.h"

namespace game
{

class ModelViewerApplication : public common::Singleton<ModelViewerApplication>
{
public:

	ModelViewerApplication();
	void UpdateDefaultComparison(bool bHeld);
	void SelectLightingPreset(int64_t iSlot);
	void ResetLightingPreset();
	bool Render();
	void RenderUi();
	void CreateGraphicsResources();
	void RenderModel(int64_t iCommandBuffer);
	bool mbWorldBackground = true;

private:
	enum class ViewerFlags : uint8_t
	{
		kComparingDefaults = 0x01,
		kPlaying = 0x02,
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
	void GenerateIslands();
	void ApplyLightingValues(const std::array<float, 20>& rValues);
	void SaveLightingPreset(const std::array<float, 20>& rValues);
	std::vector<Preview> mPreviews;
	int64_t miSelected = 0;
	int64_t miFittedSelection = -1;
	int64_t miSelectedAnimation = -1;
	std::chrono::duration<float> mAnimationTime {};
	common::Flags<ViewerFlags> mFlags;
	std::array<float, 20> mLightingTweaks {};
	int64_t miSelectedLightingPreset = 0;
	uint32_t muiLightingRevision = 0;
	int64_t miDragButton = -1;
	glm::vec2 mPreviousMousePosition {};

	common::Timer mTimer;
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
