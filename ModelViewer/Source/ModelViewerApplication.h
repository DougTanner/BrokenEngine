#pragma once

namespace game
{

class ModelViewerApplication : public common::Singleton<ModelViewerApplication>
{
public:

	ModelViewerApplication();
	bool Render();
	void RenderUi();
	void CreateGraphicsResources();
	void RenderModel(int64_t iCommandBuffer);

private:
	struct Preview
	{
		common::crc_t crc = 0;
		const char* pcPath = nullptr;
		// Cache rest output once so framebuffer publication needs no pose evaluation or heap allocation.
		std::vector<common::MeshData> meshData;
		std::vector<common::JointMatrix> jointMatrices;
		XMFLOAT3A f3Center {};
		float fRadius = 0.0f;
	};
	void PreparePreview(Preview& rPreview);
	std::vector<Preview> mPreviews;
	int64_t miSelected = 0;
	int64_t miFittedSelection = -1;
	bool mbOrbiting = false;

	common::Timer mTimer;
	std::unordered_map<engine::GridCoord, FrameInterpolate> mRenderInterpolates;
	std::vector<engine::GridCoord> mActiveCoordinates;
	int64_t miFrame = 0;
};

inline ModelViewerApplication* gpModelViewerApplication = nullptr;

int RunModelViewer(HINSTANCE hinstance);

} // namespace game
