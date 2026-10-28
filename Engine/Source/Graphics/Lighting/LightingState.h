#pragma once

#if defined(BT_CLIENT)

#include "../../../Data/Shaders/Lighting/LightingSceneDiagnostics.h"
#include "Lighting.h"
#include "LightingAccelerationStructure.h"
#include "LightingFixture.h"
#include "LightingModels.h"
#include "Terrain.h"

namespace engine
{

struct LightingModelRange
{
	common::crc_t modelCrc = 0;
	common::crc_t instanceBufferCrc = 0;
	int64_t iFirstInstance = 0;
	int64_t iInstanceCount = 0;
};

struct LightingFramebuffer
{
	// TEMP START matched memory cost
	double fMatchedTerrainPrepareMs = 0.0;
	double fMatchedTerrainPrepareTotalMs = 0.0;
	uint64_t uiMatchedTerrainPrepareCalls = 0;
	// TEMP END matched memory cost
	Buffer sources;
	Buffer publication;
	Buffer instances;
	Buffer geometries;
	Buffer accelerationInstances;
	Buffer samples;
	Buffer fixtureMaterials;
	Buffer diagnosticParameters;
	Buffer diagnosticResult;
	std::unique_ptr<StagingBuffer> diagnosticReadback;
	uint64_t uiDiagnosticPublication = 0;
	LightingTerrainOperations diagnosticOperations;
	LightingTerrainOperations modelOperations;
	LightingTerrainOperations sceneOperations;
	LightingAccelerationStructure scene;
	uint32_t uiInstanceCount = 0;
	uint32_t uiGeometryCount = 0;
	uint64_t uiGeneration = 0;
	uint64_t uiHeldPublication = 0;
	uint64_t uiRecordedPublication = 0;
	uint64_t uiBuiltSceneGeneration = std::numeric_limits<uint64_t>::max();
	uint32_t uiFixtureFirstInstance = 0;
	uint32_t uiFixtureFirstGeometry = 0;
	uint32_t uiFixtureAnimationInstance = std::numeric_limits<uint32_t>::max();
	std::vector<LightingModelRange> modelRanges;
	LightingModelCounts modelCounts;
	LightingTerrainCounts terrainCounts;
	LightingDiagnostics pendingDiagnostics;
	std::vector<std::tuple<GridCoord, int64_t, float>> frameIdentities;
};

struct Lighting::State
{
	enum class CaptureFlags : uint8_t
	{
		kCancel = 1,
		kReferenceValid = 2,
		kMutationPending = 4,
	};
	struct Comparison
	{
		static constexpr VkExtent3D kExtent {1'920, 1'080, 1};
		std::array<Texture, 4> receivers;
		Texture depth;
		Texture reference;
		VkRenderPass vkRenderPass = VK_NULL_HANDLE;
		VkFramebuffer vkFramebuffer = VK_NULL_HANDLE;
		VkCommandPool vkCommandPool = VK_NULL_HANDLE;
		VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;
		VkFence vkFence = VK_NULL_HANDLE;
		VkQueryPool vkQueryPool = VK_NULL_HANDLE;
		Pipeline compute;
		Pipeline terrain;
		Pipeline water;
		Pipeline solidReceiver;
		Pipeline solidForward;
		Buffer solidDraws;
		using ModelKey = std::tuple<common::crc_t, common::crc_t, int64_t>;
		struct ModelKeyHash
		{
			std::size_t operator()(const ModelKey& rKey) const
			{
				return std::hash<common::crc_t> {}(std::get<0>(rKey)) ^ (std::hash<common::crc_t> {}(std::get<1>(rKey)) << 1) ^ (std::hash<int64_t> {}(std::get<2>(rKey)) << 2);
			}
		};
		std::unordered_map<ModelKey, std::unique_ptr<ModelPipeline>, ModelKeyHash> models;
		std::unique_ptr<ModelPipeline> animationForward;
		uint64_t uiRequest = 0;
		uint64_t uiHeldPublication = 0;
		uint64_t uiSequence = 0;
		uint64_t uiResumePublication = 0;
		int64_t iCompletedTiles = 0;
		int64_t iTotalTiles = 0;
		std::string status = "not-run";
		std::string error;
		std::filesystem::path directory;
		std::string manifestPath;
		std::string manifestHash;
		nlohmann::json manifest;
		std::chrono::steady_clock::time_point start;
		std::chrono::duration<double, std::milli> durationReference = 0ms;
		double fFirstTileGpuMs = 0.0;
		common::Flags<CaptureFlags> flags;
		TextureLayout eReferenceLayout = TextureLayout::kGeneral;
		LightingAllocation exportReadback;
		LightingTerrainOperations receiverOperations;
		LightingTerrainOperations referenceOperations;
		LightingTerrainOperations exportOperations;
	};
	std::unique_ptr<Comparison> comparison;
	// TEMP START original native request capture
	struct NativeCapture
	{
		enum class Flags : uint8_t
		{
			kActive = 1,
			kCleanupFailed = 2,
		};
		RENDERDOC_API_1_6_0* pApi = nullptr;
		RENDERDOC_DevicePointer pDevice = nullptr;
		RENDERDOC_WindowHandle pWindow = nullptr;
		common::Flags<Flags> flags;
		uint64_t uiRequest = 0;
		uint64_t uiPublication = 0;
		uint64_t uiGeneration = 0;
		int64_t iFramebuffer = -1;
		int64_t iBaselineCount = 0;
		int64_t iFinalCount = 0;
		int64_t iBeforeActive = 0;
		int64_t iStartActive = 0;
		int64_t iEndResult = 0;
		int64_t iEndActive = 0;
		int64_t iDiscardResult = 0;
		int64_t iDiscardActive = 0;
		int64_t iSizingResult = 0;
		int64_t iReadResult = 0;
		uint32_t uiPathBytes = 0;
		uint32_t uiReadBytes = 0;
		const char* pcStatus = "not-requested";
		std::string path;

