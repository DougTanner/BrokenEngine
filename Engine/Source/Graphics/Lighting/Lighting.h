#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"

namespace engine
{

struct FrameInterpolateBase;

void DrainLightingCaptureCommands();

enum class LightingComparisonMode
{
	kDisabled,
	kFixtureZero,
	kFixtureReference,
};

enum class LightingComparisonCamera
{
	kClose,
	kWide,
	kEvent,
};

enum class LightingFixtureScenario
{
	kDensity,
	kIsolatedPoint,
	kIsolatedRectangle,
};

struct LightingComparisonSettings
{
	LightingComparisonMode eMode = LightingComparisonMode::kDisabled;
	LightingComparisonCamera eCamera = LightingComparisonCamera::kClose;
	LightingFixtureScenario eScenario = LightingFixtureScenario::kDensity;
	int64_t iSourceCount = 100;
	int64_t iEvent = 0;
	int64_t iAreaSamples = 256;
	uint64_t uiCaptureRequest = 0;
	uint64_t uiValidationRequest = 0;
	bool operator==(const LightingComparisonSettings&) const = default;
};

struct LightingCameraPose
{
	GridCoord coordinate {};
	XMFLOAT3A f3Eye {};
	XMFLOAT3A f3Target {};
	XMFLOAT3A f3Up {0.0f, 0.0f, 1.0f};
	float fVerticalFieldOfView = 0.0f;
	float fNear = 0.1f;
	float fFar = 1'000.0f;
};

struct LightingAllocation
{
	uint64_t uiPayload = 0;
	uint64_t uiAllocated = 0;
};

struct LightingDiagnostics
{
	uint64_t uiPublication = 0;
	uint64_t uiCapture = 0;
	int64_t iCompletedTiles = 0;
	int64_t iTotalTiles = 0;
	int64_t iSources = 0;
	int64_t iInstances = 0;
	int64_t iTriangles = 0;
	int64_t iMaterials = 0;
	uint64_t uiPersistentBytes = 0;
	uint64_t uiPeakBytes = 0;
	std::chrono::duration<double, std::milli> durationPreparation = 0ms;
	std::chrono::duration<double, std::milli> durationReference = 0ms;
	uint64_t uiResourceGeneration = 0;
	int64_t iFramebuffer = -1;
	uint64_t uiValidationRequested = 0;
	uint64_t uiValidationCompleted = 0;
	int64_t iTerrainWrittenOnce = 0;
	int64_t iTerrainUntouched = 0;
	int64_t iTerrainDuplicate = 0;
	int64_t iTerrainMismatches = 0;
	std::chrono::duration<double, std::milli> durationTerrain = 0ms;
	std::chrono::duration<double, std::milli> durationWorstTerrainTile = 0ms;
	std::chrono::duration<double, std::milli> durationTerrainValidation = 0ms;
	uint64_t uiSceneDiagnosticPublication = 0;
	int64_t iBadSources = 0;
	int64_t iBadSamples = 0;
	std::string_view residentPolicy = "Resident active-cell opaque models and terrain; MASK opaque; BLEND, water, smoke, sprites and shields omitted";
	std::string_view error;
};

struct Lighting
{
public:
	Lighting();
	~Lighting();
	void BeginRender(int64_t iFramebuffer);
	void AddFrame(const FrameInterpolateBase& rFrame);
	void AddModels(common::crc_t modelCrc, common::crc_t instanceBufferCrc, int64_t iFirstInstance, int64_t iInstanceCount);
	void EndRender();
	void RecordPreparation(VkCommandBuffer vkCommandBuffer);
	void SetComparison(const LightingComparisonSettings& rSettings);
	LightingComparisonSettings mRequestedComparisonSettings;
	void ApplyComparisonSettings();
	bool CapturePreparation(VkCommandBuffer vkPreparation, VkSemaphore vkGlobalFinished, VkSemaphore vkPreparationFinished);
	void FinishCapture(int64_t iFramebuffer);
	bool IsCaptureHeld() const;
	void CancelCapture();
	void WriteProfile(nlohmann::json& rResult) const;
	void RecordFixtureDraws(VkCommandBuffer vkCommandBuffer, int64_t iFramebuffer);
	bool GetComparisonCamera(LightingCameraPose& rPose) const;
	const LightingDiagnostics& GetDiagnostics() const;

private:
	friend class Graphics;
	void CreateResources();
	void DestroyResources();
	void CreateTerrainPipelines();
	void DestroyTerrainPipelines();
	void BindQueryDescriptors(const class Pipeline& rPipeline, int64_t iFramebuffer) const;
	void AddQueryDescriptorInfos(struct PipelineInfo& rInfo, int64_t iFramebuffer) const;
	void CreateFixtureResources();
	void PublishFixtureAnimation();
	void CreateSceneDiagnosticResources();
	void CreateSceneDiagnosticPipelines();
	void DestroySceneDiagnosticPipelines();
	void RecordSceneDiagnostics(VkCommandBuffer vkCommandBuffer);
	void CompleteSceneDiagnostics(int64_t iFramebuffer);
	void CompletePreparation(int64_t iFramebuffer);
	uint64_t HoldPublication(int64_t iFramebuffer);
	void ReleasePublication(int64_t iFramebuffer, uint64_t uiPublication);
	void CreateComparisonResources();
	void DestroyComparisonResources();
	void PrepareComparisonPipelines();
	void RecordReceivers(VkCommandBuffer vkCommandBuffer);
	void WriteCaptureManifest();
	void RecordCaptureStage(std::string_view stage);
	static LightingAllocation GetBufferAllocation(const class Buffer& rBuffer);
	void WriteFixtureSolids(std::span<shaders::LightingInstance> instances, std::span<shaders::LightingGeometry> geometries, std::span<VkAccelerationStructureInstanceKHR> accelerationInstances);
	struct State;
	std::unique_ptr<State> mpState = nullptr;
};

} // namespace engine

#endif // defined(BT_CLIENT)
