#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"

namespace engine
{

enum class LightingComparisonMode
{
	kDisabled,
	kFixtureZero,
	kFixtureReference,
};

enum class LightingComparisonCamera
{
	kLowest,
	kDefault,
	kEvent,
};

enum class LightingFixtureScenario
{
	kDensity,
	kIsolatedPoint,
	kIsolatedRectangle,
};

enum class LightingComparisonFlags : uint8_t
{
	kIndirect = 1 << 0,
	kShadows  = 1 << 1,
};

struct LightingComparisonSettings
{
	LightingComparisonMode eMode = LightingComparisonMode::kDisabled;
	LightingComparisonCamera eCamera = LightingComparisonCamera::kLowest;
	LightingFixtureScenario eScenario = LightingFixtureScenario::kDensity;
	int64_t iSourceCount = 100;
	int64_t iEvent = 0;
	int64_t iAreaSamples = 256;
	uint64_t uiCaptureRequest = 0;
	common::Flags<LightingComparisonFlags> flags = LightingComparisonFlags::kShadows;
	bool operator==(const LightingComparisonSettings&) const = default;
};

struct LightingCameraPose
{
	GridCoord coordinate {};
	XMFLOAT3A f3Eye {};
	XMFLOAT3A f3Target {};
	XMFLOAT3A f3Up {0.0f, 0.0f, 1.0f};
	float fVerticalFieldOfView = 0.0f;
	float fFraming = 0.0f;
	float fNear = 0.1f;
	float fFar = 1'000.0f;
};

struct LightingDiagnostics
{
	int64_t iSources = 0;
	int64_t iInstances = 0;
	int64_t iTraceOnlyInstances = 0;
	int64_t iTriangles = 0;
	// Capture status: not-run, receivers, tracing, composing, complete or failed.
	std::string_view status = "not-run";
	int64_t iCompletedTiles = 0;
	int64_t iTotalTiles = 0;
	std::string error;
	std::filesystem::path directory;
};

struct Lighting
{
public:
	Lighting();
	~Lighting();
	void BeginRender(int64_t iFramebuffer);
	void AddModels(common::crc_t modelCrc, common::crc_t instanceBufferCrc, int64_t iFirstInstance, int64_t iInstanceCount);
	void EndRender();
	void RecordPreparation(VkCommandBuffer vkCommandBuffer);
	LightingComparisonSettings requestedComparisonSettings;
	void ApplyComparisonSettings(bool bCaptureAllowed);
	void FinishCapture(int64_t iFramebuffer);
	void WriteProfile(nlohmann::json& rResult) const;
	void RecordFixtureDraws(VkCommandBuffer vkCommandBuffer, int64_t iFramebuffer);
	bool GetComparisonCamera(LightingCameraPose& rPose) const;
	// Latched counts plus capture progress; capture status survives Graphics resource recreation.
	LightingDiagnostics diagnostics;

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
	void CreateComparisonResources();
	void DestroyComparisonResources();
	bool IsCaptureTerrainReady() const;
	void PrepareComparisonPipelines();
	void RecordReceivers(VkCommandBuffer vkCommandBuffer);
	void RecordCapture(VkCommandBuffer vkCommandBuffer);
	void WriteFixtureSolids(std::span<shaders::LightingInstance> instances, std::span<shaders::LightingGeometry> geometries, std::span<VkAccelerationStructureInstanceKHR> accelerationInstances);
	struct State;
	std::unique_ptr<State> pState = nullptr;
};

} // namespace engine

#endif // defined(BT_CLIENT)