		void Abandon() noexcept
		{
			if (!(flags & Flags::kActive))
			{
				return;
			}

			if (flags & Flags::kCleanupFailed)
			{
				return;
			}
			iDiscardResult = pApi->DiscardFrameCapture(pDevice, pWindow);
			iDiscardActive = pApi->IsFrameCapturing();
			if (iDiscardResult == 0 || iDiscardActive != 0)
			{
				flags.Set(Flags::kCleanupFailed);
				pcStatus = "discard-failed";
				return;
			}
			flags.Set(Flags::kActive, false);
			pcStatus = "aborted";
		}
	} nativeCapture;
	// TEMP END original native request capture
	struct FixtureSolid
	{
		Buffer vertices;
		Buffer indices;
		LightingAccelerationStructure acceleration;
		bool bBuilt = false;
	};
	static constexpr common::crc_t kFixtureAnimationBuffer = common::CrcConsteval("LightingFixtureAnimation");
	std::vector<std::unique_ptr<FixtureSolid>> fixtureSolids;
	LightingTerrain terrain;
	std::unique_ptr<LightingModels> models;
	std::unique_ptr<LightingFixture> fixture;
	std::vector<std::unique_ptr<LightingFramebuffer>> framebuffers;
	Pipeline diagnosticPipeline;
	LightingFixtureScenario eScenario = LightingFixtureScenario::kDensity;
	int64_t iFramebuffer = 0;
	LightingComparisonSettings settings;
	LightingDiagnostics completedDiagnostics;
	shaders::LightingSceneDiagnosticResult completedSceneDiagnostics {};
	uint64_t uiPublication = 0;
	uint64_t uiPersistentBytes = 0;
	uint64_t uiPeakBytes = 0;
	uint64_t uiValidationRequest = 0;
	uint64_t uiResourceGeneration = 0;
	uint64_t uiServicedCapture = 0;
	uint64_t uiServicedValidation = 0;
	nlohmann::json lastCaptureReport;
	nlohmann::json pendingResume;
	std::filesystem::path pendingResumeDirectory;
};

} // namespace engine

#endif // defined(BT_CLIENT)
